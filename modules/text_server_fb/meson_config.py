def get_name() -> str:
    return 'text_server_fb'

def can_build(env):
    return True


def configure(env):
    pass


def is_enabled():
    # The module is disabled by default. Use module_text_server_fb_enabled=yes to enable it.
    return False
