#ifndef SCENE_H
#define SCENE_H

#include "InlineControl.h"
#include "AxisAlignedRect.h"
#include "Box.h"
#include "HittableList.h"
#include "Light.h"
#include "Material.h"
#include "Sphere.h"
#include "Vec3.h"
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

struct RoomLayout {
    double half_width;
    double half_depth;
    double floor_y;
    double ceiling_y;
    double back_wall_z;
    double front_opening_z;
};

RAYTRACER_INLINE RoomLayout default_room_layout() {
    return RoomLayout{
        5.0,   // half_width
        6.0,   // half_depth
        -2.5,  // floor_y
        2.5,   // ceiling_y
        -12.0, // back_wall_z
        -2.0   // front_opening_z
    };
}

struct Scene {
    HittableList objects;
    std::vector<Light> lights;
    RoomLayout layout;

    std::size_t object_count() const { return objects.objects.size(); }
    std::size_t light_count() const { return lights.size(); }
};

RAYTRACER_INLINE Scene create_scene(const RoomLayout& layout = default_room_layout(),
                          std::vector<Light> lights = {}) {
    Scene scene;
    scene.layout = layout;

    // Room dimensions
    const double half_room_width = layout.half_width;
    const double half_room_depth = layout.half_depth;
    const double floor_y = layout.floor_y;
    const double ceiling_y = layout.ceiling_y;
    const double back_wall_z = layout.back_wall_z;
    const double front_opening_z = layout.front_opening_z;
    const double room_center_z = back_wall_z + half_room_depth;

    // Materials
    auto floor_material = std::make_shared<Matte>(Color(0.45, 0.38, 0.32));
    auto ceiling_material = std::make_shared<Matte>(Color(0.85, 0.85, 0.83));
    auto wall_material = std::make_shared<Matte>(Color(0.75, 0.75, 0.72));
    auto accent_wall_material = std::make_shared<Matte>(Color(0.55, 0.62, 0.78));
    auto table_material = std::make_shared<Matte>(Color(0.58, 0.44, 0.33));
    auto table_leg_material = std::make_shared<Matte>(Color(0.35, 0.30, 0.26));
    auto cabinet_material = std::make_shared<Matte>(Color(0.45, 0.48, 0.55));
    auto sofa_material = std::make_shared<Matte>(Color(0.55, 0.22, 0.22));
    auto cushion_material = std::make_shared<Matte>(Color(0.90, 0.90, 0.92));
    auto lamp_shade_material = std::make_shared<Matte>(Color(0.95, 0.93, 0.82));
    auto metal_material = std::make_shared<Reflective>(Color(0.8, 0.8, 0.8), 0.15);
    auto art_material = std::make_shared<Matte>(Color(0.25, 0.45, 0.78));

    // Room surfaces
    scene.objects.add(std::make_shared<XZRect>(
        -half_room_width, half_room_width,
        back_wall_z, front_opening_z,
        floor_y, floor_material
    ));

    scene.objects.add(std::make_shared<XZRect>(
        -half_room_width, half_room_width,
        back_wall_z, front_opening_z,
        ceiling_y, ceiling_material, true
    ));

    scene.objects.add(std::make_shared<YZRect>(
        floor_y, ceiling_y,
        back_wall_z, front_opening_z,
        -half_room_width, wall_material
    ));

    scene.objects.add(std::make_shared<YZRect>(
        floor_y, ceiling_y,
        back_wall_z, front_opening_z,
        half_room_width, wall_material, true
    ));

    scene.objects.add(std::make_shared<XYRect>(
        -half_room_width, half_room_width,
        floor_y, ceiling_y,
        back_wall_z, accent_wall_material
    ));

    // Wall artwork slightly offset from the back wall
    const double art_offset = 0.02;
    scene.objects.add(std::make_shared<XYRect>(
        -3.0, -0.2,
        floor_y + 1.0, floor_y + 3.2,
        back_wall_z + art_offset, art_material
    ));

    // Table top
    const double table_height = 1.1;
    const double table_top_thickness = 0.12;
    const double table_depth = 2.4;
    const double table_width = 3.2;
    const double table_center_z = room_center_z;

    Point3 table_top_min(
        -table_width / 2,
        floor_y + table_height - table_top_thickness,
        table_center_z - table_depth / 2
    );
    Point3 table_top_max(
        table_width / 2,
        floor_y + table_height,
        table_center_z + table_depth / 2
    );
    scene.objects.add(std::make_shared<Box>(table_top_min, table_top_max, table_material));

    // Table legs
    const double leg_offset_x = table_width / 2 - 0.25;
    const double leg_offset_z = table_depth / 2 - 0.25;
    const double leg_width = 0.22;
    const double leg_height = table_top_min.y() - floor_y;
    const std::vector<Vec3> leg_offsets = {
        Vec3(-leg_offset_x, 0, -leg_offset_z),
        Vec3( leg_offset_x - leg_width, 0, -leg_offset_z),
        Vec3(-leg_offset_x, 0,  leg_offset_z - leg_width),
        Vec3( leg_offset_x - leg_width, 0,  leg_offset_z - leg_width)
    };

    for (const Vec3& offset : leg_offsets) {
        Point3 leg_min(
            offset.x_component,
            floor_y,
            table_center_z + offset.z_component
        );
        Point3 leg_max = leg_min + Vec3(leg_width, leg_height, leg_width);
        scene.objects.add(std::make_shared<Box>(leg_min, leg_max, table_leg_material));
    }

    // Cabinet along the left wall
    Point3 cabinet_min(-4.5, floor_y, -10.5);
    Point3 cabinet_max(-2.6, floor_y + 2.0, -8.5);
    scene.objects.add(std::make_shared<Box>(cabinet_min, cabinet_max, cabinet_material));

    // Sofa on the right side
    Point3 sofa_base_min(2.0, floor_y, -8.0);
    Point3 sofa_base_max(4.6, floor_y + 0.9, -5.0);
    scene.objects.add(std::make_shared<Box>(sofa_base_min, sofa_base_max, sofa_material));

    Point3 sofa_back_min(2.0, floor_y + 0.9, -8.0);
    Point3 sofa_back_max(4.6, floor_y + 2.0, -7.2);
    scene.objects.add(std::make_shared<Box>(sofa_back_min, sofa_back_max, sofa_material));

    // Cushions
    Point3 cushion1_min(2.2, floor_y + 0.9, -7.4);
    Point3 cushion1_max(3.2, floor_y + 1.5, -5.4);
    scene.objects.add(std::make_shared<Box>(cushion1_min, cushion1_max, cushion_material));

    Point3 cushion2_min(3.4, floor_y + 0.9, -7.4);
    Point3 cushion2_max(4.4, floor_y + 1.5, -5.4);
    scene.objects.add(std::make_shared<Box>(cushion2_min, cushion2_max, cushion_material));

    // Decorative objects
    scene.objects.add(std::make_shared<Sphere>(
        Point3(-3.6, floor_y + 2.1, -9.5),
        0.35,
        metal_material
    ));

    scene.objects.add(std::make_shared<Sphere>(
        Point3(0.0, floor_y + table_height + 0.35, table_center_z + 0.2),
        0.35,
        lamp_shade_material
    ));

    if (lights.empty()) {
        lights.emplace_back(
            Point3(0.0, ceiling_y - 0.3, -6.0),
            Color(18.0, 18.0, 17.0)
        );
        lights.emplace_back(
            Point3(-2.5, ceiling_y - 0.4, room_center_z + 2.0),
            Color(10.0, 11.0, 12.0)
        );
    }

    scene.lights = std::move(lights);
    
    return scene;
}

#endif
