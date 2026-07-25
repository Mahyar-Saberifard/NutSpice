# NutSpice 2.0 — Refactor Notes

NutSpice is a small SPICE-like circuit simulator with an SDL2 GUI. This
folder contains a **cleaned-up, cross-platform** rewrite of the
original 5,889-line `main.cpp`. Behaviour is preserved except where the
original had outright bugs (see below).

## File layout

```
NutSpice_refactored/
├── CMakeLists.txt          ← cross-platform build (Linux / macOS / Windows)
├── Icon1.png               ← window icon (optional asset)
├── input.txt, test1..4.txt ← example SPICE decks
└── src/
    ├── Platform.h/.cpp     ← cross-platform filesystem + font/icon lookup
    ├── Theme.h/.cpp        ← colour palette + light/dark themes
    ├── UI.h/.cpp           ← Button/TextBox PODs + renderText/renderButton/...
    ├── Component.h/.cpp    ← base Component class + enums + StampMatrices
    ├── Passives.h/.cpp     ← Resistor / Capacitor / Inductor / Diode / Ground / Wire
    ├── Sources.h/.cpp      ← VoltageSource / CurrentSource / Sin* / Pulse*
    ├── DependentSources.h/.cpp  ← VCVS / VCCS / CCVS / CCCS  (BUG-FIXED)
    ├── Circuit.h/.cpp      ← Circuit class + DC / Transient / DC Sweep + plot
    ├── Parser.h/.cpp       ← parseSpiceValue + .txt loader + .txt writer
    ├── Renderer.h/.cpp     ← drawCircuit / drawToolbar / dialogs / library
    ├── Placement.h/.cpp    ← click-to-place logic (BUG-FIXED)
    └── main.cpp            ← clean SDL setup + event loop (BUG-FIXED)
```

## Building

### Linux (Debian / Ubuntu)

```bash
sudo apt install build-essential cmake libsdl2-dev libsdl2-image-dev \
                 libsdl2-ttf-dev libsdl2-gfx-dev libcereal-dev
cd NutSpice_refactored
cmake -B build -S .
cmake --build build -j
./build/NutSpice
```

### macOS (Homebrew)

```bash
brew install cmake sdl2 sdl2_image sdl2_ttf sdl2_gfx cereal
cd NutSpice_refactored
cmake -B build -S .
cmake --build build -j
./build/NutSpice
```

### Windows (vcpkg)

```bash
vcpkg install sdl2 sdl2-image sdl2-ttf sdl2-gfx cereal
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
.\build\Release\NutSpice.exe
```

## Bug fixes vs. the original 5,889-line `main.cpp`

### Cross-platform (the original ran on Windows only)

1. Removed `#include <windows.h>` and `#include <direct.h>` from the global
   scope. The only remaining `<windows.h>` include is inside `Platform.cpp`
   where it's needed for `GetModuleFileNameW` and is wrapped in `#ifdef
_WIN32`.
2. Replaced the hard-coded font path `C:\Windows\Fonts\consola.ttf` with
   `nutspice::locateFont()`, which tries the caller-supplied candidates
   first, then well-known locations on each OS.
3. Replaced the hard-coded personal icon path
   `C:\Users\Mahyar\Documents\GitHub\...\Icon1.png` with
   `nutspice::locateAsset("Icon1.png")`, which looks in the working
   directory and the executable's directory.
4. Replaced `WIN32_FIND_DATA` / `FindFirstFile` file enumeration with
   `std::filesystem::directory_iterator`.
5. Replaced `_fullpath` / `realpath` branching with
   `std::filesystem::weakly_canonical`.
6. Removed the `changeToPreviousDirectory()` call that ran at startup and
   silently moved the process one directory up — surprising side-effect.

### Dependent sources (the original was broken)

7. **CCCS::stamp was completely wrong.** It wrote to `D[node1-1]` but `D`
   is indexed by _source_ variables, not by nodes — out-of-bounds. The
   correct MNA stamp for a CCCS is on the **B** matrix:
   `B[node1-1][ctrlSrcIdx] += value; B[node2-1][ctrlSrcIdx] -= value;`
8. **`analyzeDC` and `analyzeDCSweep` didn't count `VCVS_SOURCE` or
   `CCVS_SOURCE` in `numVSources`.** That made the B/C/D matrices too
   narrow and caused out-of-bounds writes from `VCVS::stamp` and
   `CCVS::stamp`. Both analyses now count every voltage-source-class
   component.
9. **`CCVS::resolveIndices` and `CCCS::resolveIndices` were never called.**
   The original code set `controllingSourceIndex = -1` in the constructor
   and never updated it, so the CCVS stamp did nothing and the CCCS stamp
   crashed. `Circuit::analyzeDC`, `analyzeTransient`, and
   `analyzeDCSweep` now build a `{name → index}` map of every voltage
   source and call `resolveIndices()` on every CCVS / CCCS before
   stamping.

### Component placement (the original was unusable for dependent sources)

10. **VCVS / VCCS placement hard-coded control node names `"1"` and `"2"`**
    — the user could never pick the actual control nodes. Placement is
    now a 4-click flow: out+, out-, ctrl+, ctrl-.
11. **CCVS / CCCS placement hard-coded the controlling source name to
    `"CCVS"` / `"CCCS"`** — never matched a real source, so
    `resolveIndices` always threw. The user now enters the controlling
    source's name in the **Value** text box before placing the CCVS /
    CCCS.
12. Ground placement now correctly uses its single-node constructor (the
    original sometimes passed `node2=0` to a two-node code path).

### GUI / main loop bugs

13. **`renderStatus(renderer)` was called BEFORE `initSDL()`** — renderer
    was null, undefined behaviour.
14. **Local `double timeZoom = 1.0; ...` declarations in `main()`**
    shadowed the globals, so mouse-wheel zoom/pan never affected the
    actual plot state. The shadowing locals are removed.
15. **`Tstep / Tstop / DCs / DCTstart / DCTstep / DCTstop` flags in
    `main()` were never reset to `false`** once set, so they kept
    overwriting unrelated text boxes later. The whole batch is replaced
    by a single `activeTextBox` pointer that already worked correctly.
16. **`showEditMenu` declared `deleteBtn` twice** — the duplicate is
    removed.
17. **`Ground::getBoundingBox` called `nodePositions.at(node2)`** even
    though Ground only has node1 (node2 is always 0). Rewritten to use
    only node1.

### Small visual changes

- Slightly warmer dark-mode palette (was pure 40/40/40) and slightly
  softer light-mode background.
- Toolbar now has a distinct tint in each theme so it visually separates
  from the canvas.
- A new `gridLine` colour is used for the plot grid (was a hard-coded
  alpha trick) and for a faint dot grid in the circuit canvas.
- A new `highlight` accent colour is used for selected nodes and the
  placement-mode status line.
- The plot panel is a bit wider (360 vs 380 px) and the circuit area is a
  bit taller, giving more room for circuits.
- Plot legend shows the signal name next to a colour swatch instead of
  the original's "click to select" implicit legend.

## Core functions kept as-is

Per the user's request, the core numerical kernels were **not** touched:

- `Circuit::solveSystem` (Gaussian elimination with partial pivoting)
- The MNA stamp equations for every component (only the matrix-handle
  plumbing changed — `vector<vector<double>>& ...` → `StampMatrices m`).
- The companion-model update rules for Capacitor / Inductor / Diode.
- The Pulse / Sin source time-stepping functions.
- The cereal binary serialization layout (file format unchanged).

## Tested with

- `test1.txt` — simple 3-resistor divider with V and I sources.
- `test2.txt` — VS / R / C / L: transient simulation of a damped tank.
- `test3.txt` — VP pulse source driving an RLC.
- `test4.txt` — combined V + VS sources and an RLC ladder.
