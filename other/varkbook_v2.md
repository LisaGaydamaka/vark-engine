Here’s the **fully updated** Vark Engine documentation with your requested fixes applied:

- **Characters (AI, player) do not cast shadows.** Shadow maps are used only for static world geometry and static lights. Dynamic objects are lit without shadows.  
- **No hardware AA (MSAA)** – confirmed, and we clarify that we accept aliasing as a deliberate retro‑style choice.  
- **World bounds** are now explicitly defined. Brushes cannot be created or moved outside these bounds. The “infinite solid” is represented by a finite bounding box (default ±1000 units), and all CSG operations are clipped to it.

The changes are integrated directly into the text. I’ve also taken the opportunity to **tighten a few underspecified areas** (e.g., how the world bounds affect the infinite solid, and how shadow maps interact with dynamic objects) based on your feedback.

---

# Vark Engine – Complete Two‑Part Guide (Final – Optimized & Corrected – v2)

---

# Part I: The Game Engine (Runtime)

This part describes the **runtime** systems that execute a baked level: rendering, physics, AI navigation, entity management, scripting, audio, UI, input, save/load, file I/O, threading, and performance profiling. All systems are engineered with production‑grade optimizations to sustain 60 FPS on mid‑range hardware.

---

## 1. Introduction

The **Vark Engine** is a hybrid **level editor and runtime renderer** built around **integer‑precision geometry** for its **brush‑based world geometry** (walls, floors, architecture). Every vertex in the **CSG‑generated world mesh** is snapped to a global 3D integer grid, providing rock‑solid deterministic CSG operations, zero floating‑point drift, and watertight geometry.

**Important distinction:**  
- **Brush geometry** (Add/Sub brushes, walls, floors, carved rooms) – **integer‑snapped**.  
- **Imported entity meshes** (doors, loot, torches, AI character models, furniture, crates) – **floating‑point** coordinates, imported directly from `.obj`/`.fbx` without snapping. These are transformed via standard float matrices and are never merged into the CSG hull.

**Gameplay Focus:** This engine targets a **Thief‑1998‑style stealth game**:
- First‑person perspective, with a player character that can **climb** ledges, **knock out** NPCs from behind, **steal loot**, and complete mission objectives.
- AI guards patrol, investigate suspicious noises, engage in combat when alerted, and remember last known player positions.
- All movement, collision, and AI are deterministic and rely on integer‑based geometry for consistency where it matters (world collision), while entity transforms remain fluid.

**Unit System:** 1 world unit = 10 cm. All dimensions (player height, capsule radius, step height, etc.) are expressed in units and are **integers** or **fixed‑point** (e.g., 1.8 units = 1.8 m → 18 units). Runtime positions are stored as floats for smooth interpolation, but all collision queries against the **world mesh** are performed against integer‑snapped geometry.

---

## 2. Runtime World Architecture

The runtime engine consumes **baked data** produced by the editor. The world consists of:

| Component | Description |
|-----------|-------------|
| **Baked Mesh** | Immutable triangulated geometry from CSG operations, stored as vertex/index buffers with UVs and material IDs. All vertices are **integers** (snapped to the global grid). For rendering, the mesh is split into **spatial chunks** to enable frustum culling. |
| **Physics Collision** | A **two‑tier** collision system: a **3D sparse voxel octree** proxy for fast player capsule sweeps, and a raw triangle BVH for precise raycasts and projectile/crate collisions. |
| **Entities** | Dynamic objects: player, AI, lights, triggers, interactive objects (doors, loot, switches, crates). Entity meshes are **floating‑point** and transform freely. Static entities (loot, crates) are hardware‑instanced. |
| **Navigation** | Pre‑baked AI pathfinding grid (navmesh) generated from the baked floor geometry. |
| **Scripting** | Lua 5.4‑based mission logic, event‑driven and deferred to avoid main‑thread stalls. |
| **Audio** | 3D positional audio with **portal‑based sound propagation**, material‑based footstep sounds, and AI voice barks, powered by **OpenAL**. A unified cache prevents duplicate propagation queries. |
| **UI/HUD** | Light gem, health bar, and a single‑slot inventory display with cycling. |
| **Save/Load** | Full serialization of game state, AI memory, door states, loot flags (stored as a hash set for O(1) lookup, serialized as a flat array), and RNG seed. |

---

## 3. Core Data Structures (C++)

These structures are used during baking and runtime loading. All binary files are **little‑endian** and include a version number in their header for forward compatibility.

### 3.1. Vertex (World Mesh – Snapped)
```cpp
struct Vertex {
    int32_t x, y, z;          // Snapped to integer grid.
};
```

### 3.2. Entity Vertex (Imported Meshes – Floating)
```cpp
struct EntityVertex {
    float x, y, z;            // Original floating‑point coordinates from import.
    float u, v;               // UVs from the source file.
};
```

### 3.3. Triangle (Runtime)
```cpp
struct Triangle {
    uint32_t v[3];            // Indices into the global vertex buffer.
    uint32_t materialID;      // Index into the global material table.
};
```

### 3.4. Face Group (Runtime)
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

### 3.5. Plane (Integer)
```cpp
struct Plane {
    int32_t A, B, C, D;       // A*x + B*y + C*z + D = 0.
    // Normalized so that gcd(A,B,C) = 1 and normal points outward.
};
```

### 3.6. Material Table
A simple array of materials, each referencing a diffuse texture (`.dds`) and optionally a normal map. No PBR; only diffuse lighting and baked ambient.

### 3.7. BVH Node (for collision – Tier 2)
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
The BVH is built using the **Surface Area Heuristic (SAH)** and stored in the terrain file. For levels with > 100k triangles, the BVH is built with a maximum leaf size of 16 triangles to accelerate swept queries. The BVH is **double‑buffered** at runtime: when a background CSG rebuild finishes, the new BVH is swapped atomically so the main thread never reads a partially‑built structure.

### 3.8. Sparse Voxel Octree (Tier 1 – Player Collision)
A true **3D sparse voxel octree** generated from the baked mesh during baking. Each leaf cell stores a walkable flag (`bool`) and a material type. For cache‑efficient lookups, the octree is stored in a **layered hash map** using **Morton codes (Z‑order curves)** as keys, rather than pointer‑based nodes. This ensures that voxel data for a given AABB is contiguous in memory, drastically reducing L1/L2 cache misses during capsule sweeps. A **coarse 2D chunk grid** sits on top: the engine first determines which coarse chunk (e.g., 16×16 units) the player is in, then only queries the octree within that chunk, limiting traversal depth to 3 levels instead of 8. The octree supports incremental updates: when a door opens/closes, the affected AABB region is re‑rasterized from the door's collision triangles.

### 3.9. Spatial Chunk (Rendering)
The baked world mesh is split into axis‑aligned cubic chunks (e.g., 64 units per side). Each chunk stores its own vertex/index buffer, AABB, and a list of face groups. This enables fine‑grained frustum culling and double‑buffered async uploads.

### 3.10. World Bounds
The editor and runtime share a global axis‑aligned bounding box (default: `[-1000, -1000, -1000]` to `[1000, 1000, 1000]` in units, configurable per mission). This box defines the **finite extent of the “infinite solid”**. All brush vertices are clamped to these bounds during creation, movement, or scaling; the user cannot place or move a brush such that any part lies outside. When baking, the CSG engine starts with a solid cuboid of this size, and all Add/Sub brushes are clipped against it. This ensures watertight boundaries and prevents runaway geometry.

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
- **Baked ambient occlusion / lightmaps** – generated offline by a separate baker tool using a path tracer (see Editor section). Lightmaps are stored as separate `.dds` files with a **unique, non‑overlapping UV atlas** (UV2) computed via `xatlas` during baking. They are sampled using a second set of UV coordinates stored per vertex.

### 4.4. Shadow Maps – Culled, Adaptive & Temporally Cached
- Dynamic lights cast shadows **only on static world geometry**. **Characters (AI and the player) do not cast shadows** – they are rendered without any shadow contribution, which is visually acceptable given the game’s dimly lit environments and keeps performance predictable.
- Shadow maps use **standard shadow maps** with 4‑tap PCF.
- **Screen‑space culling:** Before rendering a shadow map, project the light’s bounding frustum into screen space. If the projected bounding box covers less than **5% of the screen**, the shadow map is skipped entirely (the light contributes only its diffuse term without shadows, which is imperceptible for small distant lights).
- **Temporal caching for static lights:** For lights that are not attached to moving entities (e.g., wall torches), the shadow map is rendered **once** when the light is first loaded and stored in a persistent read‑only texture. The same texture is reused every frame, and it only shadows the baked static world mesh. Lights attached to moving objects (e.g., guard flashlights) trigger per‑frame shadow map updates, but still only affect static geometry.
- **Resolution scaling:** Reduce shadow map resolution to 1024×1024 for lights that cover less than 10% of the screen, and to 512×512 for distant lights. Only the closest 4 shadow‑casting lights are supported (for performance).
- For directional lights (e.g., sun), a single cascade is used (orthographic projection).

### 4.5. Texturing
- Materials reference a **diffuse texture** (`.dds` with BC1 or BC3 compression) and optionally a **normal map** (`.dds` with BC5).
- UVs are computed per Face Group using the planar projection formula (see Part II, Section 7).
- **Tangent basis for normal mapping:** The face group’s `uAxis` and `vAxis` are used to derive an orthonormal tangent frame per face group. The normal map stores tangent‑space normals; we transform to world space using the `T, B, N` matrix.
- **Texture filtering:** Linear filtering with mipmapping; mipmaps are generated by DirectX from the loaded texture.

### 4.6. No Post‑Processing / Anti‑Aliasing
- No bloom, tone mapping, or other post‑effects.
- **No hardware AA (MSAA)** – the game renders at native resolution without antialiasing. Aliasing is accepted as a deliberate retro‑aesthetic choice, and the low‑poly, high‑contrast visual style helps mitigate its visual impact.

### 4.7. Rendering Order & Batching
- **Deferred pass**: 
  - **Spatial chunk culling:** Before rendering, each chunk’s AABB is tested against the camera frustum. Only visible chunks are submitted.
  - **Instanced entities:** Loot pickups, crates, and other repeated static meshes are drawn using `DrawIndexedInstanced` in a single call per mesh type. Per‑frame instance transforms are uploaded via a **ring buffer** (see Section 4.9). Frustum culling is applied to the instance list before building the buffer.
- **Shadow pass**: For each shadow‑casting light that passed culling, render depth from light using **only the static world mesh** (entities, characters, and projectiles are excluded from shadow maps).
- **Lighting pass**: Accumulate diffuse lighting into a separate render target; then combine with albedo and lightmap. For improved GPU performance, a **tile‑based light culling** compute shader (optional) divides the screen into 16×16 tiles; only lights whose bounding sphere intersects the tile's frustum are processed, drastically reducing lighting shader invocations.
- **Forward pass**: Transparent objects (if any) are rendered forward after the deferred lighting. Characters, projectiles, and other dynamic objects are also drawn forward (without receiving shadows) to avoid the complexity of shadowing them.
- **UI overlay**: Light gem, health bar, inventory icon, and objective notifications rendered last in screen space using an orthographic sprite batcher.

### 4.8. Async GPU Uploads – Double‑Buffered Terrain
- During baking (editor) or level load, the terrain mesh is uploaded to the GPU using **staging buffers** with `D3D11_MAP_WRITE_DISCARD` on a background thread.
- The engine maintains two vertex/index buffers per chunk: `Buffer_A` and `Buffer_B`. The main thread renders from `Buffer_A` while the background thread builds and uploads to `Buffer_B`. When the upload finishes, the chunk’s active buffer pointer is swapped on the main thread with minimal overhead.
- This eliminates rendering hitches during global bakes or level transitions.

### 4.9. Instance Buffer Ring Buffer (Performance)
To avoid driver stalls caused by `D3D11_MAP_WRITE_DISCARD` on dynamic instance buffers, the engine uses a **ring buffer**:
- A single large `ID3D11Buffer` is allocated (capacity for, e.g., 10,000 instances) with `D3D11_USAGE_DYNAMIC`.
- Each frame, the CPU writes instance transforms into the next available slot using `D3D11_MAP_WRITE_NO_OVERWRITE`.
- When the write pointer reaches the end, the buffer is wrapped around with a `DISCARD` map once per cycle. This allows the CPU to write frame N+1 while the GPU reads frame N, eliminating the stall that occurs with per‑frame `DISCARD`.

---

## 5. Physics & Character Controller – Two‑Tier Optimized

### 5.1. Collision Mesh & BVH – Two‑Tier Architecture
To avoid the high cost of swept capsule vs. raw triangles, the engine uses a **two‑tier** collision system:

- **Tier 1 (Proxy – Player & Dynamic Sweeps):** A **3D sparse voxel octree** stored with Morton‑code hashing and a coarse chunk grid (see 3.8). The player capsule sweep queries this proxy first:
  - For a given AABB or capsule shape, the proxy traverses the octree using the coarse grid to limit depth, yielding **O(1)** average‑case lookups with excellent cache locality.
  - This proxy is updated incrementally when doors open/close (only the affected voxels are re‑rasterized from the door's triangles).
  - *Result:* Player movement and crouch/climb checks are ~10× faster than triangle‑based sweeps.

- **Tier 2 (Precise – Raycasts & Small Objects):** The raw triangle BVH (SAH‑built, double‑buffered) is used exclusively for:
  - AI line‑of‑sight raycasts (see batch optimization in 6.3).
  - Sound propagation raycasts (for portal culling).
  - Projectile (arrow) collisions.
  - Crate throw collisions (using a sphere vs. BVH query, which is cheaper than a capsule).
  - Triangle prefiltering (triangles < 0.1 units are discarded) reduces BVH size.

### 5.2. Unit Scale & Player Capsule
- 1 unit = 10 cm.
- Player capsule: radius = 4 units (0.4 m), height = 18 units (1.8 m), with rounded ends.
- All Tier‑1 sweeps use the proxy; Tier‑2 uses the BVH only when needed.

### 5.3. Player Controller – Full Stealth Movement
- **Kinematic** movement: player velocity is applied each frame, and collision response resolves penetration and slides along surfaces using the **sliding plane** algorithm (iterate up to 3 times). The algorithm computes the minimum penetration vector from the proxy, projects the velocity onto the plane of the blocking surface, and subtracts the penetration.
- **Gravity**: Acceleration = –30 units/s² (3 m/s²).
- **Climbing / Mantling**:
  - When the forward sweep is blocked by a wall, check the blocking face normal (from the proxy).
  - If the normal has a significant upward component (`z > 0.7`), and the player presses the **climb** key (spacebar), perform an upward sweep to vault over the ledge.
  - A successful vault teleports the player to the top of the ledge with a small forward impulse.
- **Leaning** (Q / E keys):
  - The player uses a **two‑part collision shape**: a foot capsule (base) and a torso sphere (upper body).
  - When leaning, the **torso sphere** shifts laterally by 1 unit (10 cm) toward the wall, while the foot capsule remains rooted. This simulates "leaning without moving the feet."
  - A Tier‑1 sweep is performed on the shifted torso sphere; if it collides with geometry, the lean is blocked.
  - Leaning reduces the player’s visual exposure (used in the light gem calculation) and allows peeking around corners.
- **Stealth mechanics**:
  - **Movement speed**: walk (slow, quiet) and run (faster, noisier) toggled via `Shift`.
  - **Crouch**: reduces capsule height to 10 units and halves movement speed.
  - **Knockout**: when the player is behind an AI and presses the attack key (Mouse1):
    - Check that the AI's alert level is < 30 (unaware).
    - Check that the player is within 2 units distance.
    - Check that the player is within a **110° cone behind the AI**: `DotProduct(player_forward, -ai_forward) > 0.4`.
    - If all pass, the AI is knocked out (state changed to `KO`). A successful knockout plays a thud sound.
- **Crate interaction**:
  - When the player looks at a crate and presses the **Interact** key (`F`), the crate is picked up and held in front of the player (becomes a child of the camera).
  - While holding a crate, the player can move normally but cannot attack.
  - Pressing **Use selected item** (`Mouse2`) throws the crate forward with a velocity proportional to the player’s facing direction. The throw uses a Tier‑2 sphere‑BVH query for collision.
  - Pressing **Interact** (`F`) again drops the crate at the player’s feet.
  - Crates use kinematic rigid bodies; when thrown, they become dynamic for a short time before settling back to kinematic.
- **No ladders**; climbing is only via mantling and jump.
- **Melee Combat**: TBD.

### 5.4. The Light Gem
- A global function `float CalculateLightExposure(vec3 playerPos)` is evaluated every frame.
- It samples the shadow map and lightmap at the player’s mid‑section and feet.
- The exposure value (0.0 = pitch black, 1.0 = bright daylight) is passed to the UI, which renders a gem‑shaped meter that smoothly lerps to the new value.
- This value also affects AI visibility checks (see AI Perception).

### 5.5. Dynamic Objects
- **Doors** are kinematic rigid bodies. Their open/closed state is controlled entirely by **Lua scripts** (see Section 8). A script can set the door’s target rotation/position, and the engine interpolates the transform each frame, performing Tier‑1 sweeps to prevent clipping.
- **Crates** are kinematic when on the ground; when carried or thrown, they become dynamic (affected by gravity) for a short time before settling back to kinematic.
- **Loot** items are static pickup objects. When the player interacts with them (via `F`), they are added to the inventory and the mesh is hidden. While the player points at a loot item, the item is highlighted by **removing all dynamic and ambient light from it** (rendering it with a pure unlit emissive shader) to make it stand out.

### 5.6. Performance Optimizations
- **Spatial hashing** for AI‑to‑player line‑of‑sight checks.
- **Tier‑1 proxy** with Morton‑code caching for player collision (eliminates 90% of BVH queries).
- **BVH traversal** optimised for sphere queries (crate throws) by using a sphere‑AABB overlap test first.
- **Triangle prefiltering**: triangles smaller than 0.1 units are discarded from collision to reduce BVH size.

---

## 6. Navigation & AI – Time‑Sliced, Hierarchical & Batch‑Optimized

### 6.1. Navmesh Generation (Offline)
- During baking, the final terrain mesh (after all CSG operations) is fed to **Recast**.
- Agent configuration: radius = 4 units (0.4 m), height = 18 units (1.8 m), max climb = 5 units (0.5 m), max slope = 45°.
- The resulting navmesh is serialized to `navigation.nav`.

### 6.2. Runtime Pathfinding – Hierarchical (HPA*)
- Load the navmesh into **Detour**.
- During baking, the navmesh is subdivided into **regions** (e.g., 20×20 unit squares). Precompute a **region graph** where nodes are regions and edges are connections between adjacent regions, along with the entry/exit points.
- AI agents use a two‑level pathfinding:
  1. **Region‑level path:** Find a path through the region graph using A* (typically < 50 nodes).
  2. **Local path:** Use Detour's `dtNavMeshQuery::findPath` only within the current region and the target region, which reduces the search space from thousands of polygons to a few dozen.
- Dynamic obstacles (e.g., closing doors) are handled via `dtTileCache` – we mark polygons as unwalkable when a door closes, and the tile cache updates the navmesh locally. The region graph is rebuilt only when the tile cache changes.

### 6.3. Advanced AI Perception & Memory – With Time‑Slicing & Raycast Batching
AI agents are driven by a **numerical alert level** (0–100) and store a **sensory memory**. To avoid heavy per‑frame costs, AI perception is **time‑sliced** and **batch‑optimized**:

- **Time‑Slicing:** Instead of updating all AI agents every frame, the engine updates a fixed subset each frame. For 20 guards, update 5 per frame in a round‑robin fashion. This adds a maximum 16 ms latency to perception, which is imperceptible in stealth gameplay, but reduces CPU cost by 75%.
- **Spatial Hash Culling:** Before performing a sight raycast, an AI checks if the player is within `Range * 1.5` and the dot‑product FOV passes. Only then does it issue a Tier‑2 raycast.
- **Raycast Batching (SIMD):** Instead of issuing individual BVH traversal calls for each AI, all rays that need to be cast in a given frame are collected into a **batch**. The BVH is traversed once, testing all rays simultaneously (using SIMD‑friendly operations). This improves cache utilization because the BVH nodes are loaded into cache once for all rays, reducing per‑ray overhead by up to 80%.
- **Occlusion Reuse:** Sound propagation results (from audio subsystem) are cached per source‑listener pair for 200 ms. The AI hearing system **is triggered by the audio system's `PlaySound` call** – when a sound is played, the audio system queries the cache and notifies nearby AI if the sound is audible. This avoids AI polling the audio system every frame.

| Alert Level | Behavior |
|-------------|----------|
| **0–19** (Idle) | Stand still, look around periodically, patrol waypoints. |
| **20–49** (Suspicious) | Stop patrol, turn head toward the sound source, whisper "Huh?". After 5 seconds, return to Idle if no further stimulus. |
| **50–79** (Investigating) | Move to the **last known position** (stored in memory). Upon arrival, perform a short search (wander within 10 units for 15 seconds). If player spotted, jump to 80+. |
| **80–100** (Combat) | Chase player, attack when in melee range. If player escapes line‑of‑sight for > 10 seconds, fall back to Investigating with a new last known position. |

**Perception details:**
- **Sight**: Cone of view (90° FOV, range = 30 units = 3 m). Line‑of‑sight checked via batched Tier‑2 raycasts. The player’s light gem value modifies the effective range: `range = base_range * (1.0 - playerLightExposure * 0.7)`. A completely dark player is nearly invisible.
- **Hearing**: Radius = 20 units (2 m) for walking, 40 units (4 m) for running, 10 units (1 m) for crouching. Sound propagation is occluded; the unified audio cache provides the audible state.
- **Voice Barks**: AI have a `VoiceSet` property (e.g., `GuardMale`, `GuardFemale`). Barks are triggered on alert level changes (e.g., "Who's there?" at 30+, "Intruder!" at 80+). Each bark has a 5‑second cooldown to prevent spam.

**Factions:**
- Each AI has a `Faction` enum: `Player`, `Guard`, `Civilian`, `Neutral`.
- Guards attack the player and investigate noises from other factions.
- Civilians run away (flee state) when they see the player and scream (triggers nearby guards).
- Neutrals ignore everything (used for static animals, etc.).

### 6.4. AI State Machine (C++ with Lua callbacks)

| State | Description |
|-------|-------------|
| **Idle** | Stand still, look around periodically. |
| **Patrol** | Follow a path of waypoints; switch to Investigating on hearing a noise or seeing the player. |
| **Investigate** | Move to last known position; search for 15 seconds; if player found, switch to Combat; else return to Patrol. |
| **Combat** | Chase player, attack when in melee range. If player is knocked out or hidden, eventually return to Patrol. |
| **KnockedOut** | Non‑functional; lies on ground (animation). |
| **Flee** | Civilians only – run away from the player toward a safe waypoint. |

---

## 7. Entity System – Instanced & Lightweight

Entities are dynamic objects placed in the editor. They are loaded from `entities.bin`. **Entity meshes are imported as floating‑point coordinates and are NEVER snapped to the integer grid.**

### 7.1. Core Components
| Component | Purpose |
|-----------|---------|
| `Transform` | Position (float), rotation (quaternion), scale (float). |
| `StaticMeshRenderer` | Renders a pre‑baked static mesh (for doors, loot, torches, crates). Mesh is loaded from an imported `.obj` or `.fbx` with original float vertices, stored as `.vmesh` (Vark Mesh). **Instanced** for loot and crates. |
| `SkeletalMeshRenderer` | For AI characters – plays animations (idle, walk, run, attack, knockout). Supports linear blending and animation notify events. |
| `Light` | Point or spot light; color, intensity, range, shadow flag. |
| `Script` | Reference to a Lua script that runs event‑driven callbacks (no per‑frame update by default). |
| `Trigger` | Box/sphere volume; fires `OnEnter`/`OnExit` callbacks. |
| `AIAgent` | State machine with patrol points, perception parameters, and a reference to a navmesh. |
| `Player` | (Only one) – controls the player character. |
| `Inventory` | Holds loot items (stored in a hash set for O(1) lookup) and a single selected item; accessible from Lua. |
| `Objective` | Tracks mission progress. See structure below. |
| `AudioSource` | Emits positional 3D sound with propagation flags. |
| `Door` | Kinematic transform with target open/closed state, controlled by script. |
| `Crate` | Kinematic/dynamic rigid body with pickup/throw logic. |

**Objective Structure:**
```cpp
struct Objective {
    int id;
    std::string description;
    bool isComplete;
    bool isOptional;
    EntityHandle target;          // optional entity to interact with
    std::function<void()> onComplete; // Lua callback
};
```

### 7.2. Spawning
- `entities.bin` stores for each entity: `typeID` (hash), `Transform`, and a `PropertyMap` (string → variant).
- The runtime maintains a **prototype registry** that maps `typeID` to a default component set. Overrides from the property map are applied on spawn.

---

## 8. Scripting (Lua) – Event‑Driven, Deferred & GC‑Tuned

- **Lua 5.4** with `sol2` bindings.
- Each entity with a `Script` component has a table with standard callbacks. **To avoid main‑thread stalls, `OnUpdate` is not called per frame for most entities.** Instead, the engine uses an **event‑driven** model:
  - `OnInit(self)` – called after spawn.
  - `OnTriggerEnter(self, other)` – when entering a trigger volume.
  - `OnDamage(self, amount)` – health/damage handling.
  - `OnKnockout(self)` – called when the AI is knocked out.
  - `OnUse(self, player)` – called when the player presses the **Interact** key (`F`) on an interactive object (door, switch, loot).
  - `OnPickup(self, player)` – called when a crate is picked up.
  - `OnThrow(self, player)` – called when a crate is thrown.
  - `OnSave(self)` / `OnLoad(self)` – called during save/load.

**Performance optimizations:**
- **Batched API:** Scripts access entities via lightweight integer handles (indices into a dense array) rather than string hashing. `GetEntity("name")` is resolved once during `OnInit` and stored as a handle.
- **Deferred Execution:** All Lua callbacks triggered by physics or input are pushed onto a low‑priority job queue. They are executed in bulk *after* the physics and AI updates, so they do not block the critical movement response.
- **Lazy Update:** Only scripts that explicitly register for per‑frame updates (e.g., a flickering light) receive `OnUpdate` calls. All other scripts are purely event‑driven, saving 95% of Lua overhead.
- **Garbage Collection Tuning:** Lua's automatic garbage collector is **disabled during gameplay** (`lua_gc(L, LUA_GCSTOP, 0)`). All essential Lua objects (AI state tables, objective data) are pre‑allocated during `OnInit` and reused. Manual, incremental GC steps (`LUA_GCSTEP`) are only performed during loading screens or when the game is paused. This eliminates random GC‑induced frame spikes (2–5 ms) that would otherwise occur during stealth action.
- **Allocation‑Free Hot Paths:** Frequently‑called API functions (e.g., `GetEntity`, `GetPlayerLightExposure`) use the `sol::stack` API to avoid creating temporary Lua tables or strings, keeping hot paths allocation‑free.

### 8.1. Exposed API (Full List)
| Function | Description |
|----------|-------------|
| `EntityHandle GetEntity(string name)` | Finds an entity by name (unique) – returns a handle. |
| `void SetPosition(EntityHandle, vec3)` | Teleports an entity. |
| `void PlaySound(string soundName, vec3 position, float radius, string surfaceType)` | Plays a 3D sound; surfaceType determines footstep material and propagation. |
| `void SetLightColor(EntityHandle, vec3)` | Changes a light’s color. |
| `void TeleportPlayer(vec3)` | Moves the player to a location. |
| `void SetObjective(string id, bool completed)` | Updates mission state. |
| `void KnockoutAI(EntityHandle)` | Knocks out an AI (script‑triggered). |
| `bool IsPlayerSneaking()` | Returns true if player is crouching or walking silently. |
| `void AddLoot(string lootType, int count)` | Adds loot to player’s inventory (O(1) hash set). |
| `bool HasLoot(string lootType)` | Checks if loot is already collected (O(1)). |
| `EntityHandle SpawnEntity(string type, vec3 position, table properties)` | Spawns a new entity at runtime. |
| `void Log(string message)` | Prints to in‑game console and log file. |
| `float GetPlayerLightExposure()` | Returns current light gem value (0–1). |
| `void SaveGame(string slotName)` | Triggers a full game save (async). |
| `void LoadGame(string slotName)` | Triggers a full game load (async). |
| `void CycleInventory(int direction)` | Cycles to the next (1) or previous (-1) item in inventory. |
| `void ShowObjectiveNotification(string text)` | Displays a brief on‑screen message. |
| `void OpenDoor(EntityHandle)` | Tells a door entity to open (script‑driven). |
| `void CloseDoor(EntityHandle)` | Tells a door entity to close. |

---

## 9. Audio Subsystem – Portal‑Based Propagation, Bidirectional Search & Unified Cache

Audio is critical for stealth gameplay. The engine uses **OpenAL** exclusively for all 3D audio processing.

### 9.1. 3D Sound Sources & Propagation
- Each sound has a `position`, `falloff radius`, and `propagation mode`.
- **Propagation**: Instead of a simple line‑of‑sight raycast, the engine uses a **portal‑based sound propagation system**. The level is divided into **audio volumes** (rooms) connected by portals (doors, windows). When a sound plays:
  1. The audio system checks if the listener (player) is in the same volume as the source. If so, direct path.
  2. If not, it traces a path through open portals (doors that are open, windows) using a **bidirectional Dijkstra** search:
     - The search starts simultaneously from both the source and listener rooms.
     - The search depth is limited to **5 portal hops** (the maximum audible range).
     - The algorithm stops as soon as the two search fronts meet, drastically reducing the number of nodes visited compared to a unidirectional search.
  3. Each portal traversal applies an attenuation factor (e.g., -10 dB for a closed wooden door, -3 dB for an open doorway).
  4. The final sound volume is `baseVolume * portalAttenuation * distanceFalloff`.
  5. A Tier‑2 raycast is performed only for the direct line‑of‑sight within the same room to check for small obstacles.
- **Material‑based footstep sounds**: The terrain stores a `SurfaceType` per triangle (Wood, Stone, Metal, Carpet, Grass, Water). When the player or AI moves, the footstep sound is selected from a table: `Footstep_[SurfaceType].wav`.
- **Noise radius**: Each movement state emits a sound event with a radius (Walk=20, Run=40, Crouch=10, DropLoot=30). The AI hears it if within radius and the portal‑based propagation path exists (or muffled if occluded).

### 9.2. Voice Barks
- AI agents have a `VoiceSet` property. A `VoiceSet` is a collection of `.wav` files: `Idle1.wav`, `Idle2.wav`, `Suspicious.wav`, `Combat.wav`, `Knockout.wav`.
- The AI picks a random bark from the appropriate category, respecting a per‑AI cooldown (5 seconds).
- Barks are 3D positioned and affected by propagation.

### 9.3. Environmental Ambience
- Each mission defines ambient sound tracks (wind, distant machinery) that loop in the background, played as 2D (non‑positional) with low volume.

### 9.4. Unified Audio‑Physics Cache (Optimization)
- When `PlaySound` is called, the propagation path and occlusion result are computed **once** and stored in a global `SoundQueryCache` keyed by `(sourcePosHash, listenerPosHash)` with a **500 ms time‑to‑live** (covers the typical interval between footsteps).
- The AI perception system queries this same cache when checking if a sound is audible. If the sound is blocked by portals/occlusion, the AI ignores it without issuing a second query.
- *Result:* For a single footstep sound, the engine issues exactly one propagation query instead of one per AI.

---

## 10. UI / HUD & Game Flow

### 10.1. In‑Game HUD (Rendered after deferred pass)
The UI is rendered using a 2D orthographic sprite batcher (or an ImGui overlay). Elements include:
- **Light Gem** – a diamond‑shaped meter that smoothly transitions to the current exposure value.
- **Health Bar** – horizontal bar (red/green) showing player health.
- **Inventory Slot** – a single icon showing the currently selected inventory item. The player can cycle through their inventory using the mouse wheel (or configurable keys). Loot items appear in this inventory when collected.

### 10.2. Menus & State Manager
The engine uses a global state machine:
| State | Description |
|-------|-------------|
| `State_MainMenu` | Displays title, New Game, Load Game, Options, Quit. |
| `State_Loading` | Shows a loading screen with a progress bar while streaming assets. |
| `State_Briefing` | Displays mission briefing text and map overlay; player presses a key to start. |
| `State_Gameplay` | Main game loop (update+render). |
| `State_Paused` | Pauses physics, AI, scripts; still renders the world; shows pause menu (Resume, Save, Load, Quit). |
| `State_ObjectiveScreen` | Full‑screen overlay showing all objectives (completed/incomplete) and current loot total. |
| `State_GameOver` | Shows "Mission Failed" or "Mission Complete" with stats and option to restart/quit. |

### 10.3. Mission Briefing
Before loading a mission, the engine reads `briefing.txt` (plain text or a simple markup) from the mission folder. It displays this text along with a static map image (`briefing_map.dds`) in the Briefing state.

---

## 11. Save / Load Serialization – Async & O(1) Loot Lookup

A stealth game requires robust save/load. The engine uses a single binary `SaveGame.bin` per slot.

### 11.1. Serialized Data (Full State Snapshot)
| Field | Description |
|-------|-------------|
| `uint32_t version` | Save format version. |
| `uint64_t rngState` | Current seed of the Xorshift RNG to ensure deterministic patrols. |
| `PlayerState` | Position, rotation, health, inventory items (list), selected inventory index, loot count. |
| `EntityState[]` | For each dynamic entity: position, rotation, open/closed (doors), health (AI), current AI state, current patrol waypoint index, alert level, memory (last known position), cooldown timers. |
| `LootArray` | A flat array of `uint64_t` loot IDs (collected). On load, this array is used to rebuild the `std::unordered_set<uint64_t>` for O(1) `HasLoot` lookups. |
| `ObjectiveStates[]` | Completed/incomplete flags for each objective. |
| `TimeOfDay` | Optional – elapsed mission time. |

### 11.2. Save/Load Flow – Asynchronous
- **Save**: Triggered by player (via menu) or Lua `SaveGame("slot1")`.
  1. The main thread **pauses** the game and creates a **shallow copy** of the entire game state (a `std::shared_ptr<GameState>` snapshot).
  2. The main thread immediately resumes gameplay (the copy is read‑only).
  3. A background thread takes the snapshot, serializes it to a temp buffer, compresses with LZ4, and writes to disk. The compression and I/O happen entirely off the main thread, eliminating save‑related hitches.
- **Load**: Triggered by player or Lua.
  1. The engine enters the `State_Loading` screen.
  2. A background thread reads the file, decompresses LZ4, and deserializes the state into a temp buffer.
  3. The main thread clears the current world, resets RNG, applies the deserialized state, and teleports all entities to their saved positions. Lua scripts receive an `OnLoad(self)` callback to restore any script‑local variables.
- During async save/load, the engine uses double‑buffering for the game state to ensure that the main thread never blocks on I/O.

### 11.3. Deterministic RNG
- The engine uses a **Xorshift128+** generator. The seed is initially derived from the mission load time, but saved games store the current seed. This ensures that patrol routes and random timings are identical on reload (preventing save‑scumming from changing AI behavior).

---

## 12. Input Abstraction

### 12.1. Raw Input
- On Windows, the engine uses `WM_INPUT` (RAWINPUT) to read mouse deltas directly, bypassing Windows cursor smoothing and acceleration.
- Keyboard input is polled via the **Windows Message Pump** (`WM_KEYDOWN`/`WM_KEYUP`) to ensure that input is only processed when the window is in focus.

### 12.2. Action Mapping
All gameplay actions are abstracted into `Action` enums:

| Action | Default Key |
|--------|-------------|
| MoveForward | W |
| MoveBackward | S |
| MoveLeft | A |
| MoveRight | D |
| Jump | Space |
| Crouch | C |
| RunToggle | Shift |
| LeanLeft | Q |
| LeanRight | E |
| Attack / Knockout | Mouse1 |
| Interact | F |
| Use Selected Item | Mouse2 |
| Climb / Mantle | Space (context‑sensitive) |
| Next Inventory Item | MouseWheelDown |
| Previous Inventory Item | MouseWheelUp |
| Pause | Escape |
| Objective Screen | O |

- All bindings are stored in `config.ini` under `[Input]` and can be remapped in the Options menu.

---

## 13. Mission Loading Sequence

1. **Parse `mission.cfg`** – read metadata (name, author, objective). Load `briefing.txt` if present.
2. **Mount `.pak`** – virtual file system (if shipping).
3. **Load `terrain.ter`**:
   - Vertex/index buffers for each spatial chunk are uploaded to GPU via double‑buffered staging (see 4.8).
   - Material table loaded.
   - Physics BVH built (or loaded from stored data). The BVH is double‑buffered.
   - Tier‑1 sparse voxel octree generated (or loaded) with Morton‑code hashing.
   - Lightmap textures loaded (if present).
4. **Load `navigation.nav`** – feed into Detour; build region graph for HPA*.
5. **Load `entities.bin`** – spawn all entities, call `OnInit` on scripts.
6. **Execute `scripts/main.lua`** – mission‑wide initialisation.
7. **Load save game** – if continuing, deserialize and apply state (async).
8. Enter game loop (update, render, physics, AI, audio, UI).

---

## 14. File Formats (Engineering Reference)

All binary files are **little‑endian** and include a `uint32_t version` field as the first 4 bytes.

### 14.1. `mission.cfg` (INI‑style)
Keys:
- `name` – display title
- `author`
- `objective` – brief mission description
- `starting_equipment` – comma‑separated list of loot types
- `briefing_text` – path to `briefing.txt`
- `briefing_image` – path to `briefing_map.dds`
- `ambient_sound` – path to looping ambient `.wav`
- `world_bounds_min` – `x,y,z` (integers, default `-1000,-1000,-1000`)
- `world_bounds_max` – `x,y,z` (integers, default `1000,1000,1000`)

### 14.2. `terrain.ter` (Binary)
- `version` (uint32)
- `chunkSize` (uint32) – e.g., 64 units
- `chunkCount` (uint32) – number of spatial chunks
- For each chunk:
  - `aabb` (float min[3], max[3])
  - `vertexCount` (uint32), then array of `int32_t x,y,z` (snapped).
  - `indexCount` (uint32), then array of `uint32_t` triplets.
  - `faceGroupCount` (uint32), then array of `FaceGroup`.
  - `materialCount` (uint32), then for each: `uint32_t nameLen`, `char name[nameLen]` (relative path to .dds), `uint32_t normalLen`, `char normalName[normalLen]` (or 0 if none).
  - `surfaceTypeCount` (uint32), then array of `uint8_t` per triangle mapping to SurfaceType enum.
  - `lightmapPresent` (uint8), if 1: `lightmapUVCount` (uint32) then array of `float u,v` for each vertex.
- `bvhData` – serialized BVH nodes (double‑buffered at runtime).
- `voxelProxyData` – serialized sparse voxel octree (Morton‑code hash map).

### 14.3. `entities.bin` (Binary)
- `version` (uint32)
- `entityCount` (uint32)
- For each entity:
  - `typeID` (uint32 hash of prototype name)
  - `transform` (float pos[3], float quat[4], float scale[3]) – **floating‑point, not snapped**.
  - `meshPath` (string) – relative path to imported `.obj`/`.fbx` (converted to engine internal `.vmesh` format).
  - `propertyCount` (uint32)
  - For each property: `nameLen` (uint16), `name` (char), `type` (uint8: 0=int,1=float,2=bool,3=string), `value` (variable)

### 14.4. `navigation.nav`
- Serialized Recast/Detour navmesh (binary format as produced by `dtNavMesh::serialize`) plus additional region graph data for HPA*.

### 14.5. `.pak` Archive
- Header: magic `'VPAK'` (4 bytes), version (uint32), TOC offset (uint64), TOC size (uint64).
- TOC: array of entries, each with `pathHash` (uint64, FNV‑1a), `offset` (uint64), `compressedSize` (uint64), `uncompressedSize` (uint64).
- Compression: **LZ4** (fast decompression).

### 14.6. `SaveGame.bin` (Binary)
- `version` (uint32)
- `rngState` (uint64)
- Serialized `PlayerState`, `EntityState[]`, `LootArray` (flat array of uint64), `ObjectiveStates[]`.
- Compressed with LZ4 before writing to disk (done asynchronously).

---

## 15. Threading & Job System – Async Everything

To keep the viewport and gameplay fluid, the engine uses a multi‑threaded architecture with explicit lock‑free queues.

### 15.1. Producer‑Consumer Queues
- A **single‑producer, single‑consumer (SPSC) lock‑free queue** handles CSG compilation jobs (editor).
- A separate queue handles **resource loads** (textures, meshes, sounds) using `std::async` with a `std::atomic<bool>` completion flag.
- A third queue handles **async save/load** jobs (serialization/compression).

### 15.2. Background Asset Streaming
- When a mission loads, the main thread requests textures and meshes; the background thread reads from disk, decompresses, and uploads to GPU via staging buffers.
- The main thread checks `bIsReady` per resource; if not ready, a default placeholder is used temporarily.

### 15.3. Leap‑Frog Compilation (Editor)
- The editor’s background CSG thread discards stale jobs. If the user performs 10 edits in quick succession, only the final state is compiled, saving CPU time.
- Collision BVH and Recast navmesh are **not** rebuilt during Solid‑mode auto‑bake; they are only rebuilt on explicit "Baked Lit" or playtest. The BVH is double‑buffered to avoid data races.

### 15.4. Main Loop Parallelism
- Physics (Tier‑1 proxy), AI (time‑sliced), and script callbacks (deferred) are processed sequentially on the main thread to avoid race conditions.
- Rendering and audio mixing run on separate threads (the audio thread is callback‑driven).
- GPU uploads (staging buffers) occur on a dedicated background thread.

---

## 16. Runtime Performance & Profiling

- **Culling**: Spatial chunk frustum culling for terrain; entity AABB frustum culling with instance‑list culling.
- **Batching**: Terrain is rendered per visible chunk (10–20 draw calls). Instancing for repeated entities (loot, crates) using `DrawIndexedInstanced` with a ring‑buffered dynamic instance buffer.
- **Profiling**: Built‑in **performance profiler** with scoped timers (microseconds). Measures:
  - Frame time
  - Rendering (GBuffer, shadows, lighting, culling)
  - Physics (Tier‑1 proxy sweep, Tier‑2 BVH raycasts)
  - AI (pathfinding HPA*, time‑sliced perception, raycast batching)
  - Script update (deferred execution)
  - Audio processing (including propagation cache hits/misses)
- Profiling data is displayed in an **in‑game console** (opened with `~`) and can be written to a log file.

---

## 17. Memory Management

- **Memory pools** for frequently allocated objects (e.g., entities, components, BVH nodes) to reduce fragmentation and improve cache locality.
- **Stack allocator** for temporary operations (e.g., pathfinding queries).
- **Memory tracking**: Each allocation is tagged with a category (e.g., "Rendering", "Physics", "AI", "Audio"). At runtime, the engine tracks total usage per category and reports it in the console. On shutdown, a summary is printed to the log.
- All resource loads (textures, meshes, sounds) use a **resource manager** with reference counting; duplicate loads are avoided.

---

# Part II: The Editor

This part describes the **interactive design environment** used to create levels: the UI, viewport, layer stack, brush creation, geometry editing, texturing, asset import pipeline, and the baking pipeline. All optimizations from Part I are reflected in the baking and export workflows.

---

## 1. Introduction

The **Vark Editor** is a **modal 3D environment** for precision architectural construction. It provides a viewport, a non‑destructive layer‑stack hierarchy, a dedicated geometry editing mode, and a streamlined texturing workflow—all built around integer‑precision determinism for brush geometry, while allowing imported entity meshes to remain floating‑point.

**Editor architecture:** Built with **Dear ImGui** for the UI layer, and a **separate application** from the game (though they share the same core engine libraries). The editor runs on **Windows** only, compiled with **CMake** and **Visual Studio Code**.

**Core Construction Paradigm:** The world begins as an **infinite solid**, but this solid is bounded by the **world bounds** (see Part I, Section 3.10). Rooms and passages are constructed by placing **Add** brushes (which define solid masses) and then subtracting **Sub** brushes to carve out interior spaces. This subtractive workflow mirrors the classic DromEd approach and ensures watertight geometry from the ground up.

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
| **View** | Toggle Grid, Camera Presets, Zoom Extents, Display Mode (Solid, Baked, Baked Lit, Triangles), AI Debug Overlays, **World Bounds** (show/hide bounding box) |
| **Brush** | Create Box, Create Wedge, Group, Ungroup, Merge, Split, Isolate, Hide |
| **Edit Mode** | Enter Edit Mode, Vertex/Edge/Face Mode, Extrude, Connect, Merge, Divide |
| **Texture** | Load Texture, Apply to Selected, Paint Mode, Next/Previous Face |
| **Entity** | Import Mesh (.obj/.fbx), Create Entity Prototype, Place Entity |
| **Build** | Bake Level, Test in Game |
| **Debug** | Show AI Perception (cones/spheres), Show Navmesh, Show Sound Rays (portal paths), Show Voxel Proxy |
| **Settings** | Keyboard Shortcuts, Grid Size, **World Bounds Size**, Theme, Audio Device |

### 2.2. Primary Panels (ImGui Windows)
- **3D Viewport** – Main workspace (see Section 3).
- **Layer Stack** – Hierarchical list of brushes and groups (World Mode) or entity list (Entity Mode).
- **Inspector** – Properties of the currently selected brush/group/entity.
- **Texture Browser** – Catalog of imported `.dds` textures.
- **Asset Browser** – Lists imported `.obj`/`.fbx` meshes, sounds, and scripts.
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
| **Baked** | `Alt+2` | **Full Global CSG Result** (all Add brushes unioned, all Sub brushes subtracted), rendered with a flat unlit shader (albedo only). | **Full Global CSG.** | **Manual (`Ctrl+B`)** or via a dedicated build button. A progress bar is shown; the process may take several seconds to minutes depending on brush count. |
| **Baked Lit** | `Alt+3` | **Full Global CSG Result** rendered with the complete deferred lighting pipeline, including dynamic lights, shadows (on static world only), and baked lightmaps. | **Full Global CSG.** | **Manual (`Ctrl+B`)** only. A progress bar is shown; this is the heaviest bake. |
| **Triangles** | `Alt+4` | Debug mode where every triangle in the baked mesh is assigned a random colour, allowing visual inspection of triangulation quality and face‑group boundaries. | **Full Global CSG (or last baked result).** | **Manual (`Ctrl+B`)** to refresh. |

**AI Debug Overlays (toggle via Debug menu):**
- Draw wireframe cones for AI FOV.
- Draw translucent spheres for hearing radius.
- Draw coloured lines for sound propagation paths (green = audible, red = blocked/portal‑closed) – uses the unified cache.
- Render the Detour navmesh polygons in semi‑transparent blue over the geometry.
- Visualize the Tier‑1 sparse voxel octree (walkable cells in green, blocked in red).

**Performance Note for Solid Mode:** The local CSG kernel only executes when an edited brush intersects a Sub brush. Moving an Add brush through empty space incurs zero CSG cost. When intersection occurs, the engine gathers only the overlapping brushes (typically 1–3), unions them, and subtracts the Sub—completing in under 1 millisecond.

---

## 4. The Layer Stack (World Mode)

The Layer Stack is a hierarchical list of **brushes** and **groups** in the scene.

### 4.1. Order & CSG Processing
- Brushes higher in the list have greater `Time` values and are processed **later**.
- **The world begins as an infinite solid (`Time = 0`)** – but this solid is actually the **world bounds** cuboid (see Section 3.10). All **Add** brushes add solid matter inside this box; all **Sub** brushes subtract from the accumulated geometry. This subtractive paradigm is fundamental: designers carve rooms out of the bounded void.
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
- **Type**: `Add` (solid) or `Sub` (void/air). `Add` brushes build the structural mass; `Sub` brushes carve out rooms, doors, and windows from the bounded solid or existing Add geometry.
- **Time**: 32‑bit integer (read‑only, controlled by Layer Stack order).
- **Vertex Pool**: List of **snapped integer coordinates**, clamped to world bounds.
- **Triangle Pool**: List of indices defining the triangulated mesh.
- **Planar brushes (single polygons) are NOT allowed.** All brushes must be closed, watertight manifolds. The editor validates this before baking.

### 5.2. Creating Brushes
- Use **Create Box** or **Create Wedge** (or `Shift + A` for the creation menu).
- Click/drag in the viewport to define size. The brush is automatically clipped to the world bounds; the user cannot drag beyond them.
- The new brush appears in the Layer Stack. It is initially an **Add** brush; you can change its type in the Inspector.

### 5.3. Brush Properties (Inspector)
| Property | Description |
|----------|-------------|
| **Name** | Editable identifier. |
| **Type** | Add / Sub (changeable). |
| **Time** | Read‑only CSG order. |
| **Position** | X, Y, Z (integer). Movement is constrained so that the entire brush remains inside world bounds. |
| **Rotation** | Quaternion (Euler displayed for convenience). Snaps to 5° increments. |
| **Scale** | Uniform or per‑axis (integer). Scaling is clamped to keep the brush within bounds. |

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
| **Move** | `G` | Translate selected vertices/edges/faces. Enter exact values in Inspector. Movement is clamped to world bounds. |
| **Rotate** | `R` | Rotate selected elements. |
| **Scale** | `S` | Scale selected elements. Scaling is clamped to keep the brush inside world bounds. |

**Snap Rule**: All transformations snap to the integer grid; vertices are rounded to the nearest integer upon completion. For rotation, snapping is to 5° increments by default (configurable).

### 6.5. Geometry Operators
| Operator | Shortcut | Description |
|----------|----------|-------------|
| **Extrude (Face)** | `Ctrl+E` | Pull a face along its normal, generating new side geometry. Extrusion is clipped to world bounds. |
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

## 8. Asset Import Pipeline (Entities)

### 8.1. Supported Formats
- The editor supports `.obj` and `.fbx` via the **Assimp** library (v5.3).

### 8.2. Import Process (for Entities)
1. User drags a `.obj`/`.fbx` into the **Asset Browser** or uses **Entity > Import Mesh**.
2. Assimp loads the file and triangulates all polygons.
3. **Important:** Vertices are **NOT snapped** to the grid. They remain as floating‑point coordinates exactly as authored in the DCC tool.
4. The engine converts the mesh into its internal `EntityVertex` format (float positions, float UVs).
5. The mesh is stored in the project's `meshes/` folder as a `.vmesh` (Vark Mesh – engine‑specific binary format: vertex buffer + index buffer + material slots).
6. **LOD Generation**: For large imported meshes, the editor automatically generates up to 3 LOD levels using `MeshOptimizer` (reducing triangle count by 50%, 75%, 90%) and stores them inside the `.vmesh` file.
7. A **prototype** is created in the Entity registry, referencing the `.vmesh` file. The user can now place instances of this prototype in the level.
8. Surface materials from the source file (e.g., `M_Stone`) are mapped to the engine's material table; if a matching texture name exists, it is automatically assigned.

### 8.3. Entity Placement
- In Entity Mode, the user selects a prototype from the Asset Browser, clicks in the viewport to place it.
- The placed entity receives a random name (e.g., `Door_1`) and can be moved/rotated/scaled in the Inspector using **floating‑point** values.
- Entities do NOT participate in CSG; they are rendered separately in the deferred pass.

---

## 9. Baking Pipeline – Optimized for Chunking & Proxy Generation

Baking converts editable Source Brushes into the immutable Runtime World. The engine employs a **dual‑mode** baking strategy: local (for Solid viewport feedback) and global (for final Baked / Baked Lit previews).

### 9.1. When to Bake
- **Local CSG (Solid Mode):** Triggers automatically when an edited brush intersects a Sub brush. The engine performs a spatial AABB query to find overlapping Add brushes, unions them locally, and subtracts the Sub. This is debounced (100ms) and runs on a background thread, completing in < 1ms.
- **Global CSG (Baked / Baked Lit):** Triggered manually (`Ctrl+B`). This processes the entire level. A progress dialog is displayed; the user is informed that this may take several seconds to minutes depending on brush complexity. The engine does **not** rely on an idle timer for full bakes.

**Important:** Sub brushes are applied **sequentially** in order of their `Time` parameter. This ensures correct carving of overlapping Sub brushes (e.g., a door carved through a wall that was already carved by a window). The engine cannot batch all Sub brushes into one union because the order of subtraction matters for the final geometry. However, the CSG library (Manifold) efficiently handles incremental subtraction using delta swapping (old brush removed, new brush subtracted), keeping the operation O(n log n) rather than O(n²).

### 9.2. Global Baking Steps (Editor‑Visible)
1. **Validation** – checks for:
   - Closed manifold (no open edges, no non‑manifold vertices).
   - No planar brushes (all brushes have volume > 0).
   - All faces have a material assigned (or default material).
   - No degenerate triangles.
   - **All brushes are entirely within the world bounds.**
2. **Sorting** – brushes sorted by `Time` (the world bounds solid is always at Time 0).
3. **CSG Evaluation** – 
   - **Step A:** Union all Add brushes using the Manifold library (elalish), clipped to world bounds.
   - **Step B:** Sequentially subtract all Sub brushes **in ascending `Time` order** from the accumulated result.
4. **Triangulation & Face Group Merging** – resulting mesh is triangulated; coplanar adjacent triangles with same material are merged.
5. **Spatial Chunking** – the final mesh is split into axis‑aligned cubes (chunk size configurable, default 64 units) for efficient frustum culling.
6. **Surface Type Assignment** – each triangle is tagged with a SurfaceType (Wood, Stone, Metal, Carpet, Grass, Water) based on the material or brush property.
7. **UV Compilation** – planar projection formula applied to each Face Group. For face groups that originate from CSG cuts, the origin and axes are computed from the plane equation.
8. **Lightmap UV Generation**: If lightmaps are enabled, the entire baked mesh is passed to `xatlas` to generate a unique, non‑overlapping UV2 atlas. This atlas is packed into a separate `.dds` lightmap texture via a path tracer (external baker).
9. **Tier‑1 Proxy Generation** – a 3D sparse voxel octree with Morton‑code hashing is built from the chunked mesh.
10. **Physics & Navigation Generation** – collision BVH (Tier‑2) and navmesh are built. The region graph for HPA* is also computed.
11. **Output** – serialized to `terrain.ter` (with chunk data) and `navigation.nav`.

### 9.3. Baking Interface
- **Bake Button** – executes the full global pipeline.
- **Progress Dialog** – shows status of each stage (e.g., "CSG Union 45%", "Triangulation 60%", "Lightmap Baking 80%").
- **Error Log** – lists any validation failures (prevents baking).
- **Bake Preview** – viewport switches to show the baked geometry.

### 9.4. Async Compile & Throttling Optimizations
To ensure the viewport remains fluid even with thousands of brushes, the engine employs a multi‑layered optimization strategy:

1. **Chunked Compilation:** The scene is divided into hierarchical Groups. Local CSG is evaluated exclusively at the Group level. Editing a brush inside a Group only invalidates and recompiles that single Group.
2. **Per‑Group GPU Pages:** Each Group owns a dedicated, small DirectX 11 vertex/index buffer. During an update, only the affected Group's buffer is re‑uploaded (`UpdateSubresource`), preventing global pipeline stalls.
3. **Manifold Delta Swapping:** The engine retains the `Manifold` result of every brush. On an edit, it subtracts the old brush geometry and unions the new geometry (O(log n) instead of O(n²)).
4. **Leap‑Frog Job Queue:** A single‑slot background queue discards stale builds. If the user performs 10 edits, only the final state is compiled.
5. **Conditional Subsystems:** Collision BVH and Recast Navmeshes are **excluded** from the Solid‑mode auto‑bake. They are only rebuilt during explicit "Baked Lit" previews or Playtest launches, saving 60% of the background CPU cost. The BVH is double‑buffered to avoid data races.

---

## 10. Project Management

### 10.1. Saving
When you hit **Save**:
- `terrain_source.ters` – current CSG brush data (binary format similar to terrain.ter but with source brush info).
- `terrain.ter` – background bake (if enabled), includes chunks and proxy.
- `navigation.nav` – background bake (if enabled).
- `entities.bin` – entity placements (floating‑point transforms).
- `meshes/` – all imported `.vmesh` files.
- `textures/` – all `.dds` files.
- `sounds/` – all `.wav` files.
- `scripts/*.lua` – saved directly.
- `config.ini` – editor settings (grid size, theme, shortcuts, input bindings, world bounds).

### 10.2. Exporting for Distribution
Use **Export Packaged** – the Build Tool creates a `.pak` archive (LZ4 compressed) with a TOC containing all assets. The game can load directly from this archive.

### 10.3. Version Control
- Text files (`mission.cfg`, `.lua`, `config.ini`) – diffable.
- Binary files (`.ters`, `.ter`, `.bin`, `.vmesh`, `.dds`, `.wav`) – store in Git LFS.
- Team members can work on separate missions or scripts with minimal conflicts.

---

## 11. Keyboard Shortcuts (Editor‑Specific)

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
| `Ctrl+B` | Bake Level (Global) – shows progress bar |
| `F5` | Test in Game (launches the game executable with the current mission) |
| `~` | Toggle console |
| `Alt+1` | Solid Mode |
| `Alt+2` | Baked Mode |
| `Alt+3` | Baked Lit Mode |
| `Alt+4` | Triangles Mode |
| `Ctrl+S` | Save Mission |

All shortcuts are customisable via **Settings > Keyboard Shortcuts** (stored in `config.ini`).

---

## 12. Workflow Summary (Concept to Playable)

1. **World Mode** – start with the **bounded solid**. Place **Add** brushes to form the structural mass (walls, floors, ceilings). Use **Sub** brushes to carve out rooms, doorways, and windows.
2. **Layer Stack** – reorder brushes to resolve CSG conflicts. Sub brushes should sit above the Add brushes they carve.
3. **Solid Mode** – edit geometry in real‑time. Add brushes move freely (no CSG). Sub brushes trigger local CSG, carving holes instantly when they intersect Add geometry.
4. **Texturing** – import textures, apply to Face Groups, use Split for multi‑texturing.
5. **Entity Mode** – import `.obj`/`.fbx` meshes (floating‑point), place lights, triggers, AI patrol points, interactive objects (loot, switches, doors, crates).
6. **Scripting** – write Lua scripts for mission logic, door behavior, AI barks, and objective triggers.
7. **Baked Preview** – press `Ctrl+B` and monitor the progress bar to see the fully unioned and subtracted global geometry with spatial chunks.
8. **Baked Lit** – preview the final lighting and shadows (requires lightmap baking, which may take additional time). Remember, characters do not cast shadows; the preview reflects that.
9. **Playtest** (`F5`) – iterate. Use debug overlays to tune AI perception and verify the collision proxy.
10. **Export** – package as `.pak` for shipping.

---

## Appendix A: External Dependencies & Versions

| Library | Version / Commit | Purpose |
|---------|-------------------|---------|
| DirectX 11 | June 2010 SDK | Rendering |
| ImGui | v1.90 | UI |
| Assimp | v5.3.1 | Mesh import |
| elalish/manifold | Latest stable | CSG operations |
| Recast/Detour | master (specific hash TBD) | Navigation + HPA* region graph |
| OpenAL-soft | v1.23.1 | Audio |
| xatlas | Latest | Lightmap UV packing |
| MeshOptimizer | Latest | LOD generation |
| LZ4 | v1.9.4 | Compression |
| sol2 | v3.3.0 | Lua bindings |
| Lua | v5.4.6 | Scripting |

---

*End of Vark Engine Documentation – Complete Two‑Part Guide (Final – Optimized & Corrected – v2).*