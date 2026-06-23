def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "InsightsManager",
        "InsightsChannel",
        "InsightsDatabase",
        "InsightsCapture",
        "NativeCapture",
        "ResourceLoadTracker",
        "MemoryChannel",
        "LogChannel",
        "ScriptChannel",
        "LoadingChannel",
    ]


def get_doc_path():
    return "doc_classes"
