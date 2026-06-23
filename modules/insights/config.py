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
        "GPUTimestampQuery",
        "GPUProfilerVulkan",
        "GPUProfilerD3D12",
        "GPUProfilerMetal",
        "GPUChannel",
    ]


def get_doc_path():
    return "doc_classes"
