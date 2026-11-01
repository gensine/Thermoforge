#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>

#include "../src/core/fea/thermal_fea.h"
#include "../src/core/parsers/stl_parser.h"
#include "../src/core/parsers/ansys_csv_parser.h"
#include "../src/core/multicore/batch_solver.h"
#include "../src/core/embedded/mock_adapters.h"
#include "../src/core/backprop/backprop_engine.h"

using namespace workbench;

int tests_run = 0;
int tests_passed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        tests_run++; \
        if (!(cond)) { \
            std::cerr << "[-] FAILED: " << msg << " (line " << __LINE__ << ")\n"; \
        } else { \
            tests_passed++; \
            std::cout << "[+] PASSED: " << msg << "\n"; \
        } \
    } while(0)

void test_thermal_fea() {
    std::cout << "\n=== Running Task C: Thermal FEA Tests ===\n";
    fea::ThermalParams p;
    p.L = 1.0;
    p.A = 1e-4;
    p.k = 10.0;
    p.q = 10000.0;
    p.T0 = 100.0;
    p.TL = 20.0;
    p.num_elements = 10;

    fea::FEAResult res = fea::ThermalSolver1D::solve(p);

    TEST_ASSERT(res.node_T_fe.size() == 11, "FEA grid has N+1 = 11 nodes");
    TEST_ASSERT(std::abs(res.node_T_fe.front() - 100.0) < 1e-12, "T(0) = 100 C boundary condition strictly imposed");
    TEST_ASSERT(std::abs(res.node_T_fe.back() - 20.0) < 1e-12, "T(L) = 20 C boundary condition strictly imposed");

    // Check nodal exactness due to 1D superconvergence
    for (size_t i = 0; i < res.node_T_fe.size(); ++i) {
        TEST_ASSERT(res.node_abs_error[i] < 1e-10, "Nodal FE temperature matches analytical solution exactly");
    }

    // Check midpoint error
    TEST_ASSERT(res.midpoints.size() == 10, "FEA has 10 element midpoints");
    // Analytical midpoint error formula: 125 * h^2 = 125 * 0.01 = 1.25 C
    TEST_ASSERT(std::abs(res.max_midpoint_error - 1.25) < 1e-4, "Midpoint error matches theoretical O(h^2) formula (1.25 C for N=10)");

    // Convergence study test
    auto conv_cases = fea::ThermalSolver1D::run_convergence_study(p);
    TEST_ASSERT(conv_cases.size() == 4, "Convergence study ran for N=10, 20, 40, 80");
    TEST_ASSERT(std::abs(conv_cases[1].max_midpoint_error - 0.3125) < 1e-4, "N=20 error is 0.3125 C");
    TEST_ASSERT(std::abs(conv_cases[2].max_midpoint_error - 0.078125) < 1e-4, "N=40 error is 0.078125 C");
    TEST_ASSERT(std::abs(conv_cases[3].max_midpoint_error - 0.01953125) < 1e-4, "N=80 error is 0.01953125 C");

    // Observed convergence rate should be ~ 2.0 (quadratic)
    for (size_t i = 1; i < conv_cases.size(); ++i) {
        TEST_ASSERT(std::abs(conv_cases[i].convergence_rate - 2.0) < 0.05, "Observed order of convergence is 2nd order (rate ~ 2.0)");
    }
}

void test_cad_ansys_parsers() {
    std::cout << "\n=== Running Task B: CAD & ANSYS Parsers Tests ===\n";

    // 1. Valid STL test
    std::string valid_stl = 
        "solid test_wedge\n"
        "  facet normal 0.0 0.0 1.0\n"
        "    outer loop\n"
        "      vertex 0.0 0.0 0.0\n"
        "      vertex 1.0 0.0 0.0\n"
        "      vertex 0.0 1.0 0.0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid test_wedge\n";

    auto stl_res = parsers::STLParser::parse_string(valid_stl);
    TEST_ASSERT(stl_res.is_valid, "Valid ASCII STL parses successfully");
    TEST_ASSERT(stl_res.facet_count == 1, "Facet count is 1");
    TEST_ASSERT(stl_res.solid_name == "test_wedge", "Solid name parsed correctly");

    // 2. Reject empty STL
    auto empty_stl = parsers::STLParser::parse_string("");
    TEST_ASSERT(!empty_stl.is_valid, "Empty STL is rejected with actionable error");

    // 3. Reject non-finite STL
    std::string nan_stl = 
        "solid nan_test\n"
        "  facet normal 0.0 0.0 1.0\n"
        "    outer loop\n"
        "      vertex NaN 0.0 0.0\n"
        "      vertex 1.0 0.0 0.0\n"
        "      vertex 0.0 1.0 0.0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid nan_test\n";
    auto bad_stl = parsers::STLParser::parse_string(nan_stl);
    TEST_ASSERT(!bad_stl.is_valid, "STL with NaN coordinate is safely rejected");

    // 4. Valid ANSYS CSV
    std::string valid_csv = 
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,100.0\n"
        "2,0.5,0.0,0.0,185.0\n"
        "3,1.0,0.0,0.0,20.0\n";
    auto ansys_res = parsers::ANSYSCSVParser::parse_string(valid_csv);
    TEST_ASSERT(ansys_res.is_valid, "Valid ANSYS CSV parses successfully");
    TEST_ASSERT(ansys_res.node_count == 3, "Node count is 3");
    TEST_ASSERT(ansys_res.min_temp_C == 20.0 && ansys_res.max_temp_C == 185.0, "Min/Max temperatures calculated accurately");

    // 5. Reject malformed CSV with non-finite value
    std::string inf_csv = 
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,100.0\n"
        "2,0.5,0.0,0.0,Inf\n";
    auto bad_csv = parsers::ANSYSCSVParser::parse_string(inf_csv);
    TEST_ASSERT(!bad_csv.is_valid, "ANSYS CSV with Inf temperature is safely rejected");
}

void test_multicore() {
    std::cout << "\n=== Running Task D: Multicore Execution Tests ===\n";
    int num_cases = 100;
    auto cases = multicore::BatchSolver::generate_batch(num_cases, 20, 12345);
    TEST_ASSERT(cases.size() == 100, "Deterministic batch of 100 cases generated");

    auto serial_res = multicore::BatchSolver::solve_serial(cases);
    auto parallel_res = multicore::BatchSolver::solve_parallel(cases, 2);

    TEST_ASSERT(serial_res.size() == parallel_res.size(), "Serial and Parallel result count match");

    double max_diff = 0.0;
    for (size_t c = 0; c < cases.size(); ++c) {
        TEST_ASSERT(serial_res[c].case_id == parallel_res[c].case_id, "Results maintained in deterministic input order");
        const auto& s_nodes = serial_res[c].fea_result.node_T_fe;
        const auto& p_nodes = parallel_res[c].fea_result.node_T_fe;
        for (size_t n = 0; n < s_nodes.size(); ++n) {
            double diff = std::abs(s_nodes[n] - p_nodes[n]);
            if (diff > max_diff) max_diff = diff;
        }
    }

    TEST_ASSERT(max_diff <= 1e-8, "Serial and parallel solutions agree with max diff <= 1e-8 C");
    std::cout << "    [i] Measured max absolute difference: " << max_diff << " C\n";
}

void test_embedded_adapters() {
    std::cout << "\n=== Running Task E: Embedded Integration Tests ===\n";
    embedded::EmbeddedWorkbenchManager mgr;
    auto caps = mgr.discover_devices();
    TEST_ASSERT(caps.size() == 3, "Discovered exactly 3 mock devices (MCU, FPGA, JTAG)");

    auto* mcu = mgr.get_adapter("mcu_stm32g4");
    TEST_ASSERT(mcu != nullptr, "MCU adapter located");
    TEST_ASSERT(mcu->connect(), "MCU connects successfully");
    auto telem = mcu->read_telemetry();
    TEST_ASSERT(!telem.fault_active, "Initial telemetry reports healthy status");
    TEST_ASSERT(telem.rail_voltage_V > 3.0 && telem.rail_voltage_V < 3.6, "MCU voltage within 3.3V operating band");

    // Recoverable fault test
    mcu->inject_fault(embedded::FaultType::COMMUNICATION_TIMEOUT);
    auto fault_telem = mcu->read_telemetry();
    TEST_ASSERT(fault_telem.fault_active, "Timeout fault active in telemetry");
    TEST_ASSERT(mcu->get_state() == embedded::DeviceState::FAULTED, "State transitioned to FAULTED");

    bool recovered = mcu->recover();
    TEST_ASSERT(recovered, "Recovery mechanism succeeded");
    auto rec_telem = mcu->read_telemetry();
    TEST_ASSERT(!rec_telem.fault_active, "Telemetry resumed healthy state after recovery");
}

void test_backprop() {
    std::cout << "\n=== Running Task F: Backpropagation Tests ===\n";
    // Check initial forward pass and gradients
    backprop::NetworkWeights w0{0.5, 0.0, 1.0, 0.0};
    double y_hat0 = 0.0, h0 = 0.0;
    double loss0 = backprop::BackpropEngine::compute_loss(1.0, 2.0, w0, &y_hat0, &h0);

    // h = tanh(0.5) ~ 0.462117157
    TEST_ASSERT(std::abs(h0 - 0.462117157) < 1e-6, "Hidden state h0 = tanh(0.5) verified");
    TEST_ASSERT(std::abs(y_hat0 - 0.462117157) < 1e-6, "Prediction y_hat0 = v*h0 + c verified");
    TEST_ASSERT(std::abs(loss0 - 1.18254182) < 1e-5, "Initial Loss J0 = 0.5*(y_hat - y)^2 verified");

    // Central finite differences gradient verification
    auto checks = backprop::BackpropEngine::verify_gradients(1.0, 2.0, w0, 1e-6, 1e-5);
    TEST_ASSERT(checks.size() == 4, "4 parameters verified (w, b, v, c)");
    for (const auto& c : checks) {
        TEST_ASSERT(c.passed, ("Gradient for parameter " + c.param_name + " matches central finite differences (|err| <= 1e-5)").c_str());
        std::cout << "    [i] Param " << c.param_name << ": Analytical=" << c.analytical_grad 
                  << " | Numerical=" << c.numerical_grad << " | Err=" << c.abs_error << "\n";
    }

    // 100-step training test
    auto train_res = backprop::BackpropEngine::run_training(1.0, 2.0, 0.1, 100);
    TEST_ASSERT(train_res.history.size() == 101, "History has 101 records (step 0 to 100)");
    TEST_ASSERT(train_res.final_loss < train_res.initial_loss, "Training loss strictly decreases over 100 steps");
    std::cout << "    [i] Initial Loss: " << train_res.initial_loss 
              << " -> Final Loss (100 steps): " << train_res.final_loss << "\n";
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "   UNIFIED ENGINEERING WORKBENCH - AUTOMATED TEST SUITE \n";
    std::cout << "========================================================\n";

    test_thermal_fea();
    test_cad_ansys_parsers();
    test_multicore();
    test_embedded_adapters();
    test_backprop();

    std::cout << "\n========================================================\n";
    std::cout << "Test Summary: " << tests_passed << " / " << tests_run << " assertions passed.\n";
    if (tests_passed == tests_run) {
        std::cout << "STATUS: ALL TESTS PASSED SUCCESSFULLY! (100% PASS RATE)\n";
        std::cout << "========================================================\n";
        return 0;
    } else {
        std::cerr << "STATUS: SOME TESTS FAILED!\n";
        std::cout << "========================================================\n";
        return 1;
    }
}
