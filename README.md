# Extreme Tux Racer for the web

A port of [Extreme Tux Racer](https://sourceforge.net/projects/extremetuxracer/)
0.8.4 to the browser. The original C++ game runs unchanged as WebAssembly,
rendered with WebGL 2. Steer Tux down snowy mountains, collect herrings, beat
the clock: practice on any course or play through the event cups.

When GitHub Pages is enabled for this repository (see [Deploying](#deploying)),
the game is published at `https://<owner>.github.io/extremetuxracer-web/`.

## Playing

| Key | Action |
| --- | --- |
| ← → (or A D) | Steer |
| ↑ (or W) | Paddle |
| ↓ (or S) | Brake |
| Space | Jump (hold to charge) |
| T + arrows | Tricks in the air |
| P / R / Esc | Pause / reset Tux / abort the race |
| 1, 2, 3 | Camera mode |
| H / F | Toggle HUD / frame rate |
| C | Save a screenshot (downloads a PNG) |

Menus work with the mouse, the keyboard or by tapping. On touch devices an
on-screen pad (steer, paddle, brake, jump, pause) appears during races. Gamepads
are supported through the browser Gamepad API (left stick or D-pad, buttons as
configured in the game).

Options, players and high scores are stored in the browser (IndexedDB), so they
survive reloads on the same device and browser.

**Requirements:** a browser with WebGL 2 and WebAssembly exception handling:
current Chrome, Edge, Firefox or Safari (15.2 or later). The first visit
downloads about 45 MB of game data, which the browser then caches.

## Building

You need [Emscripten](https://emscripten.org/docs/getting_started/downloads.html)
(tested with 6.0.13) and CMake 3.16 or later.

```bash
emcmake cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build -j
```

The game is written to `build/web/` (`index.html`, `etr.js`, `etr.wasm`,
`etr.data`). Browsers don't run WebAssembly from `file://` URLs, so serve the
directory over HTTP, for example:

```bash
python3 -m http.server -d build/web 8080
```

Then open <http://localhost:8080>.

For a build with debug info, Emscripten assertions and OpenGL error checking,
configure with `-DETR_DEBUG=ON`.

## Deploying

`.github/workflows/web.yml` builds the game on every push and pull request. On
pushes to `main` or `master` it also publishes the result to GitHub Pages. To
enable publishing, open the repository's **Settings → Pages** and set
**Source** to **GitHub Actions**.

Any other static web host works too: upload the four files from `build/web/`.

## How the port works

The game's own code (`src/`) is the upstream 0.8.4 source with only a few
browser-specific changes, each marked with `#ifdef __EMSCRIPTEN__`. Everything
the game needs from the platform is provided under `port/`:

- **`port/glcompat/`: fixed-function OpenGL on WebGL 2.** ETR is written
  against the OpenGL 1.x fixed-function pipeline, which WebGL does not have.
  This layer implements the parts the game uses on top of a single shader:
  - immediate mode (`glBegin`/`glEnd`), matrix stacks and client-side vertex
    arrays;
  - per-vertex lighting with colour material, fog, object-linear texture
    coordinate generation and alpha test;
  - texture environments, quads and `gluSphere`.

  `port/include/GL/*.h` forward the game's `<GL/gl.h>` and `<GL/glu.h>`
  includes to it. The terrain's interleaved vertex arrays are uploaded as-is,
  and redundant uniform updates are skipped, so the CPU stays mostly idle
  while racing.
- **`port/sfml/`: an SFML 2 compatible subset.** It covers the System, Window,
  Graphics and Audio APIs the game uses:
  - windowing and input on the Emscripten HTML5 API (keyboard, mouse, touch,
    gamepads);
  - images via stb_image, and text via stb_truetype glyph atlases, with
    SFML's layout semantics;
  - sprites, shapes and render textures drawn through the GL layer;
  - sound effects through the Web Audio API and music through an `<audio>`
    element. Ogg Vorbis is decoded with stb_vorbis where the browser can't play
    it natively.
- **`port/web/`: the page and browser glue.** This has the HTML shell (download
  progress, canvas sizing, full screen, touch controls), the IndexedDB-backed
  save directory, and the code that saves progress.

Changes to the game code:

| Change | Files |
| --- | --- |
| The browser drives the main loop (`emscripten_set_main_loop`). | `states.cpp` |
| Data is read from `/data` (preloaded); settings, players and scores go to `/persist` (IndexedDB). | `game_config.cpp` |
| The splash and loading screens are shown for one frame before the blocking resource loading starts. | `splash_screen.cpp`, `loading.cpp` |
| Progress is saved when switching screens and when the page is hidden, because closing a tab never reaches the game's "quit". | `states.cpp` |
| The canvas follows the browser window. A resize is letterboxed immediately and applied at the next screen change, when every screen lays itself out again. | `winsys.cpp`, `states.cpp` |
| The options screen's *Fullscreen* checkbox controls browser full screen; the resolution is always *auto*. | `config_screen.cpp` |
| Screenshots are offered as PNG downloads. | `winsys.cpp` |

## Licence

Extreme Tux Racer is free software released under the GNU General Public
License, version 2 or later (see `COPYING`), and so is this port. Credits for
the game, its courses, characters and music are in `AUTHORS`, `data/credits.lst`
and `data/music/readme`. Extreme Tux Racer is based on Tux Racer by Jasmin
F. Patry.

The bundled [stb](https://github.com/nothings/stb) libraries in
`third_party/stb/` are public domain (or MIT, at your option).
