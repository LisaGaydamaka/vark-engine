#pragma once
#include "csg_types.h"
#include "world/vmb_format.h"   // <-- ADD THIS
#include <vector>

// ---- Main entry points ----
void export_OBJ(const std::vector<CSGPoly>& polys, const char* path);
bool save_vmb(const char* path, const std::vector<CSGPoly>& polys);

// CSG compilation – needs brushes, per‑brush material indices, and the material table
std::vector<CSGPoly> compile_csg(const std::vector<Brush>& brushes,
                                 const std::vector<int>& materialIndices,
                                 const std::vector<VMBMaterial>& materials);

// Legacy: compile .vm → .vmb (kept for compatibility)
bool compile_vm_to_vmb(const char* vmPath, const char* vmbPath);

// NEW: compile .vm → .vmis
bool compile_vm_to_vmis(const char* vmPath, const char* vmisPath);