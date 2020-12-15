def can_build(env):
    return True#not env["disable_3d"]

def configure(env):
    pass

def get_name():
    return 'gridmap'

def get_doc_classes():
    return [
        "GridMap",
    ]


def get_doc_path():
    return "doc_classes"
