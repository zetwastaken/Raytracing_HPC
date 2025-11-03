# Architecture & Design Decisions

## Table of Contents
1. [System Overview](#system-overview)
2. [Core Components](#core-components)
3. [Design Decisions](#design-decisions)
4. [Data Flow](#data-flow)
5. [Performance Considerations](#performance-considerations)

---

## System Overview

This is a **CPU-based Monte Carlo path tracer** built in C++17. The architecture follows a modular design where each component has a single, well-defined responsibility. The system implements physically-based rendering using:

- **Backward ray tracing**: Rays are traced from the camera into the scene
- **Monte Carlo integration**: Multiple samples per pixel reduce noise
- **Recursive path tracing**: Rays bounce through the scene gathering light
- **Direct illumination**: Point lights provide immediate lighting on diffuse surfaces

### Why This Architecture?

1. **Simplicity**: Each module is independent and easy to understand
2. **Extensibility**: New materials, objects, or light types can be added without modifying existing code
3. **Educational value**: The code structure mirrors standard ray tracing textbooks
4. **Testability**: Components can be tested in isolation

---

## Core Components

### 1. Vec3 - The Foundation (`Vec3.h`, `Vec3.cpp`)

**Purpose**: Unified 3D vector representation for positions, directions, and colors.

**Design Decision**: Use a single class for multiple purposes rather than separate Point3D, Direction3D, and Color classes.

**Rationale**:
- Reduces code duplication (all three concepts need the same math operations)
- Type aliases (`using Point3 = Vec3`) provide semantic clarity
- Simpler implementation and fewer bugs
- Standard in computer graphics (used by GLSL, Unity, etc.)

**Key Operations**:
```cpp
// Addition/subtraction for position and direction math
Vec3 operator+(const Vec3& a, const Vec3& b);

// Dot product for projections, angles, lighting calculations
double dot(const Vec3& a, const Vec3& b);

// Component-wise multiplication (Hadamard product) for color blending
Vec3 operator*(const Vec3& a, const Vec3& b);

// Normalization for direction vectors
Vec3 unit_vector(const Vec3& v);
```

**Trade-offs**:
- ✅ Less code, fewer bugs
- ✅ Familiar to graphics programmers
- ❌ No compile-time type safety (can't prevent adding a color to a position)
- ❌ RGB stored as doubles (uses more memory than bytes, but needed for HDR)

---

### 2. Ray - Light Propagation (`Ray.h`)

**Purpose**: Represents a half-line in 3D space (origin + direction).

**Design Decision**: Store origin and direction, compute points parametrically.

```cpp
Point3 at(double t) const { return origin_point + t * direction_vector; }
```

**Rationale**:
- Parametric form `P(t) = O + t*D` is standard in ray tracing
- Simple intersection tests with geometry
- `t` value naturally encodes distance along the ray

**Why not store length?**
- Rays are infinite half-lines in ray tracing
- Length is determined by intersection, not by the ray itself
- Keeps the class minimal and focused

---

### 3. Camera - View Generation (`Camera.h`)

**Purpose**: Generate primary rays from the camera through each pixel.

**Design Decision**: Pinhole camera model with explicit viewport definition.

```cpp
Camera(double aspect_ratio, double viewport_height = 2.0, double focal_length = 1.0)
```

**Rationale**:
- **Pinhole model**: Simplest camera (no lens distortion, depth of field, etc.)
- **Fixed position**: Camera at origin looking down -Z axis (standard OpenGL/DirectX convention)
- **Explicit viewport**: Direct control over field of view without angles

**Why these defaults?**
- `viewport_height = 2.0`: Gives 90° vertical FOV at focal_length=1.0
- `focal_length = 1.0`: Natural unit distance
- Position at origin: Simplifies math, scene transforms can move the world instead

**Future Extensions**:
- Depth of field (add lens radius, focus distance)
- Camera transforms (rotation, position)
- Orthographic projection (for technical drawings)

---

### 4. Material System (`Material.h`)

**Purpose**: Define how light interacts with surfaces.

**Design Decision**: Polymorphic material base class with scatter method.

```cpp
class Material {
    virtual bool scatter(const Ray& ray_in, const HitRecord& hit,
                        ScatterRecord& record) const = 0;
};
```

**Why virtual methods?**
- Different materials need completely different scattering logic
- Runtime polymorphism allows heterogeneous object lists
- Clean separation of concerns (geometry vs material)

**Material Types**:

#### **Matte (Lambertian Diffuse)**
```cpp
Vec3 scatter_direction = random_cosine_direction(hit_info.surface_normal);
```

**Decision**: Use cosine-weighted hemisphere sampling instead of uniform random.

**Why?**
- ✅ Matches physical reality (Lambert's cosine law)
- ✅ Dramatically reduces noise
- ✅ Converges faster to correct solution
- Standard technique in production renderers

#### **Reflective (Metal)**
```cpp
Vec3 reflected = reflect(ray_direction, normal) + fuzziness * random_unit_vector();
```

**Decision**: Add fuzziness parameter for rough metals.

**Why?**
- Allows smooth transition from mirror to brushed metal
- More artistically controllable
- Simple approximation of microfacet roughness

#### **Transparent (Dielectric)**
```cpp
bool cannot_refract = refraction_ratio * sin_theta > 1.0;
if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_double()) {
    direction = reflect(...);
} else {
    direction = refract(...);
}
```

**Decision**: Use Schlick's approximation for Fresnel, random choice for reflection vs refraction.

**Why?**
- **Schlick's approximation**: Fast, good enough for most cases (vs full Fresnel equations)
- **Stochastic selection**: Simpler than splitting rays, works with Monte Carlo
- **Total internal reflection**: Properly handled by cannot_refract check

---

### 5. Hittable System (`Hittable.h`, `HittableList.h`)

**Purpose**: Abstract interface for ray-object intersection.

**Design Decision**: Pure virtual interface with HitRecord output parameter.

```cpp
virtual bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const = 0;
```

**Why output parameter instead of return value?**
- Avoids allocation/copying of HitRecord
- C++ performance idiom (pass by reference)
- Clear ownership semantics

**HitRecord Design**:
```cpp
struct HitRecord {
    Point3 hit_point;
    Vec3 surface_normal;
    double distance_from_ray;
    bool is_front_face;
    std::shared_ptr<Material> material_ptr;
    
    void set_face_normal(const Ray& ray, const Vec3& outward_normal);
};
```

**Decision**: Store front-face information and always outward-facing normals.

**Why?**
- Materials need to know ray direction relative to surface
- Dielectrics care about inside vs outside
- Avoids confusion about normal direction in material code

**HittableList Implementation**:
```cpp
bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override {
    double closest_so_far = t_max;
    for (const auto& object : objects) {
        if (object->hit(ray, t_min, closest_so_far, temp_rec)) {
            closest_so_far = temp_rec.distance_from_ray;
            rec = temp_rec;
        }
    }
    return hit_anything;
}
```

**Decision**: Linear search, updating closest hit.

**Trade-offs**:
- ✅ Simple, no bugs
- ✅ Good cache locality
- ❌ O(n) per ray (BVH would be O(log n))
- **Acceptable for small scenes** (< 100 objects)

---

### 6. Renderer - The Core Algorithm (`Renderer.cpp`)

**Purpose**: Orchestrate the rendering process.

**Design Decision**: Separate pixel sampling from ray color calculation.

```cpp
std::vector<unsigned char> render_image(config, camera, scene, max_depth)
    └─> Color render_pixel(col, row, ...)
           └─> Color calculate_ray_color(ray, scene, depth)
```

**Why this structure?**
- **Separation of concerns**: Sampling strategy vs lighting calculation
- **Parallelization ready**: Each pixel is independent
- **Testability**: Can test ray_color independently

#### **Sampling Strategy**

```cpp
for (int sample = 0; sample < samples_per_pixel; ++sample) {
    double u = (col + random_double()) / (width - 1);
    double v = (row + random_double()) / (height - 1);
    Ray ray = camera.get_ray(u, v);
    color += calculate_ray_color(ray, scene, max_depth);
}
color /= samples_per_pixel;
```

**Decision**: Jittered sampling (random offset within pixel).

**Why?**
- **Anti-aliasing**: Smooths jagged edges
- **Stochastic sampling**: Foundation of Monte Carlo integration
- **Simple**: More complex patterns (stratified, Halton) offer marginal improvement

#### **Ray Color Calculation**

```cpp
Color calculate_ray_color(const Ray& ray, const Scene& scene, int depth) {
    if (depth <= 0) return BLACK;
    
    if (hit_object) {
        Color direct = compute_diffuse_lighting(scene, hit_info);
        
        if (material.scatter(ray, hit_info, scatter_record)) {
            return direct + attenuation * calculate_ray_color(scattered_ray, scene, depth-1);
        }
        return direct;
    }
    
    return calculate_sky_color(ray);
}
```

**Key Design Decisions**:

1. **Depth limiting**: Stop at max_depth to prevent infinite recursion
   - Why not Russian roulette? Simpler, more predictable performance

2. **Direct + Indirect split**: 
   ```cpp
   total_light = direct_from_lights + scattered_indirect
   ```
   - Diffuse materials get explicit light sampling (reduces noise)
   - Reflective materials only use indirect (correct for mirrors)

3. **Recursive path tracing**: Each bounce spawns one new ray
   - Simple, elegant
   - Unbiased estimator
   - Converges to correct solution with enough samples

#### **Direct Lighting**

```cpp
Color compute_diffuse_lighting(const Scene& scene, const HitRecord& hit) {
    for (const auto& light : scene.lights) {
        Vec3 to_light = light.position - hit_point;
        Vec3 light_dir = unit_vector(to_light);
        
        // Shadow test
        Ray shadow_ray(hit_point + bias * normal, light_dir);
        if (scene.objects.hit(shadow_ray, bias, distance_to_light, shadow_hit))
            continue;  // In shadow
        
        // Accumulate light
        double n_dot_l = max(0, dot(normal, light_dir));
        accumulated += n_dot_l * light.intensity / distance_squared;
    }
    return accumulated;
}
```

**Decisions**:
- **Point lights**: Simpler than area lights, good enough for this demo
- **Shadow bias**: Prevents self-intersection (numerical precision issue)
- **Inverse square falloff**: Physical accuracy
- **Dot product**: Lambertian cosine term

---

### 7. Scene Construction (`Scene.cpp`)

**Purpose**: Build the world geometry and lighting.

**Design Decision**: Procedural scene generation with RoomLayout parameters.

```cpp
Scene create_scene(const RoomLayout& layout, std::vector<Light> lights);
```

**Why procedural?**
- **Parametric control**: Easy to vary room size
- **Self-documenting**: Code shows exactly what's in the scene
- **No file I/O**: Simpler, fewer dependencies

**RoomLayout Structure**:
```cpp
struct RoomLayout {
    double half_width, half_depth;
    double floor_y, ceiling_y;
    double back_wall_z, front_opening_z;
};
```

**Decision**: Store half-extents, not full dimensions.

**Why?**
- Symmetric rooms easier to build (objects relative to center)
- Reduces arithmetic errors
- Common pattern in physics engines

---

## Design Decisions

### Why C++17?

**Chosen features**:
- `std::shared_ptr`: Shared ownership of materials
- `std::vector`: Dynamic arrays for objects, lights, pixel data
- `std::make_shared`: Safe memory management
- Structured bindings (not heavily used, but available)

**Why not C++20/23?**
- Broader compiler support (works on older systems)
- Sufficient for this project's needs
- No benefits from modules, ranges, coroutines here

### Why Monte Carlo Path Tracing?

**Alternatives considered**:
1. **Whitted ray tracing**: Simpler but can't do soft shadows, caustics
2. **Photon mapping**: More complex, requires two passes
3. **Radiosity**: Good for diffuse, bad for specular

**Why path tracing wins**:
- ✅ Handles all light transport paths
- ✅ Physically correct (unbiased)
- ✅ Simple to implement
- ✅ Naturally parallelizable
- ❌ Noisy (needs many samples)
- ❌ Slow (CPU-bound)

### Memory Management

**Decision**: `std::shared_ptr` for materials, raw pointers avoided.

**Why?**
- **Shared ownership**: Multiple objects can share same material
- **Automatic cleanup**: No manual delete needed
- **Exception safety**: RAII guarantees

**Why not unique_ptr?**
- Materials are shared across objects
- Copy semantics would be complex

### Output Format

**Decision**: PNG output via lodepng library.

**Why PNG?**
- Lossless (unlike JPEG)
- Widely supported
- Single-file header library (lodepng)

**Why not EXR/HDR?**
- HDR would preserve full dynamic range
- But PNG is simpler, more universally viewable
- Tone mapping happens before write

---

## Data Flow

### Complete Render Pipeline

```
1. main.cpp
   ├─> Creates RenderConfig (image size, samples)
   ├─> Creates Camera (viewport setup)
   ├─> Creates Scene (geometry + lights)
   └─> Calls render_image()

2. render_image()
   ├─> For each pixel (row, col):
   │   └─> render_pixel()
   │       ├─> For each sample:
   │       │   ├─> Generate jittered ray
   │       │   └─> calculate_ray_color()
   │       │       ├─> Test scene intersection
   │       │       ├─> If hit:
   │       │       │   ├─> compute_diffuse_lighting() [direct]
   │       │       │   ├─> material.scatter() [get next ray]
   │       │       │   └─> calculate_ray_color() [recursive, indirect]
   │       │       └─> If miss: calculate_sky_color()
   │       └─> Average samples
   └─> write_color() [gamma correction]

3. save_image()
   └─> PNG encoding via lodepng
```

### Ray Lifecycle

```
Camera Ray
    ↓
Primary Intersection → Material::scatter() → Secondary Ray
    ↓                                              ↓
HitRecord                                   Recursive call
    ↓                                              ↓
Direct Lighting                            Indirect Lighting
    ↓                                              ↓
    └──────────────── Sum ────────────────────────┘
                       ↓
                 Final Color
```

---

## Performance Considerations

### Current Bottlenecks

1. **Linear scene traversal**: O(n) per ray
   - **Solution**: BVH (Bounding Volume Hierarchy) → O(log n)
   - **Trade-off**: Added complexity, slower for tiny scenes

2. **No parallelization**: Single-threaded
   - **Solution**: OpenMP or std::thread (pixels are independent)
   - **Expected speedup**: Linear with cores (4-16x)

3. **Recursive function calls**: Stack overhead
   - **Solution**: Iterative path tracing with explicit stack
   - **Benefit**: Marginal (2-5%)

### Why These Weren't Implemented

This is an **educational/experimental** project where **clarity > performance**:
- Simple code is easier to understand and modify
- Current speed is acceptable for development iteration
- Optimizations would obscure the core algorithms

### Optimization Roadmap (if needed)

**Low-hanging fruit** (big gains, small effort):
1. Compile with `-O3` optimizations ✅ (already done in Release build)
2. Parallelize pixel loop with OpenMP (1 line change)
3. Reduce samples_per_pixel for preview renders

**Medium effort**:
4. BVH acceleration structure
5. SIMD vectorization (AVX2)
6. Fast random number generator (PCG instead of std::mt19937)

**High effort**:
7. GPU port (CUDA/OpenCL)
8. Adaptive sampling (more samples for noisy pixels)
9. Denoising (AI-based or bilateral filter)

---

## Extending the System

### Adding a New Material

1. Create class inheriting from `Material`
2. Implement `scatter()` method
3. Optionally implement `is_diffuse()` and `base_color()`
4. Use in scene construction

Example: Emissive material
```cpp
class Emissive : public Material {
    Color emission;
    
    bool scatter(...) const override {
        return false;  // Absorb rays, don't scatter
    }
    
    Color emitted() const {
        return emission;  // Add to renderer
    }
};
```

### Adding a New Shape

1. Create class inheriting from `Hittable`
2. Implement `hit()` method
   - Solve ray-surface intersection
   - Populate HitRecord if hit
3. Add to scene in `create_scene()`

Example: Triangle
```cpp
class Triangle : public Hittable {
    Point3 v0, v1, v2;
    std::shared_ptr<Material> mat;
    
    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override {
        // Möller-Trumbore intersection algorithm
        // ...
    }
};
```

### Adding a New Light Type

Currently only point lights exist. To add area lights:

1. Modify `Light` struct or create hierarchy
2. Update `compute_diffuse_lighting()` to sample area
3. Consider adding to `Hittable` list as emissive geometry

---

## Summary

This ray tracer demonstrates fundamental rendering concepts with a focus on:
- **Clean architecture**: Each component has a single responsibility
- **Physical correctness**: Follows light transport equations
- **Extensibility**: Easy to add new materials, objects, features
- **Educational value**: Code structure mirrors theory

The design favors **simplicity and clarity** over raw performance, making it ideal for learning, experimentation, and as a foundation for more advanced techniques.
