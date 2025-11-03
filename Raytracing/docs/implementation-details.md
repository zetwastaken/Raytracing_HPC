# Implementation Details

## Table of Contents
1. [Mathematical Foundations](#mathematical-foundations)
2. [Ray-Object Intersection](#ray-object-intersection)
3. [Lighting Calculations](#lighting-calculations)
4. [Sampling and Random Numbers](#sampling-and-random-numbers)
5. [Color Management](#color-management)
6. [Numerical Stability](#numerical-stability)

---

## Mathematical Foundations

### The Rendering Equation

The core of physically-based rendering:

```
L_o(p, ω_o) = L_e(p, ω_o) + ∫_Ω f_r(p, ω_i, ω_o) L_i(p, ω_i) (n · ω_i) dω_i
```

**In plain English**:
- **L_o**: Light leaving point `p` in direction `ω_o` (what we see)
- **L_e**: Light emitted by surface (for light sources)
- **Integral**: Sum light from all incoming directions `ω_i`
- **f_r**: BRDF (Bidirectional Reflectance Distribution Function) - how light scatters
- **L_i**: Incoming light (recursive - comes from other surfaces)
- **(n · ω_i)**: Cosine term (surface receives less light at grazing angles)

**How our code implements this**:

```cpp
Color calculate_ray_color(const Ray& ray, const Scene& scene, int depth) {
    // L_e - emitted light (implicitly zero for non-emissive materials)
    
    // Direct lighting (sample lights directly)
    Color direct = compute_diffuse_lighting(scene, hit_info);
    
    // Indirect lighting (Monte Carlo integration of the rendering equation)
    if (material.scatter(ray, hit_info, scatter_record)) {
        // f_r * L_i * (n · ω_i) approximated by Monte Carlo:
        Color indirect = attenuation * calculate_ray_color(scattered_ray, scene, depth-1);
        return direct + indirect;
    }
}
```

### Monte Carlo Integration

**Problem**: The integral in the rendering equation has infinite directions to consider.

**Solution**: Monte Carlo - estimate by random sampling.

```
∫ f(x) dx ≈ (1/N) Σ f(x_i) / p(x_i)
```

**In the code**:
```cpp
// Sample N rays per pixel
for (int sample = 0; sample < samples_per_pixel; ++sample) {
    accumulated_color += calculate_ray_color(ray, scene, max_depth);
}
final_color = accumulated_color / samples_per_pixel;
```

**Why this works**:
- Each sample is an independent estimate of the integral
- By the Law of Large Numbers, the average converges to the true value
- Variance decreases as O(1/√N) - doubling quality requires 4x samples

---

## Ray-Object Intersection

### Sphere Intersection

**Math**: Solve `(P - C) · (P - C) = r²` where `P = O + t*D`

**Derivation**:
```
(O + tD - C) · (O + tD - C) = r²
D·D t² + 2D·(O-C) t + (O-C)·(O-C) - r² = 0

Quadratic formula:
a = D·D
b = 2 D·(O-C)
c = (O-C)·(O-C) - r²

discriminant = b² - 4ac
t = (-b ± √discriminant) / 2a
```

**Implementation** (`Sphere.h`):
```cpp
bool Sphere::hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const {
    Vec3 oc = ray.origin() - center;
    double a = ray.direction().length_squared();
    double half_b = dot(oc, ray.direction());
    double c = oc.length_squared() - radius * radius;
    
    double discriminant = half_b * half_b - a * c;
    if (discriminant < 0) return false;  // No intersection
    
    // Find nearest root in valid range
    double sqrtd = std::sqrt(discriminant);
    double root = (-half_b - sqrtd) / a;
    if (root < t_min || root > t_max) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || root > t_max)
            return false;
    }
    
    rec.distance_from_ray = root;
    rec.hit_point = ray.at(root);
    Vec3 outward_normal = (rec.hit_point - center) / radius;
    rec.set_face_normal(ray, outward_normal);
    rec.material_ptr = material;
    
    return true;
}
```

**Key decisions**:
- Use `half_b` instead of `b`: Reduces arithmetic (2× less multiplications)
- Check both roots: Need the closest valid hit
- Normalize normal by dividing by radius (faster than `unit_vector()`)

### Axis-Aligned Rectangle Intersection

**Math**: Plane equation + bounds check

For an XY-aligned rectangle at z = k:
1. Find t where ray hits plane: `t = (k - ray.origin.z) / ray.direction.z`
2. Calculate hit point: `P = ray.at(t)`
3. Check bounds: `x0 ≤ P.x ≤ x1` and `y0 ≤ P.y ≤ y1`

**Implementation** (`AxisAlignedRect.cpp`):
```cpp
bool XYRect::hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const {
    double t = (k - ray.origin().z()) / ray.direction().z();
    
    if (t < t_min || t > t_max)
        return false;
    
    Point3 hit_point = ray.at(t);
    double x = hit_point.x();
    double y = hit_point.y();
    
    if (x < x0 || x > x1 || y < y0 || y > y1)
        return false;
    
    rec.distance_from_ray = t;
    rec.hit_point = hit_point;
    Vec3 outward_normal = Vec3(0, 0, invert_normal ? -1 : 1);
    rec.set_face_normal(ray, outward_normal);
    rec.material_ptr = material;
    
    return true;
}
```

**Edge case**: Division by zero when ray is parallel to plane
- Check is implicit: if `direction.z == 0`, `t` becomes ±∞
- Out-of-range `t` fails the bounds check

### Box Intersection

**Method**: Box is 6 axis-aligned rectangles

**Implementation** (`Box.h`):
```cpp
class Box : public Hittable {
    HittableList faces;
    
    Box(Point3 min, Point3 max, shared_ptr<Material> mat) {
        // Create 6 rectangles for each face
        faces.add(make_shared<XYRect>(min.x(), max.x(), min.y(), max.y(), max.z(), mat));
        // ... 5 more faces
    }
    
    bool hit(...) const override {
        return faces.hit(...);  // Delegate to face list
    }
};
```

**Why this approach?**
- Simple, reuses existing rectangle code
- Alternative: Slab method (intersect 3 pairs of parallel planes)
- Trade-off: 6 intersection tests vs 3, but code reuse wins for simplicity

---

## Lighting Calculations

### Lambertian Diffuse (Matte)

**Physical model**: Surface scatters light uniformly in all directions (weighted by cosine).

**BRDF**: `f_r = albedo / π`

**Cosine-weighted hemisphere sampling**:

Instead of uniform random directions, sample with probability `p(ω) ∝ cos(θ)`:

```cpp
Vec3 random_cosine_direction(const Vec3& normal) {
    // Create orthonormal basis (ONB) around normal
    Vec3 w = unit_vector(normal);
    Vec3 a = (fabs(w.x()) > 0.9) ? Vec3(0,1,0) : Vec3(1,0,0);
    Vec3 v = unit_vector(cross(w, a));
    Vec3 u = cross(w, v);
    
    // Sample unit disk, then project to hemisphere
    double r1 = random_double();
    double r2 = random_double();
    double phi = 2 * M_PI * r1;
    
    double x = cos(phi) * sqrt(r2);
    double y = sin(phi) * sqrt(r2);
    double z = sqrt(1 - r2);
    
    return unit_vector(x*u + y*v + z*w);
}
```

**Why cosine-weighted?**
- Matches the `(n · ω)` term in the rendering equation
- Importance sampling: spend more samples where they matter
- Reduces variance → faster convergence → less noise

**Comparison**:
- Uniform sampling: ~1000 samples for clean image
- Cosine-weighted: ~100 samples for same quality
- **10x speedup** for same visual quality!

### Reflective (Metal)

**Perfect mirror reflection**:

```
R = D - 2(D·N)N
```

Where:
- `D`: Incident ray direction
- `N`: Surface normal
- `R`: Reflected direction

**Implementation**:
```cpp
Vec3 reflect(const Vec3& incoming, const Vec3& normal) {
    return incoming - 2 * dot(incoming, normal) * normal;
}
```

**Fuzzy reflection** (rough metal):
```cpp
Vec3 reflected = reflect(direction, normal) + fuzziness * random_unit_vector();
```

**Physical interpretation**:
- `fuzziness = 0`: Perfect mirror (smooth surface)
- `fuzziness > 0`: Microfacet roughness (brushed metal)
- Each ray hits slightly different microfacet orientations

### Transparent (Dielectric)

**Snell's Law**: `n₁ sin(θ₁) = n₂ sin(θ₂)`

**Refraction derivation**:
```
Given: incident ray I, normal N, refraction ratio η = n₁/n₂
Goal: Find refracted ray R

Decompose I into parallel and perpendicular components:
I = I_parallel + I_perp

R_perp = η (I + cos(θ) N)
R_parallel = -√(1 - |R_perp|²) N

R = R_perp + R_parallel
```

**Implementation**:
```cpp
Vec3 refract(const Vec3& uv, const Vec3& n, double etai_over_etat) {
    double cos_theta = fmin(dot(-uv, n), 1.0);
    Vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    Vec3 r_out_parallel = -sqrt(fabs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}
```

**Fresnel effect** (Schlick's approximation):
```
R(θ) = R₀ + (1 - R₀)(1 - cos(θ))⁵

Where R₀ = ((n₁ - n₂) / (n₁ + n₂))²
```

**Implementation**:
```cpp
double reflectance(double cosine, double ref_idx) {
    double r0 = (1 - ref_idx) / (1 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cosine), 5);
}
```

**Why Schlick's approximation?**
- Physically accurate enough (< 1% error)
- Much faster than exact Fresnel equations
- Industry standard (used in games, films)

**Total internal reflection**:
```cpp
bool cannot_refract = etai_over_etat * sin_theta > 1.0;
if (cannot_refract) {
    direction = reflect(...);  // Must reflect
}
```

**Physical explanation**: When going from dense→rare medium (glass→air), light can't escape at grazing angles.

### Direct Illumination

**Point light contribution**:
```
L = (light_intensity / distance²) * max(0, N · L)
```

**Implementation**:
```cpp
Color compute_diffuse_lighting(const Scene& scene, const HitRecord& hit) {
    for (const auto& light : scene.lights) {
        Vec3 to_light = light.position - hit_point;
        double distance_squared = to_light.length_squared();
        Vec3 light_dir = unit_vector(to_light);
        
        // Shadow test
        Ray shadow_ray(hit_point + bias * normal, light_dir);
        if (scene.objects.hit(shadow_ray, bias, sqrt(distance_squared), shadow_hit))
            continue;  // In shadow
        
        double n_dot_l = max(0.0, dot(normal, light_dir));
        accumulated += n_dot_l * light.intensity / distance_squared;
    }
    return accumulated;
}
```

**Key points**:
- **Inverse square law**: Physical light attenuation
- **Shadow bias**: Offset ray start to avoid self-intersection
- **Max(0, N·L)**: Surfaces don't receive light from behind

---

## Sampling and Random Numbers

### Random Number Generation

**Implementation** (`Utils.cpp`):
```cpp
double random_double() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<double> dis(0.0, 1.0);
    return dis(gen);
}
```

**Why Mersenne Twister (mt19937)?**
- Good statistical properties (passes Diehard tests)
- Fast enough for CPU rendering
- Standard library (no external dependencies)

**Why static?**
- Avoids re-seeding on every call
- Thread-local storage would be needed for parallelization

**Alternatives considered**:
- `rand()`: Poor quality, patterns visible in renders
- PCG: Faster, better quality, but external library
- Sobol sequences: Low-discrepancy, but complex integration

### Jittered Sampling

**Without jittering** (regular grid):
```cpp
double u = col / (width - 1);  // Always hits pixel center
double v = row / (height - 1);
```
**Problem**: Creates aliasing patterns, Moiré effects

**With jittering** (random offset):
```cpp
double u = (col + random_double()) / (width - 1);
double v = (row + random_double()) / (height - 1);
```
**Benefit**: Converts aliasing → noise, which is less objectionable to human vision

**Why not stratified sampling?**
- More complex (divide pixel into sub-regions)
- Marginal improvement for high sample counts
- Simple jittering is good enough

### Random Direction Generation

**Uniform random on unit sphere**:
```cpp
Vec3 random_unit_vector() {
    // Marsaglia method: rejection sampling
    while (true) {
        Vec3 p = Vec3(random(-1,1), random(-1,1), random(-1,1));
        double len_sq = p.length_squared();
        if (len_sq <= 1.0 && len_sq > 0.0001)
            return p / sqrt(len_sq);
    }
}
```

**Why rejection sampling?**
- Simple to implement
- No trigonometry (faster than spherical coordinates)
- Expected iterations: π/6 ≈ 1.9 (very efficient)

**Alternative** (spherical coordinates):
```cpp
double theta = 2 * PI * random_double();
double phi = acos(2 * random_double() - 1);
return Vec3(sin(phi)*cos(theta), sin(phi)*sin(theta), cos(phi));
```
**Trade-off**: No rejection, but trig functions are slower

---

## Color Management

### Color Space

**Working space**: Linear RGB
- Physically correct light calculations
- Allows proper blending and accumulation

**Output space**: sRGB (gamma-corrected)
- Matches display expectations
- Corrects for human perception (we see darker colors better)

### Gamma Correction

**The problem**: Monitors display color non-linearly.

**Solution**: Apply gamma curve before display.

```cpp
void write_color(std::vector<unsigned char>& buffer, const Color& pixel_color) {
    // Clamp to valid range
    double r = std::clamp(pixel_color.x(), 0.0, 1.0);
    double g = std::clamp(pixel_color.y(), 0.0, 1.0);
    double b = std::clamp(pixel_color.z(), 0.0, 1.0);
    
    // Gamma correction (gamma = 2.0)
    r = std::sqrt(r);
    g = std::sqrt(g);
    b = std::sqrt(b);
    
    // Quantize to 8-bit
    buffer.push_back(static_cast<unsigned char>(256 * r));
    buffer.push_back(static_cast<unsigned char>(256 * g));
    buffer.push_back(static_cast<unsigned char>(256 * b));
}
```

**Why sqrt (gamma=2.0)?**
- Fast to compute
- Close enough to sRGB (gamma≈2.2)
- Industry standard for simple renderers

**Why not exact sRGB?**
```cpp
// Exact sRGB transform (more complex):
if (c <= 0.0031308)
    return 12.92 * c;
else
    return 1.055 * pow(c, 1/2.4) - 0.055;
```
- Negligible visual difference
- Slower (pow() is expensive)
- Simple sqrt is good enough

### HDR and Tone Mapping

**Current system**: Soft clamp (values > 1.0 become 1.0)

**Problem**: Bright lights get clipped (lost detail)

**Future improvement**: Tone mapping
```cpp
// Reinhard tone mapping
Color tone_map_reinhard(const Color& hdr) {
    return hdr / (hdr + Color(1,1,1));
}

// ACES filmic
Color tone_map_aces(const Color& hdr) {
    // ... (more complex curve used in movies)
}
```

**Why not implemented?**
- Scene is carefully lit to stay in [0,1] range
- Adding tone mapping is trivial extension
- Current approach works for educational purposes

---

## Numerical Stability

### Shadow Acne

**Problem**: Rays hit the surface they started from due to floating-point error.

**Visual artifact**: Surfaces appear speckled with shadow dots.

**Solution**: Offset ray origin slightly along normal.

```cpp
constexpr double shadow_bias = 0.001;
Ray shadow_ray(hit_point + shadow_bias * normal, light_dir);
```

**Why 0.001?**
- Large enough to avoid self-intersection (float precision ≈ 1e-6)
- Small enough to not cause light leaks through thin objects
- Scene-dependent (might need adjustment for very large/small scales)

### Degenerate Vectors

**Problem**: Random scattering might produce zero vector (rare but possible).

**Solution**: Fall back to surface normal.

```cpp
if (is_near_zero(scatter_direction)) {
    scatter_direction = hit_info.surface_normal;
}

bool is_near_zero(const Vec3& v) {
    const double epsilon = 1e-8;
    return (fabs(v.x()) < epsilon) && 
           (fabs(v.y()) < epsilon) && 
           (fabs(v.z()) < epsilon);
}
```

### Division by Zero

**Ray-plane intersection**:
```cpp
double t = (k - ray.origin().z()) / ray.direction().z();
```

**What if `direction.z == 0`?**
- Result is ±infinity (IEEE 754 standard)
- Subsequent range check fails naturally
- No explicit check needed (elegant!)

**Refraction calculation**:
```cpp
double cos_theta = fmin(dot(-uv, n), 1.0);
```

**Why fmin?**
- Dot product might be 1.0000001 due to floating-point error
- `acos(1.0000001)` = NaN
- Clamping ensures valid input to subsequent calculations

### Precision Considerations

**Why double instead of float?**
- Camera math accumulates errors (float precision ≈ 7 digits)
- 1920x1080 image: coordinates have ~1000x range
- float: ~4 bytes, double: 8 bytes (worth the 2x memory for accuracy)

**Where float would be acceptable**:
- Final color buffer (8-bit output anyway)
- Intermediate calculations with small ranges

**Trade-off**: Memory vs accuracy
- Current choice: accuracy (easier to debug, more robust)
- GPU renderers often use float (speed + massive parallelism)

---

## Performance Analysis

### Bottleneck Profiling

**Theoretical complexity** (per frame):
```
Time = pixels × samples × depth × objects

Example:
800×600 × 100 samples × 50 depth × 20 objects
= 48,000,000 × 50 × 20
= 48 billion ray-object tests
```

**Where time is spent** (typical profile):
1. **Ray-object intersection**: ~60%
   - Linear scene search
   - Sphere intersection math

2. **Random number generation**: ~20%
   - Called every scatter, every sample
   - ~10 calls per ray on average

3. **Vector math**: ~15%
   - Dot products, normalization
   - Cache-friendly, but frequent

4. **Other**: ~5%
   - Material logic, color conversion, I/O

### Optimization Opportunities

**Easy wins** (implemented):
- Release build optimizations (`-O3`)
- Half-b optimization in sphere intersection
- Cosine-weighted sampling (10x noise reduction)

**Not implemented** (complexity vs benefit):
- BVH: 10-100x speedup, but 500+ lines of code
- SIMD: 2-4x speedup, but platform-specific
- Multithreading: 4-16x speedup, but synchronization complexity

**Why optimization was deferred**:
- Current performance acceptable for learning/development
- Clear code > fast code for this project's goals
- Easy to add later without changing architecture

---

## Summary

The implementation makes deliberate trade-offs:

**Favored**:
- ✅ Physical correctness
- ✅ Code clarity
- ✅ Numerical stability
- ✅ Standard algorithms

**Deferred**:
- ⏳ Maximum performance
- ⏳ Advanced sampling (stratified, blue noise)
- ⏳ Acceleration structures (BVH, KD-tree)

This creates a solid foundation that can be extended with optimizations when needed, without sacrificing the educational value of the codebase.
