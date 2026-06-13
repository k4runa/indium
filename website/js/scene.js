/* ============================================================
   Indium — hero 3D scene  (classic script, global THREE)
   A molten droplet of indium: a displaced icosphere with a
   liquid-metal PBR surface, lit by a cyan key and an amber
   rim — the engine's own gizmo colours — over a charcoal void.
   GPU vertex displacement with correct per-fragment normals,
   plus a cyan fresnel emissive so the silhouette glows.

   No ES modules / no CDN imports on purpose: a classic global
   build is vendored at js/lib/three.min.js so the page also
   works opened straight from disk (file://) and offline.

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

  var renderer;
  try {
    renderer = new THREE.WebGLRenderer({ canvas: canvas, antialias: !mobile, alpha: true, powerPreference: "high-performance" });
  } catch (e) {
    document.body.classList.add("no-webgl");
    return;
  }

  var DPR = Math.min(window.devicePixelRatio || 1, mobile ? 1.5 : 2);
  renderer.setPixelRatio(DPR);
  renderer.setSize(window.innerWidth, window.innerHeight);
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.12;

  var scene = new THREE.Scene();
  var camera = new THREE.PerspectiveCamera(38, window.innerWidth / window.innerHeight, 0.1, 100);
  camera.position.set(0, 0, 5.2);

  // --- environment reflections ----------------------------------
  // A hand-painted cyan/amber gradient is what the chrome actually
  // reflects — pure metal has no diffuse, so the env map (not the
  // lights) carries the colour across the whole surface.
  var pmrem = new THREE.PMREMGenerator(renderer);
  var envTex = makeEnvTexture();
  var envMap = pmrem.fromEquirectangular(envTex).texture;
  scene.environment = envMap;
  envTex.dispose();

  // --- the droplet ----------------------------------------------
  var DETAIL = mobile ? 4 : 6;
  var geometry = new THREE.IcosahedronGeometry(1.32, DETAIL);

  var uniforms = {
    uTime:        { value: 0 },
    uAmp:         { value: 0.30 },
    uFreq:        { value: 1.25 },
    uSpeed:       { value: 0.32 },
    uRadius:      { value: 1.32 },
    uRimColor:    { value: new THREE.Color(0x29e0e0) },
    uRimStrength: { value: 1.9 }
  };

  var material = new THREE.MeshStandardMaterial({
    color: new THREE.Color(0xcfd4db),
    metalness: 1.0,
    roughness: 0.17,
    envMapIntensity: 1.55
  });

  material.onBeforeCompile = function (shader) {
    for (var k in uniforms) shader.uniforms[k] = uniforms[k];

    shader.vertexShader =
      NOISE_GLSL +
      "uniform float uTime; uniform float uAmp; uniform float uFreq;" +
      "uniform float uSpeed; uniform float uRadius;" +
      "float disp(vec3 dir){ vec3 p = dir*uFreq + vec3(0.0,0.0,uTime*uSpeed);" +
      "  float n = snoise(p)*0.6 + snoise(p*2.07+11.0)*0.3; return n*uAmp; }" +
      "vec3 surf(vec3 dir){ vec3 d=normalize(dir); return d*(uRadius+disp(d)); }" +
      shader.vertexShader;

    shader.vertexShader = shader.vertexShader.replace(
      "#include <begin_vertex>",
      "vec3 nrm0 = normalize(position);" +
      "vec3 P = surf(nrm0);" +
      "vec3 upv = abs(nrm0.y) < 0.99 ? vec3(0.0,1.0,0.0) : vec3(1.0,0.0,0.0);" +
      "vec3 tang = normalize(cross(upv, nrm0));" +
      "vec3 bita = normalize(cross(nrm0, tang));" +
      "float e = 0.012;" +
      "vec3 Pa = surf(nrm0 + tang*e);" +
      "vec3 Pb = surf(nrm0 + bita*e);" +
      "vec3 dispNormal = normalize(cross(Pa - P, Pb - P));" +
      "vec3 transformed = P;"
    );
    shader.vertexShader = shader.vertexShader.replace(
      "#include <beginnormal_vertex>",
      "vec3 objectNormal = dispNormal;" +
      "#ifdef USE_TANGENT\nvec3 objectTangent = vec3( tangent.xyz );\n#endif"
    );

    shader.fragmentShader =
      "uniform vec3 uRimColor; uniform float uRimStrength;\n" + shader.fragmentShader;
    shader.fragmentShader = shader.fragmentShader.replace(
      "#include <emissivemap_fragment>",
      "#include <emissivemap_fragment>\n" +
      "float fres = pow(1.0 - clamp(dot(normalize(normal), normalize(vViewPosition)), 0.0, 1.0), 3.0);\n" +
      "totalEmissiveRadiance += uRimColor * fres * uRimStrength;"
    );
  };

  var droplet = new THREE.Mesh(geometry, material);
  var HOME = new THREE.Vector3(mobile ? 0 : 1.25, 0.05, 0);
  droplet.position.copy(HOME);
  scene.add(droplet);

  var core = new THREE.Mesh(
    new THREE.IcosahedronGeometry(0.96, 2),
    new THREE.MeshBasicMaterial({ color: 0x0c2e2e, transparent: true, opacity: 0.55 })
  );
  core.position.copy(HOME);
  scene.add(core);

  // --- lights: engine gizmo colours -----------------------------
  var cyan = new THREE.PointLight(0x00ffff, 42, 18, 2.0);
  cyan.position.set(-3.4, 2.2, 2.6); scene.add(cyan);
  var amber = new THREE.PointLight(0xffc83c, 26, 18, 2.0);
  amber.position.set(3.6, -2.0, 1.4); scene.add(amber);
  var fill = new THREE.DirectionalLight(0xffffff, 0.5);
  fill.position.set(0.5, 1.0, 3.0); scene.add(fill);
  scene.add(new THREE.AmbientLight(0x202022, 1.0));
  var spark = new THREE.PointLight(0x6ff2f2, 10, 7, 2.0);
  scene.add(spark);

  // --- interaction ----------------------------------------------
  var pointer = { x: 0, y: 0, tx: 0, ty: 0 };
  var scrollN = 0;
  var visible = true;

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

  // --- loop ------------------------------------------------------
  var clock = new THREE.Clock();

  function render() {
    var t = clock.getElapsedTime();
    uniforms.uTime.value = t;

    pointer.x += (pointer.tx - pointer.x) * 0.05;
    pointer.y += (pointer.ty - pointer.y) * 0.05;

    droplet.rotation.y = t * 0.12 + pointer.x * 0.5;
    droplet.rotation.x = pointer.y * 0.32;
    core.rotation.copy(droplet.rotation);

    droplet.scale.setScalar(1 + Math.sin(t * 0.8) * 0.02);
    spark.position.set(Math.cos(t * 0.55) * 2.4, Math.sin(t * 0.8) * 1.8, 2.2);

    camera.position.x = pointer.x * 0.25;
    camera.position.y = -pointer.y * 0.18 - scrollN * 0.6;
    camera.position.z = 5.2 + scrollN * 1.6;
    droplet.position.y = HOME.y - scrollN * 0.5;
    core.position.y = droplet.position.y;
    camera.lookAt(HOME.x * 0.5, droplet.position.y * 0.4, 0);

    renderer.render(scene, camera);
  }

  function loop() {
    if (visible) render();
    requestAnimationFrame(loop);
  }

  render();
  if (!reduce) loop();
  requestAnimationFrame(function () { canvas.classList.add("ready"); });

  // --- helpers --------------------------------------------------
  function makeEnvTexture() {
    var c = document.createElement("canvas");
    c.width = 1024; c.height = 512;
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
    blob(120, 430, 300, [[0, "#1d3a66"], [1, "rgba(22,22,24,0)"]]);

    var tex = new THREE.CanvasTexture(c);
    tex.mapping = THREE.EquirectangularReflectionMapping;
    tex.colorSpace = THREE.SRGBColorSpace;
    tex.needsUpdate = true;
    return tex;
  }

  /* Ashima Arts simplex noise 3D — public domain (MIT). */
  var NOISE_GLSL =
    "vec3 mod289(vec3 x){return x-floor(x*(1.0/289.0))*289.0;}" +
    "vec4 mod289(vec4 x){return x-floor(x*(1.0/289.0))*289.0;}" +
    "vec4 permute(vec4 x){return mod289(((x*34.0)+1.0)*x);}" +
    "vec4 taylorInvSqrt(vec4 r){return 1.79284291400159-0.85373472095314*r;}" +
    "float snoise(vec3 v){" +
    "const vec2 C=vec2(1.0/6.0,1.0/3.0); const vec4 D=vec4(0.0,0.5,1.0,2.0);" +
    "vec3 i=floor(v+dot(v,C.yyy)); vec3 x0=v-i+dot(i,C.xxx);" +
    "vec3 g=step(x0.yzx,x0.xyz); vec3 l=1.0-g; vec3 i1=min(g.xyz,l.zxy); vec3 i2=max(g.xyz,l.zxy);" +
    "vec3 x1=x0-i1+C.xxx; vec3 x2=x0-i2+C.yyy; vec3 x3=x0-D.yyy; i=mod289(i);" +
    "vec4 p=permute(permute(permute(i.z+vec4(0.0,i1.z,i2.z,1.0))+i.y+vec4(0.0,i1.y,i2.y,1.0))+i.x+vec4(0.0,i1.x,i2.x,1.0));" +
    "float n_=0.142857142857; vec3 ns=n_*D.wyz-D.xzx;" +
    "vec4 j=p-49.0*floor(p*ns.z*ns.z); vec4 x_=floor(j*ns.z); vec4 y_=floor(j-7.0*x_);" +
    "vec4 x=x_*ns.x+ns.yyyy; vec4 y=y_*ns.x+ns.yyyy; vec4 h=1.0-abs(x)-abs(y);" +
    "vec4 b0=vec4(x.xy,y.xy); vec4 b1=vec4(x.zw,y.zw);" +
    "vec4 s0=floor(b0)*2.0+1.0; vec4 s1=floor(b1)*2.0+1.0; vec4 sh=-step(h,vec4(0.0));" +
    "vec4 a0=b0.xzyw+s0.xzyw*sh.xxyy; vec4 a1=b1.xzyw+s1.xzyw*sh.zzww;" +
    "vec3 p0=vec3(a0.xy,h.x); vec3 p1=vec3(a0.zw,h.y); vec3 p2=vec3(a1.xy,h.z); vec3 p3=vec3(a1.zw,h.w);" +
    "vec4 norm=taylorInvSqrt(vec4(dot(p0,p0),dot(p1,p1),dot(p2,p2),dot(p3,p3)));" +
    "p0*=norm.x; p1*=norm.y; p2*=norm.z; p3*=norm.w;" +
    "vec4 m=max(0.6-vec4(dot(x0,x0),dot(x1,x1),dot(x2,x2),dot(x3,x3)),0.0); m=m*m;" +
    "return 42.0*dot(m*m,vec4(dot(p0,x0),dot(p1,x1),dot(p2,x2),dot(p3,x3)));}";
})();
