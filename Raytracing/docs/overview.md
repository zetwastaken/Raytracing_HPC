# Overview

This ray tracer renders a Cornell-box inspired room using recursive ray-tracing, Monte Carlo sampling, and physically based shading primitives. The program is intentionally compact and self-contained, making it ideal for experimentation and high-performance computing coursework.

## Architecture
- **Entry point** (`src/main.cpp`) configures the render, builds the scene, invokes the renderer, and exports a PNG.
- **Renderer** (`src/Renderer.{h,cpp}`) handles camera ray generation, recursive shading (`calculate_ray_color`), and sample accumulation.
- **Scene graph** (`src/Scene.{h,cpp}`) assembles hittable geometry (spheres, rectangles, boxes) and light sources.
- **Math utilities** (`src/Vec3.*`, `src/Utils.*`) provide vector algebra, random sampling, and interval helpers.
- **Output** (`src/PngWriter.*`) wraps `lodepng` to persist RGB buffers.

![Sample render](../render.png)

## Key Data Flow
1. `RenderConfig` encodes image dimensions, samples per pixel, and output path.
2. `Camera` builds primary rays per pixel.
3. `Renderer` traces each ray, collecting emitted light and BRDF contributions.
4. Accumulated samples are gamma-corrected and written to `render.png`.

## Performance Notes
- Anti-aliasing cost scales with `samples_per_pixel`.
- Recursive depth (`max_depth` in `main.cpp`) controls path termination.
- Compile with `Release` for optimized builds; debug mode disables inlining to ease step-through debugging.

## Extending
- Add new geometry by implementing `Hittable::hit`.
- Introduce materials by extending `Material` helpers and branching in `scatter`.
- Register additional lights in `Scene::lights` or adjust `RoomLayout` to move existing fixtures.
