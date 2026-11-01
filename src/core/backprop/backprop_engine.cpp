#include "backprop_engine.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace workbench::backprop {

double BackpropEngine::compute_loss(double x, double y, const NetworkWeights& wts, double* out_y_hat, double* out_h) {
    double z = wts.w * x + wts.b;
    double h = std::tanh(z);
    double y_hat = wts.v * h + wts.c;
    double diff = y_hat - y;
    double loss = 0.5 * diff * diff;

    if (out_y_hat) *out_y_hat = y_hat;
    if (out_h) *out_h = h;

    return loss;
}

NetworkGradients BackpropEngine::compute_analytical_gradients(double x, double y, const NetworkWeights& wts) {
    double y_hat = 0.0;
    double h = 0.0;
    compute_loss(x, y, wts, &y_hat, &h);

    // dJ / dy_hat = y_hat - y
    double dJ_dyhat = y_hat - y;

    // dy_hat / dc = 1  => dJ / dc = dJ_dyhat * 1
    double dJ_dc = dJ_dyhat;

    // dy_hat / dv = h  => dJ / dv = dJ_dyhat * h
    double dJ_dv = dJ_dyhat * h;

    // dJ / dh = dJ_dyhat * v
    // dh / dz = 1 - tanh^2(z) = 1 - h^2
    // dJ / dz = (dJ / dh) * (dh / dz)
    double dh_dz = 1.0 - h * h;
    double dJ_dz = dJ_dyhat * wts.v * dh_dz;

    // dz / dw = x  => dJ / dw = dJ_dz * x
    double dJ_dw = dJ_dz * x;

    // dz / db = 1  => dJ / db = dJ_dz * 1
    double dJ_db = dJ_dz;

    return {dJ_dw, dJ_db, dJ_dv, dJ_dc};
}

std::vector<GradientCheckResult> BackpropEngine::verify_gradients(
    double x, double y, const NetworkWeights& wts, double epsilon, double tolerance) {
    
    NetworkGradients analytical = compute_analytical_gradients(x, y, wts);
    std::vector<GradientCheckResult> results;
    results.reserve(4);

    auto check_param = [&](const std::string& name, double analytical_grad, auto perturb_fn) {
        NetworkWeights w_plus = wts;
        NetworkWeights w_minus = wts;
        perturb_fn(w_plus, +epsilon);
        perturb_fn(w_minus, -epsilon);

        double J_plus = compute_loss(x, y, w_plus);
        double J_minus = compute_loss(x, y, w_minus);
        double num_grad = (J_plus - J_minus) / (2.0 * epsilon);

        double err = std::abs(analytical_grad - num_grad);
        bool pass = (err <= tolerance);

        results.push_back({name, analytical_grad, num_grad, err, pass});
    };

    check_param("w", analytical.dJ_dw, [](NetworkWeights& nw, double eps) { nw.w += eps; });
    check_param("b", analytical.dJ_db, [](NetworkWeights& nw, double eps) { nw.b += eps; });
    check_param("v", analytical.dJ_dv, [](NetworkWeights& nw, double eps) { nw.v += eps; });
    check_param("c", analytical.dJ_dc, [](NetworkWeights& nw, double eps) { nw.c += eps; });

    return results;
}

TrainingResult BackpropEngine::run_training(double x, double y, double learning_rate, int steps) {
    TrainingResult tr;
    tr.initial_weights = {0.5, 0.0, 1.0, 0.0};
    tr.gradient_checks = verify_gradients(x, y, tr.initial_weights, 1e-6, 1e-5);

    tr.all_gradient_checks_passed = true;
    for (const auto& gc : tr.gradient_checks) {
        if (!gc.passed) tr.all_gradient_checks_passed = false;
    }

    NetworkWeights cur_w = tr.initial_weights;
    tr.history.reserve(steps + 1);

    // Initial step 0
    double y_hat0 = 0.0, h0 = 0.0;
    double loss0 = compute_loss(x, y, cur_w, &y_hat0, &h0);
    NetworkGradients g0 = compute_analytical_gradients(x, y, cur_w);
    tr.initial_loss = loss0;
    tr.history.push_back({0, cur_w, g0, y_hat0, loss0});

    // 100 training steps
    for (int step = 1; step <= steps; ++step) {
        NetworkGradients g = compute_analytical_gradients(x, y, cur_w);

        // Simultaneous gradient descent update using gradients from current parameters
        cur_w.w -= learning_rate * g.dJ_dw;
        cur_w.b -= learning_rate * g.dJ_db;
        cur_w.v -= learning_rate * g.dJ_dv;
        cur_w.c -= learning_rate * g.dJ_dc;

        double cur_y_hat = 0.0;
        double cur_loss = compute_loss(x, y, cur_w, &cur_y_hat);

        tr.history.push_back({step, cur_w, g, cur_y_hat, cur_loss});
    }

    tr.final_weights = cur_w;
    tr.final_loss = tr.history.back().loss;

    return tr;
}

std::string BackpropEngine::export_csv(const TrainingResult& res) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    oss << "# Unified Engineering Workbench - Backpropagation Training History\n";
    oss << "# Architecture: 1-Hidden-tanh-Linear, Target: x=1.0, y=2.0, LearningRate=0.1\n";
    oss << "# Initial Loss: " << res.initial_loss << " | Final Loss (100 steps): " << res.final_loss << "\n";
    oss << "step,w,b,v,c,dJ_dw,dJ_db,dJ_dv,dJ_dc,y_hat,loss\n";

    for (const auto& h : res.history) {
        oss << h.step << "," << h.weights.w << "," << h.weights.b << "," 
            << h.weights.v << "," << h.weights.c << ","
            << h.gradients.dJ_dw << "," << h.gradients.dJ_db << ","
            << h.gradients.dJ_dv << "," << h.gradients.dJ_dc << ","
            << h.y_hat << "," << h.loss << "\n";
    }

    return oss.str();
}

std::string BackpropEngine::to_json(const TrainingResult& res) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    oss << "{";
    oss << "\"all_checks_passed\":" << (res.all_gradient_checks_passed ? "true" : "false") << ",";
    oss << "\"initial_loss\":" << res.initial_loss << ",";
    oss << "\"final_loss\":" << res.final_loss << ",";
    oss << "\"gradient_checks\":[";
    for (size_t i = 0; i < res.gradient_checks.size(); ++i) {
        const auto& c = res.gradient_checks[i];
        if (i > 0) oss << ",";
        oss << "{";
        oss << "\"param\":\"" << c.param_name << "\",";
        oss << "\"analytical\":" << c.analytical_grad << ",";
        oss << "\"numerical\":" << c.numerical_grad << ",";
        oss << "\"error\":" << c.abs_error << ",";
        oss << "\"passed\":" << (c.passed ? "true" : "false");
        oss << "}";
    }
    oss << "],";

    oss << "\"history\":[";
    for (size_t i = 0; i < res.history.size(); ++i) {
        const auto& h = res.history[i];
        if (i > 0) oss << ",";
        oss << "{";
        oss << "\"step\":" << h.step << ",";
        oss << "\"w\":" << h.weights.w << ",";
        oss << "\"b\":" << h.weights.b << ",";
        oss << "\"v\":" << h.weights.v << ",";
        oss << "\"c\":" << h.weights.c << ",";
        oss << "\"y_hat\":" << h.y_hat << ",";
        oss << "\"loss\":" << h.loss;
        oss << "}";
    }
    oss << "]}";

    return oss.str();
}

} // namespace workbench::backprop
