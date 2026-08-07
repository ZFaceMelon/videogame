# videogame

A from-scratch game engine starter in **C++** (rendering, windowing, low-level systems) and **OCaml** (game logic layer via FFI). The first milestone: a window with a rotating colored cube.

## Architecture

```
videogame/
├── engine/          # C++ engine core (window, OpenGL, renderer)
├── game/            # OCaml game layer (calls into the C++ engine)
├── CMakeLists.txt   # Builds the C++ engine + standalone binary
└── dune-project     # Builds the OCaml entry point
```

| Layer | Language | Responsibility (long-term) |
|-------|----------|----------------------------|
| Engine | C++ | Window, input, rendering, physics, animation |
| Game | OCaml | Combat rules, AI, quests, networking logic |

For now, both entry points run the same thing: GLFW window + OpenGL cube.

See [ARCHITECTURE.md](ARCHITECTURE.md) for a file-by-file explanation of the codebase.

## Prerequisites (Windows)

Install these before building:

1. **Visual Studio 2022** (or Build Tools) with the "Desktop development with C++" workload  
   https://visualstudio.microsoft.com/downloads/

2. **CMake** 3.20+  
   https://cmake.org/download/

3. **Git** (CMake downloads GLFW via FetchContent)  
   https://git-scm.com/download/win

4. **OCaml** (optional, for the OCaml entry point)  
   - Install [opam for Windows](https://fdopen.github.io/opam-repository-mingw/installation/)
   - Then: `opam init` and `opam install dune`

Open a **Developer PowerShell for VS 2022** (or any shell where `cl` and `cmake` are on PATH).

## Build — C++ (recommended first step)

```powershell
cd c:\Users\zanek\videogame
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\videogame_cxx.exe
```

You should see a 1280×720 window with a slowly rotating cube. Press the window close button to exit.

## Controls

- `W`, `A`, `S`, `D` move relative to where the camera is looking (on the ground plane).
- The character turns to face the direction it is moving.
- Left click swings the sword at the training dummy on the far side of the dais.
- Space jumps; press Space again while airborne to enter flight.
- While flying, hold Space to rise, Ctrl to descend, and Shift to boost.
- F toggles flight directly and can be used to exit flight.
- V swaps the over-the-shoulder camera between the left and right sides.
- Move the mouse to orbit the camera.
- `Esc` closes the window.
- `R` toggles the clean atmospheric ray mode.
- `C` switches between the animated samurai and the experimental Pharaoh character.
- Right click fires the Pharaoh's finger projectile in the direction he is facing.

Debug build:

```powershell
cmake -B build
cmake --build build
.\build\Debug\videogame_cxx.exe
```

## Build — OCaml entry point

After the C++ engine is built once (CMake writes `game/config/link_flags.sexp` automatically):

```powershell
opam exec -- dune build game/bin/main.exe
opam exec -- dune exec game/bin/main.exe
```

This prints a line from OCaml, then opens the same cube window.

## What is "from scratch" here?

- **Hand-rolled OpenGL loader** — only the GL functions we need are loaded via `glfwGetProcAddress` (no GLAD/GLEW).
- **Minimal math** — a small `Mat4` type instead of GLM.
- **Embedded shaders** — GLSL 330 core, compiled at startup.
- **GLFW** — used only for window/context/input; fetched automatically by CMake.

You own and can replace every line in `engine/src/`.

## Suggested next steps (when you're ready)

1. **Input** — read keyboard/mouse from GLFW, pass events to OCaml via FFI.
2. **Camera** — third-person orbit camera (essential for a For Honor-style game).
3. **Mesh loading** — load a `.obj` character blockout instead of a cube.
4. **Game loop split** — C++ runs frame timing; OCaml receives `delta_time` and returns game state.
5. **Vulkan** — if you outgrow OpenGL, swap the renderer backend behind the same engine API.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `cmake` not found | Add CMake to PATH or use the VS CMake component |
| OpenGL function load fails | Update GPU drivers; need OpenGL 3.3+ |
| GLFW fetch fails | Ensure Git is installed and you have internet |
| OCaml link errors | Re-run `cmake -B build` so `link_flags.sexp` is regenerated |
| Black window | Run from a machine with a discrete GPU / updated drivers |

## License

Your project — add a license when you're ready.
