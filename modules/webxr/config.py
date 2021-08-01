def can_build(env):
    return True#not env["disable_3d"]


def get_name():
    return 'webxr'

def configure(env):
    pass


def get_doc_classes():
    return ["WebXRInterface"]


def get_doc_path():
    return "doc_classes"
