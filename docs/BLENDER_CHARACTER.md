# Blender character + skeleton guide for videogame

This guide walks through creating a rigged humanoid in Blender, exporting it in a format the engine can load later, and matching the conventions already used by the procedural character in this project.

**Current engine status:** meshes are built in code (`engine/src/character_mesh.h`) and drawn as flat vertex colors. There is **no model loader or skeletal animation yet**. Follow this guide to produce assets that are ready the moment you add a glTF loader and skinned mesh shader.

---

## What the engine expects today

| Topic | Convention |
|-------|------------|
| Up axis | **+Y** |
| Ground plane | **X / Z** |
| Facing (default yaw = 0) | **+Z** (character walks toward +Z when not rotated) |
| Character height | **1.54 units** ≈ 6 ft (`kCharacterHeightUnits` in `character_mesh.h`) |
| Player origin | **Feet on the ground** — `player_x`, `player_y`, `player_z` is the root position on the floor |
| Vertex layout | 9 floats: position (3), normal (3), color (3) |
| Draw mode | **Triangles only** (no quads/ngons at runtime) |
| Animation today | Separate mesh pieces rotated in code (legs, weapon arm). Your skeleton replaces this. |

Recommended export format: **glTF 2.0 binary (`.glb`)** — mesh, skeleton, weights, and animations in one file.

> **Note:** `.gitignore` ignores `*.obj` (build artifacts). Use `.glb` under `assets/` so your character files are tracked by git.

---

## Part 1 — Blender project setup

### Step 1: Match the game units

1. Open Blender (3.x or 4.x).
2. **File → New → General**.
3. Select the default cube, press `X` → **Delete**.
4. Open **Scene Properties** (cone icon) → **Units**:
   - Unit system: **Metric**
   - Unit scale: **1.0**
   - Length: **Meters** (1 Blender unit = 1 game unit)

The playable character is **1.54 m tall**. A 6 ft humanoid should reach **Y = 1.54** at the top of the head.

### Step 2: Match the game axes in Blender

Blender defaults to **Z-up**. The game uses **Y-up**.

**Option A (recommended): model in Z-up, fix on export**

- Model standing on the XY ground plane, facing **+Y** in Blender.
- On glTF export, enable **+Y Up** (glTF’s native orientation). The exporter converts to Y-up for the game.

**Option B: change Blender to Y-up for the whole project**

1. Edit → Preferences → View → **Track Axis / Up Axis** (or use a Y-up startup file).
2. Model on the XZ ground plane, facing **+Z**.

Either way, before export verify in the glTF viewer: character stands on the ground, head at ~1.54, faces the direction you want as “forward” in game (+Z).

### Step 3: Create an asset folder in the repo

```text
videogame/
└── assets/
    └── characters/
        └── knight/
            ├── knight.blend      # working file
            ├── knight.glb        # exported game asset
            └── knight_walk.glb   # optional: animations-only variant
```

Keep `.blend` sources in git; export `.glb` after each meaningful change.

---

## Part 2 — Block out the mesh

### Step 4: Reference scale

Add an empty or bone at the world origin — this becomes the **foot root**.

Rough proportions matching the current blockout:

| Body part | Approx. center (Y) | Approx. size (half-extents) |
|-----------|-------------------|-----------------------------|
| Feet / root | 0.0 | — |
| Legs | 0.30 | 0.10 × 0.30 × 0.10 |
| Torso / shirt | 0.90 | 0.22 × 0.30 × 0.12 |
| Head | 1.34 | 0.20 cube |
| Arms | 0.88 | 0.08 × 0.28 × 0.08 |

Use **Shift+A → Mesh → Cube** and scale in Edit Mode, or **Shift+A → Mesh → UV Sphere / Cylinder** for a smoother body. Stay low-poly for now (hundreds of tris, not millions).

### Step 5: One connected mesh

1. Join all body parts into **one mesh object** (`Ctrl+J`).
2. Name it `Body`.
3. **Object mode → Object → Set Origin → Origin to 3D Cursor**, with the 3D cursor at **(0, 0, 0)** on the ground between the feet.
4. Confirm the bottom of the feet sits at **Y = 0** (or Z = 0 if you stayed Z-up — adjust before export).

The engine applies **one root transform** (position + yaw) then bone matrices. Do not bake the character floating above the origin.

### Step 6: Face count and topology

1. **Edit Mode → Mesh → Clean Up → Merge by Distance** (remove doubles).
2. **Mesh → Normals → Recalculate Outside** (`Shift+N`).
3. Avoid ngons on deform areas (shoulders, hips, knees). Use **Ctrl+T** to triangulate before export (or rely on the glTF exporter’s triangulation).

Target: **1k–8k triangles** for a first character.

### Step 7: Vertex colors (matches current renderer)

The shader reads **per-vertex RGB**, not textures.

1. Select `Body` → **Object Data Properties** → **Color Attributes** → **+** → name `Col`, base color white.
2. Switch to **Vertex Paint** mode.
3. Paint skin, shirt, pants, etc. (similar palette to `character_mesh.h`: skin ~ `(0.72, 0.70, 0.68)`, shirt gray-blue, pants dark gray).

On glTF export, enable **Vertex Colors** so colors survive import.

---

## Part 3 — Skeleton (armature)

### Step 8: Bone naming (match future engine code)

Use lowercase names with underscores. Suggested hierarchy:

```text
root                 ← feet / ground contact; parent of everything
├── spine
│   ├── chest
│   │   ├── neck
│   │   │   └── head
│   │   ├── upperarm_l
│   │   │   └── lowerarm_l
│   │   └── upperarm_r          ← weapon / sword arm
│   │       └── lowerarm_r
│   │           └── hand_r      ← optional socket for sword
│   ├── thigh_l
│   │   └── shin_l
│   └── thigh_r
│       └── shin_r
```

These map cleanly to animations the game already fakes in code:

| Game logic today | Bone(s) to rotate |
|------------------|-------------------|
| Walk cycle | `thigh_l`, `shin_l`, `thigh_r`, `shin_r` |
| Sword swing | `upperarm_r`, `lowerarm_r` (or `chest` + arm) |
| Body yaw | `root` (or whole armature object) |

### Step 9: Add the armature

1. **Shift+A → Armature**. Name it `rig`.
2. Tab into **Edit Mode**. Delete the default bone.
3. Add bones from the root up ( **Shift+A** in armature edit mode, or extrude with **E** ):
   - Place **root** bone tip at the pelvis (~Y 0.85), tail at the ground (Y 0).
   - Extrude upward for spine → chest → neck → head.
   - Extrude arms from chest; legs from lower spine or root.
4. Align bones so **+Y (or local bone axis) points toward the child bone** (standard Blender humanoid setup).
5. **Right arm** is the weapon side (same as `-X` side in the current blockout: weapon on the character’s right, which appears on the **left side of the screen** in third-person).

### Step 10: Mirror bones (optional)

1. Select arm bones on one side.
2. **Armature → Symmetrize** (or mirror manually with naming `.L` / `.R` then rename to `_l` / `_r` for consistency).

### Step 11: Parent mesh to armature

1. Select `Body`, then **Shift-select** `rig`.
2. **Ctrl+P → With Automatic Weights**.
3. Test with **Pose Mode** on the armature: rotate thigh/ arm bones; mesh should follow.

### Step 12: Weight paint cleanup

1. Select `Body` → **Weight Paint** mode.
2. Normalize trouble spots:
   - Knees: mostly `shin_*`, not `thigh_*`.
   - Shoulders: smooth blend between `chest` and `upperarm_*`.
   - Hips: balance `root`, `spine`, and `thigh_*`.
3. **Weights → Normalize All** if vertices look stretched.

---

## Part 4 — Animations

### Step 13: Action basics

1. Select `rig` → **Dope Sheet** → switch dropdown to **Action Editor**.
2. **+ New** action for each clip; name actions exactly (engine will look these up by string):

| Action name | Purpose | Loop |
|-------------|---------|------|
| `idle` | Standing breathe | Yes |
| `walk` | Locomotion | Yes |
| `swing` | Sword attack | No |
| `jump` | Optional | No |

### Step 14: Walk cycle (replaces procedural leg swing)

1. Timeline: **0–24** frames at **24 FPS** (1 second loop).
2. Keyframe **root** subtle bob on Y.
3. Keyframe **thigh_l / shin_l** and **thigh_r / shin_r** in opposition (one forward, one back).
4. Current code uses ~±0.45 rad leg swing and ~0.10 stride — use that as a rotation guide, not a hard limit.
5. **Action Editor → Fake User** (shield icon) so Blender keeps the action when you save.

### Step 15: Sword swing (replaces weapon arm rotation)

1. Timeline **0–12** frames (~0.35 s to match `kSwingDuration` in `engine.cpp`).
2. Start pose: arm down/side.
3. Mid: arm rotated forward/down (`upperarm_r` + `lowerarm_r`).
4. End: follow-through or return toward idle.
5. Do **not** keyframe root yaw here — the game still rotates the whole character with `player_yaw`.

### Step 16: Idle

Small chest/head noise or subtle arm movement; single-frame loop is fine at first.

---

## Part 5 — Sword as separate mesh (optional)

The current game draws the sword as a separate procedural mesh parented to the weapon arm.

**Option A — separate object in the same `.glb`**

1. Model a simple sword as mesh `Sword`.
2. Parent `Sword` to bone `hand_r` ( **Ctrl+P → Bone** ).
3. Export both objects; engine loads two skinned or rigid-child meshes.

**Option B — engine still spawns sword in code**

Keep sword out of the `.glb`; attach position from bone `hand_r` world matrix at runtime (same idea as `sword_tip_world()` in `engine.cpp`).

For a first import, **Option A** is easier to verify visually.

---

## Part 6 — Export for this project

### Step 17: glTF export settings

1. **File → Export → glTF 2.0 (.glb/.gltf)**.
2. Save to `assets/characters/knight/knight.glb`.

| Setting | Value |
|---------|--------|
| Format | **glTF Binary (.glb)** |
| Include | **Selected Objects** or visible collection (Body, rig, Sword) |
| Transform | **+Y Up** |
| Geometry | Apply Modifiers, **UVs**, **Normals**, **Vertex Colors** |
| Animation | **Bake Animation**, include all actions or selected `idle` / `walk` / `swing` |
| Skinning | **Include** (default) |

3. Click **Export glTF 2.0**.

### Step 18: Validate outside the game

Before writing C++ loader code, sanity-check the file:

1. Drag `knight.glb` into https://gltf-viewer.donmccurdy.com/ or Blender **File → Import → glTF**.
2. Checklist:
   - [ ] Character on ground, ~1.54 m tall
   - [ ] Faces +Z (or your chosen forward) when rotation is zero
   - [ ] Vertex colors visible
   - [ ] Skeleton deforms mesh
   - [ ] `walk` / `swing` play correctly

Fix in Blender and re-export until all pass.

---

## Part 7 — Hooking into the engine (next code steps)

When you implement loading, wire it to data the game already computes each frame.

### Scene parameters already available

`SceneDrawParams` (`engine/src/renderer.h`) provides:

- `player_x`, `player_y`, `player_z`, `player_yaw` — root transform
- `walk_phase` — drive `walk` animation time or blend
- `swing_angle` / swing state from `engine.cpp` — drive `swing` clip or layer
- `time` — generic phase for idle

Replace `compute_scene_transforms()` piecewise matrices with **bone palette uploads** from the glTF skeleton.

### Minimal loader checklist (C++)

1. Add a small glTF loader (e.g. [tinygltf](https://github.com/syoyo/tinygltf) header-only, or [fastgltf](https://github.com/spnda/fastgltf)).
2. Parse `.glb` from `assets/characters/knight/knight.glb`.
3. Upload positions, normals, vertex colors, indices to a VBO.
4. Read joint indices + weights; upload bone indices/weights attributes.
5. Build inverse bind matrices from the skin.
6. Extend vertex shader with:
   - `uniform mat4 uBones[MAX_BONES];`
   - Skinning: `pos = sum(weight[i] * uBones[joint[i]] * bindPos)`
7. Map action names (`walk`, `swing`, `idle`) to animation channels.
8. Each frame: compute bone matrices from current clip time + root `player_model` transform.

### Vertex shader attribute layout (target)

| Location | Attribute | Source |
|----------|-----------|--------|
| 0 | `vec3 aPos` | POSITION |
| 1 | `vec3 aColor` | COLOR_0 (vertex color) |
| 2 | `vec3 aNormal` | NORMAL (when you add lighting again) |
| 3 | `ivec4 aJoints` | JOINTS_0 |
| 4 | `vec4 aWeights` | WEIGHTS_0 |

Today’s renderer only uses locations 0 and 1; skinning adds 3–4.

### Combat hit detection

`sword_tip_world()` currently uses hard-coded hand offsets. After import, read **bone `hand_r` world matrix × sword tip offset** instead of `kWeaponHandX` constants.

---

## Part 8 — Quick checklist before you call it done

**In Blender**

- [ ] Single skinned mesh (plus optional sword)
- [ ] Origin at feet, height ≈ 1.54
- [ ] Forward axis matches game +Z
- [ ] Vertex colors painted
- [ ] All mesh triangles, manifold, normals outward
- [ ] Bones named consistently (`root`, `thigh_l`, `upperarm_r`, …)
- [ ] Weight painting clean on hips, shoulders, knees
- [ ] Actions: `idle`, `walk`, `swing` saved with Fake User
- [ ] Exported `knight.glb` validated in glTF viewer

**In repo**

- [ ] `assets/characters/.../knight.blend` committed
- [ ] `assets/characters/.../knight.glb` committed
- [ ] `.gitignore` does **not** block `.glb` (only `*.obj` build objects are ignored)

**In engine (when you implement loading)**

- [ ] Load glTF at startup
- [ ] Skinned draw path replaces `build_character_*()` meshes
- [ ] Walk / swing driven from existing `SceneDrawParams`
- [ ] Sword hit uses bone `hand_r` position

---

## Troubleshooting

| Problem | Likely cause | Fix |
|---------|--------------|-----|
| Character lies on side in viewer | Z-up vs Y-up | Re-export with **+Y Up** |
| Tiny or giant in game | Wrong unit scale | Re-scale in Blender to 1.54 m height; apply scale **Ctrl+A** |
| Feet float / sink | Origin not at feet | Set origin to ground between feet |
| Mesh doesn’t move with bones | Not parented / no weights | Re-parent with automatic weights; check vertex groups |
| Black / white in game | Vertex colors not exported | Enable vertex colors on export; add `COLOR_0` in loader |
| Animations missing | Actions not exported | Enable **Bake Animation**; Fake User on each action |
| `.obj` not in git | `.gitignore` rule | Use `.glb`, not Wavefront `.obj` |

---

## Related files in this repo

| File | Role |
|------|------|
| `engine/src/character_mesh.h` | Current procedural mesh + scale constants |
| `engine/src/mesh_builder.h` | Vertex format (9 floats) |
| `engine/src/renderer.cpp` | Draw path + scene transforms to replace |
| `engine/src/engine.cpp` | Walk phase, swing timing, sword hit logic |
| `README.md` | Build instructions |

When the glTF loader lands, update `README.md` with the asset path and re-run:

```powershell
cmake --build c:\Users\zanek\videogame\build --config Release
.\build\Release\videogame_cxx.exe
```
