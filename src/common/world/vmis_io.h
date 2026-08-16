// vmis_io.h
#pragma once
#include "vmis_format.h"
#include "world/level.h"   // for Brush, Vertex, etc.
#include "physics/physics_world.h"  // for Triangle
#include <vector>
#include <string>

bool write_vmis(const char* path,
                const std::vector<Brush>& brushes,
                const std::vector<Vertex>& bakedVerts,
                const std::vector<uint32_t>& bakedIndices,
                const std::vector<VMISMaterial>& materials,
                const std::vector<Triangle>& physicsTris,
                const std::vector<Vertex>& debugLines); // optional

bool read_vmis(const char* path,
               std::vector<Brush>& outBrushes,
               std::vector<Vertex>& outBakedVerts,
               std::vector<uint32_t>& outBakedIndices,
               std::vector<VMISMaterial>& outMaterials,
               std::vector<Triangle>& outPhysicsTris,
               std::vector<Vertex>& outDebugLines);