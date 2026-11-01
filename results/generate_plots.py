import os
import csv
import math
import matplotlib.pyplot as plt
import numpy as np

# Set dark/engineering style
plt.style.use('dark_background')
plt.rcParams['font.sans-serif'] = 'Segoe UI, Helvetica, Arial'
plt.rcParams['axes.edgecolor'] = '#334155'
plt.rcParams['axes.linewidth'] = 0.8
plt.rcParams['grid.color'] = '#1e293b'
plt.rcParams['grid.linestyle'] = '--'
plt.rcParams['grid.alpha'] = 0.7

RESULTS_DIR = os.path.dirname(os.path.abspath(__file__))

def plot_thermal_fea():
    csv_path = os.path.join(RESULTS_DIR, 'thermal_fea_solution.csv')
    if not os.path.exists(csv_path):
        print(f"Skipping thermal plot: {csv_path} not found")
        return

    nodes_x, nodes_T, nodes_exact = [], [], []
    mids_x, mids_T, mids_exact, mids_err = [], [], [], []

    with open(csv_path, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].startswith('#') or row[0] == 'type':
                continue
            r_type = row[0]
            x = float(row[2])
            t_fe = float(row[3])
            t_ex = float(row[4])
            err = float(row[5])
            if r_type == 'node':
                nodes_x.append(x)
                nodes_T.append(t_fe)
                nodes_exact.append(t_ex)
            elif r_type == 'midpoint':
                mids_x.append(x)
                mids_T.append(t_fe)
                mids_exact.append(t_ex)
                mids_err.append(err)

    fig, ax = plt.subplots(figsize=(8.5, 5), dpi=300)

    # Continuous exact analytical curve
    x_fine = np.linspace(0.0, 1.0, 200)
    # T_exact(x) = 100 - 80*x + 500*x*(1 - x)
    T_fine = 100.0 - 80.0 * x_fine + 500.0 * x_fine * (1.0 - x_fine)
    ax.plot(x_fine, T_fine, color='#f59e0b', linewidth=2.5, label='Exact Analytical: $T_{exact}(x) = 100 - 80x + 500x(1-x)$', zorder=2)

    # FEA Linear Piecewise Solution
    ax.plot(nodes_x, nodes_T, color='#38bdf8', linewidth=2.0, linestyle='-', label=f'FEA Linear Solution (N={len(nodes_x)-1} elements)', zorder=3)
    ax.scatter(nodes_x, nodes_T, color='#38bdf8', s=40, edgecolor='#ffffff', linewidth=1, label='FEA Nodes ($T_0=100^\\circ$C, $T_L=20^\\circ$C)', zorder=4)

    # Midpoints with error indicators
    ax.scatter(mids_x, mids_T, color='#10b981', marker='s', s=45, edgecolor='#ffffff', linewidth=1, label=f'Element Midpoints ($E_{{max}}={max(mids_err):.4f}^\\circ$C)', zorder=5)

    ax.set_title("1D Steady-State Thermal FEA vs. Analytical Solution", fontsize=13, fontweight='bold', pad=12, color='#f8fafc')
    ax.set_xlabel("Position along Rod $x$ [meters]", fontsize=11, labelpad=8)
    ax.set_ylabel("Temperature $T$ [$^\\circ$C]", fontsize=11, labelpad=8)
    ax.grid(True)
    ax.set_xlim(-0.02, 1.02)
    ax.set_ylim(0, 210)
    ax.legend(frameon=True, facecolor='#0f172a', edgecolor='#334155', fontsize=9.5, loc='lower center')

    out_file = os.path.join(RESULTS_DIR, 'thermal_fea_comparison.png')
    plt.tight_layout()
    plt.savefig(out_file, dpi=300)
    plt.close()
    print(f"[+] Saved: {out_file}")

def plot_convergence():
    csv_path = os.path.join(RESULTS_DIR, 'mesh_convergence.csv')
    if not os.path.exists(csv_path):
        return

    elements, h_vals, max_errors = [], [], []
    with open(csv_path, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].startswith('#') or row[0] == 'elements':
                continue
            elements.append(int(row[0]))
            h_vals.append(float(row[1]))
            max_errors.append(float(row[2]))

    fig, ax = plt.subplots(figsize=(7.5, 5), dpi=300)

    ax.loglog(h_vals, max_errors, marker='o', markersize=8, color='#38bdf8', linewidth=2.5, label='Measured FEA Midpoint Error', zorder=3)

    # Theoretical O(h^2) slope guideline: E = 125 * h^2
    h_ref = np.array(h_vals)
    e_ref = 125.0 * (h_ref ** 2)
    ax.loglog(h_ref, e_ref, linestyle='--', color='#f59e0b', linewidth=2.0, label='Theoretical 2nd-Order: $E_{mid}(h) = 125 h^2$ (Slope = 2.0)', zorder=2)

    for el, h, err in zip(elements, h_vals, max_errors):
        ax.annotate(f"N={el}\n({err:.4f}$^\\circ$C)", (h, err), textcoords="offset points", xytext=(12, -8),
                    color='#f8fafc', fontsize=8.5, fontweight='bold')

    ax.set_title("Mesh Convergence Study: Midpoint Error vs. Element Size $h$", fontsize=13, fontweight='bold', pad=12, color='#f8fafc')
    ax.set_xlabel("Element Length $h = L/N$ [meters] (Log Scale)", fontsize=11, labelpad=8)
    ax.set_ylabel("Maximum Absolute Midpoint Error [$^\\circ$C] (Log Scale)", fontsize=11, labelpad=8)
    ax.grid(True, which="both", ls="--")
    ax.legend(frameon=True, facecolor='#0f172a', edgecolor='#334155', fontsize=9.5, loc='upper left')

    out_file = os.path.join(RESULTS_DIR, 'mesh_convergence_loglog.png')
    plt.tight_layout()
    plt.savefig(out_file, dpi=300)
    plt.close()
    print(f"[+] Saved: {out_file}")

def plot_benchmark():
    csv_path = os.path.join(RESULTS_DIR, 'multicore_benchmark.csv')
    if not os.path.exists(csv_path):
        return

    workers, medians, speedups, efficiencies = [], [], [], []
    with open(csv_path, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].startswith('#') or row[0] == 'workers':
                continue
            workers.append(int(row[0]))
            medians.append(float(row[6]))
            speedups.append(float(row[7]))
            efficiencies.append(float(row[8]))

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4.5), dpi=300)

    # Speedup plot
    x_pos = np.arange(len(workers))
    width = 0.45

    ax1.bar(x_pos, speedups, width=width, color='#38bdf8', edgecolor='#ffffff', linewidth=1, label='Measured Speedup $T_1 / T_p$')
    ax1.plot(x_pos, workers, color='#f59e0b', linestyle='--', linewidth=2, marker='^', label='Ideal Linear Scaling')
    ax1.set_xticks(x_pos)
    ax1.set_xticklabels([f"{w} Workers" for w in workers])
    ax1.set_title("Parallel Compute Speedup", fontsize=11, fontweight='bold', color='#f8fafc')
    ax1.set_ylabel("Speedup Factor $S_p$", fontsize=10)
    ax1.set_ylim(0, max(workers) + 0.5)
    ax1.grid(True)
    ax1.legend(frameon=True, facecolor='#0f172a', edgecolor='#334155', fontsize=8.5)

    for i, s in enumerate(speedups):
        ax1.text(i, s + 0.12, f"{s:.2f}x", ha='center', color='#ffffff', fontweight='bold', fontsize=9)

    # Efficiency plot
    ax2.bar(x_pos, [e * 100 for e in efficiencies], width=width, color='#10b981', edgecolor='#ffffff', linewidth=1, label='Parallel Efficiency')
    ax2.axhline(100, color='#f59e0b', linestyle='--', linewidth=1.5, label='100% Ideal Target')
    ax2.set_xticks(x_pos)
    ax2.set_xticklabels([f"{w} Workers" for w in workers])
    ax2.set_title("Parallel Worker Efficiency", fontsize=11, fontweight='bold', color='#f8fafc')
    ax2.set_ylabel("Efficiency [$\\%$]", fontsize=10)
    ax2.set_ylim(0, 115)
    ax2.grid(True)
    ax2.legend(frameon=True, facecolor='#0f172a', edgecolor='#334155', fontsize=8.5)

    for i, e in enumerate(efficiencies):
        ax2.text(i, e*100 + 3, f"{e*100:.1f}%", ha='center', color='#ffffff', fontweight='bold', fontsize=9)

    fig.suptitle("Task D: Multicore Execution Scaling Profile (500 Solves, 40 Elements, 5 Repetitions)", fontsize=12, fontweight='bold', y=0.98)
    out_file = os.path.join(RESULTS_DIR, 'multicore_benchmark_scaling.png')
    plt.tight_layout()
    plt.savefig(out_file, dpi=300)
    plt.close()
    print(f"[+] Saved: {out_file}")

def plot_backprop():
    csv_path = os.path.join(RESULTS_DIR, 'backprop_training_history.csv')
    if not os.path.exists(csv_path):
        return

    steps, losses, y_hats = [], [], []
    with open(csv_path, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].startswith('#') or row[0] == 'step':
                continue
            steps.append(int(row[0]))
            y_hats.append(float(row[9]))
            losses.append(float(row[10]))

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4.5), dpi=300)

    # Loss Curve
    ax1.plot(steps, losses, color='#a855f7', linewidth=2.5, label='Loss $J = 0.5(\\hat{y} - y)^2$')
    ax1.scatter([0], [losses[0]], color='#f43f5e', s=60, zorder=4, label=f'Step 0: J={losses[0]:.4f}')
    ax1.scatter([steps[-1]], [losses[-1]], color='#10b981', s=60, zorder=4, label=f'Step 100: J={losses[-1]:.2e}')
    ax1.set_title("Training Loss Convergence (100 Steps)", fontsize=11, fontweight='bold', color='#f8fafc')
    ax1.set_xlabel("Optimization Step", fontsize=10)
    ax1.set_ylabel("Loss Value $J$", fontsize=10)
    ax1.grid(True)
    ax1.legend(frameon=True, facecolor='#0f172a', edgecolor='#334155', fontsize=8.5)

    # Prediction y_hat convergence
    ax2.plot(steps, y_hats, color='#38bdf8', linewidth=2.5, label='Prediction $\\hat{y} = vh + c$')
    ax2.axhline(2.0, color='#f59e0b', linestyle='--', linewidth=1.5, label='Target Value $y = 2.0$')
    ax2.set_title("Output Prediction $\\hat{y}$ vs. Target $y$", fontsize=11, fontweight='bold', color='#f8fafc')
    ax2.set_xlabel("Optimization Step", fontsize=10)
    ax2.set_ylabel("Value", fontsize=10)
    ax2.grid(True)
    ax2.legend(frameon=True, facecolor='#0f172a', edgecolor='#334155', fontsize=8.5)

    fig.suptitle("Task F: Neural Network Gradient Descent Optimization ($\\eta = 0.1$)", fontsize=12, fontweight='bold', y=0.98)
    out_file = os.path.join(RESULTS_DIR, 'backpropagation_loss_curve.png')
    plt.tight_layout()
    plt.savefig(out_file, dpi=300)
    plt.close()
    print(f"[+] Saved: {out_file}")

if __name__ == '__main__':
    plot_thermal_fea()
    plot_convergence()
    plot_benchmark()
    plot_backprop()
