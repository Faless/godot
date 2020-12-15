def can_build(env):
    return env.tools_enabled


def configure(env):
    pass

def get_name():
    return 'gltf'

def get_doc_classes():
    return [
        "EditorSceneImporterGLTF",
        "GLTFAccessor",
        "GLTFAnimation",
        "GLTFBufferView",
        "GLTFCamera",
        "GLTFDocument",
        "GLTFLight",
        "GLTFMesh",
        "GLTFNode",
        "GLTFSkeleton",
        "GLTFSkin",
        "GLTFSpecGloss",
        "GLTFState",
        "GLTFTexture",
        "PackedSceneGLTF",
    ]


def get_doc_path():
    return "doc_classes"
