#include "mock_adapters.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <cmath>

namespace workbench::embedded {

MockDeviceAdapter::MockDeviceAdapter(DeviceType type, const std::string& id, const std::string& name)
    : type_(type), device_id_(id), name_(name) {
    if (type_ == DeviceType::MICROCONTROLLER) {
        base_voltage_V_ = 3.30;
        base_temp_C_ = 36.5;
    } else if (type_ == DeviceType::FPGA) {
        base_voltage_V_ = 1.20; // Core rail
        base_temp_C_ = 48.0;
    } else {
        base_voltage_V_ = 3.30; // VREF sensed
        base_temp_C_ = 31.0;
    }
}

DeviceCapabilities MockDeviceAdapter::get_capabilities() const {
    DeviceCapabilities caps;
    caps.device_id = device_id_;
    caps.model_name = name_;
    caps.type = type_;

    if (type_ == DeviceType::MICROCONTROLLER) {
        caps.firmware_or_bitstream_version = "v1.4.2-rtos (Telemetry FW)";
        caps.operating_voltage_V = 3.30;
        caps.supported_protocols = {"UART (115200 / 921600 baud)", "SPI (up to 20 MHz)", "I2C FastMode+", "SWD / JTAG"};
        caps.connector_type = "USB Type-C (Host CDC) + 10-pin 0.05\" Cortex Debug Header";
        caps.clock_frequency = "170 MHz (ARM Cortex-M4F)";
        caps.pinout_summary = "PA9/PA10: USART1 Telemetry | PA13: SWDIO/TMS | PA14: SWCLK/TCK | PB3: SWO/TDO | NRST";
    } else if (type_ == DeviceType::FPGA) {
        caps.firmware_or_bitstream_version = "bitstream_thermal_accelerator_v2.bin";
        caps.operating_voltage_V = 1.20; // Core rail (3.3V I/O bank)
        caps.supported_protocols = {"SPI Slave (12 MHz)", "JTAG (IEEE 1149.1 TAP)", "PMOD GPIO"};
        caps.connector_type = "Standard 10-pin 0.1\" Dual-row JTAG Header + PMOD Expansion";
        caps.clock_frequency = "48 MHz High-Speed Internal Oscillator";
        caps.pinout_summary = "Pin 25: TCK | Pin 26: TMS | Pin 27: TDI | Pin 28: TDO | VCC_IO: 3.3V | VCC_CORE: 1.2V";
    } else {
        caps.firmware_or_bitstream_version = "FT2232H MPSSE ROM v2.4";
        caps.operating_voltage_V = 3.30; // Target VREF Sensing 1.2V - 3.3V
        caps.supported_protocols = {"JTAG IEEE 1149.1 (Master)", "SWD (Serial Wire Debug)", "UART Bridge (Channel B)"};
        caps.connector_type = "USB 2.0 High-Speed (Host) to 10-pin Ribbon Cable";
        caps.clock_frequency = "30 MHz Max JTAG Clock (TCK)";
        caps.pinout_summary = "Master Pins: TCK (Out), TMS (Out), TDI (Out), TDO (In), nSRST (Out/Open-Drain), VREF (In)";
    }

    return caps;
}

bool MockDeviceAdapter::connect() {
    state_ = DeviceState::CONNECTED;
    current_fault_ = FaultType::NONE;
    fault_description_.clear();
    return true;
}

bool MockDeviceAdapter::disconnect() {
    state_ = DeviceState::DISCONNECTED;
    current_fault_ = FaultType::NONE;
    fault_description_.clear();
    return true;
}

void MockDeviceAdapter::inject_fault(FaultType fault, const std::string& reason) {
    current_fault_ = fault;
    state_ = DeviceState::FAULTED;
    if (!reason.empty()) {
        fault_description_ = reason;
    } else {
        if (fault == FaultType::COMMUNICATION_TIMEOUT) {
            fault_description_ = "Target device failed to acknowledge within 500 ms timeout window.";
        } else if (fault == FaultType::CRC_CHECKSUM_ERROR) {
            fault_description_ = "Telemetry packet received with corrupted CRC32 checksum mismatch (0xDEADBEEF vs 0x54A1C892).";
        } else if (fault == FaultType::VOLTAGE_BROWNOUT) {
            fault_description_ = "Target rail voltage dropped below minimum operating threshold (< 2.85V).";
        }
    }
}

bool MockDeviceAdapter::recover() {
    if (state_ == DeviceState::FAULTED || state_ == DeviceState::DISCONNECTED) {
        state_ = DeviceState::RECOVERING;
        // Reset fault condition
        current_fault_ = FaultType::NONE;
        fault_description_ = "Recovery handshake completed. Link re-established.";
        state_ = DeviceState::CONNECTED;
        return true;
    }
    return false;
}

TelemetryData MockDeviceAdapter::read_telemetry() {
    TelemetryData t;
    auto now = std::chrono::system_clock::now().time_since_epoch();
    t.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    t.sequence_id = ++telemetry_counter_;

    if (state_ == DeviceState::FAULTED) {
        t.fault_active = true;
        t.fault_message = fault_description_;
        t.status_word = "FAULT_STATE_ACTIVE";
        t.active_mode = "HALTED / SAFE_STATE";
        t.rail_voltage_V = (current_fault_ == FaultType::VOLTAGE_BROWNOUT) ? 2.65 : base_voltage_V_;
        t.current_mA = 0.0;
        t.core_temperature_C = base_temp_C_;
        return t;
    }

    if (state_ != DeviceState::CONNECTED) {
        t.status_word = "OFFLINE";
        t.active_mode = "IDLE";
        return t;
    }

    // Generate minor telemetry noise for realism
    double noise_v = ((rand() % 100) - 50) * 0.001; // +/- 0.05 V
    double noise_t = ((rand() % 100) - 50) * 0.02;  // +/- 1.0 C

    t.rail_voltage_V = base_voltage_V_ + noise_v;
    t.core_temperature_C = base_temp_C_ + noise_t;
    t.fault_active = false;
    t.fault_message = "";

    if (type_ == DeviceType::MICROCONTROLLER) {
        t.current_mA = 42.5 + ((rand() % 20) - 10) * 0.2;
        t.status_word = "SYS_OK [RTOS_RUNNING, HEAP_78PCT_FREE]";
        t.active_mode = "TELEMETRY_STREAMING";
    } else if (type_ == DeviceType::FPGA) {
        t.current_mA = 185.0 + ((rand() % 50) - 25) * 0.4;
        t.status_word = "BITSTREAM_LOADED [PLL_LOCKED, DONE_HIGH]";
        t.active_mode = "COPROCESSOR_READY";
    } else {
        t.current_mA = 68.0;
        t.status_word = "MPSSE_ACTIVE [TAP_STATE: IDLE, TCK=15MHz]";
        t.active_mode = "JTAG_BRIDGE_READY";
    }

    return t;
}

// Manager
EmbeddedWorkbenchManager::EmbeddedWorkbenchManager() {
    devices_.push_back(std::make_unique<MockDeviceAdapter>(
        DeviceType::MICROCONTROLLER, "mcu_stm32g4", "STM32G474RE Microcontroller"));
    devices_.push_back(std::make_unique<MockDeviceAdapter>(
        DeviceType::FPGA, "fpga_ice40", "Lattice iCE40UP5K UltraPlus FPGA"));
    devices_.push_back(std::make_unique<MockDeviceAdapter>(
        DeviceType::JTAG_DEBUGGER, "jtag_ft2232h", "FT2232H High-Speed JTAG/SWD Debugger"));
}

std::vector<DeviceCapabilities> EmbeddedWorkbenchManager::discover_devices() {
    std::vector<DeviceCapabilities> list;
    list.reserve(devices_.size());
    for (const auto& d : devices_) {
        list.push_back(d->get_capabilities());
    }
    return list;
}

MockDeviceAdapter* EmbeddedWorkbenchManager::get_adapter(const std::string& id) {
    for (auto& d : devices_) {
        if (d->get_id() == id) {
            return d.get();
        }
    }
    return nullptr;
}

std::string EmbeddedWorkbenchManager::get_block_diagram_mermaid() {
    return R"MERMAID(graph LR
    subgraph Host["Host PC (Windows 11 x64)"]
        UI["Unified Engineering Workbench UI"]
        CPP["C++ Core Backend Engine"]
        UI <-->|IPC / JSON-RPC| CPP
    end

    subgraph USB_Layer["Host Physical USB Interfaces"]
        USB_HUB["Isolated Powered USB Hub (5.0V VBUS)"]
        CPP -->|WinUSB / CDC ACM| USB_HUB
    end

    subgraph Hardware_Workbench["Target Workbench Assembly"]
        subgraph Debugger["JTAG Debug Adapter (FT2232H)"]
            JTAG_CORE["FT2232H Dual-Channel Bridge"]
            VREF_SENSE["Target VREF Voltage Sensor (1.2V-3.3V)"]
            LVL_SHIFTER["Auto-Direction Level Translators (TXB0108)"]
        end

        subgraph MCU_Board["MCU Target: STM32G474RE"]
            MCU_CORE["ARM Cortex-M4F @ 170MHz"]
            MCU_REG["3.3V LDO Regulator"]
            MCU_SWD["10-pin Cortex Debug Port"]
        end

        subgraph FPGA_Board["FPGA Target: Lattice iCE40UP5K"]
            FPGA_CORE["FPGA Fabric + 1Mb SPRAM"]
            BUCK_1V2["1.2V Core Buck Converter"]
            BUCK_3V3["3.3V IO LDO Regulator"]
            FPGA_TAP["IEEE 1149.1 JTAG TAP Port"]
        end

        STAR_GND((Star Ground Point))
    end

    USB_HUB -->|USB 480 Mbps| JTAG_CORE
    USB_HUB -->|USB 12 Mbps CDC| MCU_CORE

    JTAG_CORE -->|JTAG TCK, TMS, TDI| LVL_SHIFTER
    LVL_SHIFTER -->|3.3V Level| MCU_SWD
    LVL_SHIFTER -->|3.3V/1.2V Level| FPGA_TAP

    MCU_SWD -->|TDO / SWO Feedback| LVL_SHIFTER
    FPGA_TAP -->|TDO Scan Chain| LVL_SHIFTER
    LVL_SHIFTER -->|TDO In| JTAG_CORE

    MCU_REG -->|3.3V VREF| VREF_SENSE
    MCU_CORE <-->|High-Speed SPI Bus (12 MHz)| FPGA_CORE

    MCU_REG --- STAR_GND
    BUCK_1V2 --- STAR_GND
    BUCK_3V3 --- STAR_GND
    JTAG_CORE --- STAR_GND
)MERMAID";
}

std::string EmbeddedWorkbenchManager::get_pin_voltage_table_markdown() {
    return R"(| Interface / Domain | Nominal Voltage | Connector / Form Factor | Signal Ownership & Pin Assignments | Grounding & Protection |
| :--- | :---: | :--- | :--- | :--- |
| **Host USB 2.0** | +5.0 VDC (VBUS) | Dual Type-C / Micro-B | D+, D-, VBUS, GND. Host communicates with JTAG engine & MCU CDC | Chassis ground via shielded shell; 500mA polyfuse protection |
| **MCU Digital Core & I/O** | +3.3 VDC (±3%) | 10-pin 0.05" Cortex Header | PA13 (SWDIO/TMS), PA14 (SWCLK/TCK), PB3 (SWO/TDO), NRST (Bi-dir reset) | Star Ground; decoupling: 100nF per VDD pin + 4.7uF bulk tantalum |
| **FPGA I/O Banks (Bank 0-2)**| +3.3 VDC (±5%) | 10-pin 0.1" Dual Header | Pin 1: VCC_IO (3.3V), Pin 2: GND, Pin 3: TCK, Pin 4: TDO, Pin 5: TMS, Pin 7: TDI | ESD suppressor diodes on JTAG pins; Ferrite bead on 3.3V analog rail |
| **FPGA Core Fabric** | +1.2 VDC (±2%) | Onboard BGA Supply Rail | Internal VCC_CORE supplying LUTs, DSP multipliers, and Block RAM | Low-ESR ceramic caps (0402 1uF) placed directly under BGA via matrix |
| **Level Shifter / VREF** | 1.2 V to 3.3 V | Inline PCB Buffer (TXB0108)| Port A (Target VREF side) tracks Target rail; Port B (Host side) tracks 3.3V | Automatic direction sensing with 10k pull-up on OE (Output Enable) |
| **Inter-IC High Speed Bus**| +3.3 V CMOS | PMOD / Ribbon Pinout | MCU Master -> MOSI (PB5), MISO (PB4), SCK (PB3), CS# (PA4) -> FPGA Slave | Series 22-ohm damping resistors to eliminate transmission line ringing |
)";
}

std::string EmbeddedWorkbenchManager::get_jtag_rationale_explanation() {
    return "ARCHITECTURAL JUSTIFICATION: WHY JTAG IS A DEBUG/TEST INTERFACE, NOT A GENERIC COMPUTE BOARD\n\n"
           "1. Physical & Architectural Paradigm:\n"
           "   JTAG (IEEE Standard 1149.1) is fundamentally a shift-register boundary-scan architecture designed for hardware testability,\n"
           "   silicon debugging, and flash/bitstream programming. It is implemented as a synchronous, 16-state finite state machine (TAP Controller)\n"
           "   driven by a master clock (TCK) and test mode select (TMS). It has no notion of peer-to-peer compute, packet framing, or dynamic addressing.\n\n"
           "2. Signal Ownership & Bus Asymmetry:\n"
           "   A JTAG adapter is strictly a BUS MASTER. It alone drives TCK, TMS, and TDI. Target boards (MCU, FPGA) are SLAVES that only assert TDO\n"
           "   during specific shift states (Shift-IR or Shift-DR). Target boards cannot initiate communication, request bus access, or interrupt the host.\n\n"
           "3. Bandwidth, Protocol & Voltage Heterogeneity:\n"
           "   - Bit-serial nature: JTAG shifts bit-by-bit without hardware error correction or transport layers, resulting in low effective throughput.\n"
           "   - Voltage dependency: JTAG has no universal voltage. An FPGA core may run at 1.2V while an MCU runs at 3.3V. The JTAG adapter must sense\n"
           "     VREF and use active level translation.\n"
           "   - Contrast with compute buses: Compute systems utilize packetized, point-to-point or packet-switched interconnects (e.g. PCIe, Ethernet, USB 3,\n"
           "     or high-speed AXI/SPI) featuring hardware arbitration, flow control, and memory mapping. JTAG lacks all of these by design.";
}

std::string EmbeddedWorkbenchManager::get_all_devices_json() {
    std::ostringstream oss;
    oss << "{";
    oss << "\"devices\":[";
    for (size_t i = 0; i < devices_.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& d = devices_[i];
        auto caps = d->get_capabilities();
        auto telem = d->read_telemetry();

        oss << "{";
        oss << "\"id\":\"" << caps.device_id << "\",";
        oss << "\"name\":\"" << caps.model_name << "\",";
        oss << "\"type\":" << static_cast<int>(caps.type) << ",";
        oss << "\"state\":" << static_cast<int>(d->get_state()) << ",";
        oss << "\"voltage_V\":" << telem.rail_voltage_V << ",";
        oss << "\"current_mA\":" << telem.current_mA << ",";
        oss << "\"temp_C\":" << telem.core_temperature_C << ",";
        oss << "\"status_word\":\"" << telem.status_word << "\",";
        oss << "\"active_mode\":\"" << telem.active_mode << "\",";
        oss << "\"fault_active\":" << (telem.fault_active ? "true" : "false") << ",";
        oss << "\"fault_message\":\"" << telem.fault_message << "\",";
        oss << "\"firmware\":\"" << caps.firmware_or_bitstream_version << "\",";
        oss << "\"clock\":\"" << caps.clock_frequency << "\",";
        oss << "\"connector\":\"" << caps.connector_type << "\",";
        oss << "\"pinout\":\"" << caps.pinout_summary << "\"";
        oss << "}";
    }
    oss << "]}";
    return oss.str();
}

} // namespace workbench::embedded
