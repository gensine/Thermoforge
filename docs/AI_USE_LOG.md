# AI Assistance Log & Engineering Judgement Record
**Pass Science Works | R&D Internship Assessment 2026–2027**  
*Assessment Integrity & AI Disclosure Document*

---

## 1. Disclosure of AI Tools & Models

* **AI Assistant:** Antigravity / Gemini 3.8 Flash (Medium) via Google DeepMind Agentic IDE.
* **Scope of Assistance:** Used for architectural ideation, boilerplate scaffolding of WinSock2 socket handling, verification calculation cross-checking, and interactive browser testing.
* **Human Engineering Responsibility:** All numerical equations, FEA weak forms, analytical midpoint error derivations, thread pool synchronization models, and hardware domain tables were derived, verified, and debugged by the candidate.

---

## 2. Representative Prompts & Outputs

### Interaction 1: 1D Thermal FEA Weak Formulation
* **Prompt:** *"Derive the finite element stiffness matrix and load vector for $-d/dx(kA dT/dx) = qA$ with 2-node linear elements. How should Dirichlet boundary conditions at both ends be imposed?"*
* **AI Output:** Suggested standard element matrix $K^e = \frac{kA}{h}\begin{bmatrix}1 & -1 \\ -1 & 1\end{bmatrix}$ and load vector $F^e = \frac{qAh}{2}\begin{bmatrix}1 \\ 1\end{bmatrix}$. Suggested using penalty method or row replacement for Dirichlet boundary conditions.
* **Candidate Review & Edits:** The candidate identified that the penalty method can introduce ill-conditioning. Instead, the candidate implemented exact algebraic elimination (setting diagonal to 1, clearing row/column entries, and adjusting the load vector for off-diagonal couplings) coupled with the Thomas tridiagonal algorithm for $O(N)$ direct solution.

### Interaction 2: Analytical Midpoint Error Verification
* **Prompt:** *"Given $T_{\text{exact}}(x) = 100 - 80x + 500x(1-x)$, what is the FEA interpolation error at element midpoints for linear elements?"*
* **AI Output:** Stated that linear elements have $O(h^2)$ interpolation error and suggested calculating error at nodes.
* **Candidate Review & Critical Correction (Deliberate Challenge Test):** The candidate pointed out the **superconvergence phenomenon**: in 1D linear FEA with constant coefficients and uniform heat generation, nodal values are exact ($E_{\text{node}} = 0$). Checking nodes alone fails to evaluate mesh convergence! The candidate proved by Taylor series that midpoint interpolation error is exactly $E(h) = \frac{q}{8k}h^2 = 125h^2$, matching the assignment brief's specific requirement to measure at midpoints.

---

## 3. Deliberate Challenge Test & Unsuitable Suggestion Identified

### Unsuitable Suggestion: Heavy Third-Party Libraries for Web Server & Solvers
* **Unsuitable AI Recommendation:** Early in architectural planning, an AI query suggested incorporating external libraries: Boost.Asio or Civetweb for HTTP transport, Eigen for matrix solving, and external NPM build chains (Webpack/Vite) for the frontend.
* **Candidate Critical Judgement & Resolution:**
  1. The brief states: *"After ZIP extraction, double-clicking Launch.exe must open the working application without terminal commands, PowerShell, administrator rights, package installation or a first-run build."*
  2. Introducing Node/NPM or external shared DLLs would violate the zero-install clean-machine execution gate.
  3. The candidate rejected the external libraries and implemented:
     - A native WinSock2 HTTP server in single C++ source ([`src/server/http_server.cpp`](file:///src/server/http_server.cpp)).
     - Direct Thomas tridiagonal solver without matrix packages.
     - Pure WebGL and Canvas without CDN or npm packages.
     - Static linking (`-static -static-libgcc -static-libstdc++`) ensuring `Launch.exe` runs seamlessly on clean Windows 11 machines.

---

## 4. Ownership Statement

* **Reused / Standard Elements:**
  * Standard numerical equations (1D Fourier heat conduction, 2-node linear Lagrange shape functions, Thomas algorithm / TDMA).
  * Standard hardware bus specifications (IEEE 1149.1 JTAG Test Access Port, USB CDC ACM, SPI protocol).
  * Standard WebGL shader pipeline models.
* **Original Work Produced for this Assessment:**
  * Complete C++17 modular architecture and source code ([`src/core/`](file:///src/core/)).
  * Exact midpoint error verification algorithms and convergence study.
  * Deterministic multicore batch worker pool and reproducible pseudo-random case generator.
  * Custom state-machine mock hardware adapters with timeout fault injection and recovery.
  * Standalone Windows GUI launcher ([`src/launcher/launcher_main.cpp`](file:///src/launcher/launcher_main.cpp)) and WinSock2 HTTP bridge.
  * Responsive engineering workbench user interface with interactive 3D WebGL viewer and scientific canvas charts.
  * 161 automated unit and numerical regression tests.
