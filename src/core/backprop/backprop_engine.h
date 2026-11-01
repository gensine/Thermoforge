#ifndef BACKPROP_ENGINE_H
#define BACKPROP_ENGINE_H

#include <vector>
#include <string>

namespace workbench::backprop {

struct NetworkWeights {
    double w = 0.5; // Hidden weight
    double b = 0.0; // Hidden bias
    double v = 1.0; // Output weight
    double c = 0.0; // Output bias
};

struct NetworkGradients {
    double dJ_dw = 0.0;
    double dJ_db = 0.0;
    double dJ_dv = 0.0;
    double dJ_dc = 0.0;
};

struct GradientCheckResult {
    std::string param_name;
    double analytical_grad = 0.0;
    double numerical_grad = 0.0;
    double abs_error = 0.0;
    bool passed = false;
};

struct StepRecord {
    int step = 0;
    NetworkWeights weights;
    NetworkGradients gradients;
    double y_hat = 0.0;
    double loss = 0.0;
};

struct TrainingResult {
    NetworkWeights initial_weights;
    NetworkWeights final_weights;
    std::vector<GradientCheckResult> gradient_checks;
    bool all_gradient_checks_passed = false;
    std::vector<StepRecord> history; // 100 steps
    double initial_loss = 0.0;
    double final_loss = 0.0;
};

class BackpropEngine {
public:
    // Forward pass: returns {y_hat, h, z, loss}
    static double compute_loss(double x, double y, const NetworkWeights& wts, double* out_y_hat = nullptr, double* out_h = nullptr);

    // Analytical gradient computation using manual chain rule (no autodiff)
    static NetworkGradients compute_analytical_gradients(double x, double y, const NetworkWeights& wts);

    // Central finite differences gradient verification: [J(theta+eps) - J(theta-eps)] / (2*eps)
    static std::vector<GradientCheckResult> verify_gradients(double x = 1.0, double y = 2.0, 
                                                            const NetworkWeights& wts = {0.5, 0.0, 1.0, 0.0}, 
                                                            double epsilon = 1e-6, 
                                                            double tolerance = 1e-5);

    // Trains the single sample for 100 subsequent steps with learning rate eta = 0.1
    static TrainingResult run_training(double x = 1.0, double y = 2.0, 
                                      double learning_rate = 0.1, 
                                      int steps = 100);

    // Export history to CSV formatted string
    static std::string export_csv(const TrainingResult& res);
    static std::string to_json(const TrainingResult& res);
};

} // namespace workbench::backprop

#endif // BACKPROP_ENGINE_H
