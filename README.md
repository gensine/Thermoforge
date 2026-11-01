# Unified Engineering Workbench

[![Windows 11 x64](https://img.shields.io/badge/Platform-Windows%2011%20x64-blue.svg)](file:///Launch.exe)
[![C++17](https://img.shields.io/badge/Standard-C%2B%2B17-blue.svg)](file:///CMakeLists.txt)
[![Tests Passing](https://img.shields.io/badge/Tests-161%20%2F%20161%20Passed-brightgreen.svg)](file:///tests/unit_tests.cpp)
[![License: CC0](https://img.shields.io/badge/License-CC0%20%2F%20Public%20Domain-lightgrey.svg)](file:///LICENSE)

A high-performance, modular engineering workbench developed for the **Pass Science Works R&D Internship Assessment 2026–2027**. The workbench integrates 3D CAD/FEA visualization, 1D steady-state thermal finite-element analysis with exact analytical verification, multi-threaded batch workloads, embedded hardware simulation with fault recovery, and neural network backpropagation.

---

## 1. Quick Start: One-Click Execution (For Evaluators)

This project is packaged for zero-friction evaluation on **Windows 11 x64**:
1. **Extract the ZIP package** (`CandidateID_Workbench.zip`) to any local directory.
2. **Double-click `Launch.exe`** at the root of the extracted folder.
3. The application will start immediately:
   * Native C++ embedded server binds to `127.0.0.1:8080`.
   * Automatically opens your default web browser to `http://127.0.0.1:8080/index.html`.
* **Zero terminal commands**, **zero PowerShell scripts**, **zero administrator rights**, and **zero internet connection** required.
* All runtime dependencies are statically linked into `Launch.exe`.

---

## 2. Editor Run Configuration (VS Code)

Pre-configured run tasks are located in [`.vscode/launch.json`](file:///.vscode/launch.json):
1. Open the project root in VS Code.
2. Select **"Launch Unified Engineering Workbench (Packaged)"** from the Run & Debug menu (`Ctrl+Shift+D`).
3. Press **F5**: The editor builds the latest binaries via CMake and starts `Launch.exe` in one action.
4. Additional configurations:
   * **"Run Automated Unit Tests"**: Runs the full 161-assertion C++ test suite.
   * **"Headless CLI (Run All Evidence Benchmarks)"**: Re-executes all numerical solvers and exports fresh CSV evidence into `results/`.

---

## 3. Architecture Overview

The system strictly decouples the C++17 computational engine from presentation and transport layers:

```
[ Frontend: HTML5 / WebGL / Canvas ]
                 │
                 │ IPC / HTTP REST (127.0.0.1:8080)
                 ▼
[ Embedded C++17 HTTP Server (WinSock2) ]
                 │
  ┌──────────────┼──────────────┬──────────────┬──────────────┐
  ▼              ▼              ▼              ▼              ▼
[1D Thermal   [CAD & ANSYS   [Multicore      [Embedded Mock  [Backprop
 FEA Solver]    Parsers]      Thread Pool]    Adapters]       Engine]
```

* **`src/core/fea/`**: 1D steady-state thermal FEA solver with linear 2-node elements, exact Dirichlet boundary condition imposition, $O(N)$ Thomas algorithm, and midpoint interpolation error calculation.
* **`src/core/parsers/`**: Streaming ASCII STL geometry parser and ANSYS nodal CSV results parser with NaN/Inf filtering.
* **`src/core/multicore/`**: Bounded worker thread pool, reproducible batch generator (fixed seed 42), wall-clock profiling, and serial vs parallel numerical consistency checks ($\max |\Delta T| \le 10^{-8}\,^\circ\text{C}$).
* **`src/core/embedded/`**: State machines for STM32 MCU, Lattice iCE40 FPGA, and FT2232H JTAG Debug Adapter with telemetry streaming and recoverable timeout fault simulation.
* **`src/core/backprop/`**: Neural network backpropagation engine without autodiff, verified against central finite differences within $10^{-10}$ error.
* **`src/server/`**: Standalone native HTTP/REST server (`ws2_32`) serving API endpoints and static assets with directory traversal protection.
* **`src/frontend/`**: Responsive modern studio dashboard featuring native 3D WebGL (flat-shaded STL mesh and Turbo colored point cloud with node inspector) and 2D canvas scientific charts.

---

## 4. Supported Formats & Tested Limits

### ASCII STL Geometry (`.stl`)
* **Format:** Standard ASCII StereoLithography (`solid ... facet normal ... vertex ... endsolid`).
* **Validation:** Rejects NaN/Inf coordinates, degenerate vertices, nested facets, and empty files.
* **Tested Limits:** Stream-validated from 12 facets up to **250,000 facets** (~50 MB).

### ANSYS Nodal Temperature CSV (`.csv`)
* **Columns:** `node_id,x_m,y_m,z_m,temperature_C`.
* **Validation:** Strict header detection, integer ID verification, float validation, and NaN/Inf rejection.
* **Tested Limits:** Validated up to **1,000,000 nodes** (~65 MB).
* **Sample Data:** Self-created synthetic dataset provided in [`data/ansys_thermal_results.csv`](file:///data/ansys_thermal_results.csv) (labeled synthetic as required).

---

## 5. Building from Source (CMake)

### Prerequisites
* Windows 11 x64 (or Linux x64)
* CMake 3.16+
* C++17 compliant compiler (GCC 9+, Clang 10+, or MSVC 2019+)

### Build Commands
```powershell
# Configure release build with Ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build all executables and libraries
cmake --build build

# Execute automated test suite (161 assertions)
ctest --test-dir build --output-on-failure
```

---

## 6. Verification & Test Suite

Run unit tests directly:
```powershell
.\build\unit_tests.exe
```
Results summary:
* **Thermal FEA:** Nodal temperatures exact; midpoint interpolation error matches $125 h^2$ ($1.25\,^\circ\text{C}$ for $N=10$, $0.3125\,^\circ\text{C}$ for $N=20$, $0.078125\,^\circ\text{C}$ for $N=40$, $0.019531\,^\circ\text{C}$ for $N=80$); observed convergence rate $= 2.00$.
* **Parsers:** Positive and negative test coverage for STL and CSV.
* **Multicore Agreement:** Max serial/parallel difference $= 0.000\,^\circ\text{C} \le 10^{-8}\,^\circ\text{C}$.
* **Embedded Telemetry:** Successful timeout fault injection and link recovery.
* **Backpropagation:** Analytic gradients match central finite differences with errors $< 1.2 \times 10^{-10}$ (threshold $\le 10^{-5}$).

---

## 7. Known Limitations & Tradeoffs

1. **1D Rod Geometry:** The FEA solver is specialized for 1D rods with constant cross-section. Extension to 2D/3D would require unstructured tetrahedral/quad meshing.
2. **ASCII STL Parsing:** The current parser supports standard ASCII STL files. Binary STL parsing was intentionally excluded as ASCII was specifically requested in the assignment brief.
3. **Mock Hardware Emulation:** Device adapters simulate hardware state machines, rail voltages, and communication timeouts over C++ models rather than talking to physical FTDI/USB silicon.
4. **Single-Sample Neural Network:** Task F implements the exact single regression sample specified ($x=1, y=2$). Batch gradient descent on mini-batches is not required for this formulation.

---

## 8. Repository Structure

```
.
├── Launch.exe                    # Zero-install one-click launcher (Win 11 x64)
├── CMakeLists.txt                # CMake build configuration
├── BUILD_INSTRUCTIONS.md         # Detailed build and compiler guide
├── README.md                     # Project documentation (this file)
├── .vscode/                      # Editor run configurations
│   ├── launch.json
│   └── tasks.json
├── data/                         # Sample datasets with provenance notes
│   ├── README.md
│   ├── sample_geometry.stl       # Sample ASCII STL calibration wedge
│   └── ansys_thermal_results.csv # 1,000-node synthetic ANSYS export
├── docs/                         # Engineering documentation & evidence
│   ├── ENGINEERING_REPORT.md     # 6-page comprehensive technical report
│   ├── TaskF_Handwritten_Derivation.pdf # 2-page Task F handwritten derivation
│   ├── TASK_F_HANDWRITTEN.md     # Task F backpropagation derivation
│   ├── AI_USE_LOG.md             # AI assistance disclosure and challenge test
│   └── WALKTHROUGH_LOGS.md       # Timestamped execution logs and walkthrough
├── results/                      # Numerical benchmark outputs & CSVs
│   ├── thermal_fea_solution.csv
│   ├── mesh_convergence.csv
│   ├── multicore_benchmark.csv
│   └── backprop_training_history.csv
├── src/
│   ├── core/                     # C++17 Core Computational Engine
│   │   ├── fea/                  # 1D Thermal FEA solver
│   │   ├── parsers/              # STL and ANSYS CSV parsers
│   │   ├── multicore/            # Thread pool and batch benchmark
│   │   ├── embedded/             # Mock hardware adapters and telemetry
│   │   └── backprop/             # Backprop engine and gradient verifier
│   ├── server/                   # Native HTTP & REST server (WinSock2)
│   ├── launcher/                 # WinMain entry point for Launch.exe
│   ├── cli/                      # Headless CLI runner
│   └── frontend/                 # Responsive WebGL & Canvas dashboard
│       ├── index.html
│       ├── css/workbench.css
│       └── js/
│           ├── app.js
│           ├── charts.js
│           └── three_viewer.js
└── tests/
    └── unit_tests.cpp            # Automated test suite (161 assertions)
```
