#ifndef BATCH_SOLVER_H
#define BATCH_SOLVER_H

#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include "../fea/thermal_fea.h"

namespace workbench::multicore {

struct BatchCase {
    int case_id;
    fea::ThermalParams params;
};

struct CaseSolveResult {
    int case_id;
    fea::FEAResult fea_result;
};

struct BenchmarkRun {
    int workers;
    std::vector<double> repetition_times_ms; // 5 repetitions
    double median_time_ms = 0.0;
    double mean_time_ms = 0.0;
    double speedup = 1.0;
    double efficiency = 1.0;
};

struct SystemSpecs {
    std::string cpu_model;
    unsigned int logical_cores = 1;
    unsigned int physical_cores = 1;
    std::string ram_info;
    std::string compiler;
    std::string optimization_flags;
};

struct BenchmarkSuiteResult {
    SystemSpecs system_specs;
    int total_cases = 0;
    int elements_per_case = 0;
    double warmup_time_ms = 0.0;
    std::vector<BenchmarkRun> runs;
    double max_serial_parallel_diff_C = 0.0;
    bool agreement_verified = false;
    std::string notes;
};

// Bounded Worker Pool implementation using standard C++17 concurrency primitives
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    void enqueue(std::function<void()> task);
    void wait_all();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv_task;
    std::condition_variable cv_finished;
    size_t active_tasks = 0;
    bool stop = false;
};

class BatchSolver {
public:
    // Generates reproducible batch cases with fixed seed and varied positive k, q
    static std::vector<BatchCase> generate_batch(int case_count = 200, 
                                                 int elements_per_case = 40, 
                                                 uint32_t seed = 42);

    // Solves batch serially (baseline)
    static std::vector<CaseSolveResult> solve_serial(const std::vector<BatchCase>& cases);

    // Solves batch with a bounded pool of p workers (deterministic output ordering)
    static std::vector<CaseSolveResult> solve_parallel(const std::vector<BatchCase>& cases, int num_workers);

    // Safe non-blocking cancellation mechanism
    static void request_cancellation();
    static void reset_cancellation();
    static bool is_cancelled();

    // Runs comprehensive multicore benchmark suite (1, 2, and up to 4 workers)
    static BenchmarkSuiteResult run_benchmarks(int case_count = 500, int elements_per_case = 40);

    // System hardware detection
    static SystemSpecs detect_system();

    // Export benchmark results to CSV and JSON
    static std::string export_csv(const BenchmarkSuiteResult& result);
    static std::string to_json(const BenchmarkSuiteResult& result);
};

} // namespace workbench::multicore

#endif // BATCH_SOLVER_H
