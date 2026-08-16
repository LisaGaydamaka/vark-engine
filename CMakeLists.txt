cmake_minimum_required(VERSION 3.10)
project(VarkEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Manifold library (external CSG library) ----
add_library(vark_manifold STATIC
    external/manifold/src/boolean2.cpp
    external/manifold/src/boolean2_diagnostics.cpp
    external/manifold/src/boolean2_offset.cpp
    external/manifold/src/boolean2_predicates.cpp
    external/manifold/src/boolean2_sweep.cpp
    external/manifold/src/boolean3.cpp
    external/manifold/src/boolean_result.cpp
    external/manifold/src/constructors.cpp
    external/manifold/src/cross_section.cpp
    external/manifold/src/csg_tree.cpp
    external/manifold/src/edge_op.cpp
    external/manifold/src/execution_impl.cpp
    external/manifold/src/face_op.cpp
    external/manifold/src/impl.cpp
    external/manifold/src/manifold.cpp
    external/manifold/src/minkowski.cpp
    external/manifold/src/polygon.cpp
    external/manifold/src/properties.cpp
    external/manifold/src/quickhull.cpp
    external/manifold/src/sdf.cpp
    external/manifold/src/smoothing.cpp
    external/manifold/src/sort.cpp
    external/manifold/src/subdivision.cpp
    external/manifold/src/tree2d.cpp
)
target_include_directories(vark_manifold PUBLIC
    external/manifold/include
    external/manifold/src
)
target_compile_definitions(vark_manifold PRIVATE
    NOMINMAX
    MANIFOLD_PAR=-1
    _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
    target_compile_options(vark_manifold PRIVATE /wd4244 /wd4267 /wd4390)
endif()

# ---- Common library (core engine systems, no CSG) ----
add_library(vark_common STATIC
    src/common/core/math.cpp
    src/common/core/memory.cpp
    src/common/core/geometry.cpp
    src/common/core/camera.cpp
    src/common/core/font.cpp
    src/common/core/settings.cpp
    src/common/core/logger.cpp
    src/common/platform/platform_win32.cpp
    src/common/renderer/renderer_d3d11.cpp
    src/common/world/level.cpp
    src/common/world/vmis_io.cpp
    src/common/engine/engine.cpp
    src/common/physics/physics_world.cpp
)
target_compile_definitions(vark_common PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_include_directories(vark_common PUBLIC src/common)
target_link_libraries(vark_common PRIVATE d3d11 d3dcompiler windowscodecs)

# ---- CSG library (compiler logic) ----
add_library(vark_csg STATIC
    src/compiler/csg_solver.cpp
)
target_include_directories(vark_csg PUBLIC
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/compiler
)
target_link_libraries(vark_csg PRIVATE vark_common vark_manifold)
target_compile_definitions(vark_csg PRIVATE
    NOMINMAX
    MANIFOLD_PAR=-1
    _CRT_SECURE_NO_WARNINGS
)

# ---- Game library ----
add_library(vark_game STATIC
    src/game/player/player.cpp
    src/game/game.cpp
)
target_include_directories(vark_game PUBLIC src)
target_link_libraries(vark_game PRIVATE vark_common)

# ---- UI library (2D overlay, shared) ----
add_library(vark_ui STATIC
    src/ui/ui_renderer.cpp
    src/ui/ui_widget.cpp
    src/ui/ui_root.cpp
    src/ui/ui_layout.cpp
    src/ui/ui_splitter.cpp   # <-- ADD
)
target_compile_definitions(vark_ui PRIVATE NOMINMAX)
target_include_directories(vark_ui PUBLIC
    src/ui
    src/common                     # <-- ADD so that "core/logger.h" works
)
target_link_libraries(vark_ui PRIVATE d3d11 d3dcompiler)

# ---- Editor library (editor logic, uses CSG) ----
add_library(vark_editor STATIC
    src/editor/editor.cpp
    src/editor/editor_ui.cpp
    src/editor/editor_camera.cpp
    src/editor/editor_settings.cpp
)
target_compile_definitions(vark_editor PRIVATE NOMINMAX)
target_include_directories(vark_editor PUBLIC
    src/editor
    src
)
target_link_libraries(vark_editor PRIVATE
    vark_common
    vark_ui
    vark_csg
)

# ---- Engine executable (game) ----
add_executable(engine_app WIN32 src/apps/engine/main.cpp)
target_include_directories(engine_app PRIVATE src)
target_link_libraries(engine_app PRIVATE vark_common vark_game)

# ---- Editor executable (standalone, no game) ----
add_executable(editor_app WIN32 src/apps/editor/editor_main.cpp)
target_include_directories(editor_app PRIVATE src)
target_link_libraries(editor_app PRIVATE
    vark_common
    vark_ui
    vark_editor
    vark_csg
)

# ---- Compiler executable (console tool) ----
add_executable(vcompiler
    src/compiler/main.cpp
)
target_include_directories(vcompiler PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/compiler
)
target_link_libraries(vcompiler PRIVATE vark_csg)

# ---- Copy assets (with directory creation) ----
add_custom_command(TARGET engine_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:engine_app>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:engine_app>/assets
)

add_custom_command(TARGET editor_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:editor_app>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:editor_app>/assets
)

add_custom_command(TARGET vcompiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:vcompiler>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:vcompiler>/assets
)