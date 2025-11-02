# Rendering Pipeline

## Sampling Strategy
- Each pixel fires `samples_per_pixel` camera rays with jittered sub-pixel offsets.
- Russian roulette termination is not used; recursion stops when depth reaches `max_depth`.
- Diffuse lighting adds direct illumination from visible point lights atop recursive scattering.

## Shading Model
- **Lambertian**: returns cosine-weighted hemisphere samples using random unit vectors.
- **Metal**: reflects rays with optional fuzziness for blurred highlights.
- **Dielectric**: computes refraction by Snell's law and uses Schlick's approximation for Fresnel blending.
- **Emissive**: contributes radiance directly without scattering new rays.

See `src/Material.h` for scatter implementations.

## Sky Gradient
Primary rays that miss all geometry return a vertical gradient via `calculate_sky_color`. Tweak the top/bottom colors in `src/Renderer.cpp` to change the mood of the scene.

## Tone Mapping
- Sample contributions accumulate in linear color space.
- The final color is divided by `samples_per_pixel`, clamped, gamma-corrected (`gamma = 2.0`), and quantized to 8-bit unsigned integers in `write_color`.

## Tips
- Raise `samples_per_pixel` for smoother glossy reflections.
- Increase `max_depth` cautiously; each bounce multiplies runtime.
- For test renders, drop `image_width` and sample count dramatically to iterate quickly.
