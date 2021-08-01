def can_build(env):
    return True#not env["disable_3d"]

def configure(env):
    pass

def get_name():
    return 'mobile_vr'

def get_doc_classes():
    return [
        "MobileVRInterface",
    ]


def get_doc_path():
    return "doc_classes"
