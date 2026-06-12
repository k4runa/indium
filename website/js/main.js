/* Indium site — minimal JS: nav toggle, scroll reveals, copy buttons,
   tiny C++ highlighter for docs code blocks, latest-release links. */

(function () {
  "use strict";

  /* ---- year ---- */
  var y = document.getElementById("year");
  if (y) y.textContent = new Date().getFullYear();

  /* ---- mobile nav ---- */
  var toggle = document.querySelector(".nav-toggle");
  var links = document.getElementById("navlinks");
  if (toggle && links) {
    toggle.addEventListener("click", function () {
      var open = links.classList.toggle("open");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
  }

  /* ---- scroll reveal ---- */
  if ("IntersectionObserver" in window) {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting) { e.target.classList.add("in"); io.unobserve(e.target); }
      });
    }, { threshold: 0.12 });
    document.querySelectorAll(".reveal").forEach(function (el) { io.observe(el); });
  } else {
    document.querySelectorAll(".reveal").forEach(function (el) { el.classList.add("in"); });
  }

  /* ---- tiny C++ highlighter (docs pages: pre > code without manual tokens) ---- */
  var KEYWORDS = /\b(class|struct|public|private|protected|virtual|override|const|constexpr|static|void|bool|int|float|double|char|auto|return|if|else|for|while|switch|case|break|continue|new|delete|nullptr|true|false|using|namespace|template|typename|enum|include|this|operator|sizeof|union|long|unsigned|short|default|do|try|catch|throw)\b/g;

  function esc(s) { return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;"); }

  function highlightCpp(src) {
    var out = "";
    var i = 0;
    var n = src.length;
    while (i < n) {
      var rest = src.slice(i);
      var m;
      if ((m = rest.match(/^\/\/[^\n]*/)) || (m = rest.match(/^\/\*[\s\S]*?\*\//))) {
        out += '<span class="tk-c">' + esc(m[0]) + "</span>"; i += m[0].length; continue;
      }
      if ((m = rest.match(/^"(?:[^"\\\n]|\\.)*"/)) || (m = rest.match(/^'(?:[^'\\\n]|\\.)*'/))) {
        out += '<span class="tk-s">' + esc(m[0]) + "</span>"; i += m[0].length; continue;
      }
      if ((m = rest.match(/^#\s*\w+/))) {
        out += '<span class="tk-p">' + esc(m[0]) + "</span>"; i += m[0].length; continue;
      }
      if ((m = rest.match(/^\d[\d_']*\.?\d*[fuUlL]*/))) {
        out += '<span class="tk-n">' + esc(m[0]) + "</span>"; i += m[0].length; continue;
      }
      if ((m = rest.match(/^[A-Za-z_]\w*/))) {
        var w = m[0];
        var after = rest.slice(w.length);
        var cls = null;
        KEYWORDS.lastIndex = 0;
        if (KEYWORDS.test(w)) cls = "tk-k";
        else if (/^\s*\(/.test(after)) cls = "tk-f";
        else if (/^[A-Z]/.test(w)) cls = "tk-t";
        out += cls ? '<span class="' + cls + '">' + esc(w) + "</span>" : esc(w);
        i += w.length; continue;
      }
      out += esc(src[i]); i += 1;
    }
    return out;
  }

  document.querySelectorAll(".docs pre > code").forEach(function (code) {
    if (code.querySelector("span")) return;            // already tokenized
    var lang = (code.className.match(/language-(\w+)/) || [])[1] || "cpp";
    if (lang === "cpp" || lang === "c" || lang === "cxx") {
      code.innerHTML = highlightCpp(code.textContent);
    }
  });

  /* ---- copy buttons on code blocks ---- */
  document.querySelectorAll(".docs pre, .codecard pre").forEach(function (pre) {
    var btn = document.createElement("button");
    btn.className = "copy-btn";
    btn.type = "button";
    btn.textContent = "COPY";
    btn.addEventListener("click", function () {
      var code = pre.querySelector("code") || pre;
      navigator.clipboard.writeText(code.textContent).then(function () {
        btn.textContent = "COPIED";
        btn.classList.add("ok");
        setTimeout(function () { btn.textContent = "COPY"; btn.classList.remove("ok"); }, 1600);
      });
    });
    pre.appendChild(btn);
  });

  /* ---- docs sidebar: highlight current section ---- */
  var sideLinks = document.querySelectorAll(".docs aside a[href^='#']");
  if (sideLinks.length && "IntersectionObserver" in window) {
    var map = {};
    sideLinks.forEach(function (a) { map[a.getAttribute("href").slice(1)] = a; });
    var current = null;
    var spy = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting && map[e.target.id]) {
          if (current) current.classList.remove("active");
          current = map[e.target.id];
          current.classList.add("active");
        }
      });
    }, { rootMargin: "-80px 0px -70% 0px" });
    document.querySelectorAll(".docs main h2[id], .docs main h1[id]").forEach(function (h) { spy.observe(h); });
  }

  /* ---- latest release links (index download cards) ---- */
  var grid = document.getElementById("dl-grid");
  if (grid && window.fetch) {
    fetch("https://api.github.com/repos/k4runa/indium/releases/latest")
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (rel) {
        if (!rel || !rel.assets) return;
        var ver = document.getElementById("hero-version");
        if (ver && rel.tag_name) ver.textContent = rel.tag_name + " · Linux / macOS / Windows";
        grid.querySelectorAll(".dl-card").forEach(function (card) {
          var suffix = card.getAttribute("data-suffix");
          var asset = rel.assets.find(function (a) { return a.name.indexOf(suffix) !== -1; });
          if (asset) {
            card.href = asset.browser_download_url;
            var f = card.querySelector(".file");
            if (f) f.textContent = asset.name + "  ·  " + (asset.size / 1048576).toFixed(1) + " MB";
          }
        });
      })
      .catch(function () { /* cards already link to the releases page */ });
  }
})();
