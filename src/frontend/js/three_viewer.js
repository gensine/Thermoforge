/**
 * Unified Engineering Workbench - Native 3D WebGL Engine
 * Self-contained 3D renderer for ASCII STL triangles & ANSYS temperature point clouds.
 * Features: Orbit controls (Rotate, Pan, Zoom, Fit-to-View), Flat-shaded mesh,
 * Turbo/Jet temperature colormap shader, and interactive raycasting node inspector.
 */

class WebGL3DViewer {
  constructor(canvasId, tooltipId) {
    this.canvas = document.getElementById(canvasId);
    this.tooltip = document.getElementById(tooltipId);
    this.gl = this.canvas.getContext('webgl', { antialias: true, alpha: false });

    if (!this.gl) {
      console.error("WebGL not supported");
      return;
    }

    // Camera state
    this.camera = {
      target: [0, 0, 0],
      distance: 2.5,
      azimuth: 0.8,   // horizontal angle (radians)
      elevation: 0.5, // vertical angle (radians)
      fov: 45 * Math.PI / 180,
      near: 0.01,
      far: 1000.0,
      pan: [0, 0]
    };

    // Render modes: 'stl', 'pointcloud', 'both'
    this.renderMode = 'stl';

    // Model buffers & data
    this.stlData = null;
    this.pointCloudData = null;

    // Interaction state
    this.isDragging = false;
    this.isPanning = false;
    this.lastMouseX = 0;
    this.lastMouseY = 0;

    this.initGL();
    this.setupEvents();
    this.resize();
    this.render();
  }

  initGL() {
    const gl = this.gl;
    gl.clearColor(0.933, 0.945, 0.980, 1.0); // Clean studio background (#EEF1FA)
    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);

    // 1. Shaders for STL Mesh (Flat/Lambert shading)
    const vsMesh = `
      attribute vec3 a_position;
      attribute vec3 a_normal;
      uniform mat4 u_mvp;
      uniform mat4 u_model;
      varying vec3 v_normal;
      varying vec3 v_pos;
      void main() {
        gl_Position = u_mvp * vec4(a_position, 1.0);
        v_normal = mat3(u_model) * a_normal;
        v_pos = (u_model * vec4(a_position, 1.0)).xyz;
      }
    `;

    const fsMesh = `
      precision mediump float;
      varying vec3 v_normal;
      varying vec3 v_pos;
      uniform vec3 u_lightDir;
      uniform vec3 u_baseColor;
      void main() {
        vec3 norm = normalize(v_normal);
        vec3 light = normalize(u_lightDir);
        float diff = max(dot(norm, light), 0.0);
        float ambient = 0.40;
        vec3 color = u_baseColor * (ambient + 0.65 * diff);
        gl_FragColor = vec4(color, 1.0);
      }
    `;

    this.progMesh = this.createProgram(vsMesh, fsMesh);

    // 2. Shaders for Colored Temperature Point Cloud
    const vsPoints = `
      attribute vec3 a_position;
      attribute float a_temp;
      uniform mat4 u_mvp;
      uniform float u_pointSize;
      uniform float u_minTemp;
      uniform float u_maxTemp;
      varying float v_normTemp;
      void main() {
        gl_Position = u_mvp * vec4(a_position, 1.0);
        gl_PointSize = u_pointSize;
        float range = max(u_maxTemp - u_minTemp, 0.0001);
        v_normTemp = clamp((a_temp - u_minTemp) / range, 0.0, 1.0);
      }
    `;

    const fsPoints = `
      precision mediump float;
      varying float v_normTemp;
      
      // Turbo / Thermal heat colormap approximation
      vec3 colormap(float t) {
        // blue -> cyan -> green -> yellow -> red
        float r = clamp(1.5 - abs(4.0 * t - 3.0), 0.0, 1.0);
        float g = clamp(1.5 - abs(4.0 * t - 2.0), 0.0, 1.0);
        float b = clamp(1.5 - abs(4.0 * t - 1.0), 0.0, 1.0);
        return vec3(r, g, b);
      }

      void main() {
        // Make circular points
        vec2 coord = gl_PointCoord - vec2(0.5);
        if (length(coord) > 0.5) discard;
        vec3 color = colormap(v_normTemp);
        gl_FragColor = vec4(color, 1.0);
      }
    `;

    this.progPoints = this.createProgram(vsPoints, fsPoints);

    // Buffers
    this.meshVBO = gl.createBuffer();
    this.meshNBO = gl.createBuffer();
    this.pointsVBO = gl.createBuffer();
    this.pointsTBO = gl.createBuffer();
  }

  createShader(type, src) {
    const gl = this.gl;
    const s = gl.createShader(type);
    gl.shaderSource(s, src);
    gl.compileShader(s);
    if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
      console.error(gl.getShaderInfoLog(s));
      gl.deleteShader(s);
      return null;
    }
    return s;
  }

  createProgram(vsSrc, fsSrc) {
    const gl = this.gl;
    const vs = this.createShader(gl.VERTEX_SHADER, vsSrc);
    const fs = this.createShader(gl.FRAGMENT_SHADER, fsSrc);
    const p = gl.createProgram();
    gl.attachShader(p, vs);
    gl.attachShader(p, fs);
    gl.linkProgram(p);
    return p;
  }

  setupEvents() {
    const c = this.canvas;

    c.addEventListener('mousedown', (e) => {
      this.isDragging = true;
      this.isPanning = (e.button === 2 || e.shiftKey);
      this.lastMouseX = e.clientX;
      this.lastMouseY = e.clientY;
    });

    window.addEventListener('mouseup', () => {
      this.isDragging = false;
      this.isPanning = false;
    });

    c.addEventListener('contextmenu', (e) => e.preventDefault());

    c.addEventListener('mousemove', (e) => {
      if (this.isDragging) {
        const dx = e.clientX - this.lastMouseX;
        const dy = e.clientY - this.lastMouseY;
        this.lastMouseX = e.clientX;
        this.lastMouseY = e.clientY;

        if (this.isPanning) {
          const factor = this.camera.distance * 0.0015;
          this.camera.pan[0] += dx * factor;
          this.camera.pan[1] -= dy * factor;
        } else {
          this.camera.azimuth -= dx * 0.008; // Natural orbit: dragging right rotates object right
          this.camera.elevation = Math.max(-1.5, Math.min(1.5, this.camera.elevation + dy * 0.008));
        }
        this.render();
      } else {
        this.handleHover(e);
      }
    });

    c.addEventListener('wheel', (e) => {
      e.preventDefault();
      const zoomFactor = e.deltaY > 0 ? 1.1 : 0.9;
      this.camera.distance = Math.max(0.01, Math.min(500.0, this.camera.distance * zoomFactor));
      this.render();
    }, { passive: false });

    window.addEventListener('resize', () => {
      this.resize();
      this.render();
    });
  }

  resize() {
    const rect = this.canvas.parentElement.getBoundingClientRect();
    this.canvas.width = rect.width * (window.devicePixelRatio || 1);
    this.canvas.height = rect.height * (window.devicePixelRatio || 1);
    this.gl.viewport(0, 0, this.canvas.width, this.canvas.height);
  }

  loadSTL(stlJson) {
    this.stlData = stlJson;
    const gl = this.gl;

    gl.bindBuffer(gl.ARRAY_BUFFER, this.meshVBO);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(stlJson.vertices), gl.STATIC_DRAW);

    gl.bindBuffer(gl.ARRAY_BUFFER, this.meshNBO);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(stlJson.normals), gl.STATIC_DRAW);

    this.fitToView(stlJson.bbox);
    this.render();
  }

  loadANSYS(ansysJson) {
    this.pointCloudData = ansysJson;
    const gl = this.gl;

    gl.bindBuffer(gl.ARRAY_BUFFER, this.pointsVBO);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(ansysJson.positions), gl.STATIC_DRAW);

    gl.bindBuffer(gl.ARRAY_BUFFER, this.pointsTBO);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(ansysJson.temperatures), gl.STATIC_DRAW);

    this.fitToView(ansysJson.bbox);
    this.render();
  }

  fitToView(bbox) {
    if (!bbox) return;
    this.camera.target = [...bbox.center];
    this.camera.pan = [0, 0];
    this.camera.distance = Math.max(bbox.diagonal * 1.5, 0.5);
    this.camera.azimuth = 0.75;
    this.camera.elevation = 0.55;
    this.render();
  }

  handleHover(e) {
    if (!this.pointCloudData || !this.tooltip) return;
    const rect = this.canvas.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;

    // Project points to screen coordinates to find closest point within threshold
    const width = rect.width;
    const height = rect.height;
    const mvp = this.getMVPMatrix();

    const pts = this.pointCloudData.positions;
    const temps = this.pointCloudData.temperatures;
    const ids = this.pointCloudData.node_ids;

    let closestNode = null;
    let minDist = 18.0; // pixel threshold

    for (let i = 0; i < ids.length; ++i) {
      const idx = i * 3;
      const x = pts[idx], y = pts[idx + 1], z = pts[idx + 2];

      // Transform by MVP
      const clip = this.transformVec4(mvp, [x, y, z, 1.0]);
      if (clip[3] <= 0) continue; // Behind camera

      const ndcX = clip[0] / clip[3];
      const ndcY = clip[1] / clip[3];

      const screenX = (ndcX * 0.5 + 0.5) * width;
      const screenY = (-ndcY * 0.5 + 0.5) * height;

      const dist = Math.hypot(screenX - mouseX, screenY - mouseY);
      if (dist < minDist) {
        minDist = dist;
        closestNode = {
          id: ids[i],
          x: x,
          y: y,
          z: z,
          temp: temps[i],
          screenX: mouseX,
          screenY: mouseY
        };
      }
    }

    if (closestNode) {
      this.tooltip.style.display = 'block';
      this.tooltip.style.left = `${closestNode.screenX + 15}px`;
      this.tooltip.style.top = `${closestNode.screenY - 15}px`;
      this.tooltip.innerHTML = `
        <strong>Node ID: ${closestNode.id}</strong><br/>
        <span>Pos: (${closestNode.x.toFixed(4)}, ${closestNode.y.toFixed(4)}, ${closestNode.z.toFixed(4)}) m</span><br/>
        <span style="color: #38bdf8; font-weight: 600;">Temp: ${closestNode.temp.toFixed(2)} °C</span>
      `;
    } else {
      this.tooltip.style.display = 'none';
    }
  }

  transformVec4(m, v) {
    return [
      m[0]*v[0] + m[4]*v[1] + m[8]*v[2]  + m[12]*v[3],
      m[1]*v[0] + m[5]*v[1] + m[9]*v[2]  + m[13]*v[3],
      m[2]*v[0] + m[6]*v[1] + m[10]*v[2] + m[14]*v[3],
      m[3]*v[0] + m[7]*v[1] + m[11]*v[2] + m[15]*v[3]
    ];
  }

  getMVPMatrix() {
    const aspect = this.canvas.width / this.canvas.height;
    const proj = this.perspective(this.camera.fov, aspect, this.camera.near, this.camera.far);

    // Compute eye position from spherical angles
    const cx = this.camera.distance * Math.cos(this.camera.elevation) * Math.sin(this.camera.azimuth);
    const cy = this.camera.distance * Math.sin(this.camera.elevation);
    const cz = this.camera.distance * Math.cos(this.camera.elevation) * Math.cos(this.camera.azimuth);

    const target = [
      this.camera.target[0] + this.camera.pan[0],
      this.camera.target[1] + this.camera.pan[1],
      this.camera.target[2]
    ];

    const eye = [target[0] + cx, target[1] + cy, target[2] + cz];
    const view = this.lookAt(eye, target, [0, 1, 0]);
    return this.multiplyMatrices(proj, view);
  }

  render() {
    const gl = this.gl;
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

    const mvp = this.getMVPMatrix();

    // 1. Render STL Mesh if available
    if (this.stlData && (this.renderMode === 'stl' || this.renderMode === 'both')) {
      gl.useProgram(this.progMesh);
      const uMVP = gl.getUniformLocation(this.progMesh, "u_mvp");
      const uModel = gl.getUniformLocation(this.progMesh, "u_model");
      const uLight = gl.getUniformLocation(this.progMesh, "u_lightDir");
      const uColor = gl.getUniformLocation(this.progMesh, "u_baseColor");

      gl.uniformMatrix4fv(uMVP, false, new Float32Array(mvp));
      gl.uniformMatrix4fv(uModel, false, new Float32Array(this.identity()));
      gl.uniform3f(uLight, 0.6, 0.8, 0.5);
      gl.uniform3f(uColor, 0.42, 0.35, 0.92); // Vibrant modern violet CAD mesh

      gl.bindBuffer(gl.ARRAY_BUFFER, this.meshVBO);
      const aPos = gl.getAttribLocation(this.progMesh, "a_position");
      gl.enableVertexAttribArray(aPos);
      gl.vertexAttribPointer(aPos, 3, gl.FLOAT, false, 0, 0);

      gl.bindBuffer(gl.ARRAY_BUFFER, this.meshNBO);
      const aNorm = gl.getAttribLocation(this.progMesh, "a_normal");
      gl.enableVertexAttribArray(aNorm);
      gl.vertexAttribPointer(aNorm, 3, gl.FLOAT, false, 0, 0);

      gl.drawArrays(gl.TRIANGLES, 0, this.stlData.facet_count * 3);
    }

    // 2. Render ANSYS Point Cloud if available
    if (this.pointCloudData && (this.renderMode === 'pointcloud' || this.renderMode === 'both')) {
      gl.useProgram(this.progPoints);
      const uMVP = gl.getUniformLocation(this.progPoints, "u_mvp");
      const uSize = gl.getUniformLocation(this.progPoints, "u_pointSize");
      const uMinT = gl.getUniformLocation(this.progPoints, "u_minTemp");
      const uMaxT = gl.getUniformLocation(this.progPoints, "u_maxTemp");

      gl.uniformMatrix4fv(uMVP, false, new Float32Array(mvp));
      gl.uniform1f(uSize, 12.0 * (window.devicePixelRatio || 1));
      gl.uniform1f(uMinT, this.pointCloudData.min_temp_C);
      gl.uniform1f(uMaxT, this.pointCloudData.max_temp_C);

      gl.bindBuffer(gl.ARRAY_BUFFER, this.pointsVBO);
      const aPos = gl.getAttribLocation(this.progPoints, "a_position");
      gl.enableVertexAttribArray(aPos);
      gl.vertexAttribPointer(aPos, 3, gl.FLOAT, false, 0, 0);

      gl.bindBuffer(gl.ARRAY_BUFFER, this.pointsTBO);
      const aTemp = gl.getAttribLocation(this.progPoints, "a_temp");
      gl.enableVertexAttribArray(aTemp);
      gl.vertexAttribPointer(aTemp, 1, gl.FLOAT, false, 0, 0);

      gl.drawArrays(gl.POINTS, 0, this.pointCloudData.node_count);
    }
  }

  // Linear algebra utility matrices
  identity() {
    return [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1];
  }

  perspective(fovy, aspect, near, far) {
    const f = 1.0 / Math.tan(fovy / 2);
    const nf = 1 / (near - far);
    return [
      f / aspect, 0, 0, 0,
      0, f, 0, 0,
      0, 0, (far + near) * nf, -1,
      0, 0, 2 * far * near * nf, 0
    ];
  }

  lookAt(eye, center, up) {
    let z0 = eye[0] - center[0], z1 = eye[1] - center[1], z2 = eye[2] - center[2];
    let len = 1 / Math.hypot(z0, z1, z2);
    z0 *= len; z1 *= len; z2 *= len;

    let x0 = up[1] * z2 - up[2] * z1, x1 = up[2] * z0 - up[0] * z2, x2 = up[0] * z1 - up[1] * z0;
    len = 1 / Math.hypot(x0, x1, x2);
    x0 *= len; x1 *= len; x2 *= len;

    let y0 = z1 * x2 - z2 * x1, y1 = z2 * x0 - z0 * x2, y2 = z0 * x1 - z1 * x0;

    return [
      x0, y0, z0, 0,
      x1, y1, z1, 0,
      x2, y2, z2, 0,
      -(x0*eye[0] + x1*eye[1] + x2*eye[2]),
      -(y0*eye[0] + y1*eye[1] + y2*eye[2]),
      -(z0*eye[0] + z1*eye[1] + z2*eye[2]),
      1
    ];
  }

  multiplyMatrices(a, b) {
    const out = new Array(16);
    for (let i = 0; i < 4; i++) {
      for (let j = 0; j < 4; j++) {
        out[j * 4 + i] = 
          a[i] * b[j * 4] +
          a[i + 4] * b[j * 4 + 1] +
          a[i + 8] * b[j * 4 + 2] +
          a[i + 12] * b[j * 4 + 3];
      }
    }
    return out;
  }
}
