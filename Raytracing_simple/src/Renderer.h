#ifndef RENDERER_H
#define RENDERER_H

#include "Camera.h"
#include "Color.h"
#include "Hittable.h"
#include "Material.h"
#include "Ray.h"
#include "RenderConfig.h"
#include "Scene.h"
#include "Utils.h"
#include "Vec3.h"

#include <cmath>
#include <iostream>
#include <vector>

/**
 * Calculate the sky gradient color based on ray direction.
 */
Color calculate_sky_color(const Ray& ray);

Color compute_diffuse_lighting(const Scene& scene, const HitRecord& hit_info);

/**
 * Calculate the color for a ray with recursive ray tracing.
 * Handles reflections, refractions, and material interactions.
 * Adds direct diffuse lighting from all visible light sources.
 * 
 * @param ray The ray we're tracing
 * @param scene The scene containing objects and lights
 * @param depth Current recursion depth (prevents infinite bounces)
 * @return The color for this ray
 */
Color calculate_ray_color(const Ray& ray, const Scene& scene, int depth);

/**
 * Render a single pixel by casting multiple rays through it (antialiasing).
 * Takes multiple samples per pixel and averages them for smoother edges.
 */
Color render_pixel(int col, int row, const RenderConfig& config,
                   const Camera& camera, const Scene& scene, int max_depth);

/**
 * Render the entire image with antialiasing and recursive ray tracing.
 */
std::vector<unsigned char> render_image(const RenderConfig& config,
                                        const Camera& camera,
                                        const Scene& scene,
                                        int max_depth);

#endif
