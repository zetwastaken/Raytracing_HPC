#ifndef SCENE_H
#define SCENE_H

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

RoomLayout default_room_layout();

struct Scene {
    HittableList objects;
    std::vector<Light> lights;
    RoomLayout layout;

    std::size_t object_count() const { return objects.objects.size(); }
    std::size_t light_count() const { return lights.size(); }
};

Scene create_scene(const RoomLayout& layout = default_room_layout(),
                   std::vector<Light> lights = {});

#endif
