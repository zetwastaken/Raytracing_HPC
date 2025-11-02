#ifndef RENDERER_H
#define RENDERER_H

/**
 * @file Renderer.h
 * @brief Declarations for the recursive ray tracer and sampling utilities.
 */

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
 * @brief Calculate the sky gradient color for primary rays that miss geometry.
 *
 * @param ray Incident ray leaving the camera.
 * @return Background radiance sampled along the ray direction.
 */
Color calculate_sky_color(const Ray& ray);

/**
 * @brief Accumulate diffuse contributions from all visible analytical lights.
 *
 * @param scene Scene containing point lights to evaluate.
 * @param hit_info Surface interaction info at the shading point.
 * @return RGB radiance from direct diffuse lighting.
 */
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
 *
 * @param col Column index of the pixel to shade.
 * @param row Row index of the pixel to shade.
 * @param config Render configuration containing resolution and sampling hints.
 * @param camera Camera used to spawn primary rays.
 * @param scene Scene containing geometry and lights.
 * @param max_depth Maximum recursion depth for secondary rays.
 * @return Linear RGB color accumulated for the pixel.
 */
Color render_pixel(int col, int row, const RenderConfig& config,
                   const Camera& camera, const Scene& scene, int max_depth);

/**
 * Render the entire image with antialiasing and recursive ray tracing.
 *
 * @param config Render configuration containing resolution and sampling hints.
 * @param camera Camera used to spawn primary rays.
 * @param scene Scene containing geometry and lights.
 * @param max_depth Maximum recursion depth for secondary rays.
 * @return Packed RGB buffer ready for PNG writing.
 */
std::vector<unsigned char> render_image(const RenderConfig& config,
                                        const Camera& camera,
                                        const Scene& scene,
                                        int max_depth);

#endif
