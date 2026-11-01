/**
 * Unified Engineering Workbench - Native 2D Canvas Chart Engine
 * Lightweight, zero-dependency charting library for scientific & thermal plots.
 * Supports: Line plots, dual-curve comparisons, log-log convergence, bar charts, and legends.
 */

class WorkbenchChart {
  constructor(canvasId) {
    this.canvas = document.getElementById(canvasId);
    if (!this.canvas) return;
    this.ctx = this.canvas.getContext('2d');
    this.padding = { top: 30, right: 30, bottom: 45, left: 55 };
    this.resize();
    window.addEventListener('resize', () => {
      this.resize();
      if (this.lastDraw) this.lastDraw();
    });
  }

  resize() {
    const rect = this.canvas.parentElement.getBoundingClientRect();
    this.canvas.width = rect.width * (window.devicePixelRatio || 1);
    this.canvas.height = rect.height * (window.devicePixelRatio || 1);
    this.width = this.canvas.width;
    this.height = this.canvas.height;
  }

  clear() {
    this.ctx.clearRect(0, 0, this.width, this.height);
  }

  // Draw 1D Thermal FEA vs Exact Solution comparison
  drawThermalComparison(feaData) {
    this.lastDraw = () => this.drawThermalComparison(feaData);
    if (!this.ctx || !feaData) return;
    this.clear();
    const ctx = this.ctx;
    const pad = this.padding;

    const plotW = this.width - pad.left - pad.right;
    const plotH = this.height - pad.top - pad.bottom;

    // Range determination
    const xMin = 0.0, xMax = 1.0;
    const allTemps = [...feaData.nodes_T_fe, ...feaData.nodes_T_exact];
    const yMin = 0.0;
    const yMax = Math.ceil(Math.max(...allTemps) / 20) * 20 + 20;

    const scaleX = (x) => pad.left + ((x - xMin) / (xMax - xMin)) * plotW;
    const scaleY = (y) => pad.top + plotH - ((y - yMin) / (yMax - yMin)) * plotH;

    // Draw Grid & Axes
    this.drawAxes(ctx, pad, plotW, plotH, "Position x [meters]", "Temperature T [°C]", xMin, xMax, yMin, yMax, 5, 6);

    // 1. Draw Exact Analytical Curve (smooth dashed/solid gold line)
    ctx.beginPath();
    ctx.strokeStyle = "#f59e0b"; // Gold
    ctx.lineWidth = 2.5;
    const numFine = 100;
    for (let i = 0; i <= numFine; ++i) {
      const x = i / numFine;
      // Exact analytical equation from brief: T_exact(x) = 100 - 80*x + 500*x*(1 - x)
      const tExact = 100.0 - 80.0 * x + 500.0 * x * (1.0 - x);
      const px = scaleX(x);
      const py = scaleY(tExact);
      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.stroke();

    // 2. Draw FE Linear Elements (solid blue segments)
    ctx.beginPath();
    ctx.strokeStyle = "#38bdf8"; // Electric Blue
    ctx.lineWidth = 2.0;
    for (let i = 0; i < feaData.nodes_x.length; ++i) {
      const px = scaleX(feaData.nodes_x[i]);
      const py = scaleY(feaData.nodes_T_fe[i]);
      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.stroke();

    // 3. Draw Nodes (circular cyan dots)
    ctx.fillStyle = "#38bdf8";
    for (let i = 0; i < feaData.nodes_x.length; ++i) {
      const px = scaleX(feaData.nodes_x[i]);
      const py = scaleY(feaData.nodes_T_fe[i]);
      ctx.beginPath();
      ctx.arc(px, py, 4.5, 0, Math.PI * 2);
      ctx.fill();
    }

    // 4. Draw Midpoints & Error Indicators (emerald squares)
    if (feaData.midpoints) {
      ctx.fillStyle = "#10b981"; // Emerald
      ctx.strokeStyle = "#ffffff";
      ctx.lineWidth = 1;
      for (const m of feaData.midpoints) {
        const px = scaleX(m.x);
        const py = scaleY(m.T_fe);
        ctx.beginPath();
        ctx.rect(px - 3.5, py - 3.5, 7, 7);
        ctx.fill();
        ctx.stroke();
      }
    }

    // Legend
    this.drawLegend(ctx, pad.left + 20, pad.top + 20, [
      { label: `Exact Analytical T_exact(x)`, color: "#f59e0b", type: "line" },
      { label: `FEA Linear Solution (${feaData.elements} elems)`, color: "#38bdf8", type: "line-point" },
      { label: `Element Midpoints (Interpolated)`, color: "#10b981", type: "square" }
    ]);
  }

  // Draw Mesh Convergence Study (Log-Log plot)
  drawConvergencePlot(cases) {
    this.lastDraw = () => this.drawConvergencePlot(cases);
    if (!this.ctx || !cases || cases.length === 0) return;
    this.clear();
    const ctx = this.ctx;
    const pad = this.padding;

    const plotW = this.width - pad.left - pad.right;
    const plotH = this.height - pad.top - pad.bottom;

    // Log scales for h and error
    const hVals = cases.map(c => c.h);
    const errVals = cases.map(c => c.max_midpoint_error);

    const logHMin = Math.log10(Math.min(...hVals)) - 0.2;
    const logHMax = Math.log10(Math.max(...hVals)) + 0.2;
    const logEMin = Math.log10(Math.min(...errVals)) - 0.4;
    const logEMax = Math.log10(Math.max(...errVals)) + 0.4;

    const scaleX = (logH) => pad.left + ((logH - logHMin) / (logHMax - logHMin)) * plotW;
    const scaleY = (logE) => pad.top + plotH - ((logE - logEMin) / (logEMax - logEMin)) * plotH;

    // Draw Axes with log labels
    ctx.strokeStyle = "rgba(255, 255, 255, 0.1)";
    ctx.lineWidth = 1;
    ctx.strokeRect(pad.left, pad.top, plotW, plotH);

    ctx.fillStyle = "#94a3b8";
    ctx.font = "11px ui-monospace, Consolas, monospace";
    ctx.textAlign = "center";
    ctx.fillText("Element Size log10(h) [m]", pad.left + plotW / 2, this.height - 12);

    ctx.save();
    ctx.translate(16, pad.top + plotH / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText("Max Midpoint Error log10(E) [°C]", 0, 0);
    ctx.restore();

    // Theoretical O(h^2) line (slope = 2)
    ctx.beginPath();
    ctx.setLineDash([6, 4]);
    ctx.strokeStyle = "rgba(255, 255, 255, 0.4)";
    ctx.lineWidth = 1.5;
    const h0 = Math.log10(hVals[0]), e0 = Math.log10(errVals[0]);
    const hEnd = Math.log10(hVals[hVals.length - 1]);
    const eEnd = e0 + 2.0 * (hEnd - h0); // slope = 2
    ctx.moveTo(scaleX(h0), scaleY(e0));
    ctx.lineTo(scaleX(hEnd), scaleY(eEnd));
    ctx.stroke();
    ctx.setLineDash([]);

    // Actual convergence curve
    ctx.beginPath();
    ctx.strokeStyle = "#10b981"; // Emerald
    ctx.lineWidth = 2.5;
    for (let i = 0; i < cases.length; ++i) {
      const px = scaleX(Math.log10(cases[i].h));
      const py = scaleY(Math.log10(cases[i].max_midpoint_error));
      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.stroke();

    // Data points & text
    for (const c of cases) {
      const px = scaleX(Math.log10(c.h));
      const py = scaleY(Math.log10(c.max_midpoint_error));
      ctx.fillStyle = "#38bdf8";
      ctx.beginPath();
      ctx.arc(px, py, 5, 0, Math.PI * 2);
      ctx.fill();

      ctx.fillStyle = "#ffffff";
      ctx.font = "10px ui-monospace, Consolas, monospace";
      ctx.textAlign = "left";
      ctx.fillText(`N=${c.elements} (${c.max_midpoint_error.toFixed(4)}°C)`, px + 8, py - 6);
    }

    this.drawLegend(ctx, pad.left + 20, pad.top + 20, [
      { label: "FEA Midpoint Error (Observed Rate ~ 2.0)", color: "#10b981", type: "line-point" },
      { label: "Theoretical 2nd-Order O(h^2) Reference", color: "rgba(255,255,255,0.6)", type: "dashed" }
    ]);
  }

  // Draw Multicore Speedup & Efficiency Bar Chart
  drawBenchmarkBars(runs) {
    this.lastDraw = () => this.drawBenchmarkBars(runs);
    if (!this.ctx || !runs || runs.length === 0) return;
    this.clear();
    const ctx = this.ctx;
    const pad = this.padding;

    const plotW = this.width - pad.left - pad.right;
    const plotH = this.height - pad.top - pad.bottom;

    const barGroupWidth = plotW / runs.length;
    const barWidth = Math.min(barGroupWidth * 0.35, 45);

    const maxSpeedup = Math.max(4.0, ...runs.map(r => r.workers));
    const scaleY = (val) => pad.top + plotH - (val / maxSpeedup) * plotH;

    this.drawAxes(ctx, pad, plotW, plotH, "Worker Thread Count", "Metric Value", 0, runs.length, 0, maxSpeedup, runs.length, 5, false);

    // Baseline ideal speedup line
    ctx.beginPath();
    ctx.setLineDash([4, 4]);
    ctx.strokeStyle = "rgba(255, 255, 255, 0.25)";
    ctx.lineWidth = 1.5;
    for (let i = 0; i < runs.length; ++i) {
      const cx = pad.left + i * barGroupWidth + barGroupWidth / 2;
      const cy = scaleY(runs[i].workers);
      if (i === 0) ctx.moveTo(cx, cy);
      else ctx.lineTo(cx, cy);
    }
    ctx.stroke();
    ctx.setLineDash([]);

    // Draw bars
    for (let i = 0; i < runs.length; ++i) {
      const r = runs[i];
      const cx = pad.left + i * barGroupWidth + barGroupWidth / 2;

      // 1. Speedup Bar (Blue)
      const sH = (r.speedup / maxSpeedup) * plotH;
      ctx.fillStyle = "#38bdf8";
      ctx.fillRect(cx - barWidth - 3, pad.top + plotH - sH, barWidth, sH);

      // 2. Efficiency Bar (Emerald, scaled out of 1.0 mapped to maxSpeedup or percentage)
      const eH = (r.efficiency / 1.0) * (plotH * 0.5); // scale nicely
      ctx.fillStyle = "#10b981";
      ctx.fillRect(cx + 3, pad.top + plotH - eH, barWidth, eH);

      // Text labels
      ctx.fillStyle = "#ffffff";
      ctx.font = "11px ui-monospace, Consolas, monospace";
      ctx.textAlign = "center";
      ctx.fillText(`${r.workers}p`, cx, pad.top + plotH + 18);
      ctx.fillText(`${r.speedup.toFixed(2)}x`, cx - barWidth / 2 - 3, pad.top + plotH - sH - 6);
      ctx.fillText(`${(r.efficiency * 100).toFixed(0)}%`, cx + barWidth / 2 + 3, pad.top + plotH - eH - 6);
    }

    this.drawLegend(ctx, pad.left + 20, pad.top + 20, [
      { label: "Measured Speedup (T1 / Tp)", color: "#38bdf8", type: "square" },
      { label: "Parallel Efficiency ((T1/Tp)/p)", color: "#10b981", type: "square" },
      { label: "Linear Scaling Target", color: "rgba(255,255,255,0.4)", type: "dashed" }
    ]);
  }

  // Draw Backpropagation Loss vs Step curve
  drawBackpropLoss(history) {
    this.lastDraw = () => this.drawBackpropLoss(history);
    if (!this.ctx || !history || history.length === 0) return;
    this.clear();
    const ctx = this.ctx;
    const pad = this.padding;

    const plotW = this.width - pad.left - pad.right;
    const plotH = this.height - pad.top - pad.bottom;

    const maxLoss = history[0].loss * 1.05;
    const scaleX = (step) => pad.left + (step / (history.length - 1)) * plotW;
    const scaleY = (loss) => pad.top + plotH - (loss / maxLoss) * plotH;

    this.drawAxes(ctx, pad, plotW, plotH, "Training Step", "Loss J = 0.5 * (y_hat - y)^2", 0, history.length - 1, 0, maxLoss, 5, 5);

    ctx.beginPath();
    ctx.strokeStyle = "#a855f7"; // Purple
    ctx.lineWidth = 2.5;

    for (let i = 0; i < history.length; ++i) {
      const px = scaleX(history[i].step);
      const py = scaleY(history[i].loss);
      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.stroke();

    this.drawLegend(ctx, pad.left + 20, pad.top + 20, [
      { label: `Loss Curve (J0 = ${history[0].loss.toFixed(4)} -> J100 = 0.000)`, color: "#a855f7", type: "line" }
    ]);
  }

  drawAxes(ctx, pad, plotW, plotH, xLabel, yLabel, xMin, xMax, yMin, yMax, xTicks = 5, yTicks = 5, drawTickLabels = true) {
    ctx.strokeStyle = "rgba(255, 255, 255, 0.08)";
    ctx.lineWidth = 1;
    ctx.strokeRect(pad.left, pad.top, plotW, plotH);

    ctx.fillStyle = "#64748b";
    ctx.font = "10px ui-monospace, Consolas, monospace";

    // Horizontal grid & ticks
    for (let i = 0; i <= yTicks; ++i) {
      const y = pad.top + (i / yTicks) * plotH;
      const val = yMax - (i / yTicks) * (yMax - yMin);
      ctx.beginPath();
      ctx.moveTo(pad.left, y);
      ctx.lineTo(pad.left + plotW, y);
      ctx.stroke();

      if (drawTickLabels) {
        ctx.textAlign = "right";
        ctx.fillText(val.toFixed(1), pad.left - 8, y + 3);
      }
    }

    // Vertical grid & ticks
    for (let i = 0; i <= xTicks; ++i) {
      const x = pad.left + (i / xTicks) * plotW;
      const val = xMin + (i / xTicks) * (xMax - xMin);
      ctx.beginPath();
      ctx.moveTo(x, pad.top);
      ctx.lineTo(x, pad.top + plotH);
      ctx.stroke();

      if (drawTickLabels) {
        ctx.textAlign = "center";
        ctx.fillText(val.toFixed(2), x, pad.top + plotH + 16);
      }
    }

    // Axis Labels
    ctx.fillStyle = "#94a3b8";
    ctx.font = "11px -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif";
    ctx.textAlign = "center";
    ctx.fillText(xLabel, pad.left + plotW / 2, this.height - 10);

    ctx.save();
    ctx.translate(14, pad.top + plotH / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText(yLabel, 0, 0);
    ctx.restore();
  }

  drawLegend(ctx, x, y, items) {
    ctx.fillStyle = "rgba(15, 23, 42, 0.85)";
    ctx.strokeStyle = "rgba(255, 255, 255, 0.1)";
    ctx.lineWidth = 1;
    const h = items.length * 20 + 10;
    ctx.fillRect(x, y, 260, h);
    ctx.strokeRect(x, y, 260, h);

    ctx.font = "11px -apple-system, BlinkMacSystemFont, Segoe UI, sans-serif";
    for (let i = 0; i < items.length; ++i) {
      const it = items[i];
      const itemY = y + 18 + i * 20;

      ctx.strokeStyle = it.color;
      ctx.fillStyle = it.color;
      ctx.lineWidth = 2;

      if (it.type === "line" || it.type === "line-point") {
        ctx.beginPath();
        ctx.moveTo(x + 12, itemY - 3);
        ctx.lineTo(x + 32, itemY - 3);
        ctx.stroke();
        if (it.type === "line-point") {
          ctx.beginPath();
          ctx.arc(x + 22, itemY - 3, 3.5, 0, Math.PI * 2);
          ctx.fill();
        }
      } else if (it.type === "dashed") {
        ctx.setLineDash([3, 3]);
        ctx.beginPath();
        ctx.moveTo(x + 12, itemY - 3);
        ctx.lineTo(x + 32, itemY - 3);
        ctx.stroke();
        ctx.setLineDash([]);
      } else if (it.type === "square") {
        ctx.fillRect(x + 14, itemY - 8, 10, 10);
      }

      ctx.fillStyle = "#cbd5e1";
      ctx.textAlign = "left";
      ctx.fillText(it.label, x + 40, itemY);
    }
  }
}
