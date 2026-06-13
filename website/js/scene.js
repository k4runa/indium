/* ============================================================
   Indium — hero 3D scene
   A molten droplet of indium: a displaced icosphere with a
   liquid-metal PBR surface, lit by a cyan key and an amber
   rim — the engine's own selection/gizmo colors — over a
   neutral charcoal void. GPU vertex displacement (noise) with
   correct per-fragment normals so the surface reads as liquid.

   Degrades: no WebGL -> CSS fallback; reduced-motion -> one
   static frame; offscreen -> render loop parked.
   ============================================================ */

import * as THREE from "three";
import { EffectComposer } from "three/addons/postprocessing/EffectComposer.js";
import { RenderPass } from "three/addons/postprocessing/RenderPass.js";
import { UnrealBloomPass } from "three/addons/postprocessing/UnrealBloomPass.js";
import { OutputPass } from "three/addons/postprocessing/OutputPass.js";

function boot(canvas) {
  const reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  const mobile = window.matchMedia("(max-width: 760px)").matches;

  let renderer;
  try {
    renderer = new THREE.WebGLRenderer({ canvas, antialias: !mobile, alpha: true, powerPreference: "high-performance" });
  } catch (e) {
    document.body.classList.add("no-webgl");
    return;
  }
  if (!renderer.capabilities.isWebGL2 && !renderer.getContext()) {
    document.body.classList.add("no-webgl");
    return;
  }

  const DPR = Math.min(window.devicePixelRatio || 1, mobile ? 1.5 : 2);
  renderer.setPixelRatio(DPR);
  renderer.setSize(window.innerWidth, window.innerHeight);
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.08;

  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(38, window.innerWidth / window.innerHeight, 0.1, 100);
  camera.position.set(0, 0, 5.2);

  // --- environment reflections -------------------------------------
  // A hand-painted cyan/amber gradient is what the chrome actually
  // reflects — pure metal has no diffuse, so the env map (not the
  // lights) carries the colour across the whole surface.
  const pmrem = new THREE.PMREMGenerator(renderer);
  const envTex = makeEnvTexture();
  const envMap = pmrem.fromEquirectangular(envTex).texture;
  scene.environment = envMap;
  envTex.dispose();

  // --- the droplet --------------------------------------------------
  const DETAIL = mobile ? 4 : 6;
  const geometry = new THREE.IcosahedronGeometry(1.32, DETAIL);

  const uniforms = {
    uTime:   { value: 0 },
    uAmp:    { value: 0.30 },
    uFreq:   { value: 1.25 },
    uSpeed:  { value: 0.32 },
    uRadius: { value: 1.32 },
    uRimColor:    { value: new THREE.Color(0x29e0e0) },
    uRimStrength: { value: 2.4 },
  };

  const material = new THREE.MeshStandardMaterial({
    color: new THREE.Color(0xcfd4db),  // silvery indium
    metalness: 1.0,
    roughness: 0.17,
    envMapIntensity: 1.55,
  });

  material.onBeforeCompile = (shader) => {
    Object.assign(shader.uniforms, uniforms);
    shader.vertexShader =
      NOISE_GLSL +
      `
      uniform float uTime; uniform float uAmp; uniform float uFreq;
      uniform float uSpeed; uniform float uRadius;
      float disp(vec3 dir){
        vec3 p = dir * uFreq + vec3(0.0, 0.0, uTime * uSpeed);
        float n = snoise(p) * 0.6 + snoise(p * 2.07 + 11.0) * 0.3;
        return n * uAmp;
      }
      vec3 surf(vec3 dir){ vec3 d = normalize(dir); return d * (uRadius + disp(d)); }
      ` +
      shader.vertexShader;

    // displaced position
    shader.vertexShader = shader.vertexShader.replace(
      "#include <begin_vertex>",
      `
      vec3 nrm0 = normalize(position);
      vec3 P = surf(nrm0);
      // tangent basis on the sphere for finite-difference normals
      vec3 upv = abs(nrm0.y) < 0.99 ? vec3(0.0,1.0,0.0) : vec3(1.0,0.0,0.0);
      vec3 tang = normalize(cross(upv, nrm0));
      vec3 bita = normalize(cross(nrm0, tang));
      float e = 0.012;
      vec3 Pa = surf(nrm0 + tang * e);
      vec3 Pb = surf(nrm0 + bita * e);
      vec3 dispNormal = normalize(cross(Pa - P, Pb - P));
      vec3 transformed = P;
      `
    );
    shader.vertexShader = shader.vertexShader.replace(
      "#include <beginnormal_vertex>",
      `vec3 objectNormal = dispNormal;
       #ifdef USE_TANGENT
       vec3 objectTangent = vec3( tangent.xyz );
       #endif`
    );

    // Fresnel rim emissive — a cyan edge glow that lights the silhouette
    // and drives the bloom, the "energy core" liquid-metal read.
    shader.fragmentShader =
      `uniform vec3 uRimColor; uniform float uRimStrength;\n` + shader.fragmentShader;
    shader.fragmentShader = shader.fragmentShader.replace(
      "#include <emissivemap_fragment>",
      `#include <emissivemap_fragment>
       float fres = pow(1.0 - clamp(dot(normalize(normal), normalize(vViewPosition)), 0.0, 1.0), 3.0);
       totalEmissiveRadiance += uRimColor * fres * uRimStrength;`
    );
  };

  const droplet = new THREE.Mesh(geometry, material);
  const HOME = new THREE.Vector3(mobile ? 0 : 1.25, 0.05, 0);  // composed right of the headline
  droplet.position.copy(HOME);
  scene.add(droplet);

  // faint inner core glow sphere (adds depth, catches bloom subtly)
  const core = new THREE.Mesh(
    new THREE.IcosahedronGeometry(0.96, 2),
    new THREE.MeshBasicMaterial({ color: 0x0c2e2e, transparent: true, opacity: 0.55 })
  );
  core.position.copy(HOME);
  scene.add(core);

  // --- lights: engine gizmo colors ---------------------------------
  const cyan = new THREE.PointLight(0x00ffff, 42, 18, 2.0);   // selection cyan (key)
  cyan.position.set(-3.4, 2.2, 2.6);
  scene.add(cyan);

  const amber = new THREE.PointLight(0xffc83c, 26, 18, 2.0);  // rotation-ring amber (rim)
  amber.position.set(3.6, -2.0, 1.4);
  scene.add(amber);

  const fill = new THREE.DirectionalLight(0xffffff, 0.5);
  fill.position.set(0.5, 1.0, 3.0);
  scene.add(fill);

  scene.add(new THREE.AmbientLight(0x202022, 1.0));

  // orbiting cyan spark — a moving highlight that drifts across the metal
  const spark = new THREE.PointLight(0x6ff2f2, 10, 7, 2.0);
  scene.add(spark);

  // --- post: gentle bloom on desktop -------------------------------
  let composer = null;
  const useBloom = !mobile;
  if (useBloom) {
    composer = new EffectComposer(renderer);
    composer.addPass(new RenderPass(scene, camera));
    const bloom = new UnrealBloomPass(
      new THREE.Vector2(window.innerWidth, window.innerHeight),
      0.62, 0.55, 0.82   // strength, radius, threshold
    );
    composer.addPass(bloom);
    composer.addPass(new OutputPass());
    composer.setPixelRatio(DPR);
    composer.setSize(window.innerWidth, window.innerHeight);
  }

  // --- interaction state -------------------------------------------
  const pointer = { x: 0, y: 0, tx: 0, ty: 0 };
  let scrollN = 0;          // 0..1 over first viewport
  let visible = true;

  if (!reduce) {
    window.addEventListener("pointermove", (e) => {
      pointer.tx = (e.clientX / window.innerWidth) * 2 - 1;
      pointer.ty = (e.clientY / window.innerHeight) * 2 - 1;
    }, { passive: true });

    window.addEventListener("scroll", () => {
      scrollN = Math.min(1, Math.max(0, window.scrollY / window.innerHeight));
    }, { passive: true });

    const hero = document.querySelector(".hero");
    if (hero && "IntersectionObserver" in window) {
      new IntersectionObserver(
        (es) => es.forEach((en) => { visible = en.isIntersecting; }),
        { threshold: 0.01 }
      ).observe(hero);
    }
  }

  // --- resize -------------------------------------------------------
  function resize() {
    const w = window.innerWidth, h = window.innerHeight;
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
    renderer.setSize(w, h);
    if (composer) composer.setSize(w, h);
  }
  window.addEventListener("resize", resize);

  // --- loop ---------------------------------------------------------
  const clock = new THREE.Clock();

  function render() {
    const t = clock.getElapsedTime();
    uniforms.uTime.value = t;

    // ease pointer
    pointer.x += (pointer.tx - pointer.x) * 0.05;
    pointer.y += (pointer.ty - pointer.y) * 0.05;

    // idle rotation + mouse parallax
    droplet.rotation.y = t * 0.12 + pointer.x * 0.5;
    droplet.rotation.x = pointer.y * 0.32;
    core.rotation.copy(droplet.rotation);

    // breathing scale
    const breathe = 1 + Math.sin(t * 0.8) * 0.02;
    droplet.scale.setScalar(breathe);

    // drifting spark
    spark.position.set(Math.cos(t * 0.55) * 2.4, Math.sin(t * 0.8) * 1.8, 2.2);

    // scroll: droplet sinks + recedes as you leave the hero
    camera.position.x = pointer.x * 0.25;
    camera.position.y = -pointer.y * 0.18 - scrollN * 0.6;
    camera.position.z = 5.2 + scrollN * 1.6;
    droplet.position.y = HOME.y - scrollN * 0.5;
    core.position.y = droplet.position.y;
    camera.lookAt(HOME.x * 0.5, droplet.position.y * 0.4, 0);

    if (composer) composer.render(); else renderer.render(scene, camera);
  }

  function loop() {
    if (visible) render();
    requestAnimationFrame(loop);
  }

  // first paint always (so reduced-motion users still see the metal)
  render();
  if (!reduce) loop();

  // reveal canvas once the first frame is up (avoids a black flash)
  requestAnimationFrame(() => canvas.classList.add("ready"));
}

/* Equirectangular gradient baked from a 2D canvas. This is what the
   chrome reflects: a charcoal void with a cyan glow, an amber glow,
   and a crisp white key for sharp highlights — the engine palette. */
function makeEnvTexture() {
  const c = document.createElement("canvas");
  c.width = 1024; c.height = 512;
  const x = c.getContext("2d");

  x.fillStyle = "#161618"; x.fillRect(0, 0, 1024, 512);

  const blob = (cx, cy, r, stops) => {
    const g = x.createRadialGradient(cx, cy, 0, cx, cy, r);
    stops.forEach(([o, col]) => g.addColorStop(o, col));
    x.fillStyle = g; x.fillRect(0, 0, 1024, 512);
  };

  // cyan key (upper-left of the reflection)
  blob(300, 150, 440, [[0, "#23e6e6"], [0.35, "#0c5454"], [1, "rgba(14,14,16,0)"]]);
  // amber rim (lower-right)
  blob(800, 410, 380, [[0, "#ffc83c"], [0.4, "#5a3c08"], [1, "rgba(14,14,16,0)"]]);
  // bright white highlight for a sharp specular streak
  blob(560, 70, 170, [[0, "#ffffff"], [0.5, "#404044"], [1, "rgba(14,14,16,0)"]]);
  // soft blue fill (the handle blue) low-left, very subtle
  blob(120, 430, 300, [[0, "#1d3a66"], [1, "rgba(14,14,16,0)"]]);

  const tex = new THREE.CanvasTexture(c);
  tex.mapping = THREE.EquirectangularReflectionMapping;
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.needsUpdate = true;
  return tex;
}

/* Ashima Arts simplex noise 3D — public domain (MIT). */
const NOISE_GLSL = `
vec3 mod289(vec3 x){return x-floor(x*(1.0/289.0))*289.0;}
vec4 mod289(vec4 x){return x-floor(x*(1.0/289.0))*289.0;}
vec4 permute(vec4 x){return mod289(((x*34.0)+1.0)*x);}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159-0.85373472095314*r;}
float snoise(vec3 v){
  const vec2 C=vec2(1.0/6.0,1.0/3.0); const vec4 D=vec4(0.0,0.5,1.0,2.0);
  vec3 i=floor(v+dot(v,C.yyy)); vec3 x0=v-i+dot(i,C.xxx);
  vec3 g=step(x0.yzx,x0.xyz); vec3 l=1.0-g; vec3 i1=min(g.xyz,l.zxy); vec3 i2=max(g.xyz,l.zxy);
  vec3 x1=x0-i1+C.xxx; vec3 x2=x0-i2+C.yyy; vec3 x3=x0-D.yyy;
  i=mod289(i);
  vec4 p=permute(permute(permute(i.z+vec4(0.0,i1.z,i2.z,1.0))+i.y+vec4(0.0,i1.y,i2.y,1.0))+i.x+vec4(0.0,i1.x,i2.x,1.0));
  float n_=0.142857142857; vec3 ns=n_*D.wyz-D.xzx;
  vec4 j=p-49.0*floor(p*ns.z*ns.z);
  vec4 x_=floor(j*ns.z); vec4 y_=floor(j-7.0*x_);
  vec4 x=x_*ns.x+ns.yyyy; vec4 y=y_*ns.x+ns.yyyy; vec4 h=1.0-abs(x)-abs(y);
  vec4 b0=vec4(x.xy,y.xy); vec4 b1=vec4(x.zw,y.zw);
  vec4 s0=floor(b0)*2.0+1.0; vec4 s1=floor(b1)*2.0+1.0; vec4 sh=-step(h,vec4(0.0));
  vec4 a0=b0.xzyw+s0.xzyw*sh.xxyy; vec4 a1=b1.xzyw+s1.xzyw*sh.zzww;
  vec3 p0=vec3(a0.xy,h.x); vec3 p1=vec3(a0.zw,h.y); vec3 p2=vec3(a1.xy,h.z); vec3 p3=vec3(a1.zw,h.w);
  vec4 norm=taylorInvSqrt(vec4(dot(p0,p0),dot(p1,p1),dot(p2,p2),dot(p3,p3)));
  p0*=norm.x; p1*=norm.y; p2*=norm.z; p3*=norm.w;
  vec4 m=max(0.6-vec4(dot(x0,x0),dot(x1,x1),dot(x2,x2),dot(x3,x3)),0.0); m=m*m;
  return 42.0*dot(m*m,vec4(dot(p0,x0),dot(p1,x1),dot(p2,x2),dot(p3,x3)));
}
`;

const sceneCanvas = document.getElementById("scene");
if (sceneCanvas) boot(sceneCanvas);
