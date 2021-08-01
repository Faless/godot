def get_name() -> str:
    return 'bullet'

def can_build(env):
    # API Changed and bullet is disabled at the moment
    return False
    # Later change to return not env["disable_3d"]


def configure(env):
    pass
