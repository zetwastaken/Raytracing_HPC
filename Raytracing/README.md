# Ray Tracing Demo

CPU path tracer written in C++17 for a Cornell-box style scene. The project renders a small interior lit by an emissive light source and writes the result to a PNG file.

## Build & Run
- `cmake -S Raytracing -B build/build-release -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build/build-release`
- `./build/build-release/Raytracing`

The program produces `render.png` in the repository root. Debug builds are available with the corresponding `build-debug` directory.

## Configuration
All runtime knobs live in `src/RenderConfig.h`. Key options:
- `image_width`, `aspect_ratio` – framebuffer geometry
- `samples_per_pixel` – anti-aliasing quality
- `output_path` – PNG destination

Scene details (room geometry, light placement) are controlled in `src/Scene.cpp` via `RoomLayout` and helper builders.

## Documentation
- High-level overview: `docs/overview.md`
- Rendering details: `docs/rendering.md`
- Scene setup reference: `docs/scene-setup.md`

Generate HTML API docs with Doxygen:
- Install Doxygen 1.9+ (e.g. `brew install doxygen` on macOS, `apt install doxygen graphviz` on Linux).
- `cmake --build build/build-release --target doc` *(once configured, see `Doxyfile`)*
- Output appears under `docs/html/index.html`

## Profiling & Symbols
- Release binaries embed line tables, so Instruments and other profilers can recover source locations.
- On macOS the build invokes `dsymutil` (controlled by `RAYTRACER_GENERATE_DSYM`, default `ON`); keep the resulting `.dSYM` folder next to the binary when profiling.
- Disable the automatic dSYM step via `-DRAYTRACER_GENERATE_DSYM=OFF` if you prefer to manage symbol bundles manually.

## Repository Layout
- `src/` – core engine (camera, materials, renderer, scene)
- `render.png` – latest rendered image
- `docs/` – Markdown guides and generated API documentation
- `Doxyfile` – Doxygen configuration

## Contributing
Please keep public headers documented with Doxygen comments and update the Markdown guides when adding new features or configuration options. When in doubt, regenerate the docs and ensure no warnings are emitted.
