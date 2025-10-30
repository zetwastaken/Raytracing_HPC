#ifndef SCENE_H
#define SCENE_H

#include "HittableList.h"
#include "Light.h"
#include "Material.h"
#include "Sphere.h"
#include "Vec3.h"
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

struct Scene {
    HittableList objects;
    std::vector<Light> lights;

    std::size_t object_count() const { return objects.objects.size(); }
    std::size_t light_count() const { return lights.size(); }
};

/**
 * Create the default 3D scene with various materials.
 * Demonstrates matte, reflective, and transparent materials.
 * 
 * @return A scene containing multiple spheres with different materials
 */
inline Scene create_scene(std::vector<Light> lights = {}) {
    Scene scene;
    
    // Materials
    auto ground_material = std::make_shared<Matte>(Color(0.5, 0.5, 0.5));
    auto matte_red = std::make_shared<Matte>(Color(0.7, 0.3, 0.3));
    auto metal_gold = std::make_shared<Reflective>(Color(0.8, 0.6, 0.2), 0.3);
    auto glass = std::make_shared<Transparent>(1.5);
    auto metal_silver = std::make_shared<Reflective>(Color(0.8, 0.8, 0.8), 0.0);
    
    // Ground (large matte sphere centered below)
    scene.objects.add(std::make_shared<Sphere>(
        Point3(0, -100.5, -1),
        100.0,
        ground_material
    ));
    
    // Center sphere - matte red (on ground)
    scene.objects.add(std::make_shared<Sphere>(
        Point3(0, 0, -2.5),
        0.5,
        matte_red
    ));
    
    // Left sphere - glass (transparent, on ground)
    scene.objects.add(std::make_shared<Sphere>(
        Point3(-1.5, 0, -2.5),
        0.5,
        glass
    ));
    
    // Left sphere inner - glass bubble effect (negative radius)
    scene.objects.add(std::make_shared<Sphere>(
        Point3(-1.5, 0, -2.5),
        -0.4,
        glass
    ));
    
    // Right sphere - metallic gold (on ground)
    scene.objects.add(std::make_shared<Sphere>(
        Point3(1.5, 0, -2.5),
        0.5,
        metal_gold
    ));
    
    // Small sphere in front - shiny metal (on ground, more visible)
    scene.objects.add(std::make_shared<Sphere>(
        Point3(0.7, 0, -1.8),
        0.4,
        metal_silver
    ));
    
    if (lights.empty()) {
        lights.push_back(Light(Point3(6.0, 6.0, 0.0), Color(10.0, 10.0, 10.0)));
        lights.push_back(Light(Point3(-6.0, 7.0, -1.5), Color(6.0, 6.0, 8.0)));
    }

    scene.lights = std::move(lights);
    
    return scene;
}

#endif
