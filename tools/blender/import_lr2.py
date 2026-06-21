bl_info = {
    "name": "LEGO Racers 2 LR2 Importer",
    "author": "Codex",
    "version": (1, 0, 0),
    "blender": (3, 6, 0),
    "location": "File > Import > LEGO Racers 2 (.lr2)",
    "category": "Import-Export",
}

import json
from pathlib import Path

import bpy
from bpy.props import StringProperty
from bpy_extras.io_utils import ImportHelper
from mathutils import Matrix, Quaternion, Vector


CONVERT = Matrix(((1.0, 0.0, 0.0, 0.0), (0.0, 0.0, -1.0, 0.0), (0.0, 1.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)))
CONVERT_INV = CONVERT.inverted()


def lr2_vec(value):
    return Vector((float(value[0]), -float(value[2]), float(value[1])))


def lr2_matrix(position, rotation, scale):
    q = Quaternion((float(rotation[3]), float(rotation[0]), float(rotation[1]), float(rotation[2])))
    s = Matrix.Diagonal((float(scale[0]), float(scale[1]), float(scale[2]), 1.0))
    m = Matrix.Translation(Vector((float(position[0]), float(position[1]), float(position[2])))) @ q.to_matrix().to_4x4() @ s
    return CONVERT @ m @ CONVERT_INV


def safe_name(value, fallback):
    text = str(value or fallback)
    for character in '<>:"/\\|?*;,':
        text = text.replace(character, "_")
    text = text.strip(" .")
    return text or fallback


def new_collection(parent, name):
    collection = bpy.data.collections.new(safe_name(name, "Collection"))
    parent.children.link(collection)
    return collection


def make_material(name, image_path=None, alpha=1.0, color=(0.8, 0.8, 0.8, 1.0)):
    material = bpy.data.materials.new(safe_name(name, "Material"))
    material.diffuse_color = (color[0], color[1], color[2], alpha)
    material.use_nodes = True
    material.blend_method = "BLEND" if alpha < 1.0 else "OPAQUE"
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    if bsdf is not None:
        bsdf.inputs["Alpha"].default_value = alpha
        bsdf.inputs["Base Color"].default_value = (color[0], color[1], color[2], alpha)
        if image_path and Path(image_path).exists():
            image = bpy.data.images.load(str(image_path), check_existing=True)
            texture = material.node_tree.nodes.new("ShaderNodeTexImage")
            texture.image = image
            material.node_tree.links.new(texture.outputs["Color"], bsdf.inputs["Base Color"])
    return material


def new_shader_node(nodes, names):
    for name in names:
        try:
            return nodes.new(name)
        except RuntimeError:
            pass
    return None


def socket_or_none(sockets, names):
    for name in names:
        try:
            return sockets[name]
        except KeyError:
            pass
    return None


def make_terrain_material(root, section, material_key):
    material = bpy.data.materials.new(safe_name(Path(section.get("path", "Terrain")).stem + "_" + f"{material_key:08X}", "Terrain"))
    material.diffuse_color = (0.55, 0.72, 0.28, 1.0)
    material.use_nodes = True
    material["lr2_material_key"] = int(material_key)
    material["lr2_blend_attribute"] = "lr2_mix"

    layers = {}
    for layer in section.get("layerTextures", []):
        if layer.get("path"):
            layers[int(layer.get("index", -1))] = root / layer.get("path")
    if section.get("texture", {}).get("path"):
        layers.setdefault(0, root / section.get("texture", {}).get("path"))

    paths = []
    for layer in range(4):
        texture_index = (int(material_key) >> (layer * 8)) & 0xff
        path = layers.get(texture_index)
        paths.append(path if path and Path(path).exists() else None)
        material[f"lr2_texture_{layer}"] = str(path or "")

    nodes = material.node_tree.nodes
    links = material.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    if bsdf is None:
        return material

    attribute = new_shader_node(nodes, ("ShaderNodeAttribute",))
    separate = new_shader_node(nodes, ("ShaderNodeSeparateColor", "ShaderNodeSeparateRGBA", "ShaderNodeSeparateRGB"))
    if attribute is None or separate is None:
        first_path = next((path for path in paths if path is not None), None)
        if first_path is not None:
            texture = nodes.new("ShaderNodeTexImage")
            texture.image = bpy.data.images.load(str(first_path), check_existing=True)
            links.new(texture.outputs["Color"], bsdf.inputs["Base Color"])
        return material

    attribute.attribute_name = "lr2_mix"
    links.new(attribute.outputs["Color"], separate.inputs[0])
    channel_names = (("Red", "R"), ("Green", "G"), ("Blue", "B"), ("Alpha", "A"))
    current = None

    for layer, path in enumerate(paths):
        if path is None:
            continue
        texture = nodes.new("ShaderNodeTexImage")
        texture.image = bpy.data.images.load(str(path), check_existing=True)
        weight = socket_or_none(separate.outputs, channel_names[layer])
        if weight is None:
            continue
        multiply = new_shader_node(nodes, ("ShaderNodeMixRGB",))
        if multiply is None:
            continue
        multiply.blend_type = "MULTIPLY"
        multiply.inputs["Fac"].default_value = 1.0
        links.new(texture.outputs["Color"], multiply.inputs["Color1"])
        links.new(weight, multiply.inputs["Color2"])
        if current is None:
            current = multiply.outputs["Color"]
        else:
            add = new_shader_node(nodes, ("ShaderNodeMixRGB",))
            if add is None:
                continue
            add.blend_type = "ADD"
            add.inputs["Fac"].default_value = 1.0
            links.new(current, add.inputs["Color1"])
            links.new(multiply.outputs["Color"], add.inputs["Color2"])
            current = add.outputs["Color"]

    if current is not None:
        links.new(current, bsdf.inputs["Base Color"])

    return material


def import_glb_asset(path, name):
    asset_collection = bpy.data.collections.new(safe_name(name, "Asset"))
    before = set(bpy.data.objects)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.import_scene.gltf(filepath=str(path))
    imported = [obj for obj in bpy.data.objects if obj not in before]
    for obj in imported:
        for collection in list(obj.users_collection):
            collection.objects.unlink(obj)
        asset_collection.objects.link(obj)
    return asset_collection


def build_terrain(root, terrain_collection, section):
    vertices = section.get("vertices", [])
    triangles = section.get("triangles", [])
    coords = [lr2_vec(vertex.get("position", (0.0, 0.0, 0.0))) for vertex in vertices]
    faces = []
    face_triangles = []
    for triangle in triangles:
        indices = tuple(int(index) for index in triangle.get("indices", (0, 0, 0)))
        if all(0 <= index < len(vertices) for index in indices):
            faces.append(indices)
            face_triangles.append(triangle)
    mesh = bpy.data.meshes.new(safe_name(section.get("path"), "Terrain"))
    mesh.from_pydata(coords, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(safe_name(Path(section.get("path", "Terrain")).stem, "Terrain"), mesh)
    terrain_collection.objects.link(obj)

    uv_layer = mesh.uv_layers.new(name="LR2 UV")
    texture_scale = section.get("textureScale", (1.0, 1.0))
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index = mesh.loops[loop_index].vertex_index
            uv = vertices[vertex_index].get("uv", (0.0, 0.0))
            uv_layer.data[loop_index].uv = (float(uv[0]) * float(texture_scale[0]), float(uv[1]) * float(texture_scale[1]))

    material_indices = {}
    for polygon_index, triangle in enumerate(face_triangles):
        material_key = int(triangle.get("materialKey", 0xffffffff))
        material_index = material_indices.get(material_key)
        if material_index is None:
            material_index = len(obj.data.materials)
            obj.data.materials.append(make_terrain_material(root, section, material_key))
            material_indices[material_key] = material_index
        mesh.polygons[polygon_index].material_index = material_index

    if hasattr(mesh, "color_attributes"):
        attribute = mesh.color_attributes.new(name="lr2_mix", type="BYTE_COLOR", domain="CORNER")
        for polygon in mesh.polygons:
            for loop_index in polygon.loop_indices:
                vertex_index = mesh.loops[loop_index].vertex_index
                mix = vertices[vertex_index].get("mix", (255, 0, 0, 0))
                attribute.data[loop_index].color = (
                    float(mix[0]) / 255.0,
                    float(mix[1]) / 255.0,
                    float(mix[2]) / 255.0,
                    float(mix[3]) / 255.0,
                )

    for polygon in mesh.polygons:
        polygon.use_smooth = True
    return obj


def build_water(root, water_collection, water):
    half_width = float(water.get("width", 1.0)) * 0.5
    half_depth = float(water.get("depth", 1.0)) * 0.5
    local = [
        Vector((-half_width, 0.0, -half_depth)),
        Vector((half_width, 0.0, -half_depth)),
        Vector((half_width, 0.0, half_depth)),
        Vector((-half_width, 0.0, half_depth)),
    ]
    position = water.get("position", (0.0, 0.0, 0.0))
    rotation = water.get("rotation", (0.0, 0.0, 0.0, 1.0))
    q = Quaternion((float(rotation[3]), float(rotation[0]), float(rotation[1]), float(rotation[2])))
    world = [lr2_vec(Vector(position) + q @ point) for point in local]
    mesh = bpy.data.meshes.new("Water")
    mesh.from_pydata(world, [], [(0, 1, 2, 3)])
    mesh.update()
    uv_layer = mesh.uv_layers.new(name="LR2 UV")
    u_scale = float(water.get("uScale", 1.0))
    v_scale = float(water.get("vScale", 1.0))
    water_uvs = ((0.0, 0.0), (u_scale, 0.0), (u_scale, v_scale), (0.0, v_scale))
    for polygon in mesh.polygons:
        for offset, loop_index in enumerate(polygon.loop_indices):
            uv_layer.data[loop_index].uv = water_uvs[offset]
    obj = bpy.data.objects.new("Water", mesh)
    water_collection.objects.link(obj)
    texture = water.get("texture", {})
    texture_path = root / texture.get("path", "") if texture.get("path") else None
    obj.data.materials.append(make_material("Water", texture_path, float(water.get("alpha", 0.74)), (0.0, 0.45, 0.9, 1.0)))
    return obj


def import_lr2(context, filepath):
    path = Path(filepath)
    root = path.parent
    with path.open("r", encoding="utf-8") as file:
        data = json.load(file)

    scene_collection = context.scene.collection
    level_collection = new_collection(scene_collection, data.get("source", {}).get("name", path.stem))
    model_collection = new_collection(level_collection, "Models")
    terrain_collection = new_collection(level_collection, "Terrain")
    water_collection = new_collection(level_collection, "Water")

    asset_collections = {}
    for asset in data.get("models", []):
        asset_path = asset.get("path")
        if not asset_path:
            continue
        full_path = root / asset_path
        if not full_path.exists():
            continue
        asset_collections[int(asset.get("id", len(asset_collections)))] = import_glb_asset(full_path, asset.get("name", full_path.stem))

    for instance in data.get("instances", []):
        asset = asset_collections.get(int(instance.get("model", -1)))
        if asset is None:
            continue
        empty = bpy.data.objects.new(safe_name(instance.get("name"), "LR2 Instance"), None)
        empty.empty_display_type = "PLAIN_AXES"
        empty.instance_type = "COLLECTION"
        empty.instance_collection = asset
        empty.matrix_world = lr2_matrix(
            instance.get("position", (0.0, 0.0, 0.0)),
            instance.get("rotation", (0.0, 0.0, 0.0, 1.0)),
            instance.get("scale", (1.0, 1.0, 1.0)),
        )
        model_collection.objects.link(empty)

    for section in data.get("terrain", []):
        build_terrain(root, terrain_collection, section)

    for water in data.get("water", []):
        build_water(root, water_collection, water)

    return {"FINISHED"}


class IMPORT_SCENE_OT_lr2(bpy.types.Operator, ImportHelper):
    bl_idname = "import_scene.lr2"
    bl_label = "Import LEGO Racers 2 LR2"
    bl_options = {"REGISTER", "UNDO"}

    filename_ext = ".lr2"
    filter_glob: StringProperty(default="*.lr2", options={"HIDDEN"})

    def execute(self, context):
        return import_lr2(context, self.filepath)


def menu_func_import(self, context):
    self.layout.operator(IMPORT_SCENE_OT_lr2.bl_idname, text="LEGO Racers 2 (.lr2)")


def register():
    bpy.utils.register_class(IMPORT_SCENE_OT_lr2)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.utils.unregister_class(IMPORT_SCENE_OT_lr2)


if __name__ == "__main__":
    register()
