#include "thermal_fea.h"
#include <cmath>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

namespace workbench::fea {

double ThermalSolver1D::exact_temperature(double x, const ThermalParams& p) {
    // T_exact(x) = T0 - (T0 - TL)*x/L + [q / (2*k)] * x * (L - x)
    if (p.L <= 0.0 || p.k <= 0.0) return 0.0;
    double linear_part = p.T0 - (p.T0 - p.TL) * (x / p.L);
    double generation_part = (p.q / (2.0 * p.k)) * x * (p.L - x);
    return linear_part + generation_part;
}

FEAResult ThermalSolver1D::solve(const ThermalParams& p) {
    if (p.num_elements < 1) {
        throw std::invalid_argument("Number of elements must be at least 1.");
    }
    if (p.L <= 0.0) {
        throw std::invalid_argument("Rod length L must be positive.");
    }
    if (p.A <= 0.0) {
        throw std::invalid_argument("Cross-sectional area A must be positive.");
    }
    if (p.k <= 0.0) {
        throw std::invalid_argument("Thermal conductivity k must be positive.");
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    int N = p.num_elements;
    int num_nodes = N + 1;
    double h = p.L / static_cast<double>(N);

    // Tridiagonal system: a (sub-diagonal), b (main diagonal), c (super-diagonal), d (RHS)
    // Dimension: num_nodes x num_nodes
    std::vector<double> a(num_nodes, 0.0);
    std::vector<double> b(num_nodes, 0.0);
    std::vector<double> c(num_nodes, 0.0);
    std::vector<double> d(num_nodes, 0.0);

    // Assembly of element stiffness and load vectors
    // For each element e in [0, N-1], nodes are e and e+1
    // K_e = (k*A/h) * [ 1, -1; -1,  1]
    // F_e = (q*A*h/2) * [1; 1]
    double k_elem = (p.k * p.A) / h;
    double f_elem = (p.q * p.A * h) / 2.0;

    for (int e = 0; e < N; ++e) {
        int n0 = e;
        int n1 = e + 1;

        b[n0] += k_elem;
        c[n0] -= k_elem;
        a[n1] -= k_elem;
        b[n1] += k_elem;

        d[n0] += f_elem;
        d[n1] += f_elem;
    }

    // Apply Dirichlet Boundary Conditions:
    // T(0) = T0, T(L) = TL
    // Impose T0 at node 0:
    // Row 0: b[0] * T[0] = T0 -> set b[0] = 1, c[0] = 0, d[0] = p.T0
    // Eliminate coupling to node 1: subtract a[1]*T0 from d[1], set a[1] = 0
    d[1] -= a[1] * p.T0;
    a[1] = 0.0;
    b[0] = 1.0;
    c[0] = 0.0;
    d[0] = p.T0;

    // Impose TL at node N:
    // Row N: b[N] * T[N] = TL -> set b[N] = 1, a[N] = 0, d[N] = p.TL
    // Eliminate coupling to node N-1: subtract c[N-1]*TL from d[N-1], set c[N-1] = 0
    d[N - 1] -= c[N - 1] * p.TL;
    c[N - 1] = 0.0;
    b[N] = 1.0;
    a[N] = 0.0;
    d[N] = p.TL;

    // Solve tridiagonal system using Thomas Algorithm
    // Forward sweep
    std::vector<double> c_prime(num_nodes, 0.0);
    std::vector<double> d_prime(num_nodes, 0.0);

    c_prime[0] = c[0] / b[0];
    d_prime[0] = d[0] / b[0];

    for (int i = 1; i < num_nodes; ++i) {
        double denom = b[i] - a[i] * c_prime[i - 1];
        if (std::abs(denom) < 1e-15) {
            throw std::runtime_error("Singular matrix encountered during FEA solve.");
        }
        if (i < num_nodes - 1) {
            c_prime[i] = c[i] / denom;
        }
        d_prime[i] = (d[i] - a[i] * d_prime[i - 1]) / denom;
    }

    // Back substitution
    std::vector<double> T_sol(num_nodes, 0.0);
    T_sol[num_nodes - 1] = d_prime[num_nodes - 1];
    for (int i = num_nodes - 2; i >= 0; --i) {
        T_sol[i] = d_prime[i] - c_prime[i] * T_sol[i + 1];
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    FEAResult res;
    res.params = p;
    res.num_elements = N;
    res.num_nodes = num_nodes;
    res.element_length_h = h;
    res.compute_time_ms = elapsed_ms;

    res.node_x.resize(num_nodes);
    res.node_T_fe.resize(num_nodes);
    res.node_T_exact.resize(num_nodes);
    res.node_abs_error.resize(num_nodes);

    for (int i = 0; i < num_nodes; ++i) {
        double x_i = i * h;
        res.node_x[i] = x_i;
        res.node_T_fe[i] = T_sol[i];
        double t_ex = exact_temperature(x_i, p);
        res.node_T_exact[i] = t_ex;
        res.node_abs_error[i] = std::abs(T_sol[i] - t_ex);
    }

    // Midpoint calculations using FE shape function interpolation
    // Inside element e: N1(x_mid) = 0.5, N2(x_mid) = 0.5
    // T_fe(x_mid) = 0.5 * (T_e + T_{e+1})
    res.midpoints.resize(N);
    double max_err = 0.0;
    double sum_sq_err = 0.0;

    for (int e = 0; e < N; ++e) {
        double x_mid = (res.node_x[e] + res.node_x[e + 1]) * 0.5;
        double t_fe_mid = (res.node_T_fe[e] + res.node_T_fe[e + 1]) * 0.5;
        double t_ex_mid = exact_temperature(x_mid, p);
        double abs_err = std::abs(t_fe_mid - t_ex_mid);

        res.midpoints[e] = {e, x_mid, t_fe_mid, t_ex_mid, abs_err};

        if (abs_err > max_err) {
            max_err = abs_err;
        }
        sum_sq_err += abs_err * abs_err;
    }

    res.max_midpoint_error = max_err;
    res.l2_midpoint_error = std::sqrt(sum_sq_err / static_cast<double>(N));

    return res;
}

double ThermalSolver1D::interpolate_fe(double x, const FEAResult& res) {
    if (x <= 0.0) return res.node_T_fe.front();
    if (x >= res.params.L) return res.node_T_fe.back();

    int e = static_cast<int>(x / res.element_length_h);
    if (e >= res.num_elements) e = res.num_elements - 1;

    double x0 = res.node_x[e];
    double h = res.element_length_h;
    double xi = (x - x0) / h; // local coord in [0, 1]

    double N1 = 1.0 - xi;
    double N2 = xi;

    return N1 * res.node_T_fe[e] + N2 * res.node_T_fe[e + 1];
}

std::vector<ConvergenceCase> ThermalSolver1D::run_convergence_study(const ThermalParams& base_p) {
    std::vector<int> element_counts = {10, 20, 40, 80};
    std::vector<ConvergenceCase> cases;
    cases.reserve(element_counts.size());

    double prev_err = -1.0;
    double prev_h = -1.0;

    for (int N : element_counts) {
        ThermalParams p = base_p;
        p.num_elements = N;
        FEAResult res = solve(p);

        double h = res.element_length_h;
        double max_err = res.max_midpoint_error;
        double l2_err = res.l2_midpoint_error;
        double rate = 0.0;

        if (prev_err > 0.0 && prev_h > 0.0) {
            // Observed order: log(E1 / E2) / log(h1 / h2)
            rate = std::log(prev_err / max_err) / std::log(prev_h / h);
        }

        cases.push_back({N, h, max_err, l2_err, rate});
        prev_err = max_err;
        prev_h = h;
    }

    return cases;
}

std::string ThermalSolver1D::export_csv(const FEAResult& r) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    oss << "# Unified Engineering Workbench - 1D Thermal FEA Result\n";
    oss << "# Parameters: L=" << r.params.L << " m, A=" << r.params.A 
        << " m^2, k=" << r.params.k << " W/(m K), q=" << r.params.q << " W/m^3\n";
    oss << "# Boundary Conditions: T(0)=" << r.params.T0 << " C, T(L)=" << r.params.TL << " C\n";
    oss << "# Elements=" << r.num_elements << ", MaxMidpointError=" << r.max_midpoint_error << " C\n";
    oss << "type,index,x_m,T_fe_C,T_exact_C,abs_error_C\n";

    for (int i = 0; i < r.num_nodes; ++i) {
        oss << "node," << i << "," << r.node_x[i] << "," 
            << r.node_T_fe[i] << "," << r.node_T_exact[i] << "," 
            << r.node_abs_error[i] << "\n";
    }

    for (const auto& mp : r.midpoints) {
        oss << "midpoint," << mp.element_id << "," << mp.x_mid << ","
            << mp.T_fe_interp << "," << mp.T_exact << ","
            << mp.abs_error << "\n";
    }

    return oss.str();
}

std::string ThermalSolver1D::export_convergence_csv(const std::vector<ConvergenceCase>& cases) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    oss << "# Unified Engineering Workbench - Mesh Convergence Study\n";
    oss << "elements,h_m,max_midpoint_error_C,l2_midpoint_error_C,convergence_rate\n";
    for (const auto& c : cases) {
        oss << c.num_elements << "," << c.h << "," 
            << c.max_midpoint_error << "," << c.l2_error << "," 
            << c.convergence_rate << "\n";
    }
    return oss.str();
}

} // namespace workbench::fea
