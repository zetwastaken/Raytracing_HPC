#ifndef SCENE_H
#define SCENE_H

/**
 * @file Scene.h
 * @brief Room layout definitions and scene assembly helpers.
 */

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

/**
 * @brief Geometric description of the Cornell-box style room.
 */
struct RoomLayout {
    double half_width;       ///< Half the room width along the X axis.
    double half_depth;       ///< Half the room depth along the Z axis.
    double floor_y;          ///< Y coordinate of the floor plane.
    double ceiling_y;        ///< Y coordinate of the ceiling plane.
    double back_wall_z;      ///< Z coordinate of the back wall.
    double front_opening_z;  ///< Z coordinate of the open front (camera side).
};

/**
 * @brief Return the default room layout used by the demo scene.
 *
 * @return Canonical Cornell-box style layout dimensions.
 */
RoomLayout default_room_layout();

/**
 * @brief Full scene description with hittable geometry and analytic lights.
 */
struct Scene {
    HittableList objects;
    std::vector<Light> lights;
    RoomLayout layout;

    std::size_t object_count() const { return objects.objects.size(); }
    std::size_t light_count() const { return lights.size(); }
};

/**
 * @brief Construct a Cornell-box inspired scene with objects and lights.
 *
 * @param layout Room dimensions and wall placement.
 * @param lights Explicit light list; defaults to a ceiling emitter.
 * @return Scene ready for rendering.
 */
Scene create_scene(const RoomLayout& layout = default_room_layout(),
                   std::vector<Light> lights = {});

#endif
