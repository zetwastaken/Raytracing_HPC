# Scene Setup

## RoomLayout
Defined in `src/Scene.h`, `RoomLayout` describes the Cornell-box bounds:
- `half_width`, `half_depth` – half extents along the X and Z axes
- `floor_y`, `ceiling_y` – vertical limits
- `back_wall_z`, `front_opening_z` – depth positions for closed/open ends

`default_room_layout()` returns a canonical configuration sized for the camera and lamp used in the sample render.

## Lights
Lights are simple positional emitters (`src/Light.h`). The default scene adds a single ceiling lamp:
```cpp
lights.emplace_back(
    Point3(0.0, lamp_height, lamp_z_position),
    Color(lamp_intensity, lamp_intensity, lamp_intensity)
);
```
Add more lights by pushing into the `lights` vector before calling `create_scene`.

## Geometry
`create_scene` assembles:
- Axis-aligned rectangles representing the room surfaces
- Boxes/spheres for props
- Materials with different albedo and emission properties

All objects register in `HittableList`, so intersection order is managed automatically.

## Extending the Scene
- Add new primitives by including them in `create_scene`.
- To vary materials, tweak the surface `Material` assignments when constructing objects.
- For animations or variant layouts, consider writing helper builders that return `Scene` instances per frame/configuration.
