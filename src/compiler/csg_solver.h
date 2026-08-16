// src/compiler/csg_solver.h
#pragma once
#include "csg_types.h"
#include "world/vmis_format.h"   // now using VMISMaterial
#include <vector>

// ---- Main entry points ----
void export_OBJ(const std::vector<CSGPoly>& polys, const char* path);

// CSG compilation – uses VMISMaterial for material data
std::vector<CSGPoly> compile_csg(const std::vector<Brush>& brushes,
                                 const std::vector<int>& materialIndices,
                                 const std::vector<VMISMaterial>& materials);

// Rebake an existing .vmis file: read brushes, run CSG, write back
bool rebake_vmis(const char* vmisPath);