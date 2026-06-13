/* ============================================================
   Indium — hero 3D scene  (classic script, global THREE)
   "Scene viewport in 3D": an infinite grid floor receding to a
   foggy horizon, with entities floating above it — wireframe and
   solid shapes, one wrapped in the editor's own cyan selection
   box + corner handles + amber rotation ring. Camera parallax on
   the cursor, the floor scrolling forward, dust in the air.

   Vendored UMD build (js/lib/three.min.js) + classic script so
   the page also works opened from disk (file://) and offline.

   Degrades: no WebGL -> CSS fallback; reduced-motion -> one
   static frame; offscreen -> render loop parked.
   ============================================================ */
(function () {
  "use strict";

  var THREE = window.THREE;
  var canvas = document.getElementById("scene");
  if (!canvas) return;
  if (!THREE) { document.body.classList.add("no-webgl"); return; }

  var reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  var mobile = window.matchMedia("(max-width: 760px)").matches;

  var CHARCOAL = 0x121212;
  var CYAN = 0x29e0e0, AMBER = 0xffc83c;

  var renderer;
  try {
    renderer = new THREE.WebGLRenderer({ canvas: canvas, antialias: !mobile, alpha: true, powerPreference: "high-performance" });
  } catch (e) { document.body.classList.add("no-webgl"); return; }

  var DPR = Math.min(window.devicePixelRatio || 1, mobile ? 1.5 : 2);
  renderer.setPixelRatio(DPR);
  renderer.setSize(window.innerWidth, window.innerHeight);
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.1;

  var scene = new THREE.Scene();
  scene.fog = new THREE.Fog(CHARCOAL, 7, 30);

  var camera = new THREE.PerspectiveCamera(44, window.innerWidth / window.innerHeight, 0.1, 100);
  var CAM = new THREE.Vector3(0, 1.7, 7.2);
  camera.position.copy(CAM);

  // --- environment (for the metal entities) ---------------------
  var pmrem = new THREE.PMREMGenerator(renderer);
  var envTex = makeEnvTexture();
  var envMap = pmrem.fromEquirectangular(envTex).texture;
  scene.environment = envMap;
  envTex.dispose();

  // --- lights ---------------------------------------------------
  var key = new THREE.PointLight(0x00ffff, 26, 30, 2.0); key.position.set(-5, 5, 4); scene.add(key);
  var rim = new THREE.PointLight(AMBER, 18, 30, 2.0); rim.position.set(6, 3, -2); scene.add(rim);
  var fillL = new THREE.DirectionalLight(0xffffff, 0.55); fillL.position.set(1, 2, 4); scene.add(fillL);
  scene.add(new THREE.AmbientLight(0x1a1a1e, 1.0));

  // --- infinite grid floor --------------------------------------
  var gridUniforms = {
    uColor:  { value: new THREE.Color(0x245a5a) },
    uColor2: { value: new THREE.Color(0x49cccc) },
    uScroll: { value: 0 },
    uFar:    { value: 28.0 }
  };
  var gridMat = new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, fog: false,
    // fwidth() needs the derivatives extension under GLSL ES 1.00 (ShaderMaterial's
    // default) — without this strict drivers reject the shader.
    extensions: { derivatives: true },
    uniforms: gridUniforms,
    vertexShader:
      "varying vec3 vWorld; varying float vDist; uniform float uScroll;" +
      "void main(){ vec3 p = position;" +
      "  vec4 wp = modelMatrix * vec4(p,1.0); vWorld = wp.xyz; vWorld.z += uScroll;" +
      "  vec4 mv = viewMatrix * wp; vDist = -mv.z;" +
      "  gl_Position = projectionMatrix * mv; }",
    fragmentShader:
      "varying vec3 vWorld; varying float vDist;" +
      "uniform vec3 uColor; uniform vec3 uColor2; uniform float uFar;" +
      "float gridF(vec2 c, float s){ vec2 g = abs(fract(c/s - 0.5) - 0.5) / fwidth(c/s);" +
      "  return 1.0 - min(min(g.x, g.y), 1.0); }" +
      "void main(){ vec2 c = vWorld.xz;" +
      "  float minor = gridF(c, 1.0); float major = gridF(c, 8.0);" +
      "  float fade = clamp(1.0 - vDist/uFar, 0.0, 1.0); fade *= fade;" +
      "  float a = max(minor*0.30, major*0.85) * fade;" +
      "  if(a < 0.004) discard;" +
      "  vec3 col = mix(uColor, uColor2, major);" +
      "  gl_FragColor = vec4(col, a); }"
  });
  var grid = new THREE.Mesh(new THREE.PlaneGeometry(140, 140), gridMat);
  grid.rotation.x = -Math.PI / 2;
  grid.position.y = 0;
  scene.add(grid);

  // --- entities floating above the grid -------------------------
  var entities = [];

  function metalMat(hex, rough) {
    return new THREE.MeshStandardMaterial({ color: hex, metalness: 0.95, roughness: rough == null ? 0.25 : rough, envMapIntensity: 1.2 });
  }
  function addEntity(mesh, x, y, z, opts) {
    opts = opts || {};
    mesh.position.set(x, y, z);
    scene.add(mesh);
    entities.push({
      m: mesh, base: new THREE.Vector3(x, y, z),
      spin: opts.spin == null ? 0.2 : opts.spin,
      bob: opts.bob == null ? 0.12 : opts.bob,
      phase: Math.random() * Math.PI * 2,
      tilt: opts.tilt || 0
    });
    return mesh;
  }
  function wireframe(geo, hex) {
    return new THREE.LineSegments(new THREE.EdgesGeometry(geo), new THREE.LineBasicMaterial({ color: hex, transparent: true, opacity: 0.85, fog: true }));
  }

  // solid metal cube
  addEntity(new THREE.Mesh(new THREE.BoxGeometry(1, 1, 1), metalMat(0xcfd4db, 0.22)), 4.6, 0.9, -2.5, { spin: 0.28 });
  // the "circle" entity — a sphere
  addEntity(new THREE.Mesh(new THREE.SphereGeometry(0.62, 32, 24), metalMat(0xe8e8ec, 0.3)), 3.0, 0.72, -1.2, { spin: 0.1 });
  // cyan wireframe cube (far)
  addEntity(wireframe(new THREE.BoxGeometry(1.1, 1.1, 1.1), CYAN), 5.6, 1.9, -6, { spin: 0.32, bob: 0.18 });
  // amber wireframe pyramid
  addEntity(wireframe(new THREE.ConeGeometry(0.8, 1.2, 4), AMBER), 1.6, 1.7, -7.5, { spin: 0.36, bob: 0.16, tilt: 0.2 });
  // sprite-like quad (a textured plane standing up)
  (function () {
    var quad = new THREE.Mesh(
      new THREE.PlaneGeometry(1.1, 1.1),
      new THREE.MeshStandardMaterial({ color: 0x2a2a30, metalness: 0.1, roughness: 0.7, side: THREE.DoubleSide, emissive: 0x0c2a2a, emissiveIntensity: 0.5 })
    );
    addEntity(quad, -3.6, 1.25, -6.5, { spin: 0.14, bob: 0.14 });
  })();
  // small far wireframe sphere
  addEntity(wireframe(new THREE.IcosahedronGeometry(0.55, 1), 0x3a8c8c), -2.2, 2.15, -9, { spin: 0.4, bob: 0.2 });
  // solid small cube low
  addEntity(new THREE.Mesh(new THREE.BoxGeometry(0.7, 0.7, 0.7), metalMat(0xbfc4cb, 0.28)), -4.4, 0.6, -3.4, { spin: 0.24 });

  // --- the SELECTED entity: editor gizmos (cyan box + handles + amber ring)
  var selGroup = new THREE.Group();
  var selBody = new THREE.Mesh(new THREE.BoxGeometry(1.15, 1.15, 1.15), metalMat(0xcfd4db, 0.2));
  selGroup.add(selBody);
  // cyan selection box
  var selBox = new THREE.LineSegments(
    new THREE.EdgesGeometry(new THREE.BoxGeometry(1.32, 1.32, 1.32)),
    new THREE.LineBasicMaterial({ color: CYAN, transparent: true, opacity: 0.95 })
  );
  selGroup.add(selBox);
  // 8 corner handles
  var handleGeo = new THREE.SphereGeometry(0.055, 10, 8);
  var handleMat = new THREE.MeshBasicMaterial({ color: 0x4cd6ff });
  var hs = 0.66;
  [[-1,-1,-1],[1,-1,-1],[1,1,-1],[-1,1,-1],[-1,-1,1],[1,-1,1],[1,1,1],[-1,1,1]].forEach(function (c) {
    var h = new THREE.Mesh(handleGeo, handleMat);
    h.position.set(c[0]*hs, c[1]*hs, c[2]*hs);
    selGroup.add(h);
  });
  // amber rotation ring
  var ring = new THREE.Mesh(
    new THREE.TorusGeometry(1.15, 0.018, 8, 64),
    new THREE.MeshBasicMaterial({ color: AMBER, transparent: true, opacity: 0.8 })
  );
  selGroup.add(ring);
  selGroup.position.set(2.0, 1.0, 0.4);
  scene.add(selGroup);

  // --- dust in the air ------------------------------------------
  var dust;
  (function () {
    var N = mobile ? 90 : 220;
    var pos = new Float32Array(N * 3);
    for (var i = 0; i < N; i++) {
      pos[i*3]   = (Math.random() - 0.5) * 26;
      pos[i*3+1] = Math.random() * 9;
      pos[i*3+2] = -Math.random() * 22 + 4;
    }
    var g = new THREE.BufferGeometry();
    g.setAttribute("position", new THREE.BufferAttribute(pos, 3));
    dust = new THREE.Points(g, new THREE.PointsMaterial({
      color: 0x8fe6e6, size: 0.035, transparent: true, opacity: 0.55,
      depthWrite: false, blending: THREE.AdditiveBlending, fog: true
    }));
    scene.add(dust);
  })();

  // --- interaction ----------------------------------------------
  var pointer = { x: 0, y: 0, tx: 0, ty: 0 };
  var scrollN = 0, visible = true;
  if (!reduce) {
    window.addEventListener("pointermove", function (e) {
      pointer.tx = (e.clientX / window.innerWidth) * 2 - 1;
      pointer.ty = (e.clientY / window.innerHeight) * 2 - 1;
    }, { passive: true });
    window.addEventListener("scroll", function () {
      scrollN = Math.min(1, Math.max(0, window.scrollY / window.innerHeight));
    }, { passive: true });
    var hero = document.querySelector(".hero");
    if (hero && "IntersectionObserver" in window) {
      new IntersectionObserver(function (es) {
        for (var i = 0; i < es.length; i++) visible = es[i].isIntersecting;
      }, { threshold: 0.01 }).observe(hero);
    }
  }
  window.addEventListener("resize", function () {
    var w = window.innerWidth, h = window.innerHeight;
    camera.aspect = w / h; camera.updateProjectionMatrix();
    renderer.setSize(w, h);
  });

  // --- loop -----------------------------------------------------
  var clock = new THREE.Clock();
  var target = new THREE.Vector3();

  function render() {
    var t = clock.getElapsedTime();

    pointer.x += (pointer.tx - pointer.x) * 0.05;
    pointer.y += (pointer.ty - pointer.y) * 0.05;

    gridUniforms.uScroll.value = (t * 0.6) % 8.0;

    for (var i = 0; i < entities.length; i++) {
      var e = entities[i];
      e.m.rotation.y = t * e.spin + e.phase;
      e.m.rotation.x = e.tilt + Math.sin(t * 0.3 + e.phase) * 0.12;
      e.m.position.y = e.base.y + Math.sin(t * 0.7 + e.phase) * e.bob;
    }
    // selected entity: gentle bob, body spins inside a steady box, ring rotates
    selGroup.position.y = 1.0 + Math.sin(t * 0.6) * 0.1;
    selBody.rotation.y = t * 0.5;
    ring.rotation.z = t * 0.7;
    ring.rotation.x = Math.PI * 0.5;

    if (dust) dust.rotation.y = t * 0.01;

    // camera parallax + scroll lift
    camera.position.x = CAM.x + pointer.x * 0.7;
    camera.position.y = CAM.y - pointer.y * 0.35 + scrollN * 0.8;
    camera.position.z = CAM.z + scrollN * 2.0;
    target.set(pointer.x * 0.5, 0.7 + scrollN * 0.6, -3);
    camera.lookAt(target);

    renderer.render(scene, camera);
  }

  function loop() { if (visible) render(); requestAnimationFrame(loop); }

  render();
  if (!reduce) loop();
  requestAnimationFrame(function () { canvas.classList.add("ready"); });

  // --- env texture (cyan/amber gradient for metal reflections) --
  function makeEnvTexture() {
    var c = document.createElement("canvas"); c.width = 1024; c.height = 512;
    var x = c.getContext("2d");
    x.fillStyle = "#161618"; x.fillRect(0, 0, 1024, 512);
    function blob(cx, cy, r, stops) {
      var g = x.createRadialGradient(cx, cy, 0, cx, cy, r);
      for (var i = 0; i < stops.length; i++) g.addColorStop(stops[i][0], stops[i][1]);
      x.fillStyle = g; x.fillRect(0, 0, 1024, 512);
    }
    blob(300, 150, 440, [[0, "#23e6e6"], [0.35, "#0c5454"], [1, "rgba(22,22,24,0)"]]);
    blob(800, 410, 380, [[0, "#ffc83c"], [0.40, "#5a3c08"], [1, "rgba(22,22,24,0)"]]);
    blob(560, 70, 170, [[0, "#ffffff"], [0.50, "#404044"], [1, "rgba(22,22,24,0)"]]);
    var tex = new THREE.CanvasTexture(c);
    tex.mapping = THREE.EquirectangularReflectionMapping;
    tex.colorSpace = THREE.SRGBColorSpace;
    tex.needsUpdate = true;
    return tex;
  }
})();
