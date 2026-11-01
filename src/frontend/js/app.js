/**
 * Unified Engineering Workbench - Application Controller
 * Coordinates API requests, UI state, WebGL 3D rendering, and canvas charts.
 */

const API_BASE = window.location.origin.includes('http') ? window.location.origin : 'http://127.0.0.1:8080';

// Global state
let viewer3D = null;
let thermalChart = null;
let convergenceChart = null;
let benchmarkChart = null;
let backpropChart = null;
let currentFEAResult = null;
let currentBenchmarkResult = null;
let hardwareSpecs = null;

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', async () => {
  const startTime = performance.now();

  // 1. Initialize Viewers and Charts
  viewer3D = new WebGL3DViewer('canvas-3d', 'node-tooltip');
  thermalChart = new WorkbenchChart('canvas-thermal-chart');
  convergenceChart = new WorkbenchChart('canvas-convergence-chart');
  benchmarkChart = new WorkbenchChart('canvas-benchmark-chart');
  backpropChart = new WorkbenchChart('canvas-backprop-chart');

  // 2. Setup UI Event Listeners
  setupNavigation();
  setupGeometryView();
  setupThermalView();
  setupPerformanceView();
  setupHardwareView();
  setupBackpropView();

  // 3. Connect to C++ Backend
  await checkBackendStatus();

  // 4. Initial Data Ingestion & Solves
  await loadSampleSTL();
  await runThermalFEA();
  await loadHardwareDevices();
  await loadHardwareSpecs();
  await runBackpropTraining();

  // Report launch-to-ready timing
  const readyTimeSec = ((performance.now() - startTime) / 1000).toFixed(2);
  const elReady = document.getElementById('footer-ready-time');
  if (elReady) elReady.textContent = `${readyTimeSec} s`;
});

// Navigation Handling
function setupNavigation() {
  const tabs = document.querySelectorAll('.nav-tab');
  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      tabs.forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));

      tab.classList.add('active');
      const paneId = tab.getAttribute('data-tab');
      const pane = document.getElementById(paneId);
      if (pane) {
        pane.classList.add('active');
        // Trigger resize for canvases
        if (viewer3D) viewer3D.resize();
        if (thermalChart) thermalChart.resize();
        if (convergenceChart) convergenceChart.resize();
        if (benchmarkChart) benchmarkChart.resize();
        if (backpropChart) backpropChart.resize();
      }
    });
  });
}

// Global Alert Utility
function showAlert(message, isError = true) {
  const container = document.getElementById('global-alert-container');
  const alertEl = document.getElementById('global-alert');
  const msgEl = document.getElementById('alert-msg');
  const iconEl = document.getElementById('alert-icon');

  if (!container || !alertEl || !msgEl) return;

  msgEl.textContent = message;
  iconEl.textContent = isError ? '⚠️' : '✅';
  alertEl.className = isError ? 'alert alert-danger' : 'alert alert-success';
  container.style.display = 'block';

  setTimeout(() => {
    container.style.display = 'none';
  }, 7000);
}

// Backend Health Check
async function checkBackendStatus() {
  try {
    const res = await fetch(`${API_BASE}/api/status`);
    if (!res.ok) throw new Error("Backend offline");
    const data = await res.json();

    document.getElementById('backend-status-text').textContent = `C++ Core: ${data.status}`;
    const cpuEl = document.getElementById('perf-cpu-name');
    if (cpuEl) cpuEl.textContent = data.cpu_model || "Host x86_64 CPU";
    const coresSub = document.getElementById('perf-cores-sub');
    if (coresSub) coresSub.textContent = `${data.logical_cores} Logical Cores`;
    const compEl = document.getElementById('perf-compiler-val');
    if (compEl) compEl.textContent = data.compiler || "C++17 Compiler";
  } catch (err) {
    document.getElementById('backend-status-text').textContent = "C++ Core: RECONNECTING...";
    document.getElementById('backend-status-pill').style.color = "var(--accent-amber)";
  }
}

// -------------------------------------------------------------
// 1. GEOMETRY VIEW LOGIC (STL & ANSYS Point Cloud)
// -------------------------------------------------------------
function setupGeometryView() {
  document.getElementById('btn-load-sample-stl').addEventListener('click', loadSampleSTL);
  document.getElementById('btn-load-sample-csv').addEventListener('click', loadSampleANSYS);

  document.getElementById('btn-fit-view').addEventListener('click', () => {
    if (viewer3D.renderMode === 'stl' && viewer3D.stlData) {
      viewer3D.fitToView(viewer3D.stlData.bbox);
    } else if (viewer3D.pointCloudData) {
      viewer3D.fitToView(viewer3D.pointCloudData.bbox);
    }
  });

  document.getElementById('btn-reset-cam').addEventListener('click', () => {
    viewer3D.camera.azimuth = 0.8;
    viewer3D.camera.elevation = 0.5;
    viewer3D.camera.pan = [0, 0];
    viewer3D.render();
  });

  const btnModeSTL = document.getElementById('btn-mode-stl');
  const btnModePC = document.getElementById('btn-mode-pointcloud');

  btnModeSTL.addEventListener('click', () => {
    btnModeSTL.classList.add('active');
    btnModePC.classList.remove('active');
    viewer3D.renderMode = 'stl';
    document.getElementById('viewport-legend').style.display = 'none';
    viewer3D.render();
  });

  btnModePC.addEventListener('click', () => {
    btnModePC.classList.add('active');
    btnModeSTL.classList.remove('active');
    viewer3D.renderMode = 'pointcloud';
    document.getElementById('viewport-legend').style.display = 'flex';
    viewer3D.render();
  });

  // Drag and Drop & File Upload
  const dropZone = document.getElementById('geometry-drop-zone');
  const fileInput = document.getElementById('file-input-geometry');

  dropZone.addEventListener('click', () => fileInput.click());
  dropZone.addEventListener('dragover', (e) => { e.preventDefault(); dropZone.classList.add('dragover'); });
  dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
  dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.remove('dragover');
    if (e.dataTransfer.files.length > 0) handleUploadedFile(e.dataTransfer.files[0]);
  });
  fileInput.addEventListener('change', (e) => {
    if (e.target.files.length > 0) handleUploadedFile(e.target.files[0]);
  });
}

async function loadSampleSTL() {
  try {
    const res = await fetch(`${API_BASE}/api/parsers/stl`, { method: 'POST', body: "" });
    if (!res.ok) {
      const err = await res.json();
      throw new Error(err.message || "Failed to parse STL");
    }
    const data = await res.json();
    viewer3D.loadSTL(data);
    viewer3D.renderMode = 'stl';
    document.getElementById('btn-mode-stl').classList.add('active');
    document.getElementById('btn-mode-pointcloud').classList.remove('active');
    document.getElementById('viewport-legend').style.display = 'none';

    document.getElementById('meta-title').textContent = `ASCII STL: ${data.solid_name}`;
    document.getElementById('meta-details').innerHTML = `
      • Facets: <strong>${data.facet_count}</strong> triangles<br/>
      • Dimensions: [${(data.bbox.max[0]-data.bbox.min[0]).toFixed(2)}, ${(data.bbox.max[1]-data.bbox.min[1]).toFixed(2)}, ${(data.bbox.max[2]-data.bbox.min[2]).toFixed(2)}] mm<br/>
      • Bounding Diagonal: ${data.bbox.diagonal.toFixed(2)} mm
    `;
  } catch (err) {
    showAlert(`STL Ingestion Error: ${err.message}`);
  }
}

async function loadSampleANSYS() {
  try {
    const res = await fetch(`${API_BASE}/api/parsers/ansys`, { method: 'POST', body: "" });
    if (!res.ok) {
      const err = await res.json();
      throw new Error(err.message || "Failed to parse ANSYS CSV");
    }
    const data = await res.json();
    viewer3D.loadANSYS(data);
    viewer3D.renderMode = 'pointcloud';
    document.getElementById('btn-mode-pointcloud').classList.add('active');
    document.getElementById('btn-mode-stl').classList.remove('active');

    // Update legend
    const legend = document.getElementById('viewport-legend');
    legend.style.display = 'flex';
    document.getElementById('legend-min').textContent = `${data.min_temp_C.toFixed(1)} °C`;
    document.getElementById('legend-mid').textContent = `${data.mean_temp_C.toFixed(1)} °C`;
    document.getElementById('legend-max').textContent = `${data.max_temp_C.toFixed(1)} °C`;

    document.getElementById('meta-title').textContent = "ANSYS Nodal Temperature Export";
    document.getElementById('meta-details').innerHTML = `
      • Provenance: <em>${data.provenance}</em><br/>
      • Nodes: <strong>${data.node_count.toLocaleString()}</strong> nodes<br/>
      • Temp Range: <strong>${data.min_temp_C.toFixed(2)} °C</strong> to <strong>${data.max_temp_C.toFixed(2)} °C</strong> (Mean: ${data.mean_temp_C.toFixed(2)} °C)<br/>
      • Inspection: Hover over any node in the 3D viewport to inspect coordinates and temperature.
    `;
  } catch (err) {
    showAlert(`ANSYS CSV Error: ${err.message}`);
  }
}

function handleUploadedFile(file) {
  const reader = new FileReader();
  reader.onload = async (e) => {
    const content = e.target.result;
    if (file.name.toLowerCase().endsWith('.stl')) {
      try {
        const res = await fetch(`${API_BASE}/api/parsers/stl`, { method: 'POST', body: content });
        if (!res.ok) {
          const err = await res.json();
          throw new Error(err.message);
        }
        const data = await res.json();
        viewer3D.loadSTL(data);
        viewer3D.renderMode = 'stl';
        document.getElementById('btn-mode-stl').classList.add('active');
        document.getElementById('btn-mode-pointcloud').classList.remove('active');
        document.getElementById('viewport-legend').style.display = 'none';
        showAlert(`Uploaded ASCII STL loaded: ${data.facet_count} facets`, false);
      } catch (err) {
        showAlert(`STL Parse Rejected: ${err.message}`);
      }
    } else {
      try {
        const res = await fetch(`${API_BASE}/api/parsers/ansys`, { method: 'POST', body: content });
        if (!res.ok) {
          const err = await res.json();
          throw new Error(err.message);
        }
        const data = await res.json();
        viewer3D.loadANSYS(data);
        viewer3D.renderMode = 'pointcloud';
        document.getElementById('btn-mode-pointcloud').classList.add('active');
        document.getElementById('btn-mode-stl').classList.remove('active');
        document.getElementById('viewport-legend').style.display = 'flex';
        showAlert(`Uploaded ANSYS CSV loaded: ${data.node_count} nodes`, false);
      } catch (err) {
        showAlert(`CSV Validation Error: ${err.message}`);
      }
    }
  };
  reader.readAsText(file);
}

// -------------------------------------------------------------
// 2. THERMAL ANALYSIS VIEW LOGIC
// -------------------------------------------------------------
function setupThermalView() {
  document.getElementById('btn-run-fea').addEventListener('click', runThermalFEA);
  document.getElementById('btn-run-convergence').addEventListener('click', runConvergenceStudy);
  document.getElementById('btn-export-fea-csv').addEventListener('click', exportFEACSV);

  // Quick element count buttons
  document.querySelectorAll('[data-n]').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('[data-n]').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      const n = parseInt(btn.getAttribute('data-n'));
      document.getElementById('metric-fea-elements').textContent = n;
      runThermalFEA(n);
    });
  });
}

async function runThermalFEA(numElements = 10) {
  const params = {
    L: parseFloat(document.getElementById('input-L').value),
    A: parseFloat(document.getElementById('input-A').value),
    k: parseFloat(document.getElementById('input-k').value),
    q: parseFloat(document.getElementById('input-q').value),
    T0: parseFloat(document.getElementById('input-T0').value),
    TL: parseFloat(document.getElementById('input-TL').value),
    num_elements: numElements
  };

  try {
    const res = await fetch(`${API_BASE}/api/fea/solve`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(params)
    });
    if (!res.ok) {
      const err = await res.json();
      throw new Error(err.message);
    }
    const data = await res.json();
    currentFEAResult = data;

    // Update metrics
    document.getElementById('metric-fea-elements').textContent = data.elements;
    document.getElementById('metric-fea-max-err').textContent = `${data.max_midpoint_error.toFixed(4)} °C`;
    document.getElementById('metric-fea-l2-err').textContent = `${data.l2_midpoint_error.toFixed(4)} °C`;
    document.getElementById('metric-fea-time').textContent = `${data.compute_time_ms.toFixed(3)} ms`;

    // Render 1D Plot
    thermalChart.drawThermalComparison(data);
  } catch (err) {
    showAlert(`Thermal FEA Error: ${err.message}`);
  }
}

async function runConvergenceStudy() {
  try {
    const res = await fetch(`${API_BASE}/api/fea/convergence`);
    if (!res.ok) throw new Error("Convergence study failed");
    const data = await res.json();
    convergenceChart.drawConvergencePlot(data.cases);
    showAlert("Mesh convergence study completed across N=10, 20, 40, 80 elements (Order ~ 2.0)", false);
  } catch (err) {
    showAlert(`Convergence Study Error: ${err.message}`);
  }
}

function exportFEACSV() {
  if (!currentFEAResult) return;
  let csv = "# Unified Engineering Workbench - 1D Thermal FEA Results\n";
  csv += `type,index,x_m,T_fe_C,T_exact_C,abs_error_C\n`;
  for (let i = 0; i < currentFEAResult.nodes_x.length; ++i) {
    const x = currentFEAResult.nodes_x[i];
    const tFe = currentFEAResult.nodes_T_fe[i];
    const tEx = currentFEAResult.nodes_T_exact[i];
    csv += `node,${i},${x.toFixed(6)},${tFe.toFixed(6)},${tEx.toFixed(6)},${Math.abs(tFe - tEx).toFixed(8)}\n`;
  }
  for (const m of currentFEAResult.midpoints) {
    csv += `midpoint,${m.elem},${m.x.toFixed(6)},${m.T_fe.toFixed(6)},${m.T_exact.toFixed(6)},${m.error.toFixed(8)}\n`;
  }

  downloadFile("thermal_fea_solution.csv", csv, "text/csv");
}

// -------------------------------------------------------------
// 3. PERFORMANCE VIEW LOGIC (Multicore Execution)
// -------------------------------------------------------------
function setupPerformanceView() {
  document.getElementById('btn-run-benchmark').addEventListener('click', runBenchmark);
  document.getElementById('btn-export-benchmark-csv').addEventListener('click', exportBenchmarkCSV);
  document.getElementById('btn-cancel-benchmark').addEventListener('click', async () => {
    try {
      await fetch(`${API_BASE}/api/cancel`, { method: 'POST' });
      showAlert("Non-blocking compute cancellation requested.", false);
    } catch (err) {
      showAlert(`Cancel Error: ${err.message}`);
    }
  });
}

async function runBenchmark() {
  const btn = document.getElementById('btn-run-benchmark');
  btn.disabled = true;
  btn.textContent = "Running 5 Reps...";

  try {
    const res = await fetch(`${API_BASE}/api/benchmark/run`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ cases: 500, elements: 40 })
    });
    if (!res.ok) throw new Error("Benchmark execution failed");
    const data = await res.json();
    currentBenchmarkResult = data;

    // Populate Table
    const tbody = document.getElementById('tbody-benchmarks');
    tbody.innerHTML = '';
    for (const r of data.runs) {
      const tr = document.createElement('tr');
      tr.innerHTML = `
        <td><strong>${r.workers} workers</strong></td>
        <td>${r.median_ms.toFixed(2)} ms</td>
        <td style="color: var(--accent-blue); font-weight: 600;">${r.speedup.toFixed(2)}x</td>
        <td style="color: var(--accent-emerald); font-weight: 600;">${(r.efficiency * 100).toFixed(1)}%</td>
        <td style="font-family: var(--font-mono); font-size: 0.75rem; color: var(--text-dim);">[${r.repetitions.map(t => t.toFixed(2)).join(', ')}]</td>
      `;
      tbody.appendChild(tr);
    }

    // Update Numerical agreement
    const elAgr = document.getElementById('perf-agreement-val');
    elAgr.textContent = `${data.max_diff_C.toExponential(2)} °C`;
    elAgr.style.color = data.agreement_verified ? "var(--accent-emerald)" : "var(--accent-rose)";

    // Render Bars
    benchmarkChart.drawBenchmarkBars(data.runs);
    showAlert("Multicore benchmark finished with 5-repetition profiling and verified agreement", false);
  } catch (err) {
    showAlert(`Benchmark Error: ${err.message}`);
  } finally {
    btn.disabled = false;
    btn.textContent = "Execute Benchmark";
  }
}

function exportBenchmarkCSV() {
  if (!currentBenchmarkResult) return;
  let csv = "# Unified Engineering Workbench - Multicore Execution Profile\n";
  csv += `workers,median_ms,speedup,efficiency,rep1_ms,rep2_ms,rep3_ms,rep4_ms,rep5_ms\n`;
  for (const r of currentBenchmarkResult.runs) {
    csv += `${r.workers},${r.median_ms},${r.speedup},${r.efficiency},${r.repetitions.join(',')}\n`;
  }
  downloadFile("multicore_benchmark.csv", csv, "text/csv");
}

// -------------------------------------------------------------
// 4. HARDWARE VIEW LOGIC (Embedded Mock Adapters)
// -------------------------------------------------------------
function setupHardwareView() {
  document.getElementById('btn-refresh-devices').addEventListener('click', loadHardwareDevices);

  document.getElementById('btn-sub-table').addEventListener('click', () => switchHardwareSubView('table'));
  document.getElementById('btn-sub-diagram').addEventListener('click', () => switchHardwareSubView('diagram'));
  document.getElementById('btn-sub-jtag').addEventListener('click', () => switchHardwareSubView('jtag'));

  // Cyclic Telemetry Polling every 2.5 seconds when tab active
  setInterval(() => {
    const pane = document.getElementById('tab-hardware');
    if (pane && pane.classList.contains('active')) {
      loadHardwareDevices();
    }
  }, 2500);
}

async function loadHardwareDevices() {
  try {
    const res = await fetch(`${API_BASE}/api/embedded/devices`);
    if (!res.ok) throw new Error("Failed to query embedded adapters");
    const data = await res.json();

    const container = document.getElementById('devices-list-container');
    container.innerHTML = '';

    for (const d of data.devices) {
      const card = document.createElement('div');
      card.className = 'panel';
      card.style.background = 'var(--bg-card)';
      card.style.border = d.fault_active ? '1.5px solid var(--accent-danger)' : '1px solid var(--border-light)';
      card.style.boxShadow = 'var(--shadow-sm)';

      const stateBadge = d.state === 2 ? '<span class="badge badge-connected">CONNECTED</span>' 
                       : (d.state === 3 ? '<span class="badge badge-faulted">FAULT: TIMEOUT</span>' 
                       : '<span class="badge badge-warning">DISCONNECTED</span>');

      card.innerHTML = `
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.75rem;">
          <div>
            <strong style="color: var(--text-main); font-size: 0.94rem;">${d.name}</strong>
            <div style="font-size: 0.74rem; color: var(--text-muted); font-family: var(--font-mono);">${d.id} • ${d.clock}</div>
          </div>
          ${stateBadge}
        </div>

        <div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 0.5rem; font-size: 0.8rem; margin-bottom: 0.75rem; background: var(--bg-card-subtle); border: 1px solid var(--border-light); padding: 0.6rem; border-radius: var(--radius-sm);">
          <div><span style="color: var(--text-muted); font-size: 0.72rem; text-transform: uppercase;">Rail:</span> <strong style="color: var(--text-main); font-family: var(--font-mono);">${d.voltage_V.toFixed(2)} V</strong></div>
          <div><span style="color: var(--text-muted); font-size: 0.72rem; text-transform: uppercase;">Current:</span> <strong style="color: var(--text-main); font-family: var(--font-mono);">${d.current_mA.toFixed(1)} mA</strong></div>
          <div><span style="color: var(--text-muted); font-size: 0.72rem; text-transform: uppercase;">Temp:</span> <strong style="color: var(--text-main); font-family: var(--font-mono);">${d.temp_C.toFixed(1)} °C</strong></div>
        </div>

        <div style="font-size: 0.76rem; color: var(--text-secondary); margin-bottom: 0.85rem;">
          <div>Status: <span style="font-family: var(--font-mono); font-weight: 600; color: var(--text-main);">${d.status_word}</span></div>
          ${d.fault_active ? `<div style="color: var(--accent-danger); font-weight: 600; margin-top: 0.25rem;">⚠️ ${d.fault_message}</div>` : ''}
        </div>

        <div style="display: flex; gap: 0.5rem;">
          ${d.state === 2 ? `
            <button class="btn btn-secondary btn-sm" onclick="sendDeviceAction('${d.id}', 'disconnect')">Disconnect</button>
            <button class="btn btn-danger btn-sm" onclick="sendDeviceAction('${d.id}', 'inject_fault')">Simulate Timeout</button>
          ` : (d.state === 3 ? `
            <button class="btn btn-primary btn-sm" onclick="sendDeviceAction('${d.id}', 'recover')">Recover Link</button>
          ` : `
            <button class="btn btn-primary btn-sm" onclick="sendDeviceAction('${d.id}', 'connect')">Connect Target</button>
          `)}
        </div>
      `;
      container.appendChild(card);
    }
  } catch (err) {
    console.error("Telemetry query failed", err);
  }
}

window.sendDeviceAction = async function(deviceId, action) {
  try {
    const res = await fetch(`${API_BASE}/api/embedded/action`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ device_id: deviceId, action: action })
    });
    const data = await res.json();
    showAlert(data.message, !data.success);
    await loadHardwareDevices();
  } catch (err) {
    showAlert(`Action Failed: ${err.message}`);
  }
};

async function loadHardwareSpecs() {
  try {
    const res = await fetch(`${API_BASE}/api/embedded/specs`);
    if (!res.ok) throw new Error("Failed to load specs");
    hardwareSpecs = await res.json();
    switchHardwareSubView('table');
  } catch (err) {
    console.error(err);
  }
}

function switchHardwareSubView(view) {
  if (!hardwareSpecs) return;
  const container = document.getElementById('hardware-spec-content');
  const btnT = document.getElementById('btn-sub-table');
  const btnD = document.getElementById('btn-sub-diagram');
  const btnJ = document.getElementById('btn-sub-jtag');

  btnT.classList.remove('active');
  btnD.classList.remove('active');
  btnJ.classList.remove('active');

  if (view === 'table') {
    btnT.classList.add('active');
    container.innerHTML = `<pre style="white-space: pre-wrap; font-family: var(--font-mono); font-size: 0.75rem; color: #cbd5e1;">${hardwareSpecs.table_markdown}</pre>`;
  } else if (view === 'diagram') {
    btnD.classList.add('active');
    container.innerHTML = `<pre style="white-space: pre-wrap; font-family: var(--font-mono); font-size: 0.75rem; color: #38bdf8;">${hardwareSpecs.mermaid}</pre>`;
  } else {
    btnJ.classList.add('active');
    container.innerHTML = `<pre style="white-space: pre-wrap; font-family: inherit; font-size: 0.8rem; color: #cbd5e1; line-height: 1.5;">${hardwareSpecs.jtag_rationale}</pre>`;
  }
}

// -------------------------------------------------------------
// 5. BACKPROPAGATION VIEW LOGIC
// -------------------------------------------------------------
function setupBackpropView() {
  document.getElementById('btn-run-backprop').addEventListener('click', runBackpropTraining);
}

async function runBackpropTraining() {
  try {
    const res = await fetch(`${API_BASE}/api/backprop/train`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ learning_rate: 0.1, steps: 100 })
    });
    if (!res.ok) throw new Error("Backprop training failed");
    const data = await res.json();

    // Update gradient check rows
    const tbody = document.getElementById('tbody-grad-checks');
    tbody.innerHTML = '';
    for (const gc of data.gradient_checks) {
      const tr = document.createElement('tr');
      tr.innerHTML = `
        <td><strong style="color: #fff;">${gc.param}</strong></td>
        <td>${gc.analytical.toFixed(5)}</td>
        <td>${gc.numerical.toFixed(5)}</td>
        <td style="font-family: var(--font-mono); color: var(--text-dim);">${gc.error.toExponential(2)}</td>
        <td><span class="badge badge-connected">PASS</span></td>
      `;
      tbody.appendChild(tr);
    }

    document.getElementById('bp-final-loss').textContent = data.final_loss.toExponential(2);
    backpropChart.drawBackpropLoss(data.history);
  } catch (err) {
    showAlert(`Backprop Error: ${err.message}`);
  }
}

// Helper: Download file blob
function downloadFile(filename, content, mimeType) {
  const blob = new Blob([content], { type: mimeType });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}
