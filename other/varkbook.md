Here is the **completely revised, production‑ready documentation** that integrates our entire discussion—including the final CSG logic, viewport modes, Blender‑style navigation, and the removal of all gizmo references.

---

# Quantized CSG Engine – Complete Two‑Part Guide (Revisited)

---

# Part I: The Game Engine (Runtime)

This part describes the **runtime** systems that execute a baked level: rendering, physics, AI navigation, entity management, scripting, file I/O, and performance profiling.

---

## 1. Introduction

The Quantized CSG Engine is a hybrid **level editor and runtime renderer** built around **integer‑precision geometry**. Every vertex in the game world is snapped to a global 3D integer grid, providing rock‑solid deterministic CSG operations, zero floating‑point drift, and watertight geometry.

**Gameplay Focus:** This engine targets a **Thief‑1998‑style stealth game**:
- First‑person perspective, with a player character that can **climb** ledges, **knock out** NPCs from behind, **steal loot**, and complete mission objectives.
- AI guards patrol, investigate suspicious noises, and engage in combat when alerted.
- All movement, collision, and AI are deterministic and rely on integer‑based geometry for consistency.

**Unit System:** 1 world unit = 10 cm. All dimensions (player height, capsule radius, step height, etc.) are expressed in units and are **integers** or **fixed‑point** (e.g., 1.8 units = 1.8 m → 18 units). Runtime positions are stored as floats for smooth interpolation, but all collision queries are performed against integer‑snapped geometry.

---

## 2. Runtime World Architecture

The runtime engine consumes **baked data** produced by the editor. The world consists of:

| Component | Description |
|-----------|-------------|
| **Baked Mesh** | Immutable triangulated geometry from CSG operations, stored as vertex/index buffers with UVs and material IDs. |
| **Physics Collision** | A triangle collision mesh (identical to the baked mesh) with a bounding volume hierarchy (BVH) optimised for swept queries. |
| **Entities** | Dynamic objects: player, AI, lights, triggers, interactive objects (doors, loot, switches). |
| **Navigation** | Pre‑baked AI pathfinding grid (navmesh) generated from the baked floor geometry. |
| **Scripting** | Lua 5.4‑based mission logic and event handling. |

---

## 3. Core Data Structures (C++)

These structures are used during baking and runtime loading. All binary files are **little‑endian** and include a version number in their header for forward compatibility.

### 3.1. Vertex
```cpp
struct Vertex {
    int32_t x, y, z;          // Snapped to integer grid.
};
```

### 3.2. Triangle (Runtime)
```cpp
struct Triangle {
    uint32_t v[3];            // Indices into the global vertex buffer.
    uint32_t materialID;      // Index into the global material table.
};
```

### 3.3. Face Group (Runtime)
After baking, coplanar adjacent triangles sharing the same material are merged into Face Groups. Each stores the planar projection parameters used for UV generation.
```cpp
struct FaceGroup {
    uint32_t firstTriangle;   // Start index in the global triangle buffer.
    uint32_t triangleCount;
    uint32_t materialID;
    int32_t origin[3];        // Integer anchor (U=0, V=0).
    int32_t uAxis[3];         // Integer vector for horizontal tiling.
    int32_t vAxis[3];         // Integer vector for vertical tiling.
    float scaleU, scaleV;     // Per‑axis tiling factors.
    Plane plane;              // Coplanar plane equation (A,B,C,D) as integers.
    // Optional: lightmap origin/axes/scale if lightmaps are used.
};
```

### 3.4. Plane (Integer)
```cpp
struct Plane {
    int32_t A, B, C, D;       // A*x + B*y + C*z + D = 0.
    // Normalized so that gcd(A,B,C) = 1 and normal points outward.
};
```

### 3.5. Material Table
A simple array of materials, each referencing a diffuse texture (`.dds`) and optionally a normal map. No PBR; only diffuse lighting and baked ambient.

### 3.6. BVH Node (for collision)
The BVH is a bounding volume hierarchy over the collision triangles. Each node stores:
```cpp
struct BVHNode {
    AABB bounds;               // Axis‑aligned bounding box (floats).
    uint32_t leftChild;        // Index of left child, or 0xFFFFFFFF if leaf.
    uint32_t rightChild;       // Index of right child, or 0xFFFFFFFF if leaf.
    uint32_t firstTriangle;    // Start index in triangle list if leaf.
    uint32_t triangleCount;    // Number of triangles if leaf.
};
```
The BVH is built using the **Surface Area Heuristic (SAH)** and stored in the terrain file. For levels with > 100k triangles, the BVH is built with a maximum leaf size of 16 triangles to accelerate swept queries.

---

## 4. Rendering Pipeline (DirectX 11)

### 4.1. Shading Model
- **No PBR**. Surfaces use simple **Lambertian diffuse** shading with optional normal mapping.
- Lighting: `diffuse = max(dot(N, L), 0) * lightColor * albedo`.
- No specular, no emissive (except for light sources as separate entities).

### 4.2. GBuffer (Deferred Shading)
For dynamic lights (point lights, spotlights), a deferred pipeline is used:

| Render Target | Format | Contents |
|---------------|--------|----------|
| RT0 | R8G8B8A8_UNORM | Albedo (diffuse texture) |
| RT1 | R16G16B16A16_FLOAT | World‑space normal (encoded as `normal.xyz * 0.5 + 0.5`) |
| RT2 | R32_FLOAT | Depth (for reconstructing world position) |

*No roughness, metallic, or specular buffers.*

### 4.3. Lighting
- **Dynamic lights** (torches, guard flashlights) contribute only diffuse with attenuation `1 / (d²)`.
- **Baked ambient occlusion / lightmaps** – optional lightmap textures are generated offline and blended multiplicatively with the albedo. Lightmaps are stored as separate `.dds` files and sampled using a second set of UV coordinates (derived from a separate face‑group lightmap projection stored in the terrain file).

### 4.4. Shadow Maps
- Dynamic lights cast shadows using **standard shadow maps** with 4‑tap PCF.
- Only the **first 4 shadow‑casting lights** are supported (for performance).
- Shadow map resolution: 2048×2048 for all shadow casters.
- For directional lights (e.g., sun), a single cascade is used (orthographic projection).

### 4.5. Texturing
- Materials reference a **diffuse texture** (`.dds` with BC1 or BC3 compression) and optionally a **normal map** (`.dds` with BC5).
- UVs are computed per Face Group using the planar projection formula (see Part II, Section 7).
- **Tangent basis for normal mapping:** The face group’s `uAxis` and `vAxis` are used to derive an orthonormal tangent frame per face group. The normal map stores tangent‑space normals; we transform to world space using the `T, B, N` matrix.
- **Texture filtering:** Linear filtering with mipmapping; mipmaps are generated by DirectX from the loaded texture.

### 4.6. No Post‑Processing / Anti‑Aliasing
- No bloom, tone mapping, or other post‑effects.
- No hardware AA (MSAA) – the game renders at native resolution without antialiasing.

### 4.7. Rendering Order
- **Deferred pass**: All static terrain and static entities (doors, loot) are rendered into GBuffer.
- **Shadow pass**: For each shadow‑casting light, render depth from light.
- **Lighting pass**: Accumulate diffuse lighting into a separate render target; then combine with albedo and lightmap.
- **Forward pass**: Transparent objects (if any) are rendered forward after the deferred lighting.

---

## 5. Physics & Character Controller

### 5.1. Collision Mesh & BVH
- The baked triangle mesh is used directly as a static collision mesh (no convex decomposition).
- A **BVH** (AABB tree with SAH) is built over the triangles. The BVH is stored in the terrain file to avoid rebuilding at load time.

### 5.2. Unit Scale & Player Capsule
- 1 unit = 10 cm.
- Player capsule: radius = 4 units (0.4 m), height = 18 units (1.8 m), with rounded ends.
- All physics queries use **continuous collision detection** (swept sphere‑capsule against triangles).

### 5.3. Player Controller
- **Kinematic** movement: player velocity is applied each frame, and collision response resolves penetration and slides along surfaces using the **sliding plane** algorithm (iterate up to 3 times).
- **Gravity**: Acceleration = –30 units/s² (3 m/s²).
- **Climbing / Mantling**:
  - When the forward sweep is blocked by a wall, check the blocking face normal.
  - If the normal has a significant upward component (`z > 0.7`), and the player presses the **climb** key, perform an upward sweep to vault over the ledge.
  - A successful vault teleports the player to the top of the ledge with a small forward impulse.
- **Stealth mechanics**:
  - The player’s **movement speed** can be toggled between walk (slow, quiet) and run (faster, noisier).
  - **Crouch**: reduces capsule height to 10 units and halves movement speed.
  - **Knockout**: when the player is behind an AI and presses the attack key, a raycast checks if the AI is within 2 units and facing away; if so, the AI is knocked out (state changed to `KO`).
- **No ladders**; climbing is only via mantling and jump.

### 5.4. Dynamic Objects
- Doors, crates, and loot use **kinematic rigid bodies** – positions are interpolated, and collision sweeps are performed to prevent clipping. No full physics simulation; all movement is script‑driven.

### 5.5. Performance Optimizations
- **Spatial hashing** for AI‑to‑player line‑of‑sight checks.
- **BVH traversal** optimised for capsule sweeps by using a ray‑like query that tests nodes against the capsule’s swept AABB first.
- **Triangle prefiltering**: triangles smaller than 0.1 units are discarded from collision to reduce BVH size.

---

## 6. Navigation & AI (Recast/Detour)

### 6.1. Navmesh Generation (Offline)
- During baking, the final terrain mesh (after all CSG operations) is fed to **Recast**.
- Agent configuration: radius = 4 units (0.4 m), height = 18 units (1.8 m), max climb = 5 units (0.5 m), max slope = 45°.
- The resulting navmesh is serialized to `navigation.nav`.

### 6.2. Runtime Pathfinding
- Load the navmesh into **Detour**.
- AI agents use `dtNavMeshQuery::findPath` for point‑to‑point paths.
- Dynamic obstacles (e.g., closing doors) are handled via `dtTileCache` – we mark polygons as unwalkable when a door closes, and the tile cache updates the navmesh locally.

### 6.3. AI Behavior (Stealth‑Oriented)
AI agents are state‑machine driven with the following states (implemented in C++ with Lua callbacks):

| State | Description |
|-------|-------------|
| **Idle** | Stand still, look around periodically. |
| **Patrol** | Follow a path of waypoints; switch to Investigate on hearing a noise or seeing the player. |
| **Investigate** | Move to the last known position of the player or noise; if player is found, switch to Combat; after timeout, return to Patrol. |
| **Combat** | Chase player, attack when in melee range. If player is knocked out or hidden, eventually return to Patrol. |
| **KnockedOut** | Non‑functional; lies on ground (animation). |

**Perception:**
- **Sight**: Cone of view (90° FOV, range = 30 units = 3 m). Line‑of‑sight checked via raycast against collision mesh.
- **Hearing**: Radius = 20 units (2 m) for walking, 40 units (4 m) for running, 10 units (1 m) for crouching.

---

## 7. Entity System (Component‑Based)

Entities are dynamic objects placed in the editor. They are loaded from `entities.bin`.

### 7.1. Core Components
| Component | Purpose |
|-----------|---------|
| `Transform` | Position (float), rotation (quaternion), scale (float). |
| `StaticMeshRenderer` | Renders a pre‑baked static mesh (for doors, loot, torches). Mesh is loaded from an imported `.obj` or `.fbx` file. |
| `Light` | Point or spot light; color, intensity, range, shadow flag. |
| `Script` | Reference to a Lua script that runs `OnInit`, `OnUpdate`, etc. |
| `Trigger` | Box/sphere volume; fires `OnEnter`/`OnExit` callbacks. |
| `AIAgent` | State machine with patrol points, perception parameters, and a reference to a navmesh. |
| `Player` | (Only one) – controls the player character. |
| `Inventory` | Holds loot items and objectives; accessible from Lua. |
| `Objective` | Tracks mission progress (completed/incomplete). |

### 7.2. Spawning
- `entities.bin` stores for each entity: `typeID` (hash), `Transform`, and a `PropertyMap` (string → variant).
- The runtime maintains a **prototype registry** that maps `typeID` to a default component set. Overrides from the property map are applied on spawn.

---

## 8. Scripting (Lua)

- **Lua 5.4** with `sol2` bindings.
- Each entity with a `Script` component has a table with standard callbacks:
  - `OnInit(self)` – called after spawn.
  - `OnUpdate(self, dt)` – called every frame.
  - `OnTriggerEnter(self, other)` – when entering a trigger volume.
  - `OnDamage(self, amount)` – health/damage handling.
  - `OnKnockout(self)` – called when the AI is knocked out.

### 8.1. Exposed API (Full List)
| Function | Description |
|----------|-------------|
| `Entity GetEntity(string name)` | Finds an entity by name (unique). |
| `void SetPosition(Entity, vec3)` | Teleports an entity. |
| `void PlaySound(string soundName, vec3 position)` | Plays a 3D sound at a world position. |
| `void SetLightColor(Entity, vec3)` | Changes a light’s color. |
| `void TeleportPlayer(vec3)` | Moves the player to a location. |
| `void SetObjective(string id, bool completed)` | Updates mission state. |
| `void KnockoutAI(Entity)` | Knocks out an AI (script‑triggered). |
| `bool IsPlayerSneaking()` | Returns true if player is crouching or walking silently. |
| `void AddLoot(string lootType, int count)` | Adds loot to player’s inventory. |
| `void SetAlarmState(bool active)` | Triggers global alarm (affects all AI). |
| `void SpawnEntity(string type, vec3 position, table properties)` | Spawns a new entity at runtime. |
| `void Log(string message)` | Prints to in‑game console and log file. |

---

## 9. Mission Loading Sequence

1. **Parse `mission.cfg`** – read metadata (name, author, objective).
2. **Mount `.pak`** – virtual file system (if shipping).
3. **Load `terrain.ter`**:
   - Vertex/index buffers uploaded to GPU.
   - Material table loaded.
   - Physics BVH built (or loaded from stored data).
   - Lightmap textures loaded (if present).
4. **Load `navigation.nav`** – feed into Detour.
5. **Load `entities.bin`** – spawn all entities, call `OnInit` on scripts.
6. **Execute `scripts/main.lua`** – mission‑wide initialisation.
7. Enter game loop (update, render, physics, AI, audio).

---

## 10. File Formats (Engineering Reference)

All binary files are **little‑endian** and include a `uint32_t version` field as the first 4 bytes.

### 10.1. `mission.cfg` (INI‑style)
Keys:
- `name` – display title
- `author`
- `objective` – brief mission description
- `starting_equipment` – comma‑separated list of loot types

### 10.2. `terrain.ter` (Binary)
- `version` (uint32)
- `vertexCount` (uint32), then array of `float x,y,z, u,v` (UVs stored as floats; lightmap UVs stored in a separate array if lightmaps exist).
- `indexCount` (uint32), then array of `uint32_t` triplets.
- `faceGroupCount` (uint32), then array of `FaceGroup` as defined in 3.3.
- `materialCount` (uint32), then for each: `uint32_t nameLen`, `char name[nameLen]` (relative path to .dds), `uint32_t normalLen`, `char normalName[normalLen]` (or 0 if none).
- `lightmapPresent` (uint8), if 1: `lightmapUVCount` (uint32) then array of `float u,v` for each vertex.
- `bvhData` – serialized BVH nodes (variable length; see 3.6).

### 10.3. `entities.bin` (Binary)
- `version` (uint32)
- `entityCount` (uint32)
- For each entity:
  - `typeID` (uint32 hash of prototype name)
  - `transform` (float pos[3], float quat[4], float scale[3])
  - `propertyCount` (uint32)
  - For each property: `nameLen` (uint16), `name` (char), `type` (uint8: 0=int,1=float,2=bool,3=string), `value` (variable)

### 10.4. `navigation.nav`
- Serialized Recast/Detour navmesh (binary format as produced by `dtNavMesh::serialize`).

### 10.5. `.pak` Archive
- Header: magic `'QPAK'` (4 bytes), version (uint32), TOC offset (uint64), TOC size (uint64).
- TOC: array of entries, each with `pathHash` (uint64, FNV‑1a), `offset` (uint64), `compressedSize` (uint64), `uncompressedSize` (uint64).
- Compression: **LZ4** (fast decompression).

---

## 11. Runtime Performance & Profiling

- **Culling**: Static BVH for terrain; entity AABB frustum culling.
- **Batching**: Static terrain is a single draw call. Instancing for repeated entities using `DrawIndexedInstanced`.
- **Profiling**: Built‑in **performance profiler** with scoped timers (microseconds). Measures:
  - Frame time
  - Rendering (GBuffer, shadows, lighting)
  - Physics (BVH traversal, collision)
  - AI (pathfinding, perception)
  - Script update
- Profiling data is displayed in an **in‑game console** (opened with `~`) and can be written to a log file.

---

## 12. Memory Management

- **Memory pools** for frequently allocated objects (e.g., entities, components, BVH nodes) to reduce fragmentation and improve cache locality.
- **Stack allocator** for temporary operations (e.g., pathfinding queries).
- **Memory tracking**: Each allocation is tagged with a category (e.g., "Rendering", "Physics", "AI"). At runtime, the engine tracks total usage per category and reports it in the console. On shutdown, a summary is printed to the log.
- All resource loads (textures, meshes) use a **resource manager** with reference counting; duplicate loads are avoided.

---

# Part II: The Editor

This part describes the **interactive design environment** used to create levels: the UI, viewport, layer stack, brush creation, geometry editing, texturing, and the baking pipeline.

---

## 1. Introduction

The Quantized CSG Editor is a **modal 3D environment** for precision architectural construction. It provides a viewport, a non‑destructive layer‑stack hierarchy, a dedicated geometry editing mode, and a streamlined texturing workflow—all built around integer‑precision determinism.

**Editor architecture:** Built with **Dear ImGui** for the UI layer, and a **separate application** from the game (though they share the same core engine libraries). The editor runs on **Windows** only, compiled with **CMake** and **Visual Studio Code**.

**Core Construction Paradigm:** The world begins as an **infinite solid**. Rooms and passages are constructed by placing **Add** brushes (which define solid masses) and then subtracting **Sub** brushes to carve out interior spaces. This subtractive workflow mirrors the classic DromEd approach and ensures watertight geometry from the ground up.

### 1.1. Modal States
The editor operates in two primary modes:

- **World Mode** – The default state. You see and interact with the **ordered list of brushes and groups** (the Layer Stack). Here you can select, move, rotate, scale, group, and reorder brushes. You can also enter **Edit Mode** from here (see Section 5).
- **Entity Mode** – A separate workspace where you manage **entities** (dynamic objects, lights, triggers, AI). Order does **not** matter (entities are independent), but you can group them for convenience. Entity Mode has its own list panel and inspector.

---

## 2. Interface Overview

### 2.1. Top Menu Bar (ImGui)
| Menu | Contents |
|------|----------|
| **File** | New, Open Mission, Save, Save As, Export Packaged (.pak), Quit |
| **Edit** | Undo, Redo, Cut, Copy, Paste, Duplicate, Delete |
| **View** | Toggle Grid, Camera Presets, Zoom Extents, Display Mode (Solid, Baked, Baked Lit, Triangles) |
| **Brush** | Create Box, Create Wedge, Group, Ungroup, Merge, Split, Isolate, Hide |
| **Edit Mode** | Enter Edit Mode, Vertex/Edge/Face Mode, Extrude, Connect, Merge, Divide |
| **Texture** | Load Texture, Apply to Selected, Paint Mode, Next/Previous Face |
| **Build** | Bake Level, Test in Game |
| **Settings** | Keyboard Shortcuts, Grid Size, Theme |

### 2.2. Primary Panels (ImGui Windows)
- **3D Viewport** – Main workspace (see Section 3).
- **Layer Stack** – Hierarchical list of brushes and groups (World Mode) or entity list (Entity Mode).
- **Inspector** – Properties of the currently selected brush/group/entity.
- **Texture Browser** – Catalog of imported `.dds` textures.
- **Toolbar** – Quick access to common operations (Move, Rotate, Scale, Paint, etc.).
- **Console** – In‑game/editor log, displays errors, warnings, and profiling output.

---

## 3. The 3D Viewport

### 3.1. Navigation
All viewport navigation is mouse‑driven. Orthographic projections (Top, Front, Side) are achieved via a **Blender‑style orbit snap**: hold `Alt` and orbit with the middle mouse button; releasing near the X, Y, or Z axis snaps the camera to the corresponding orthographic projection. Orbiting again returns to perspective.

| Action | Mouse | Keyboard |
|--------|-------|----------|
| Orbit | Middle‑drag | `Alt + LMB` |
| Pan | Shift + Middle‑drag | `Shift + Alt + LMB` |
| Zoom | Scroll wheel | `Ctrl + Alt + LMB` |
| Focus Selection | – | `Numpad .` |
| Frame All | – | `Home` |

### 3.2. Viewport Display Modes
The editor provides four distinct visual modes to balance editing fluidity with visual fidelity. The CSG pipeline is strictly throttled to ensure sub‑millisecond response times during active manipulation.

| Mode | Shortcut | Render Target | CSG State | Update Strategy |
|------|----------|---------------|-----------|-----------------|
| **Solid** | `Alt+1` | **Add Brushes** rendered as raw, intersecting, unlit solids (zero CSG computation). **Sub Brushes** rendered with a **localised CSG kernel**: the engine queries overlapping Add brushes, performs a local union and subtract, and overlays the carved result. | **Local CSG only for Sub brushes.** Triggers only when an edited brush intersects a Sub brush. | **Real‑time (debounced).** Compile time < 1ms. Updates on mouse‑release or 100ms idle. |
| **Baked** | `Alt+2` | **Full Global CSG Result** (all Add brushes unioned, all Sub brushes subtracted), rendered with a flat unlit shader (albedo only). | **Full Global CSG.** | **Manual (`Ctrl+B`)** or **Idle (1.5s)**. Runs in background thread. |
| **Baked Lit** | `Alt+3` | **Full Global CSG Result** rendered with the complete deferred lighting pipeline, including dynamic lights, shadows, and baked lightmaps. | **Full Global CSG.** | **Manual (`Ctrl+B`)** only (or 3s idle). |
| **Triangles** | `Alt+4` | Debug mode where every triangle in the baked mesh is assigned a random colour, allowing visual inspection of triangulation quality and face‑group boundaries. | **Full Global CSG (or last baked result).** | **Manual (`Ctrl+B`)** to refresh. |

**Performance Note for Solid Mode:** The local CSG kernel only executes when an edited brush intersects a Sub brush. Moving an Add brush through empty space incurs zero CSG cost. When intersection occurs, the engine gathers only the overlapping brushes (typically 1–3), unions them, and subtracts the Sub—completing in under 1 millisecond.

---

## 4. The Layer Stack (World Mode)

The Layer Stack is a hierarchical list of **brushes** and **groups** in the scene.

### 4.1. Order & CSG Processing
- Brushes higher in the list have greater `Time` values and are processed **later**.
- **The world begins as an infinite solid (`Time = 0`).** All **Add** brushes add solid matter; all **Sub** brushes subtract from the accumulated geometry. This subtractive paradigm is fundamental: designers carve rooms out of the infinite void.
- Drag entries up/down to reorder; `Time` values update automatically.
- **Groups** are processed internally first, producing a single solid mesh that is then treated as an Add brush at the parent level.

### 4.2. Group Operations
| Operation | Description |
|-----------|-------------|
| **Create Group** | Select multiple brushes, right‑click > Group. |
| **Ungroup** | Select a group, right‑click > Ungroup. |
| **Nest Group** | Drag a group into another group. |
| **Rename** | Double‑click the name. |

### 4.3. Layer Management
Each brush/group supports:
- Copy / Paste / Duplicate / Cut
- Isolate (hides all others)
- Hide / Show / Delete

### 4.4. Local Carving (Masking)
A **Sub** brush placed **inside a group** only erases geometry within that parent group. This enables local, non‑destructive carving (e.g., windows cut only the room walls, not adjacent rooms).

### 4.5. Merging
Select multiple brushes or entire groups and use **Merge** to combine them into a single optimized brush (reduces draw calls). Merging performs a CSG union of all selected brushes and then retriangulates.

---

## 5. Brushes (Source Geometry)

### 5.1. Brush Definition
A **Brush** is the atomic container for source geometry:
- **Type**: `Add` (solid) or `Sub` (void/air). `Add` brushes build the structural mass; `Sub` brushes carve out rooms, doors, and windows from the infinite solid or existing Add geometry.
- **Time**: 32‑bit integer (read‑only, controlled by Layer Stack order).
- **Vertex Pool**: List of snapped integer coordinates.
- **Triangle Pool**: List of indices defining the triangulated mesh.
- **Planar brushes (single polygons) are NOT allowed.** All brushes must be closed, watertight manifolds. The editor validates this before baking.

### 5.2. Creating Brushes
- Use **Create Box** or **Create Wedge** (or `Shift + A` for the creation menu).
- Click/drag in the viewport to define size.
- The new brush appears in the Layer Stack. It is initially an **Add** brush; you can change its type in the Inspector.

### 5.3. Brush Properties (Inspector)
| Property | Description |
|----------|-------------|
| **Name** | Editable identifier. |
| **Type** | Add / Sub (changeable). |
| **Time** | Read‑only CSG order. |
| **Position** | X, Y, Z (integer). |
| **Rotation** | Quaternion (Euler displayed for convenience). |
| **Scale** | Uniform or per‑axis. |

### 5.4. Read‑Only Metrics
Volume, bounding box, vertex count, face count, triangle count.

---

## 6. Edit Mode (Geometry Editing)

### 6.1. Entering Edit Mode
1. Select a brush in the Layer Stack or viewport.
2. Press `Tab` or use the **Edit Mode** command.
3. **Note**: Groups cannot be edited directly; ungroup or drill down to the target brush.

### 6.2. Viewport Representation
While in Edit Mode, the viewport shows the **source geometry**:
- **Vertices** as points.
- **Edges** as lines.
- **Faces** as flat polygons (triangles are deliberately hidden for clarity).

### 6.3. Selection Sub‑Modes
| Mode | Shortcut | Selection Behaviour |
|------|----------|---------------------|
| **Vertex** | `1` | Click to select; `Shift+Click` to add; drag box for multiple. |
| **Edge** | `2` | Click to select; `Shift+Click` to add; `Alt+Click` to select edge ring. |
| **Face** | `3` | Click to select; `Shift+Click` to add; `Alt+Click` to select face loop. |

### 6.4. Transform Tools (Keyboard & Numeric)
All transformations are performed via **keyboard shortcuts** and **numeric input** in the Inspector. No on‑screen 3D gizmo is used.

| Tool | Shortcut | Action |
|------|----------|--------|
| **Move** | `G` | Translate selected vertices/edges/faces. Enter exact values in Inspector. |
| **Rotate** | `R` | Rotate selected elements. |
| **Scale** | `S` | Scale selected elements. |

**Snap Rule**: All transformations snap to the integer grid; vertices are rounded to the nearest integer upon completion. For rotation, snapping is to 5° increments by default (configurable).

### 6.5. Geometry Operators
| Operator | Shortcut | Description |
|----------|----------|-------------|
| **Extrude (Face)** | `Ctrl+E` | Pull a face along its normal, generating new side geometry. |
| **Add Point on Edge** | `Ctrl+Click` on edge | Insert a new vertex at the click position (snapped to grid). |
| **Connect Points** | `J` | Draw a new edge between two selected vertices, splitting faces. |
| **Merge Points** | `M` | Merge selected vertices into one at their average (snapped). |
| **Divide (Edge/Face)** | Right‑click > Divide | Subdivide an edge or face into equal segments (grid‑snapped). |

---

## 7. Texturing System

### 7.1. Face Groups
A **Face Group** is a connected set of coplanar triangles within a single brush that share the same Material ID. Textures are applied to the whole Face Group, not to individual triangles.

### 7.2. Texture Browser
- Import `.dds` files by drag‑and‑drop into the browser window. The files are copied to the project's `textures/` folder.
- Thumbnail previews, search/filter by name.

### 7.3. Applying Textures
- **Face Selection Method**: Enter Edit Mode, switch to Face Mode (`3`), click a Face Group, then click a texture in the browser.
- **Paint Method**: Press `T` to enable Paint Mode, select a texture, then click directly on faces in the 3D viewport.
- **Sequential Method**: Use **Next Face** / **Previous Face** commands to cycle through all Face Groups of the current brush.

### 7.4. Planar Projection Attributes
Each Face Group stores:
| Attribute | Description |
|-----------|-------------|
| **Origin** | Integer `(X,Y,Z)` where `(U=0, V=0)` is anchored. |
| **U‑Axis** | Integer vector defining horizontal tiling direction. |
| **V‑Axis** | Integer vector defining vertical tiling direction. |
| **Material ID** | Index into the global Material Table. |
| **Scale** | `scaleU`, `scaleV` – texture tiling factors. |

### 7.5. UV Generation (Integer Math)
```
u = dot( (Vertex - Origin), U_Axis ) / scaleU
v = dot( (Vertex - Origin), V_Axis ) / scaleV
```
Since all inputs are integers, UVs are perfectly rational, ensuring seamless alignment across adjacent coplanar faces.

### 7.6. Split Face for Multi‑Texturing
1. Select a Face Group.
2. Right‑click > **Split**.
3. Draw a grid‑snapped line across the face.
4. The engine inserts vertices, retriangulates, and creates **two new Face Groups**.
5. They inherit U‑Axis, V‑Axis, and Normal from the parent; Origin is recalculated independently.
6. Apply different textures to each group – they align perfectly at the seam.

### 7.7. Tiling Adjustment
Select a Face Group and adjust `scaleU` / `scaleV` in the Inspector. The texture updates in real‑time, and no seams appear at group boundaries.

---

## 8. Baking Pipeline

Baking converts editable Source Brushes into the immutable Runtime World. The engine employs a **dual‑mode** baking strategy: local (for Solid viewport feedback) and global (for final Baked / Baked Lit previews).

### 8.1. When to Bake
- **Local CSG (Solid Mode):** Triggers automatically when an edited brush intersects a Sub brush. The engine performs a spatial AABB query to find overlapping Add brushes, unions them locally, and subtracts the Sub. This is debounced (100ms) and runs on a background thread, completing in < 1ms.
- **Global CSG (Baked / Baked Lit):** Triggered manually (`Ctrl+B`) or via idle timer (1.5s for Baked, 3s for Baked Lit). This processes the entire level.

### 8.2. Global Baking Steps (Editor‑Visible)
1. **Validation** – checks for:
   - Closed manifold (no open edges, no non‑manifold vertices).
   - No planar brushes (all brushes have volume > 0).
   - All faces have a material assigned (or default material).
   - No degenerate triangles.
2. **Sorting** – brushes sorted by `Time` (the infinite solid is always at Time 0).
3. **CSG Evaluation** – 
   - **Step A:** Union all Add brushes using the Manifold library (elalish).
   - **Step B:** Sequentially subtract all Sub brushes from the accumulated result.
4. **Triangulation & Face Group Merging** – resulting mesh is triangulated; coplanar adjacent triangles with same material are merged.
5. **UV Compilation** – planar projection formula applied to each Face Group. For face groups that originate from CSG cuts, the origin and axes are computed from the plane equation.
6. **Physics & Navigation Generation** – collision hull (the same mesh) and navmesh are built.
7. **Output** – serialized to `terrain.ter` and `navigation.nav`.

### 8.3. Baking Interface
- **Bake Button** – executes the full global pipeline.
- **Progress Dialog** – shows status of each stage.
- **Error Log** – lists any validation failures (prevents baking).
- **Bake Preview** – viewport switches to show the baked geometry.

### 8.4. Async Compile & Throttling Optimizations
To ensure the viewport remains fluid even with thousands of brushes, the engine employs a multi‑layered optimization strategy:

1. **Chunked Compilation:** The scene is divided into hierarchical Groups. Local CSG is evaluated exclusively at the Group level. Editing a brush inside a Group only invalidates and recompiles that single Group.
2. **Per‑Group GPU Pages:** Each Group owns a dedicated, small DirectX 11 vertex/index buffer. During an update, only the affected Group's buffer is re‑uploaded (`UpdateSubresource`), preventing global pipeline stalls.
3. **Manifold Delta Swapping:** The engine retains the `Manifold` result of every brush. On an edit, it subtracts the old brush geometry and unions the new geometry (O(log n) instead of O(n²)).
4. **Leap‑Frog Job Queue:** A single‑slot background queue discards stale builds. If the user performs 10 edits, only the final state is compiled.
5. **Conditional Subsystems:** Collision BVH and Recast Navmeshes are **excluded** from the Solid‑mode auto‑bake. They are only rebuilt during explicit "Baked Lit" previews or Playtest launches, saving 60% of the background CPU cost.

---

## 9. Project Management

### 9.1. Saving
When you hit **Save**:
- `terrain_source.ters` – current CSG brush data (binary format similar to terrain.ter but with source brush info).
- `terrain.ter` – background bake (if enabled).
- `navigation.nav` – background bake (if enabled).
- `entities.bin` – entity placements (if in Entity Mode).
- `scripts/*.lua` – saved directly.
- `config.ini` – editor settings (grid size, theme, shortcuts).

### 9.2. Exporting for Distribution
Use **Export Packaged** – the Build Tool creates a `.pak` archive (LZ4 compressed) with a TOC.

### 9.3. Version Control
- Text files (`mission.cfg`, `.lua`, `config.ini`) – diffable.
- Binary files (`.ters`, `.ter`, `.bin`) – store in Git LFS.
- Team members can work on separate missions or scripts with minimal conflicts.

---

## 10. Keyboard Shortcuts (Editor‑Specific)

| Shortcut | Action |
|----------|--------|
| `Tab` | Enter/exit Edit Mode |
| `1`, `2`, `3` | Vertex, Edge, Face Mode |
| `T` | Toggle Paint Mode (tool) |
| `G` | Move/Grab (opens numeric input in Inspector) |
| `R` | Rotate (opens numeric input) |
| `S` | Scale (opens numeric input) |
| `Ctrl+E` | Extrude |
| `J` | Connect Points |
| `M` | Merge Points |
| `Ctrl+G` | Group selected |
| `Ctrl+B` | Bake Level (Global) |
| `F5` | Test in Game (launches the game executable with the current mission) |
| `~` | Toggle console |
| `Alt+1` | Solid Mode |
| `Alt+2` | Baked Mode |
| `Alt+3` | Baked Lit Mode |
| `Alt+4` | Triangles Mode |

All shortcuts are customisable via **Settings > Keyboard Shortcuts** (stored in `config.ini`).

---

## 11. Workflow Summary (Concept to Playable)

1. **World Mode** – start with the **infinite solid**. Place **Add** brushes to form the structural mass (walls, floors, ceilings). Use **Sub** brushes to carve out rooms, doorways, and windows.
2. **Layer Stack** – reorder brushes to resolve CSG conflicts. Sub brushes should sit above the Add brushes they carve.
3. **Solid Mode** – edit geometry in real‑time. Add brushes move freely (no CSG). Sub brushes trigger local CSG, carving holes instantly when they intersect Add geometry.
4. **Texturing** – import textures, apply to Face Groups, use Split for multi‑texturing.
5. **Entity Mode** – place lights, triggers, AI patrol points, and interactive objects (loot, switches).
6. **Baked Preview** – press `Ctrl+B` or pause briefly to see the fully unioned and subtracted global geometry.
7. **Baked Lit** – preview the final lighting and shadows.
8. **Playtest** (`F5`) – iterate.
9. **Export** – package as `.pak` for shipping.

---

*End of Quantized CSG Engine Documentation – Two‑Part Guide (Revisited).*