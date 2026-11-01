# Dataset Provenance, Schemas & Format Specifications

This directory contains reference datasets for CAD (ASCII STL) geometry and ANSYS FEA nodal temperature exports as mandated by **Task B** of the Pass Science Works assignment brief.

---

## 1. ASCII STL Geometry Dataset (`sample_geometry.stl`)

### Provenance
* **Origin:** Self-created synthetic standard engineering calibration wedge.
* **License:** Creative Commons Zero (CC0) / Public Domain.
* **Solid Name:** `calibration_wedge`
* **Format:** Standard ASCII StereoLithography (`.stl`).

### Units & Coordinate System
* **Spatial Coordinates ($x, y, z$):** Millimeters [mm].
* **Normal Vector:** Dimensionless unit vector $[\hat{n}_x, \hat{n}_y, \hat{n}_z]$.
* **Topology:** 12 triangular facets defining a 6-faced closed polyhedron (wedge with chamfered profile).

### Tested File-Size & Scaling Limits
* **Tested Input Limits:** Tested on meshes from $12$ facets ($1.5\text{ KB}$) up to $250,000$ facets ($\approx 50\text{ MB}$).
* **Memory Complexity:** Streaming parser processes lines in $O(1)$ auxiliary space and allocates $9 \times 4\text{ bytes}$ per facet for WebGL vertex buffers.
* **Rejection Criteria:** Rejects non-finite values (`NaN`, `Inf`), missing facet normal components, nested `facet` or `outer loop` blocks, non-triangular facets, and empty/whitespace-only files.

---

## 2. ANSYS Nodal Temperature Results CSV (`ansys_thermal_results.csv`)

### Provenance
* **Origin:** **SYNTHETIC DATASET** (Self-created thermal diffusion model, explicitly labeled synthetic in accordance with brief requirements).
* **Generator:** `workbench::parsers::ANSYSCSVParser::generate_synthetic_dataset()`
* **License:** Public Domain / Open Benchmark.

### Schema & Units
* **Header:** `node_id,x_m,y_m,z_m,temperature_C`
* **Field Specifications:**
  | Column | Data Type | Units | Physical Interpretation |
  | :--- | :---: | :---: | :--- |
  | `node_id` | Integer (64-bit signed) | Dimensionless | Unique positive node identifier |
  | `x_m` | Floating Point (64-bit IEEE) | Meters [m] | Longitudinal spatial coordinate ($0.00$ to $0.20\text{ m}$) |
  | `y_m` | Floating Point (64-bit IEEE) | Meters [m] | Transverse spatial coordinate ($0.00$ to $0.10\text{ m}$) |
  | `z_m` | Floating Point (64-bit IEEE) | Meters [m] | Normal spatial coordinate ($0.00$ to $0.05\text{ m}$) |
  | `temperature_C` | Floating Point (64-bit IEEE) | Degrees Celsius [$^\circ\text{C}$] | Nodal thermal equilibrium temperature |

### Temperature Range & Physics
* **Domain Dimensions:** $0.2\text{ m} \times 0.1\text{ m} \times 0.05\text{ m}$ ($200 \times 100 \times 50\text{ mm}$).
* **Nodes Count:** $1,000$ nodes ($10 \times 10 \times 10$ structured grid).
* **Temperature Field:** Gaussian thermal distribution with a localized internal heat dissipation core peaking at $120.0\,^\circ\text{C}$ and ambient boundaries near $25.0\,^\circ\text{C}$.

### Tested Limits
* **Tested Row Counts:** Verified up to $1,000,000$ nodes ($\approx 65\text{ MB}$ CSV file).
* **Validation Rules:** Rejects negative/missing IDs, missing columns, non-finite values (`NaN`, `Inf`), corrupted numeric tokens, and empty files.
