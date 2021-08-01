def get_name() -> str:
    return 'meshoptimizer'

def can_build(env: dict) -> bool:
    # Having this on release by default, it's small and a lot of users like to do procedural stuff
    return True#not env["disable_3d"]


def configure(env):
    pass
