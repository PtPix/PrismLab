#pragma once
#include <cstdint>
namespace prism
{
    struct SceneStats
    {
        uint32_t meshes = 0;
        uint32_t instances = 0;
        uint32_t lights = 0;
        uint32_t vertices = 0;
        uint32_t triangles = 0;
    };

}
