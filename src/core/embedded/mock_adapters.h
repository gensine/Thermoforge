#ifndef MOCK_ADAPTERS_H
#define MOCK_ADAPTERS_H

#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace workbench::embedded {

enum class DeviceType {
    MICROCONTROLLER,  // e.g. STM32G474RE ARM Cortex-M4
    FPGA,             // e.g. Lattice iCE40-UP5K / Xilinx Artix-7
    JTAG_DEBUGGER     // e.g. FT2232H / Segger J-Link Debug Adapter
};

enum class DeviceState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAULTED,
    RECOVERING
};

enum class FaultType {
    NONE,
    COMMUNICATION_TIMEOUT,
    CRC_CHECKSUM_ERROR,
    VOLTAGE_BROWNOUT
};

struct DeviceCapabilities {
    std::string device_id;
    std::string model_name;
    DeviceType type;
    std::string firmware_or_bitstream_version;
    double operating_voltage_V;
    std::vector<std::string> supported_protocols;
    std::string connector_type;
    std::string clock_frequency;
    std::string pinout_summary;
};

struct TelemetryData {
    uint64_t timestamp_ms = 0;
    uint32_t sequence_id = 0;
    double rail_voltage_V = 0.0;
    double current_mA = 0.0;
    double core_temperature_C = 0.0;
    std::string status_word;
    std::string active_mode;
    bool fault_active = false;
    std::string fault_message;
};

class MockDeviceAdapter {
public:
    explicit MockDeviceAdapter(DeviceType type, const std::string& id, const std::string& name);
    virtual ~MockDeviceAdapter() = default;

    DeviceType get_type() const { return type_; }
    std::string get_id() const { return device_id_; }
    DeviceState get_state() const { return state_; }
    DeviceCapabilities get_capabilities() const;

    bool connect();
    bool disconnect();
    TelemetryData read_telemetry();
    
    // Injects recoverable fault for demonstration
    void inject_fault(FaultType fault, const std::string& reason = "");
    bool recover();

protected:
    DeviceType type_;
    std::string device_id_;
    std::string name_;
    DeviceState state_ = DeviceState::DISCONNECTED;
    FaultType current_fault_ = FaultType::NONE;
    std::string fault_description_;
    uint32_t telemetry_counter_ = 0;
    double base_voltage_V_ = 3.3;
    double base_temp_C_ = 35.0;
};

class EmbeddedWorkbenchManager {
public:
    EmbeddedWorkbenchManager();

    std::vector<DeviceCapabilities> discover_devices();
    MockDeviceAdapter* get_adapter(const std::string& device_id);

    // Architectural system specifications
    static std::string get_block_diagram_mermaid();
    static std::string get_pin_voltage_table_markdown();
    static std::string get_jtag_rationale_explanation();

    std::string get_all_devices_json();

private:
    std::vector<std::unique_ptr<MockDeviceAdapter>> devices_;
};

} // namespace workbench::embedded

#endif // MOCK_ADAPTERS_H
