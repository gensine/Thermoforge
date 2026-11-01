#ifndef THERMAL_FEA_H
#define THERMAL_FEA_H

#include <vector>
#include <string>

namespace workbench::fea {

struct ThermalParams {
    double L = 1.0;          // Rod length [m]
    double A = 1e-4;         // Cross-sectional area [m^2]
    double k = 10.0;         // Thermal conductivity [W/(m K)]
    double q = 10000.0;      // Volumetric heat generation [W/m^3]
    double T0 = 100.0;       // Temperature at x = 0 [C]
    double TL = 20.0;        // Temperature at x = L [C]
    int num_elements = 10;   // Number of linear 2-node elements
};

struct ElementMidpointResult {
    int element_id;
    double x_mid;
    double T_fe_interp;
    double T_exact;
    double abs_error;
};

struct FEAResult {
    ThermalParams params;
    int num_elements;
    int num_nodes;
    double element_length_h;
    std::vector<double> node_x;
    std::vector<double> node_T_fe;
    std::vector<double> node_T_exact;
    std::vector<double> node_abs_error;
    std::vector<ElementMidpointResult> midpoints;
    double max_midpoint_error;
    double l2_midpoint_error;
    double compute_time_ms;
};

struct ConvergenceCase {
    int num_elements;
    double h;
    double max_midpoint_error;
    double l2_error;
    double convergence_rate; // Observed order of convergence relative to previous
};

class ThermalSolver1D {
public:
    static double exact_temperature(double x, const ThermalParams& params);
    
    // Solves the 1D steady-state heat equation with 2-node linear elements
    static FEAResult solve(const ThermalParams& params);
    
    // Evaluates FE temperature at any point x in [0, L] via shape functions
    static double interpolate_fe(double x, const FEAResult& result);
    
    // Runs standard convergence study for N = 10, 20, 40, 80
    static std::vector<ConvergenceCase> run_convergence_study(const ThermalParams& base_params);
    
    // Export result to CSV formatted string
    static std::string export_csv(const FEAResult& result);
    static std::string export_convergence_csv(const std::vector<ConvergenceCase>& cases);
};

} // namespace workbench::fea

#endif // THERMAL_FEA_H
