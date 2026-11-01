# Build and Packaging Instructions

This document provides step-by-step instructions to build the **Unified Engineering Workbench** from source using CMake and standard C++17 compilers.

---

## 1. Zero-Install Execution (For Evaluators)

If you downloaded the packaged archive (`CandidateID_Workbench.zip`):
1. **Extract the ZIP archive** to any folder on Windows 11 x64.
2. **Double-click `Launch.exe`** at the root of the extracted folder.
3. The application will immediately start its embedded C++ server and open the workbench UI in your default web browser at `http://127.0.0.1:8080/index.html`.
* **Zero terminal commands**, **zero PowerShell scripts**, **zero administrator rights**, and **zero internet access** are required.

---

## 2. Editor Run Configuration (VS Code)

The repository includes a ready-to-use editor run configuration in [`.vscode/launch.json`](file:///.vscode/launch.json):
1. Open the repository root folder in VS Code.
2. Navigate to the **Run & Debug** panel (`Ctrl+Shift+D`).
3. Select **"Launch Unified Engineering Workbench (Packaged)"** from the dropdown.
4. Press `F5` or click **Start Debugging**.
   * The editor will automatically build the latest binaries via [`.vscode/tasks.json`](file:///.vscode/tasks.json) and start `Launch.exe` in one action.
5. Alternative editor configurations available:
   * **"Run Automated Unit Tests"**: Executes the full 161-assertion C++ test suite.
   * **"Headless CLI (Run All Evidence Benchmarks)"**: Runs FEA, convergence, multicore benchmarks, and backprop training, regenerating raw CSV evidence in `results/`.
   * **"Run C++ Server (Console)"**: Starts the backend server with an attached console window for debugging.

---

## 3. Building from Source via CMake

### Prerequisites
* **C++ Compiler:** Any C++17 compliant compiler (GCC 9+, Clang 10+, or MSVC 2019+).
* **Build System:** CMake 3.16+ and Ninja or Make.

### Command-Line Build Steps

#### Using CMake & Ninja (Recommended)
```powershell
# 1. Configure the project for Release build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# 2. Compile all targets (core library, tests, CLI, server, and Launch.exe)
cmake --build build

# 3. Run the automated test suite
ctest --test-dir build --output-on-failure
```

#### Using MinGW Makefile (Alternative)
```powershell
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### Using Visual Studio (MSVC)
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

---

## 4. Built Artifacts Summary

| Executable | Location | Description |
| :--- | :--- | :--- |
| `Launch.exe` | `<root>/Launch.exe` | Standalone Windows GUI one-click launcher (zero console prompt). |
| `unit_tests.exe` | `<root>/build/unit_tests.exe` | Comprehensive unit and numerical test suite (161 assertions). |
| `workbench_cli.exe` | `<root>/build/workbench_cli.exe` | Headless batch and evidence generation runner. |
| `workbench_server.exe` | `<root>/build/workbench_server.exe` | Standalone console backend server. |
| `libworkbench_core.a` | `<root>/build/libworkbench_core.a` | Core computational static library. |
