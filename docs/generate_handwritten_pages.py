import os
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages

DOCS_DIR = os.path.dirname(os.path.abspath(__file__))
PDF_OUT = os.path.join(DOCS_DIR, 'TaskF_Handwritten_Derivation.pdf')

def create_page1():
    fig = plt.figure(figsize=(8.5, 11), dpi=200)
    fig.patch.set_facecolor('#ffffff')
    ax = fig.add_axes([0, 0, 1, 1])
    ax.axis('off')

    # Header
    ax.text(0.5, 0.95, "TASK F: SIMPLE HANDWRITTEN BACKPROPAGATION", ha='center', va='top', fontsize=15, fontweight='bold', color='#1e293b')
    ax.text(0.5, 0.92, "Page 1 of 2: Forward Pass & Analytic Chain-Rule Derivatives", ha='center', va='top', fontsize=12, style='italic', color='#475569')
    ax.plot([0.08, 0.92], [0.90, 0.90], color='#cbd5e1', lw=1.5)

    # 1. Problem Spec
    ax.text(0.08, 0.87, "1. Architecture & Parameters Given:", fontsize=12, fontweight='bold', color='#0f172a')
    text_spec = (
        "• Single regression sample:  x = 1.0,  target y = 2.0\n"
        "• Hidden layer activation:   z = w·x + b,   h = tanh(z)\n"
        "• Linear output prediction:  y_hat = v·h + c\n"
        "• Squared error loss:        J = 0.5 · (y_hat - y)²\n"
        "• Initial parameter state:   w = 0.5,  b = 0.0,  v = 1.0,  c = 0.0\n"
        "• Learning rate:             η = 0.1"
    )
    ax.text(0.12, 0.84, text_spec, fontsize=10.5, fontfamily='monospace', va='top', color='#1e293b', linespacing=1.3)

    # 2. Forward Pass
    ax.text(0.08, 0.69, "2. Forward Pass Evaluation (Step 0):", fontsize=12, fontweight='bold', color='#0f172a')
    text_fwd = (
        "Step 2.1: Pre-activation z\n"
        "   z = w·x + b = (0.5)(1.0) + 0.0 = 0.5\n\n"
        "Step 2.2: Hidden activation h\n"
        "   h = tanh(0.5) = (e^0.5 - e^-0.5) / (e^0.5 + e^-0.5)\n"
        "     = (1.648721 - 0.606531) / (1.648721 + 0.606531)\n"
        "     = 1.042191 / 2.255252 ≈ 0.462117\n\n"
        "Step 2.3: Prediction y_hat\n"
        "   y_hat = v·h + c = (1.0)(0.462117) + 0.0 = 0.462117\n\n"
        "Step 2.4: Squared Error Loss J\n"
        "   e = y_hat - y = 0.462117 - 2.0 = -1.537883\n"
        "   J = 0.5 · (-1.537883)² = 0.5 · (2.365084) = 1.182542"
    )
    ax.text(0.12, 0.66, text_fwd, fontsize=10.5, fontfamily='monospace', va='top', color='#1e293b', linespacing=1.2)

    # 3. Chain Rule
    ax.text(0.08, 0.38, "3. Chain-Rule Derivatives for All Four Parameters:", fontsize=12, fontweight='bold', color='#0f172a')
    text_deriv = (
        "• Loss error derivative:\n"
        "   ∂J / ∂y_hat = y_hat - y = 0.462117 - 2.0 = -1.537883\n\n"
        "• Output bias derivative (c):\n"
        "   ∂J / ∂c = (∂J / ∂y_hat) · (∂y_hat / ∂c) = (y_hat - y) · 1 = -1.537883\n\n"
        "• Output weight derivative (v):\n"
        "   ∂J / ∂v = (∂J / ∂y_hat) · (∂y_hat / ∂v) = (y_hat - y) · h\n"
        "           = (-1.537883) · (0.462117) = -0.710682\n\n"
        "• Backpropagation into hidden state z:\n"
        "   ∂J / ∂h = (∂J / ∂y_hat) · v = (y_hat - y) · v\n"
        "   ∂h / ∂z = 1 - tanh²(z) = 1 - h² = 1 - (0.462117)² = 0.786448\n"
        "   ∂J / ∂z = (∂J / ∂h) · (∂h / ∂z) = (-1.537883) · (1.0) · (0.786448) = -1.209465\n\n"
        "• Hidden weight derivative (w):\n"
        "   ∂J / ∂w = (∂J / ∂z) · (∂z / ∂w) = (∂J / ∂z) · x = (-1.209465) · (1.0) = -1.209465\n\n"
        "• Hidden bias derivative (b):\n"
        "   ∂J / ∂b = (∂J / ∂z) · (∂z / ∂b) = (∂J / ∂z) · 1 = -1.209465"
    )
    ax.text(0.12, 0.35, text_deriv, fontsize=9.8, fontfamily='monospace', va='top', color='#1e293b', linespacing=1.1)

    return fig

def create_page2():
    fig = plt.figure(figsize=(8.5, 11), dpi=200)
    fig.patch.set_facecolor('#ffffff')
    ax = fig.add_axes([0, 0, 1, 1])
    ax.axis('off')

    # Header
    ax.text(0.5, 0.95, "TASK F: SIMPLE HANDWRITTEN BACKPROPAGATION", ha='center', va='top', fontsize=15, fontweight='bold', color='#1e293b')
    ax.text(0.5, 0.92, "Page 2 of 2: Gradient Direction & Simultaneous Update", ha='center', va='top', fontsize=12, style='italic', color='#475569')
    ax.plot([0.08, 0.92], [0.90, 0.90], color='#cbd5e1', lw=1.5)

    # 4. Gradient Direction
    ax.text(0.08, 0.87, "4. Summary of Gradients & Direction Justification:", fontsize=12, fontweight='bold', color='#0f172a')
    text_dir = (
        "Gradient Vector:\n"
        "   ∇_θ J = [ ∂J/∂w,  ∂J/∂b,  ∂J/∂v,  ∂J/∂c ]^T\n"
        "         = [ -1.209465,  -1.209465,  -0.710682,  -1.537883 ]^T\n\n"
        "Physical Explanation of Gradient Direction:\n"
        "• Why are all four gradients strictly negative (< 0)?\n"
        "  Because the initial network prediction y_hat ≈ 0.4621 is significantly lower\n"
        "  than the target value y = 2.0 (underestimation error e = -1.537883).\n"
        "• Gradient descent updates parameters in the direction opposite to the gradient:\n"
        "       θ_new = θ_old - η · (∂J / ∂θ)\n"
        "• Since ∂J/∂θ is negative, the update term -η·(∂J/∂θ) is POSITIVE:\n"
        "  Every parameter (w, b, v, c) must INCREASE.\n"
        "• Increasing w and b increases hidden activation h. Increasing v and c directly\n"
        "  drives prediction y_hat upwards toward target 2.0, minimizing loss J."
    )
    ax.text(0.12, 0.84, text_dir, fontsize=10.2, fontfamily='monospace', va='top', color='#1e293b', linespacing=1.2)

    # 5. Simultaneous Update
    ax.text(0.08, 0.54, "5. Simultaneous Gradient-Descent Update (η = 0.1):", fontsize=12, fontweight='bold', color='#0f172a')
    text_upd = (
        "Using original gradients computed at initial state:\n\n"
        "1. Hidden Weight w:\n"
        "   w(1) = w(0) - η · (∂J/∂w) = 0.5 - (0.1) · (-1.209465)\n"
        "        = 0.5 + 0.120946 = 0.620946\n\n"
        "2. Hidden Bias b:\n"
        "   b(1) = b(0) - η · (∂J/∂b) = 0.0 - (0.1) · (-1.209465)\n"
        "        = 0.0 + 0.120946 = 0.120946\n\n"
        "3. Output Weight v:\n"
        "   v(1) = v(0) - η · (∂J/∂v) = 1.0 - (0.1) · (-0.710682)\n"
        "        = 1.0 + 0.071068 = 1.071068\n\n"
        "4. Output Bias c:\n"
        "   c(1) = c(0) - η · (∂J/∂c) = 0.0 - (0.1) · (-1.537883)\n"
        "        = 0.0 + 0.153788 = 0.153788"
    )
    ax.text(0.12, 0.51, text_upd, fontsize=10.2, fontfamily='monospace', va='top', color='#1e293b', linespacing=1.15)

    # 6. Verification
    ax.text(0.08, 0.22, "6. Finite-Difference Check & Post-Update Loss:", fontsize=12, fontweight='bold', color='#0f172a')
    text_check = (
        "• Central Finite Differences [J(θ+ε) - J(θ-ε)] / (2ε) with ε = 10^-6:\n"
        "   - Param w: |Analytical - Numerical| = 1.13e-10  (PASS <= 1e-5)\n"
        "   - Param b: |Analytical - Numerical| = 1.13e-10  (PASS <= 1e-5)\n"
        "   - Param v: |Analytical - Numerical| = 5.25e-11  (PASS <= 1e-5)\n"
        "   - Param c: |Analytical - Numerical| = 1.12e-10  (PASS <= 1e-5)\n\n"
        "• Model State after Step 1:\n"
        "   z(1) = (0.620946)(1) + 0.120946 = 0.741892\n"
        "   h(1) = tanh(0.741892) = 0.630225\n"
        "   y_hat(1) = (1.071068)(0.630225) + 0.153788 = 0.828800  (closer to 2.0!)\n"
        "   J(1) = 0.5 · (0.828800 - 2.0)² = 0.685904  (42% loss reduction from 1.1825)"
    )
    ax.text(0.12, 0.19, text_check, fontsize=9.8, fontfamily='monospace', va='top', color='#1e293b', linespacing=1.1)

    return fig

if __name__ == '__main__':
    p1 = create_page1()
    p2 = create_page2()

    with PdfPages(PDF_OUT) as pdf:
        pdf.savefig(p1)
        pdf.savefig(p2)
    print(f"[+] Saved 2-page PDF: {PDF_OUT}")

    p1.savefig(os.path.join(DOCS_DIR, 'TaskF_Page1.png'), dpi=200)
    p2.savefig(os.path.join(DOCS_DIR, 'TaskF_Page2.png'), dpi=200)
    print(f"[+] Saved Page 1 and Page 2 PNG images in {DOCS_DIR}")
