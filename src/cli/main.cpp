#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "core/fea/thermal_fea.h"
#include "core/parsers/stl_parser.h"
#include "core/parsers/ansys_csv_parser.h"
#include "core/multicore/batch_solver.h"
#include "core/embedded/mock_adapters.h"
#include "core/backprop/backprop_engine.h"

using namespace workbench;

void print_help() {
    std::cout << "Unified Engineering Workbench - Headless CLI & Test Engine\n";
    std::cout << "Usage: workbench_cli [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help              Display this help message\n";
    std::cout << "  --generate-data     Generate synthetic ANSYS CSV dataset in data/\n";
    std::cout << "  --run-fea           Execute 1D Thermal FEA solver and output summary\n";
    std::cout << "  --convergence       Run N=10,20,40,80 convergence study and export CSV\n";
    std::cout << "  --run-benchmark     Execute multicore benchmark suite and export CSV\n";
    std::cout << "  --run-backprop      Execute backprop gradient checks and 100-step training\n";
    std::cout << "  --run-all           Run all tasks and generate verification evidence in results/\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    std::string arg = argv[1];

    if (arg == "--help" || arg == "-h") {
        print_help();
        return 0;
    }

    if (arg == "--generate-data" || arg == "--run-all") {
        std::cout << "[*] Generating synthetic ANSYS CSV dataset in data/ansys_thermal_results.csv...\n";
        std::string csv_content = parsers::ANSYSCSVParser::generate_synthetic_dataset(10, 10, 10);
        std::ofstream out("data/ansys_thermal_results.csv");
        if (out.is_open()) {
            out << csv_content;
            out.close();
            std::cout << "[+] Dataset generated successfully (1,000 nodes).\n";
        } else {
            std::cerr << "[-] Error: Failed to open data/ansys_thermal_results.csv for writing.\n";
        }
    }

    if (arg == "--run-fea" || arg == "--run-all") {
        std::cout << "\n[*] Running 1D Thermal FEA (N=10 elements, L=1m, k=10, q=10000, T0=100, TL=20)...\n";
        fea::ThermalParams p;
        auto res = fea::ThermalSolver1D::solve(p);
        std::cout << "    [+] Nodes: " << res.num_nodes << ", Element length h: " << res.element_length_h << " m\n";
        std::cout << "    [+] Max Midpoint Error: " << res.max_midpoint_error << " C (Analytical: 1.25 C)\n";
        std::cout << "    [+] L2 Midpoint Error: " << res.l2_midpoint_error << " C\n";
        std::cout << "    [+] Compute time: " << res.compute_time_ms << " ms\n";

        std::ofstream out("results/thermal_fea_solution.csv");
        if (out.is_open()) {
            out << fea::ThermalSolver1D::export_csv(res);
            out.close();
            std::cout << "    [+] Exported solution to results/thermal_fea_solution.csv\n";
        }
    }

    if (arg == "--convergence" || arg == "--run-all") {
        std::cout << "\n[*] Running Mesh Convergence Study (N=10, 20, 40, 80 elements)...\n";
        fea::ThermalParams p;
        auto cases = fea::ThermalSolver1D::run_convergence_study(p);
        for (const auto& c : cases) {
            std::cout << "    - N=" << c.num_elements << ", h=" << c.h 
                      << " m | Max Midpoint Err: " << c.max_midpoint_error 
                      << " C | Convergence Rate: " << c.convergence_rate << "\n";
        }
        std::ofstream out("results/mesh_convergence.csv");
        if (out.is_open()) {
            out << fea::ThermalSolver1D::export_convergence_csv(cases);
            out.close();
            std::cout << "    [+] Exported convergence data to results/mesh_convergence.csv\n";
        }
    }

    if (arg == "--run-benchmark" || arg == "--run-all") {
        std::cout << "\n[*] Running Multicore Execution Benchmark (500 cases, 40 elements, 5 reps)...\n";
        auto bench_res = multicore::BatchSolver::run_benchmarks(500, 40);
        std::cout << "    [+] CPU: " << bench_res.system_specs.cpu_model 
                  << " (" << bench_res.system_specs.logical_cores << " logical cores)\n";
        std::cout << "    [+] Compiler: " << bench_res.system_specs.compiler << "\n";
        std::cout << "    [+] Warmup Time: " << bench_res.warmup_time_ms << " ms\n";
        std::cout << "    [+] Serial/Parallel Agreement Max Diff: " 
                  << bench_res.max_serial_parallel_diff_C << " C (Verified <= 1e-8 C: " 
                  << (bench_res.agreement_verified ? "YES" : "NO") << ")\n";

        for (const auto& r : bench_res.runs) {
            std::cout << "    - Workers: " << r.workers 
                      << " | Median: " << r.median_time_ms << " ms"
                      << " | Speedup: " << r.speedup << "x"
                      << " | Efficiency: " << (r.efficiency * 100.0) << "%\n";
        }

        std::ofstream out("results/multicore_benchmark.csv");
        if (out.is_open()) {
            out << multicore::BatchSolver::export_csv(bench_res);
            out.close();
            std::cout << "    [+] Exported benchmark data to results/multicore_benchmark.csv\n";
        }
    }

    if (arg == "--run-backprop" || arg == "--run-all") {
        std::cout << "\n[*] Running Backpropagation Engine (Target: x=1, y=2, Initial: w=0.5, b=0, v=1, c=0)...\n";
        auto train_res = backprop::BackpropEngine::run_training(1.0, 2.0, 0.1, 100);
        std::cout << "    [+] Finite Differences Verification: " 
                  << (train_res.all_gradient_checks_passed ? "PASSED (all |err| <= 1e-5)" : "FAILED") << "\n";
        for (const auto& gc : train_res.gradient_checks) {
            std::cout << "        Param " << gc.param_name << ": Analytical=" << gc.analytical_grad
                      << " | Numerical=" << gc.numerical_grad << " | Err=" << gc.abs_error << "\n";
        }
        std::cout << "    [+] Initial Loss: " << train_res.initial_loss 
                  << " -> Final Loss (100 steps): " << train_res.final_loss << "\n";

        std::ofstream out("results/backprop_training_history.csv");
        if (out.is_open()) {
            out << backprop::BackpropEngine::export_csv(train_res);
            out.close();
            std::cout << "    [+] Exported training history to results/backprop_training_history.csv\n";
        }
    }

    std::cout << "\n[+] Finished execution of command: " << arg << "\n";
    return 0;
}
