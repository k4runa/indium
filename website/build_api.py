#!/usr/bin/env python3
"""Generates website/api.html from docs/API.md.

Run from the repo root (or anywhere — paths are resolved relative to this
file) whenever docs/API.md changes:

    python3 website/build_api.py
"""
import re
from pathlib import Path

import markdown

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "docs" / "API.md"
OUT = ROOT / "website" / "api.html"

text = SRC.read_text()

# Drop the markdown's own Table of Contents — the page gets a live sidebar.
text = re.sub(r"## Table of Contents.*?\n---\n", "", text, flags=re.S)

# Repo-relative links don't resolve on the website.
text = text.replace("(../README.md)", "(https://github.com/k4runa/indium#readme)")

md = markdown.Markdown(extensions=["fenced_code", "tables", "toc"])
body = md.convert(text)

# API.md's inline anchors use GitHub-style slugs where an em-dash becomes a
# double hyphen; python-markdown collapses runs of dashes. Normalize hrefs.
body = re.sub(r'href="#([^"]+)"', lambda m: 'href="#' + re.sub(r"-{2,}", "-", m.group(1)) + '"', body)

# Sidebar from the h2 headings.
links = []
for tok in md.toc_tokens:
    if tok["level"] == 1:
        for sub in tok["children"]:
            links.append((sub["id"], sub["name"]))
    elif tok["level"] == 2:
        links.append((tok["id"], tok["name"]))
sidebar = "\n".join(
    f'        <a href="#{anchor}">{name}</a>' for anchor, name in links
)

# The first h1 + intro paragraphs come from the markdown; restyle the h1.
body = body.replace("<h1", '<h1 class="api-title"', 1)

page = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Scripting API — Indium</title>
<meta name="description" content="Indium scripting API reference — every helper, manager and event available to hot-reloaded C++ scripts.">
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Crect width='64' height='64' fill='%2310101a'/%3E%3Crect x='2' y='2' width='60' height='60' fill='none' stroke='%236e62ff' stroke-width='3'/%3E%3Ctext x='32' y='42' font-family='monospace' font-size='28' font-weight='bold' fill='%23f4f4f9' text-anchor='middle'%3EIn%3C/text%3E%3Ctext x='10' y='16' font-family='monospace' font-size='10' fill='%23767689'%3E49%3C/text%3E%3C/svg%3E">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,700;12..96,800&family=IBM+Plex+Mono:wght@400;500;600&family=IBM+Plex+Sans:wght@400;500;600&display=swap" rel="stylesheet">
<link rel="stylesheet" href="css/style.css">
</head>
<body>

<nav class="nav">
  <div class="wrap">
    <a class="brand" href="index.html" aria-label="Indium home">
      <span class="element"><b>In</b><i></i></span>
      <span>INDIUM</span>
    </a>
    <button class="nav-toggle" aria-label="Menu" aria-expanded="false">≡</button>
    <div class="links" id="navlinks">
      <a href="index.html#features">Features</a>
      <a href="index.html#scripting">Scripting</a>
      <a href="getting-started.html">Getting&nbsp;Started</a>
      <a href="api.html" aria-current="page">API</a>
      <a href="index.html#download">Download</a>
      <a class="gh" href="https://github.com/k4runa/indium" target="_blank" rel="noopener">
        <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27s1.36.09 2 .27c1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.01 8.01 0 0 0 16 8c0-4.42-3.58-8-8-8z"/></svg>
        GitHub
      </a>
    </div>
  </div>
</nav>

<div class="docs wrap">
  <aside>
    <div class="inner">
      <div class="group">
        <span class="tag">API Reference</span>
{sidebar}
      </div>
      <div class="group">
        <span class="tag">Guide</span>
        <a href="getting-started.html">Getting Started</a>
      </div>
    </div>
  </aside>

  <main>
    <span class="tag crumb"><span class="idx">DOCS</span> // api reference</span>
{body}
  </main>
</div>

<footer>
  <div class="wrap">
    <div class="row">
      <span class="made">© <span id="year">2026</span> Indium Engine · MIT License</span>
      <div class="links">
        <a href="index.html">Home</a>
        <a href="getting-started.html">Getting Started</a>
        <a href="https://github.com/k4runa/indium" target="_blank" rel="noopener">GitHub</a>
      </div>
    </div>
  </div>
</footer>

<script src="js/main.js"></script>
</body>
</html>
"""

OUT.write_text(page)
print(f"wrote {OUT} ({len(page)} bytes, {len(links)} sidebar sections)")
