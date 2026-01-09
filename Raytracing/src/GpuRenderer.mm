#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "GpuRenderer.h"
#include "AxisAlignedRect.h"
#include "Box.h"
#include "Sphere.h"
#include "Material.h"

#include <simd/simd.h>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

namespace {

struct GpuSphere {
    simd::float3 center;
    float radius;
    simd::float3 albedo;
    float fuzz;
    float ref_idx;
    uint32_t material_type; // 0 = diffuse, 1 = metal, 2 = dielectric
    uint32_t pad[2];
};

struct GpuRect {
    float a0;
    float a1;
    float b0;
    float b1;
    float k;
    uint32_t type;  // 0 = XY, 1 = XZ, 2 = YZ
    uint32_t flip;
    uint32_t pad;
    simd::float3 albedo;
    float fuzz;
    float ref_idx;
    uint32_t material_type; // 0 = diffuse, 1 = metal, 2 = dielectric
    uint32_t pad2[1];
};

struct GpuLight {
    simd::float3 position;
    float pad0;
    simd::float3 intensity;
    float pad1;
};

struct GpuCamera {
    simd::float3 origin;
    simd::float3 lower_left;
    simd::float3 horizontal;
    simd::float3 vertical;
};

struct SceneCounts {
    uint32_t sphere_count;
    uint32_t rect_count;
    uint32_t light_count;
};

simd::float3 to_float3(const Vec3& v) {
    return simd::make_float3(static_cast<float>(v.x()), static_cast<float>(v.y()), static_cast<float>(v.z()));
}

void append_rect_from_axis_rect(const AxisAlignedRect* rect, const std::shared_ptr<Material>& material, std::vector<GpuRect>& out) {
    GpuRect gpu_rect{};

    if (std::dynamic_pointer_cast<Matte>(material)) {
        gpu_rect.material_type = 0;
        gpu_rect.albedo = to_float3(material->base_color());
        gpu_rect.fuzz = 0.0f;
        gpu_rect.ref_idx = 1.0f;
    } else if (auto metal = std::dynamic_pointer_cast<Reflective>(material)) {
        gpu_rect.material_type = 1;
        gpu_rect.albedo = to_float3(material->base_color());
        gpu_rect.fuzz = static_cast<float>(metal->fuzziness);
        gpu_rect.ref_idx = 1.0f;
    } else if (auto glass = std::dynamic_pointer_cast<Transparent>(material)) {
        gpu_rect.material_type = 2;
        gpu_rect.albedo = to_float3(material->base_color());
        gpu_rect.fuzz = 0.0f;
        gpu_rect.ref_idx = static_cast<float>(glass->refractive_index);
    } else {
        gpu_rect.material_type = 0;
        gpu_rect.albedo = to_float3(material->base_color());
        gpu_rect.fuzz = 0.0f;
        gpu_rect.ref_idx = 1.0f;
    }

    // Determine type based on dynamic type.
    if (dynamic_cast<const XYRect*>(rect)) {
        gpu_rect.type = 0;
        // XY uses Z as k.
        gpu_rect.a0 = static_cast<float>(rect->u_min());
        gpu_rect.a1 = static_cast<float>(rect->u_max());
        gpu_rect.b0 = static_cast<float>(rect->v_min());
        gpu_rect.b1 = static_cast<float>(rect->v_max());
        gpu_rect.k = static_cast<float>(rect->k_value());
        gpu_rect.flip = rect->is_flipped() ? 1u : 0u;
    } else if (dynamic_cast<const XZRect*>(rect)) {
        gpu_rect.type = 1;
        // XZ uses Y as k.
        gpu_rect.a0 = static_cast<float>(rect->u_min());
        gpu_rect.a1 = static_cast<float>(rect->u_max());
        gpu_rect.b0 = static_cast<float>(rect->v_min());
        gpu_rect.b1 = static_cast<float>(rect->v_max());
        gpu_rect.k = static_cast<float>(rect->k_value());
        gpu_rect.flip = rect->is_flipped() ? 1u : 0u;
    } else if (dynamic_cast<const YZRect*>(rect)) {
        gpu_rect.type = 2;
        // YZ uses X as k.
        gpu_rect.a0 = static_cast<float>(rect->u_min());
        gpu_rect.a1 = static_cast<float>(rect->u_max());
        gpu_rect.b0 = static_cast<float>(rect->v_min());
        gpu_rect.b1 = static_cast<float>(rect->v_max());
        gpu_rect.k = static_cast<float>(rect->k_value());
        gpu_rect.flip = rect->is_flipped() ? 1u : 0u;
    } else {
        return;
    }

    out.push_back(gpu_rect);
}

void append_hittable(const std::shared_ptr<Hittable>& object,
                     std::vector<GpuSphere>& spheres,
                     std::vector<GpuRect>& rects) {
    if (auto sphere = std::dynamic_pointer_cast<Sphere>(object)) {
        GpuSphere gpu_sphere{};
        gpu_sphere.center = to_float3(sphere->center_position);
        gpu_sphere.radius = static_cast<float>(sphere->radius);
        auto mat = sphere->material_ptr;
        if (std::dynamic_pointer_cast<Matte>(mat)) {
            gpu_sphere.material_type = 0;
            gpu_sphere.albedo = to_float3(mat->base_color());
            gpu_sphere.fuzz = 0.0f;
            gpu_sphere.ref_idx = 1.0f;
        } else if (auto metal = std::dynamic_pointer_cast<Reflective>(mat)) {
            gpu_sphere.material_type = 1;
            gpu_sphere.albedo = to_float3(mat->base_color());
            gpu_sphere.fuzz = static_cast<float>(metal->fuzziness);
            gpu_sphere.ref_idx = 1.0f;
        } else if (auto glass = std::dynamic_pointer_cast<Transparent>(mat)) {
            gpu_sphere.material_type = 2;
            gpu_sphere.albedo = to_float3(mat->base_color());
            gpu_sphere.fuzz = 0.0f;
            gpu_sphere.ref_idx = static_cast<float>(glass->refractive_index);
        } else {
            gpu_sphere.material_type = 0;
            gpu_sphere.albedo = to_float3(mat->base_color());
            gpu_sphere.fuzz = 0.0f;
            gpu_sphere.ref_idx = 1.0f;
        }
        spheres.push_back(gpu_sphere);
        return;
    }

    if (auto rect = std::dynamic_pointer_cast<AxisAlignedRect>(object)) {
        append_rect_from_axis_rect(rect.get(), rect->material(), rects);
        return;
    }

    if (auto box = std::dynamic_pointer_cast<Box>(object)) {
        for (const auto& side : box->sides.objects) {
            if (auto side_rect = std::dynamic_pointer_cast<AxisAlignedRect>(side)) {
                append_rect_from_axis_rect(side_rect.get(), side_rect->material(), rects);
            }
        }
        return;
    }
}

// Metal shader source inlined to avoid extra build steps.
const char* kMetalSource = R"(
using namespace metal;

struct Sphere {
    float3 center;
    float radius;
    float3 albedo;
    float fuzz;
    float ref_idx;
    uint material_type; // 0 = diffuse, 1 = metal, 2 = dielectric
    uint pad0;
    uint pad1;
};

struct Rect {
    float a0;
    float a1;
    float b0;
    float b1;
    float k;
    uint type;  // 0 = XY, 1 = XZ, 2 = YZ
    uint flip;
    uint pad;
    float3 albedo;
    float fuzz;
    float ref_idx;
    uint material_type; // 0 = diffuse, 1 = metal, 2 = dielectric
    uint pad2;
};

struct Light {
    float3 position;
    float pad0;
    float3 intensity;
    float pad1;
};

struct CameraData {
    float3 origin;
    float3 lower_left;
    float3 horizontal;
    float3 vertical;
};

struct SceneCounts {
    uint sphere_count;
    uint rect_count;
    uint light_count;
};

struct Ray {
    float3 origin;
    float3 direction;
};

struct Hit {
    float t;
    float3 point;
    float3 normal;
    float3 albedo;
    float fuzz;
    float ref_idx;
    uint material_type;
};

inline uint lcg(thread uint& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

inline float rand01(thread uint& state) {
    return static_cast<float>(lcg(state) >> 8) * (1.0f / 16777216.0f);
}

inline float3 random_unit_vector(thread uint& state) {
    float z = rand01(state) * 2.0f - 1.0f;
    float a = rand01(state) * 6.28318530718f;
    float r = sqrt(max(0.0f, 1.0f - z * z));
    return float3(r * cos(a), r * sin(a), z);
}

inline float3 random_cosine_direction(thread uint& state, float3 normal) {
    float r1 = rand01(state);
    float r2 = rand01(state);
    float phi = 6.28318530718f * r1;
    float r = sqrt(r2);
    float x = r * cos(phi);
    float y = r * sin(phi);
    float z = sqrt(max(0.0f, 1.0f - r2));

    float3 w = normalize(normal);
    float3 a = fabs(w.x) > 0.9f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 v = normalize(cross(w, a));
    float3 u = cross(w, v);
    return normalize(u * x + v * y + w * z);
}

inline float3 reflect_vec(const float3 v, const float3 n) {
    return v - 2.0f * dot(v, n) * n;
}

inline float3 refract_vec(const float3 uv, const float3 n, float etai_over_etat) {
    float cos_theta = fmin(dot(-uv, n), 1.0f);
    float3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    float3 r_out_parallel = -sqrt(fabs(1.0f - dot(r_out_perp, r_out_perp))) * n;
    return r_out_perp + r_out_parallel;
}

inline float schlick(float cosine, float ref_idx) {
    float r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow((1.0f - cosine), 5.0f);
}

float3 sky_color(const Ray ray) {
    float3 unit_direction = normalize(ray.direction);
    float blend_factor = 0.5f * (unit_direction.y + 1.0f);
    float3 white = float3(1.0, 1.0, 1.0);
    float3 sky_blue = float3(0.5, 0.7, 1.0);
    return (1.0f - blend_factor) * white + blend_factor * sky_blue;
}

float hit_sphere(const Ray ray, const Sphere sphere, float t_min, float t_max, thread Hit& out_hit) {
    float3 oc = ray.origin - sphere.center;
    float a = dot(ray.direction, ray.direction);
    float half_b = dot(oc, ray.direction);
    float c = dot(oc, oc) - sphere.radius * sphere.radius;
    float discriminant = half_b * half_b - a * c;
    if (discriminant < 0.0f) {
        return -1.0f;
    }
    float sqrt_disc = sqrt(discriminant);

    float t = (-half_b - sqrt_disc) / a;
    if (t < t_min || t > t_max) {
        t = (-half_b + sqrt_disc) / a;
        if (t < t_min || t > t_max) {
            return -1.0f;
        }
    }

    out_hit.t = t;
    out_hit.point = ray.origin + t * ray.direction;
    out_hit.normal = normalize(out_hit.point - sphere.center);
    out_hit.albedo = sphere.albedo;
    out_hit.fuzz = sphere.fuzz;
    out_hit.ref_idx = sphere.ref_idx;
    out_hit.material_type = sphere.material_type;
    return t;
}

float hit_rect(const Ray ray, const Rect rect, float t_min, float t_max, thread Hit& out_hit) {
    float denom = 0.0f;
    if (rect.type == 0) {
        denom = ray.direction.z;
    } else if (rect.type == 1) {
        denom = ray.direction.y;
    } else {
        denom = ray.direction.x;
    }
    if (fabs(denom) < 1e-8f) {
        return -1.0f;
    }

    float t = 0.0f;
    float a_coord = 0.0f;
    float b_coord = 0.0f;
    if (rect.type == 0) {
        t = (rect.k - ray.origin.z) / denom;
        a_coord = ray.origin.x + t * ray.direction.x;
        b_coord = ray.origin.y + t * ray.direction.y;
    } else if (rect.type == 1) {
        t = (rect.k - ray.origin.y) / denom;
        a_coord = ray.origin.x + t * ray.direction.x;
        b_coord = ray.origin.z + t * ray.direction.z;
    } else {
        t = (rect.k - ray.origin.x) / denom;
        a_coord = ray.origin.y + t * ray.direction.y;
        b_coord = ray.origin.z + t * ray.direction.z;
    }

    if (t < t_min || t > t_max) {
        return -1.0f;
    }
    if (a_coord < rect.a0 || a_coord > rect.a1 || b_coord < rect.b0 || b_coord > rect.b1) {
        return -1.0f;
    }

    out_hit.t = t;
    out_hit.point = ray.origin + t * ray.direction;
    if (rect.type == 0) {
        out_hit.normal = rect.flip ? float3(0, 0, -1) : float3(0, 0, 1);
    } else if (rect.type == 1) {
        out_hit.normal = rect.flip ? float3(0, -1, 0) : float3(0, 1, 0);
    } else {
        out_hit.normal = rect.flip ? float3(-1, 0, 0) : float3(1, 0, 0);
    }
    out_hit.albedo = rect.albedo;
    out_hit.fuzz = rect.fuzz;
    out_hit.ref_idx = rect.ref_idx;
    out_hit.material_type = rect.material_type;
    return t;
}

bool any_shadow_hit(const Ray shadow_ray,
                    float max_distance,
                    constant Sphere* spheres,
                    constant Rect* rects,
                    const SceneCounts counts) {
    Hit temp;
    for (uint i = 0; i < counts.sphere_count; ++i) {
        float t = hit_sphere(shadow_ray, spheres[i], 0.001f, max_distance, temp);
        if (t > 0.0f) {
            return true;
        }
    }
    for (uint i = 0; i < counts.rect_count; ++i) {
        float t = hit_rect(shadow_ray, rects[i], 0.001f, max_distance, temp);
        if (t > 0.0f) {
            return true;
        }
    }
    return false;
}

float3 compute_direct(const Hit hit,
                      constant Light* lights,
                      const SceneCounts counts,
                      constant Sphere* spheres,
                      constant Rect* rects) {
    if (counts.light_count == 0) {
        return float3(0.0);
    }
    if (hit.material_type != 0) {
        return float3(0.0);
    }
    float3 color = float3(0.0);
    const float shadow_bias = 0.001f;
    for (uint i = 0; i < counts.light_count; ++i) {
        float3 to_light = lights[i].position - hit.point;
        float dist2 = max(dot(to_light, to_light), 1e-6f);
        float dist = sqrt(dist2);
        float3 light_dir = to_light / dist;
        float n_dot_l = dot(hit.normal, light_dir);
        if (n_dot_l <= 0.0f) {
            continue;
        }

        Ray shadow_ray;
        shadow_ray.origin = hit.point + shadow_bias * hit.normal;
        shadow_ray.direction = light_dir;

        if (any_shadow_hit(shadow_ray, dist - shadow_bias, spheres, rects, counts)) {
            continue;
        }

        float3 light_energy = lights[i].intensity / dist2;
        color += n_dot_l * light_energy;
    }
    return hit.albedo * color;
}

bool scatter_ray(const Hit hit,
                 const Ray ray_in,
                 thread uint& rng,
                 thread Ray& scattered,
                 thread float3& attenuation) {
    if (hit.material_type == 0) {
        // Diffuse
        float3 scatter_direction = random_cosine_direction(rng, hit.normal);
        scattered.origin = hit.point;
        scattered.direction = scatter_direction;
        attenuation = hit.albedo;
        return true;
    } else if (hit.material_type == 1) {
        // Metal
        float3 reflected = reflect_vec(normalize(ray_in.direction), hit.normal);
        float3 fuzz_vec = hit.fuzz * random_unit_vector(rng);
        scattered.origin = hit.point;
        scattered.direction = reflected + fuzz_vec;
        attenuation = hit.albedo;
        return dot(scattered.direction, hit.normal) > 0.0f;
    } else {
        // Dielectric
        attenuation = float3(1.0);
        float3 unit_dir = normalize(ray_in.direction);
        float cos_theta = fmin(dot(-unit_dir, hit.normal), 1.0f);
        float sin_theta = sqrt(max(0.0f, 1.0f - cos_theta * cos_theta));
        float ref_ratio = (dot(ray_in.direction, hit.normal) > 0.0f) ? hit.ref_idx : (1.0f / hit.ref_idx);
        bool cannot_refract = ref_ratio * sin_theta > 1.0f;
        float3 direction;
        if (cannot_refract || schlick(cos_theta, ref_ratio) > rand01(rng)) {
            direction = reflect_vec(unit_dir, hit.normal);
        } else {
            direction = refract_vec(unit_dir, hit.normal, ref_ratio);
        }
        scattered.origin = hit.point;
        scattered.direction = direction;
        return true;
    }
}

kernel void renderKernel(device uchar* out_image [[buffer(0)]],
                         constant Sphere* spheres [[buffer(1)]],
                         constant Rect* rects [[buffer(2)]],
                         constant Light* lights [[buffer(3)]],
                         constant CameraData& cam [[buffer(4)]],
                         constant SceneCounts& counts [[buffer(5)]],
                         constant uint& width [[buffer(6)]],
                         constant uint& height [[buffer(7)]],
                         constant uint& samples_per_pixel [[buffer(8)]],
                         constant uint& max_depth [[buffer(9)]],
                         uint2 gid [[thread_position_in_grid]]) {
    if (gid.x >= width || gid.y >= height) {
        return;
    }

    uint logical_row = (height - 1u) - gid.y; // match CPU's row variable
    uint dest_row = gid.y; // write in dispatch order; logical_row controls sampling
    uint pixel_index = dest_row * width + gid.x;

    uint rng = (gid.x * 73856093u) ^ (gid.y * 19349663u) ^ (samples_per_pixel * 83492791u);

    float3 accumulated = float3(0.0);
    for (uint sample = 0; sample < samples_per_pixel; ++sample) {
        float u = (static_cast<float>(gid.x) + rand01(rng)) / (static_cast<float>(width) - 1.0f);
        float v = (static_cast<float>(logical_row) + rand01(rng)) / (static_cast<float>(height) - 1.0f);

        Ray ray;
        ray.origin = cam.origin;
        ray.direction = cam.lower_left + u * cam.horizontal + v * cam.vertical - cam.origin;

        float3 throughput = float3(1.0);
        float3 direct_accum = float3(0.0);

        for (uint depth = 0; depth < max_depth; ++depth) {
            float t_min = 0.001f;
            float t_max = 1.0e9f;
            Hit closest_hit;
            closest_hit.t = t_max;
            bool hit_found = false;
            Hit temp_hit;

            for (uint i = 0; i < counts.sphere_count; ++i) {
                float t = hit_sphere(ray, spheres[i], t_min, closest_hit.t, temp_hit);
                if (t > 0.0f && t < closest_hit.t) {
                    closest_hit = temp_hit;
                    hit_found = true;
                }
            }

            for (uint i = 0; i < counts.rect_count; ++i) {
                float t = hit_rect(ray, rects[i], t_min, closest_hit.t, temp_hit);
                if (t > 0.0f && t < closest_hit.t) {
                    closest_hit = temp_hit;
                    hit_found = true;
                }
            }

            if (!hit_found) {
                accumulated += throughput * sky_color(ray);
                break;
            }

            float3 direct = compute_direct(closest_hit, lights, counts, spheres, rects);
            accumulated += throughput * direct;

            Ray scattered;
            float3 attenuation;
            if (!scatter_ray(closest_hit, ray, rng, scattered, attenuation)) {
                break;
            }
            throughput *= attenuation;
            ray = scattered;
        }
    }

    float scale = 1.0f / static_cast<float>(samples_per_pixel);
    float3 color = accumulated * scale;

    color = float3(sqrt(color.x), sqrt(color.y), sqrt(color.z));
    color = clamp(color, float3(0.0), float3(0.999));

    uint idx = pixel_index * 3u;
    out_image[idx + 0] = static_cast<uchar>(color.x * 256.0f);
    out_image[idx + 1] = static_cast<uchar>(color.y * 256.0f);
    out_image[idx + 2] = static_cast<uchar>(color.z * 256.0f);
}
)";

id<MTLLibrary> compileLibrary(id<MTLDevice> device, NSError** error) {
    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    return [device newLibraryWithSource:[NSString stringWithUTF8String:kMetalSource]
                                options:options
                                  error:error];
}

} // namespace

std::vector<unsigned char> render_image_gpu(const RenderConfig& config,
                                            const Camera& camera,
                                            const Scene& scene,
                                            int max_depth) {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            throw std::runtime_error("Metal device not available.");
        }

        NSError* error = nil;
        id<MTLLibrary> library = compileLibrary(device, &error);
        if (!library) {
            std::string err_msg = "Failed to compile Metal library.";
            if (error) {
                err_msg += " ";
                err_msg += [[error localizedDescription] UTF8String];
            }
            throw std::runtime_error(err_msg);
        }

        id<MTLFunction> kernel_fn = [library newFunctionWithName:@"renderKernel"];
        if (!kernel_fn) {
            throw std::runtime_error("Metal kernel not found.");
        }

        id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:kernel_fn error:&error];
        if (!pipeline) {
            std::string err_msg = "Failed to create pipeline state.";
            if (error) {
                err_msg += " ";
                err_msg += [[error localizedDescription] UTF8String];
            }
            throw std::runtime_error(err_msg);
        }

        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (!queue) {
            throw std::runtime_error("Failed to create Metal command queue.");
        }

        // Flatten scene.
        std::vector<GpuSphere> gpu_spheres;
        std::vector<GpuRect> gpu_rects;
        for (const auto& obj : scene.objects.objects) {
            append_hittable(obj, gpu_spheres, gpu_rects);
        }

        std::vector<GpuLight> gpu_lights;
        gpu_lights.reserve(scene.lights.size());
        for (const auto& light : scene.lights) {
            GpuLight gpu_light{};
            gpu_light.position = to_float3(light.position);
            gpu_light.intensity = to_float3(light.intensity);
            gpu_lights.push_back(gpu_light);
        }

        SceneCounts counts{
            static_cast<uint32_t>(gpu_spheres.size()),
            static_cast<uint32_t>(gpu_rects.size()),
            static_cast<uint32_t>(gpu_lights.size())
        };

        GpuCamera gpu_camera{
            to_float3(camera.origin),
            to_float3(camera.lower_left_corner),
            to_float3(camera.horizontal),
            to_float3(camera.vertical)
        };

        const std::size_t pixel_count = static_cast<std::size_t>(config.image_width) * static_cast<std::size_t>(config.image_height);
        const std::size_t out_size = pixel_count * 3;
        std::vector<unsigned char> output(out_size);

        // Create buffers.
        id<MTLBuffer> out_buffer = [device newBufferWithBytesNoCopy:output.data()
                                                             length:out_size
                                                            options:MTLResourceStorageModeShared
                                                        deallocator:nil];
        id<MTLBuffer> sphere_buffer = gpu_spheres.empty()
            ? [device newBufferWithLength:sizeof(GpuSphere) options:MTLResourceStorageModeShared]
            : [device newBufferWithBytes:gpu_spheres.data() length:gpu_spheres.size() * sizeof(GpuSphere) options:MTLResourceStorageModeShared];
        id<MTLBuffer> rect_buffer = gpu_rects.empty()
            ? [device newBufferWithLength:sizeof(GpuRect) options:MTLResourceStorageModeShared]
            : [device newBufferWithBytes:gpu_rects.data() length:gpu_rects.size() * sizeof(GpuRect) options:MTLResourceStorageModeShared];
        id<MTLBuffer> light_buffer = gpu_lights.empty()
            ? [device newBufferWithLength:sizeof(GpuLight) options:MTLResourceStorageModeShared]
            : [device newBufferWithBytes:gpu_lights.data() length:gpu_lights.size() * sizeof(GpuLight) options:MTLResourceStorageModeShared];
        id<MTLBuffer> camera_buffer = [device newBufferWithBytes:&gpu_camera length:sizeof(GpuCamera) options:MTLResourceStorageModeShared];
        id<MTLBuffer> counts_buffer = [device newBufferWithBytes:&counts length:sizeof(SceneCounts) options:MTLResourceStorageModeShared];
        uint32_t width = static_cast<uint32_t>(config.image_width);
        uint32_t height = static_cast<uint32_t>(config.image_height);
        id<MTLBuffer> width_buffer = [device newBufferWithBytes:&width length:sizeof(uint32_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> height_buffer = [device newBufferWithBytes:&height length:sizeof(uint32_t) options:MTLResourceStorageModeShared];
        uint32_t samples = static_cast<uint32_t>(config.samples_per_pixel);
        uint32_t depth = static_cast<uint32_t>(max_depth);
        id<MTLBuffer> samples_buffer = [device newBufferWithBytes:&samples length:sizeof(uint32_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> depth_buffer = [device newBufferWithBytes:&depth length:sizeof(uint32_t) options:MTLResourceStorageModeShared];

        id<MTLCommandBuffer> command = [queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
        [encoder setComputePipelineState:pipeline];
        [encoder setBuffer:out_buffer offset:0 atIndex:0];
        [encoder setBuffer:sphere_buffer offset:0 atIndex:1];
        [encoder setBuffer:rect_buffer offset:0 atIndex:2];
        [encoder setBuffer:light_buffer offset:0 atIndex:3];
        [encoder setBuffer:camera_buffer offset:0 atIndex:4];
        [encoder setBuffer:counts_buffer offset:0 atIndex:5];
        [encoder setBuffer:width_buffer offset:0 atIndex:6];
        [encoder setBuffer:height_buffer offset:0 atIndex:7];
        [encoder setBuffer:samples_buffer offset:0 atIndex:8];
        [encoder setBuffer:depth_buffer offset:0 atIndex:9];

        MTLSize threadsPerGroup = MTLSizeMake(8, 8, 1);
        MTLSize threadsPerGrid = MTLSizeMake(width, height, 1);
        [encoder dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerGroup];
        [encoder endEncoding];

        [command commit];
        [command waitUntilCompleted];

        return output;
    }
}
