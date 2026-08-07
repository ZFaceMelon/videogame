"""Build the runtime Pharaoh GLB from the authored FBX assets.

Run with Blender in background mode:
  blender --background --factory-startup --python scripts/export_pharaoh.py
"""

from pathlib import Path
import math

import bpy
from mathutils import Matrix, Vector


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
BODY_FBX = ASSETS / "pharoh_person.fbx"
STAFF_FBX = ASSETS / "staff.fbx"
OUTPUT_GLB = ASSETS / "pharaoh_character.glb"


def select_only(objects):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    if objects:
        bpy.context.view_layer.objects.active = objects[0]


def import_fbx(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path), use_anim=True)
    return [obj for obj in bpy.data.objects if obj not in before]


def relink_images():
    for image in bpy.data.images:
        if not image.filepath:
            continue
        local = ASSETS / Path(bpy.path.abspath(image.filepath)).name
        if local.exists():
            image.filepath = str(local)
        try:
            image.pack()
        except RuntimeError:
            print(f"Warning: could not pack image {image.name}: {image.filepath}")


def set_principled(material, base_color, metallic=None, roughness=None,
                   clear_base_texture=False):
    if not material or not material.use_nodes:
        return
    shader = next((node for node in material.node_tree.nodes
                   if node.type == "BSDF_PRINCIPLED"), None)
    if not shader:
        return
    if clear_base_texture:
        for link in list(shader.inputs["Base Color"].links):
            material.node_tree.links.remove(link)
    shader.inputs["Base Color"].default_value = base_color
    if metallic is not None:
        shader.inputs["Metallic IOR Level" if "Metallic IOR Level" in shader.inputs
                      else "Metallic"].default_value = metallic
    if roughness is not None:
        shader.inputs["Roughness"].default_value = roughness


bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)

body_import = import_fbx(BODY_FBX)
armature = next(obj for obj in body_import if obj.type == "ARMATURE")
body = max((obj for obj in body_import if obj.type == "MESH"),
           key=lambda obj: len(obj.data.vertices))

# The authored FBX also contains Blender's default cube, camera, and light. They
# are authoring leftovers, not game assets.
for obj in list(body_import):
    if obj not in {armature, body}:
        bpy.data.objects.remove(obj, do_unlink=True)

armature.name = "PharaohRig"
# The engine's animated-character loader uses these stable runtime node names.
body.name = "samurai"
body.data.name = "samurai"

# These small untextured sections are intentional: a black void beneath the
# headdress and purple glowing eyes. The FBX defaults both to gray/white.
set_principled(bpy.data.materials.get("void_black.001"), (0.003, 0.002, 0.006, 1.0), 0.0, 0.65)
set_principled(bpy.data.materials.get("glowing_eyes"), (0.18, 0.01, 0.9, 1.0), 0.0, 0.25)

action_names = {
    "idle": "breathingidle",
    "walk_foward": "armedrunforward",
    "walk_backwards": "armedrunback",
    "walk_left": "combatwalkleft",
    "walk_right": "combatwalkright",
    "running": "fastrun",
    "staff swing": "overheadslash",
}
for action in bpy.data.actions:
    source_name = action.name.rsplit("|", 1)[-1].lower()
    if source_name in action_names:
        action.name = action_names[source_name]

idle_action = bpy.data.actions.get("breathingidle")
if idle_action:
    if not armature.animation_data:
        armature.animation_data_create()
    armature.animation_data.action = idle_action
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()

# Import the staff and attach it to the animated right hand. Preserve the staff's
# authored scale, align its long Y axis vertically, and center the grip on the hand.
staff_import = import_fbx(STAFF_FBX)
staff_parts = [obj for obj in staff_import if obj.type == "MESH"]
if staff_parts:
    select_only(staff_parts)
    if len(staff_parts) > 1:
        bpy.ops.object.join()
    staff = bpy.context.view_layer.objects.active
    staff.name = "mesh_input"
    staff_color = bpy.data.images.load(str(ASSETS / "pharoh_dudes_skin.png"),
                                       check_existing=True)
    for material in staff.data.materials:
        set_principled(material, (0.48, 0.20, 0.025, 1.0), 0.7, 0.32,
                       clear_base_texture=True)
        if material and material.use_nodes:
            shader = next((node for node in material.node_tree.nodes
                           if node.type == "BSDF_PRINCIPLED"), None)
            if shader:
                texture = material.node_tree.nodes.new("ShaderNodeTexImage")
                texture.name = "Staff Color Texture"
                texture.image = staff_color
                material.node_tree.links.new(texture.outputs["Color"],
                                              shader.inputs["Base Color"])
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    bpy.ops.object.origin_set(type="ORIGIN_GEOMETRY", center="BOUNDS")
    hand_name = next((name for name in armature.data.bones.keys()
                      if name.lower().endswith("righthand")), "")
    if hand_name:
        hand_world = armature.matrix_world @ armature.pose.bones[hand_name].matrix
        staff.parent = armature
        staff.parent_type = "BONE"
        staff.parent_bone = hand_name
        # Define the desired rest-pose transform in world space, then let Blender
        # derive the bone-local attachment. The staff's long local Y axis becomes
        # world Z, with the grip centered in the right palm.
        desired_world = (Matrix.Translation(hand_world.translation + Vector((0.0, 0.0, 0.08))) @
                         Matrix.Rotation(math.radians(-90.0), 4, "X"))
        staff.matrix_world = desired_world
    else:
        print("Warning: right-hand bone not found; staff remains at its authored transform")

relink_images()

# Export the mesh, rig, staff, skin weights, embedded materials, and every FBX
# action. The GLB is self-contained and is the only file the game needs at runtime.
select_only([armature, body] + staff_parts)
kwargs = dict(
    filepath=str(OUTPUT_GLB),
    export_format="GLB",
    use_selection=True,
    export_apply=True,
    export_materials="EXPORT",
    export_image_format="AUTO",
    export_animations=True,
    export_skins=True,
    export_morph=True,
)
operator_props = bpy.ops.export_scene.gltf.get_rna_type().properties.keys()
if "export_animation_mode" in operator_props:
    kwargs["export_animation_mode"] = "ACTIONS"
elif "export_nla_strips" in operator_props:
    kwargs["export_nla_strips"] = True

bpy.ops.export_scene.gltf(**kwargs)
print(f"Exported {OUTPUT_GLB}")
print(f"Armature bones: {len(armature.data.bones)}")
print(f"Animations: {len(bpy.data.actions)}")
