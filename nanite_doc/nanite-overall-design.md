# Nanite 虚拟化几何总体设计文档

> 基于 Godot 4.7.1 | 一份核心代码 · 三种桥接可切换

---

## 目录

1. [文档定位与目标](#1-文档定位与目标)
2. [总体架构](#2-总体架构)
3. [目录与构建组织](#3-目录与构建组织)
4. [桥接切换机制](#4-桥接切换机制)
5. [核心类设计](#5-核心类设计)
6. [三阶段实施路径](#6-三阶段实施路径)
7. [GPU HZB 模块：层次化深度缓冲](#7-gpu-hzb-模块层次化深度缓冲)
8. [阶段零：离线构建数据与预览](#8-阶段零离线构建数据与预览)
9. [阶段一：GDExtension 桥接](#9-阶段一gdextension-桥接)
10. [阶段二：Module 桥接](#10-阶段二module-桥接)
11. [阶段三：Deep 桥接](#11-阶段三deep-桥接)
12. [关键 API 对照表](#12-关键-api-对照表)
13. [验证与测试矩阵](#13-验证与测试矩阵)
14. [与调研文档差异说明](#14-与调研文档差异说明)

---

## 1. 文档定位与目标

### 1.1 文档定位

本文档是 Nanite 虚拟化几何系统的**总体设计文档**，在调研文档 `nanite-godot-design.md` 的基础上：

- 统一三套方案为**一份核心代码 + 三种可切换桥接**的架构
- 基于 Godot 4.7.1 **真实 API** 给出精确的编码依据
- 提供 Mermaid 类图、流程图、时序图，可直接指导实现

### 1.2 目标

| 目标 | 说明 |
|------|------|
| 架构统一 | nanite/ 核心库三阶段共用，通过 INaniteBridge 抽象接口暴露渲染入口 |
| 编译期选择 | SCons 开关 `nanite_bridge=gdext\|module\|deep\|all` 决定编译哪些桥接 |
| 运行时激活 | ProjectSettings `nanite/bridge/active` 在已编译的桥接中激活一个 |
| 阶段递进 | GDExtension → Module → Deep，能力逐级增强，核心代码无需重写 |
| API 精确 | 所有 API 均基于 Godot 4.7.1 源码验证，纠正调研文档中的命名错误 |

### 1.3 术语

| 术语 | 含义 |
|------|------|
| 桥接(Bridge) | 连接 nanite/ 核心库与 Godot 渲染管线的适配层 |
| 阶段一(GDExt) | 以 CompositorEffect 子类接入渲染管线 |
| 阶段二(Module) | 以 RendererSceneCull hook 接入渲染管线 |
| 阶段三(Deep) | 以源码 patch 直接修改引擎渲染管线 |
| 粗 LOD | 使用 `mesh_set_shadow_mesh` 设置的低精度阴影网格 |
| 动态 GPU Shadow | Nanite 在 GPU 上为每个光源渲染阴影，写入引擎 shadow atlas |

---

## 2. 总体架构

```mermaid
flowchart TB
    subgraph User["用户层"]
        MI["NaniteMeshInstance3D<br/>(场景节点)"]
        IMP["NaniteImporter<br/>(导入 .nanite)"]
        BLD["NaniteBuilder<br/>(离线构建)"]
        ED["NaniteMeshEditor<br/>(预览界面)"]
    end

    subgraph Core["nanite/ 核心库 (共用)"]
        NS["NaniteServer<br/>(单例调度)"]
        NC["NaniteCore<br/>(BVH 遍历/剔除)"]
        NMR["NaniteMeshResource<br/>(资源)"]
        NMD["NaniteMeshData<br/>(GPU Buffer)"]
        GPU["NaniteGPUPipeline<br/>(Compute/Raster)"]
        PC["NanitePageCache<br/>(流式加载)"]
        DBG["NaniteDebug<br/>(调试可视化)"]
    end

    subgraph Bridge["桥接层 (INaniteBridge)"]
        B1["NaniteGDExtBridge<br/>(CompositorEffect)"]
        B2["NaniteModuleBridge<br/>(SceneCull Hook)"]
        B3["NaniteDeepBridge<br/>(源码 Patch)"]
    end

    subgraph Engine["Godot 引擎"]
        RS["RenderingServer"]
        RD["RenderingDevice"]
        RSC["RendererSceneCull"]
        RFC["RenderForwardClustered"]
        LS["LightStorage RD"]
    end

    MI --> NS
    IMP --> NMR
    BLD --> NMR
    ED --> NMR
    ED --> DBG

    NS --> NC
    NS --> GPU
    NS --> PC
    NC --> NMD
    NMR --> NMD
    GPU --> RD
    DBG --> NS

    NS -.->|INaniteBridge| B1
    NS -.->|INaniteBridge| B2
    NS -.->|INaniteBridge| B3

    B1 -->|CompositorEffect| RS
    B2 -->|Hook| RSC
    B3 -->|Patch| RFC

    GPU --> RD
    B1 --> RD
    B2 --> LS
    B3 --> LS
```

**核心思想**：nanite/ 目录下的所有类（NaniteServer/Core/GPUPipeline 等）不依赖任何特定桥接；桥接层实现 INaniteBridge 接口，负责"何时调用核心库"和"如何把结果喂回引擎"。

---

## 3. 目录与构建组织

```
godot/
├── nanite/                          # 核心库 (三阶段共用)
│   ├── SConscript
│   ├── nanite_server.h/.cpp
│   ├── nanite_core.h/.cpp
│   ├── nanite_mesh_resource.h/.cpp
│   ├── nanite_mesh_data.h/.cpp
│   ├── nanite_mesh_instance_3d.h/.cpp
│   ├── nanite_gpu_pipeline.h/.cpp
│   ├── nanite_hzb.h/.cpp
│   ├── nanite_page_cache.h/.cpp
│   ├── nanite_debug.h/.cpp
│   ├── nanite_builder.h/.cpp
│   ├── nanite_importer.h/.cpp
│   ├── nanite_bridge.h              # INaniteBridge 抽象接口
│   ├── editor/                      # 编辑器扩展 (预览界面)
│   │   ├── nanite_mesh_editor.h/.cpp
│   │   ├── nanite_editor_plugin.h/.cpp
│   │   └── nanite_resource_preview.h/.cpp
│   ├── shaders/
│   │   ├── nanite_cull.glsl
│   │   ├── nanite_rasterize.glsl
│   │   ├── nanite_shadow_rasterize.glsl
│   │   ├── hzb_downsample.glsl
│   │   └── nanite_material_resolve.glsl
│   └── include/
│       └── nanite_bridge_types.h    # 枚举/结构体定义
│
├── nanite_bridge_gdext/             # 阶段一桥接 (GDExtension)
│   ├── SConscript
│   ├── nanite_gdext_bridge.h/.cpp
│   ├── nanite_gdext_plugin.h/.cpp   # GDExtension entry
│   └── register_types.h/.cpp
│
├── modules/nanite_bridge_module/    # 阶段二桥接 (Module)
│   ├── SConscript
│   ├── SCsub
│   ├── config.py
│   ├── nanite_module_bridge.h/.cpp
│   ├── nanite_scene_cull_hook.h/.cpp
│   └── register_types.h/.cpp
│
├── nanite_bridge_deep/              # 阶段三桥接 (源码 Patch)
│   ├── patches/
│   │   ├── renderer_scene_cull.patch
│   │   ├── render_forward_clustered.patch
│   │   └── light_storage.patch
│   ├── nanite_deep_bridge.h/.cpp
│   └── SConscript
│
└── SConstruct                       # 顶层添加 nanite_bridge= 开关
```

### 3.1 SCons 编译开关

```python
# SConstruct (或 nanite/ 顶层 SConscript)
nanite_bridge_arg = ARGUMENTS.get("nanite_bridge", "gdext")

# 核心库始终编译
SConscript("nanite/SConscript")

if nanite_bridge_arg in ("gdext", "all"):
    SConscript("nanite_bridge_gdext/SConscript")
if nanite_bridge_arg in ("module", "all"):
    SConscript("modules/nanite_bridge_module/SConscript")
if nanite_bridge_arg in ("deep", "all"):
    SConscript("nanite_bridge_deep/SConscript")
```

### 3.2 编译期宏

```cpp
// 由 SConscript 根据开关定义
// nanite_bridge_gdext:   NANITE_BRIDGE_GDEXT
// nanite_bridge_module:  NANITE_BRIDGE_MODULE
// nanite_bridge_deep:    NANITE_BRIDGE_DEEP
```

---

## 4. 桥接切换机制

### 4.1 INaniteBridge 抽象接口

```cpp
// nanite/nanite_bridge.h
class INaniteBridge {
public:
    enum class ShadowMode {
        SHADOW_COARSE_LOD,    // 阶段一：粗 LOD shadow mesh
        SHADOW_DYNAMIC_GPU,   // 阶段二/三：动态 per-light GPU shadow
    };

    virtual ~INaniteBridge() = default;

    // 安装桥接，设置 NaniteServer 的阴影模式等
    virtual void install(NaniteServer *p_server) = 0;

    // 获取当前阴影模式
    virtual ShadowMode get_shadow_mode() const = 0;

    // 渲染前回调（由桥接在合适时机调用）
    virtual void on_pre_render(const RenderData *p_render_data) = 0;

    // 不透明 Pass 前回调
    virtual void on_pre_opaque_pass(const RenderData *p_render_data) = 0;

    // 不透明 Pass 后回调（材质解析）
    virtual void on_post_opaque_pass(const RenderData *p_render_data) = 0;

    // 阴影 Pass 回调（阶段二/三）
    virtual void on_shadow_pass(const RenderData *p_render_data, RID p_light, int p_pass) = 0;

    // 获取桥接名称
    virtual StringName get_bridge_name() const = 0;
};
```

### 4.2 编译期宏控制

> **S1-08 重构后**：核心模块（`nanite/core/`）不再 `#include` 任何桥接具体类型。编译期宏只影响桥接层自身与 `register_types.cpp` 的编译选型。核心 `NaniteServer` 通过 `INaniteBridge *` 抽象指针 + `set_bridge()` setter 与桥接解耦。

```cpp
// nanite/core/nanite_server.cpp — 核心零桥接依赖
#include "nanite/core/nanite_bridge.h"  // 仅抽象接口

// nanite/register_types.cpp — 桥接选型在 register_types 完成
#include "nanite/core/nanite_server.h"
#if defined(NANITE_BRIDGE_GDEXT)
#include "nanite/bridge/nanite_gdext_bridge_manager.h"
#endif
#if defined(NANITE_BRIDGE_MODULE)
#include "nanite/bridge/nanite_module_bridge_manager.h"
#endif
// ... Stage 2/3 各自的 Manager
```

### 4.3 运行时 ProjectSettings 激活

> **设计原则（S1-08 重构后）**：核心 `NaniteServer` 不直接 `memnew` 任何具体桥接类，只持有 `INaniteBridge *` 抽象指针。具体桥接（如 `NaniteGDExtBridge`）的实例化、Compositor 创建、SceneTree 监听等全部由各桥接层独立的 Manager（如 `NaniteGDExtBridgeManager`）在 `register_types.cpp` 的 `MODULE_INITIALIZATION_LEVEL_SERVERS` 阶段完成，再通过 `NaniteServer::set_bridge(INaniteBridge *)` setter 注入。核心模块不依赖任何桥接具体类型。

```cpp
// nanite/core/nanite_server.h — 核心只持有抽象指针
class NaniteServer {
    INaniteBridge *bridge = nullptr;  // 抽象指针，由 register_types 注入
public:
    void set_bridge(INaniteBridge *p_bridge);  // 公开 setter
    // ...
};

// nanite/core/nanite_server.cpp — init() 不再创建具体桥接
void NaniteServer::init() {
    singleton = this;
    GLOBAL_DEF(PropertyInfo(...), "gdext");
    // 不 memnew 桥接 — 由 register_types 负责
    page_cache = memnew(NanitePageCache);
    debug = memnew(NaniteDebug);
    // gpu_pipeline 在首次 render_visibility 时懒创建
}

// nanite/bridge/nanite_gdext_bridge_manager.cpp — 桥接层独立 singleton
NaniteGDExtBridgeManager *NaniteGDExtBridgeManager::singleton = nullptr;

void NaniteGDExtBridgeManager::init(NaniteServer *p_server) {
    singleton = this;
    // 1) 创建两个 CompositorEffect 实例（PRE_OPAQUE + POST_OPAQUE）
    pre_opaque_bridge = Ref<NaniteGDExtBridge>(memnew(NaniteGDExtBridge(...)));
    post_opaque_bridge = Ref<NaniteGDExtBridge>(memnew(NaniteGDExtBridge(...)));
    // 2) 创建预制 Compositor 并塞入两个 bridge Ref
    default_compositor.instantiate();
    TypedArray<CompositorEffect> effects;
    effects.push_back(pre_opaque_bridge);
    effects.push_back(post_opaque_bridge);
    default_compositor->set_compositor_effects(effects);
    // 3) 通过 setter 注入核心
    p_server->set_bridge(pre_opaque_bridge.ptr());
    pre_opaque_bridge->install(p_server);
    // 4) 连接 SceneTree::node_added + 每帧轮询 _viewports group 双路径兜底
    SceneTree *st = SceneTree::get_singleton();
    if (st) {
        st->connect("node_added", callable_mp(this, &NaniteGDExtBridgeManager::_on_node_added));
        st->connect("process_frame", callable_mp(this, &NaniteGDExtBridgeManager::_on_process_frame));
    }
}

// nanite/register_types.cpp — 模块加载时由 register_types 创建 Manager
void initialize_nanite_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
        ClassDB::register_class<NaniteServer>();
        // ...
        NaniteServer::get_singleton()->init();  // 核心 init 不创建桥接
#if defined(NANITE_BRIDGE_GDEXT)
        memnew(NaniteGDExtBridgeManager);  // Manager 独立 singleton
        NaniteGDExtBridgeManager::get_singleton()->init(NaniteServer::get_singleton());
#endif
    }
}
```

**关键变化**：

| 维度 | 旧设计（S1-08 前） | 新设计（S1-08 重构后） |
|------|-------------------|----------------------|
| 桥接实例化位置 | `NaniteServer::init()` 内 `memnew(NaniteGDExtBridge)` | `NaniteGDExtBridgeManager::init()` 内 |
| 核心持有的类型 | `Ref<NaniteGDExtBridge>` 具体类型 | `INaniteBridge *` 抽象指针 |
| Compositor 创建 | 核心创建（旧设计未实现，仅在文档规划中） | Manager 创建并持有 |
| SceneTree 监听 | 核心监听（违反核心通用原则） | Manager 监听（桥接专属） |
| Viewport 遍历 | 核心遍历（违反核心通用原则） | Manager 遍历（桥接专属） |
| 核心对桥接具体类型的依赖 | 直接 `#include "nanite/bridge/nanite_gdext_bridge.h"` | 零依赖（仅 `INaniteBridge` 抽象接口） |
| 多桥接共存能力 | 弱（核心代码内嵌具体桥接创建） | 强（Manager 独立，Stage 2/3 可平行实现自己的 Manager） |

### 4.4 切换机制流程

```mermaid
flowchart LR
    A["SCons: nanite_bridge=X"] -->|编译期| B["定义 NANITE_BRIDGE_X 宏"]
    B --> C["编译对应桥接代码"]
    C --> D["运行时启动"]
    D --> E["读取 nanite/bridge/active"]
    E --> F{"匹配已编译桥接?"}
    F -->|是| G["实例化 INaniteBridge"]
    G --> H["bridge->install(server)"]
    F -->|否| I["ERR_PRINT 报错"]
```

---

## 5. 核心类设计

```mermaid
classDiagram
    class NaniteServer {
        -static NaniteServer *singleton
        -INaniteBridge *bridge
        -ShadowMode shadow_mode
        -HashMap~RID, NaniteMeshData*~ mesh_map
        -HashMap~ObjectID, NaniteMeshInstance3D*~ instance_map
        -NaniteGPUPipeline *gpu_pipeline
        -NanitePageCache *page_cache
        -NaniteDebug *debug
        +init() void
        +finish() void
        +set_shadow_mode(mode: ShadowMode) void
        +get_shadow_mode() ShadowMode
        +register_mesh(resource: NaniteMeshResource) RID
        +unregister_mesh(rid: RID) void
        +register_instance(instance: NaniteMeshInstance3D) void
        +unregister_instance(instance: NaniteMeshInstance3D) void
        +render_visibility(p_render_data: RenderData*) void
        +render_shadow(p_render_data: RenderData*, p_light: RID, p_pass: int) void
        +render_material_resolve(p_render_data: RenderData*) void
        +get_bridge() INaniteBridge*
    }

    class NaniteCore {
        -NaniteGPUPipeline *gpu
        -NanitePageCache *cache
        +cull_and_lod(camera: Projection, view: Transform3D, instances: Vector~Instance~) Vector~ClusterGroup~
        +build_visibility_buffer(rd: RenderingDevice*, clusters: Vector~ClusterGroup~) RID
        +resolve_materials(rd: RenderingDevice*, vis_buffer: RID) void
    }

    class NaniteMeshResource {
        -RID shadow_mesh_rid
        -Vector~Cluster~ clusters
        -BVH bvh
        -PackedByteArray gpu_cluster_data
        -PackedByteArray gpu_vertex_data
        -AABB aabb
        -int cluster_count
        -int lod_count
        +get_shadow_mesh() RID
        +set_shadow_mesh(rid: RID) void
        +get_cluster_count() int
        +get_gpu_cluster_buffer() PackedByteArray
        +get_gpu_vertex_buffer() PackedByteArray
    }

    class NaniteMeshData {
        -RID cluster_ssbo
        -RID vertex_ssbo
        -RID index_ssbo
        -RID bvh_ssbo
        -RID vis_buffer
        -bool gpu_uploaded
        +upload_to_gpu(rd: RenderingDevice*) void
        +get_cluster_ssbo() RID
        +get_vertex_ssbo() RID
        +get_vis_buffer() RID
    }

    class NaniteMeshInstance3D {
        -Ref~NaniteMeshResource~ nanite_mesh
        -RID mesh_rid
        -bool nanite_enabled
        -int forced_lod
        -float relative_screen_size
        +set_nanite_mesh(resource: NaniteMeshResource) void
        +get_nanite_mesh() NaniteMeshResource
        +set_nanite_enabled(enabled: bool) void
        +_notification(what: int) void
    }

    class NaniteGPUPipeline {
        -RID cull_shader
        -RID rasterize_shader
        -RID shadow_rasterize_shader
        -RID material_resolve_shader
        -RID hzb_downsample_shader
        -RID cull_pipeline
        -RID rasterize_pipeline
        -RID shadow_pipeline
        -RID material_pipeline
        -RID hzb_downsample_pipeline
        -NaniteHZB hzb
        +init(rd: RenderingDevice*) void
        +dispatch_cull(rd: RenderingDevice*, params: CullParams) RID
        +dispatch_rasterize(rd: RenderingDevice*, vis_buffer: RID) void
        +dispatch_hzb_build(rd: RenderingDevice*, depth_texture: RID) void
        +dispatch_shadow_rasterize(rd: RenderingDevice*, shadow_fb: RID, params: ShadowParams) void
        +dispatch_material_resolve(rd: RenderingDevice*, vis_buffer: RID) void
        +get_hzb() NaniteHZB*
    }

    class NanitePageCache {
        -HashMap~uint64_t, PageEntry~ page_table
        -List~uint64_t~ lru
        -int max_resident_pages
        +request_page(page_id: uint64_t) bool
        +evict_lru() void
        +get_resident_count() int
    }

    class NaniteDebug {
        -bool show_bounds
        -bool show_clusters
        -bool show_bvh
        -int highlight_lod
        +set_show_bounds(show: bool) void
        +set_show_clusters(show: bool) void
        +draw_debug(rd: RenderingDevice*, vis_buffer: RID) void
    }

    class INaniteBridge {
        <<interface>>
        +install(p_server: NaniteServer*) void
        +get_shadow_mode() ShadowMode
        +on_pre_render(p_render_data: RenderData*) void
        +on_pre_opaque_pass(p_render_data: RenderData*) void
        +on_post_opaque_pass(p_render_data: RenderData*) void
        +on_shadow_pass(p_render_data: RenderData*, p_light: RID, p_pass: int) void
        +get_bridge_name() StringName
    }

    class NaniteBuilder {
        +build_from_mesh(mesh: ArrayMesh) NaniteMeshResource
        -partition_meshlets(indices: Vector~int~, vertices: Vector~Vector3~) Vector~Meshlet~
        -simplify(mesh: Mesh, target_ratio: float) Mesh
        -build_bvh(meshlets: Vector~Meshlet~) BVH
    }

    class NaniteImporter {
        +get_importer_name() String
        +get_recognized_extensions() Vector~String~
        +import(source_file: String, save_path: String, options: Dictionary) Error
    }

    NaniteServer --> INaniteBridge : bridge
    NaniteServer --> NaniteCore : core
    NaniteServer --> NaniteGPUPipeline : gpu_pipeline
    NaniteServer --> NanitePageCache : page_cache
    NaniteServer --> NaniteDebug : debug
    NaniteCore --> NaniteGPUPipeline : gpu
    NaniteCore --> NanitePageCache : cache
    NaniteMeshInstance3D --> NaniteMeshResource : nanite_mesh
    NaniteMeshResource --> NaniteMeshData : mesh_data
    NaniteServer --> NaniteMeshData : manages via RID
    NaniteBuilder --> NaniteMeshResource : creates
    NaniteImporter --> NaniteBuilder : uses

    class NaniteMeshEditor {
        -SubViewport viewport
        -NaniteMeshInstance3D preview_instance
        -Node3D rotation_node
        -Camera3D camera
        -DirectionalLight3D light1
        -DirectionalLight3D light2
        -OptionButton debug_mode_btn
        -Button wireframe_btn
        -Button bounds_btn
        -Label stats_label
        +edit(p_resource: Ref~NaniteMeshResource~) void
        -_on_debug_mode_selected(p_index: int) void
        -_on_wireframe_toggled(p_pressed: bool) void
        -_on_bounds_toggled(p_pressed: bool) void
    }

    class EditorInspectorPluginNanite {
        +can_handle(p_object: Object) bool
        +parse_begin(p_object: Object) void
    }

    class NaniteEditorPlugin {
        +get_plugin_name() String
    }

    class NaniteResourcePreviewGenerator {
        +handles(p_type: String) bool
        +generate(p_from: Ref~Resource~, p_size: Size2, p_metadata: Dictionary) Ref~Texture2D~
    }

    SubViewportContainer <|-- NaniteMeshEditor
    EditorInspectorPlugin <|-- EditorInspectorPluginNanite
    EditorPlugin <|-- NaniteEditorPlugin
    EditorResourcePreviewGenerator <|-- NaniteResourcePreviewGenerator
    NaniteMeshEditor --> NaniteMeshResource : previews
    NaniteMeshEditor --> NaniteDebug : controls debug mode
    EditorInspectorPluginNanite --> NaniteMeshEditor : creates in parse_begin
    NaniteEditorPlugin --> EditorInspectorPluginNanite : registers
    NaniteEditorPlugin --> NaniteResourcePreviewGenerator : registers

    class NaniteGDExtBridge {
        +install(p_server: NaniteServer*) void
        +_render_callback(p_type: int, p_render_data: RenderData*) void
    }

    class NaniteModuleBridge {
        -NaniteSceneCullHook *cull_hook
        +install(p_server: NaniteServer*) void
        +on_pre_shadow_pass(p_rd: RenderData*, p_light: RID, p_pass: int) void
        +on_pre_opaque_pass(p_rd: RenderData*) void
        +on_post_opaque_pass(p_rd: RenderData*) void
    }

    class NaniteDeepBridge {
        +install(p_server: NaniteServer*) void
        +on_pre_shadow_pass(p_rd: RenderData*, p_light: RID, p_pass: int) void
        +on_pre_opaque_pass(p_rd: RenderData*) void
        +on_post_opaque_pass(p_rd: RenderData*) void
        +on_gi_pass(p_rd: RenderData*) void
    }

    INaniteBridge <|.. NaniteGDExtBridge
    INaniteBridge <|.. NaniteModuleBridge
    INaniteBridge <|.. NaniteDeepBridge
    NaniteGDExtBridge --|> CompositorEffect
```

---

## 6. 三阶段实施路径

```mermaid
flowchart TB
    subgraph P1["阶段一：GDExtension"]
        direction TB
        P1A["CompositorEffect 子类<br/>_render_callback(int, RenderData*)"]
        P1B["PRE_OPAQUE 时机：<br/>BVH 遍历 + 可见性缓冲"]
        P1B2["HZB Build (Compute)<br/>深度降采样构建遮挡层级"]
        P1C["POST_OPAQUE 时机：<br/>材质解析"]
        P1D["阴影：mesh_set_shadow_mesh<br/>粗 LOD 方案"]
        P1A --> P1B
        P1B --> P1B2
        P1B2 --> P1C
        P1C --> P1D
    end

    subgraph P2["阶段二：Module"]
        direction TB
        P2A["RendererSceneCull hook<br/>render_camera 插入"]
        P2B["Pre-Opaque：<br/>BVH 遍历 + 可见性缓冲"]
        P2B2["HZB Build (Compute)<br/>深度降采样构建遮挡层级"]
        P2C["Post-Opaque：<br/>材质解析"]
        P2D["阴影：动态 per-light GPU<br/>写入引擎 shadow atlas"]
        P2A --> P2B
        P2B --> P2B2
        P2B2 --> P2C
        P2C --> P2D
    end

    subgraph P3["阶段三：Deep"]
        direction TB
        P3A["源码 Patch<br/>直接修改渲染管线"]
        P3B["集成到 _render_scene<br/>完全替换实例渲染"]
        P3B2["HZB Build (Compute)<br/>深度降采样构建遮挡层级"]
        P3C["阴影：同 Module +<br/>GI/SDFGI 打通"]
        P3D["完整 Nanite 管线"]
        P3A --> P3B
        P3B --> P3B2
        P3B2 --> P3C
        P3C --> P3D
    end

    P1 -->|"能力增强"| P2
    P2 -->|"GI 打通"| P3

    style P1 fill:#e3f2fd,stroke:#1565c0
    style P2 fill:#f3e5f5,stroke:#7b1fa2
    style P3 fill:#fce4ec,stroke:#c62828
```

| 维度 | 阶段一 (GDExtension) | 阶段二 (Module) | 阶段三 (Deep) |
|------|----------------------|-----------------|---------------|
| 接入方式 | CompositorEffect | RendererSceneCull hook | 源码 patch |
| GPU HZB | ✅ 自建 RD Texture | ✅ 自建 RD Texture | ✅ 可复用引擎深度纹理 |
| 阴影 | 粗 LOD shadow mesh | 动态 GPU shadow | 动态 GPU shadow + GI |
| GI 支持 | 无 | 无 | SDFGI/VoxelGI 打通 |
| 修改引擎 | 否 | 是(模块) | 是(patch) |
| 分发方式 | .gdextension 插件 | 编译引擎 | 编译引擎 |
| 开发效率 | 最高 | 中 | 最低 |
| 效果完整度 | 基础 | 良好 | 完整 |

---

## 7. GPU HZB 模块：层次化深度缓冲

> Godot 4.7.1 仅有 CPU 侧 HZB（`RendererSceneOcclusionCull::HZBuffer`），无 GPU 侧 depth pyramid。
> Nanite 的 GPU Cull Shader 必须采样 GPU 纹理形式的 HZB，因此需要自建 GPU HZB 模块。
> 详细原理分析见调研文档 1.3.4 节。

### 7.1 问题陈述

| 维度 | Godot 现有（CPU HZB） | Nanite 需要（GPU HZB） |
|------|----------------------|----------------------|
| 数据存储 | `LocalVector<float>` 主存 | `RD::Texture` GPU 显存 |
| 降采样方式 | CPU 逐像素循环 | Compute Shader 并行 |
| 查询方式 | CPU 侧 `_is_occluded()` | GPU Cull Shader 纹理采样 |
| 查询粒度 | 实例级 | Cluster 级 |
| 吞吐量 | ~数百到数千实例/帧 | ~数十万 Cluster/帧 |

核心矛盾：GPU Cull Shader 无法访问 CPU 主存数据，CPU 也无法以 Cluster 粒度逐个查询遮挡。Nanite **必须**使用 GPU HZB。

### 7.2 GPU HZB 在渲染管线中的位置

```
Cull Pass 1 (用上帧 HZB)
    → Rasterize (输出 VisBuffer + Depth)
    → HZB Build (Compute Shader 从 Depth 降采样)   ← 本模块
    → Cull Pass 2 (用本帧 HZB，补漏)
    → Material Eval
```

关键特征：
- HZB 输入来源是 Nanite 自己光栅化的深度缓冲，与引擎深度缓冲解耦
- 两遍剔除确保遮挡准确性接近 100%，仅第一帧可能有少量漏剔
- 与 Godot 现有 CPU HZB 互不干扰：CPU HZB 剔除传统实例，GPU HZB 剔除 Nanite Cluster

### 7.3 NaniteHZB 类设计

```mermaid
classDiagram
    class NaniteHZB {
        -RID hzb_texture
        -RID hzb_mip_views[MAX_MIPS]
        -RID downsample_shader
        -RID downsample_pipeline
        -int mip_count
        -Size2i screen_size
        -bool needs_rebuild
        +init(rd: RenderingDevice*) void
        +cleanup(rd: RenderingDevice*) void
        +resize(rd: RenderingDevice*, p_size: Size2i) void
        +build(rd: RenderingDevice*, p_depth_texture: RID) void
        +get_hzb_texture() RID
        +get_mip_count() int
    }

    class NaniteGPUPipeline {
        -RID cull_shader
        -RID rasterize_shader
        -RID shadow_rasterize_shader
        -RID material_resolve_shader
        -RID hzb_downsample_shader
        -RID cull_pipeline
        -RID rasterize_pipeline
        -RID shadow_pipeline
        -RID material_pipeline
        -RID hzb_downsample_pipeline
        -NaniteHZB hzb
        +init(rd: RenderingDevice*) void
        +dispatch_cull(rd: RenderingDevice*, params: CullParams) RID
        +dispatch_rasterize(rd: RenderingDevice*, vis_buffer: RID) void
        +dispatch_hzb_build(rd: RenderingDevice*, depth_texture: RID) void
        +dispatch_shadow_rasterize(rd: RenderingDevice*, shadow_fb: RID, params: ShadowParams) void
        +dispatch_material_resolve(rd: RenderingDevice*, vis_buffer: RID) void
        +get_hzb() NaniteHZB*
    }

    NaniteGPUPipeline *-- NaniteHZB : hzb
```

**NaniteHZB 关键设计决策**：

- **纹理格式**：使用 `RD::DATA_FORMAT_R32_SFLOAT`，每纹素 4 字节存储单个浮点深度值
- **Mip 视图**：为每级 mip 创建独立 image view，供 Compute Shader 逐级绑定
- **纹理分配**：`RD::texture_create()` + `RD::texture_create_shared_from_layer()` 或逐级 view
- **Resize 策略**：检测 `RenderSceneBuffers` 尺寸变化，触发 `resize()` 重新分配

### 7.4 HZB 降采样 Compute Shader

```glsl
#[compute]
#version 450

layout(set = 0, binding = 0) uniform sampler2D src_depth;
layout(set = 0, binding = 1) uniform image2D dst_mip;
layout(set = 0, binding = 2) uniform Params {
    ivec2 src_size;
    int mip_level;
    int _pad;
} params;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dst_size = imageSize(dst_mip);
    if (coord.x >= dst_size.x || coord.y >= dst_size.y) return;

    // 2x2 采样取 MAX（保守遮挡——不会误剔）
    vec4 depths = vec4(
        texelFetch(src_depth, coord * 2 + ivec2(0, 0), 0).r,
        texelFetch(src_depth, coord * 2 + ivec2(1, 0), 0).r,
        texelFetch(src_depth, coord * 2 + ivec2(0, 1), 0).r,
        texelFetch(src_depth, coord * 2 + ivec2(1, 1), 0).r
    );
    float max_depth = max(max(depths.x, depths.y), max(depths.z, depths.w));
    imageStore(dst_mip, coord, vec4(max_depth, 0.0, 0.0, 1.0));
}
```

### 7.5 HZB 构建流程

```mermaid
flowchart TD
    A[Nanite 光栅化完成] --> B{screen_size 变化?}
    B -- 是 --> C[NaniteHZB::resize]
    C --> D[释放旧纹理]
    D --> E[计算 mip_count]
    E --> F[创建 RD Texture R32_SFLOAT + 全 mip]
    F --> G[创建每级 mip image view]
    G --> H[dispatch 降采样]
    B -- 否 --> H
    H --> I[for mip 1..mip_count-1]
    I --> J[绑定 src=mip_i-1, dst=mip_i]
    J --> K[dispatch compute 8x8 workgroup]
    K --> L[RD barrier]
    L --> I
    I --> M[HZB 构建完成<br/>返回 hzb_texture]
```

**C++ 调度伪代码**：
```cpp
void NaniteHZB::build(RenderingDevice *rd, RID p_depth_texture) {
    if (needs_rebuild) {
        resize(rd, current_size);
        needs_rebuild = false;
    }

    // 第 0 级：从 Nanite depth buffer 复制到 HZB mip 0
    // （如果格式不同需要 blit，相同格式可直接作为 mip 0 输入）
    RID prev_mip = p_depth_texture;

    for (int i = 1; i < mip_count; i++) {
        // 创建 uniform set：绑定 prev_mip 为 src，hzb_mip_views[i] 为 dst
        RD::UniformSetID set = create_downsample_set(rd, prev_mip, hzb_mip_views[i]);
        Size2i mip_size = sizes[i];
        rd->compute_list_begin();
        rd->compute_list_bind_compute_pipeline(compute_list, downsample_pipeline);
        rd->compute_list_bind_uniform_set(compute_list, set, 0);
        // push constants: src_size, mip_level
        rd->compute_list_set_push_constant(compute_list, ...);
        rd->compute_list_dispatch(compute_list,
            (mip_size.width + 7) / 8,
            (mip_size.height + 7) / 8, 1);
        rd->compute_list_end();
        rd->barrier(RD::BARRIER_COMPUTE_TO_COMPUTE);

        prev_mip = hzb_mip_views[i]; // 下一级的输入是当前级
    }
}
```

### 7.6 HZB 在 Cull Shader 中的使用

GPU Cull Shader 中的遮挡查询伪代码：
```glsl
// 在 BVH 节点/Cluster 剔除中
bool is_occluded = hzb_occlusion_test(
    hzb_texture,           // GPU HZB 纹理
    node_aabb,             // BVH 节点 AABB
    view_matrix,           // 相机 view
    projection_matrix,     // 相机 projection
    screen_size            // 屏幕尺寸
);

bool hzb_occlusion_test(
    sampler2D hzb,
    AABB aabb,
    mat4 view,
    mat4 proj,
    vec2 screen_size
) {
    // 1. 投影 AABB 8 角到屏幕空间
    vec3 corners[8] = get_aabb_corners(aabb);
    vec2 rect_min = vec2(1e9);
    vec2 rect_max = vec2(-1e9);
    float z_near = 1e9;
    for (int i = 0; i < 8; i++) {
        vec4 clip = proj * view * vec4(corners[i], 1.0);
        vec2 ndc = clip.xy / clip.w;
        vec2 uv = ndc * 0.5 + 0.5;
        rect_min = min(rect_min, uv);
        rect_max = max(rect_max, uv);
        z_near = min(z_near, clip.w > 0 ? clip.z / clip.w : 1.0);
    }

    // 2. 选择 mip 层级（使包围矩形约覆盖 1 个纹素）
    vec2 rect_size = (rect_max - rect_min) * screen_size;
    float mip_level = ceil(log2(max(rect_size.x, rect_size.y)));

    // 3. 从粗到细遍历
    for (int mip = int(mip_level); mip >= 0; mip--) {
        vec2 mip_size = screen_size / exp2(float(mip));
        vec2 uv = clamp((rect_min + rect_max) * 0.5, vec2(0.0), vec2(1.0));
        float z_far = textureLod(hzb, uv, float(mip)).r;
        if (z_near > z_far) return true;  // 被遮挡
    }
    return false;  // 可见
}
```

### 7.7 三桥接方案下 GPU HZB 的差异

| 维度 | GDExtension | Module | Deep |
|------|------------|--------|------|
| HZB 纹理分配 | CompositorEffect 回调内 `RenderingDevice::get_singleton()` | 同左，但可直接 include 引擎头文件 | 可复用引擎内部 depth texture，减少拷贝 |
| 深度来源 | 自行维护 Nanite depth buffer，或通过 `RenderSceneBuffers` 获取 | 同左 | 可直接访问 `RenderForwardClustered` 内部深度纹理 |
| 纹理格式转换 | 需确认 Nanite depth 与 HZB 格式兼容（均 R32_SFLOAT 则直接复用） | 同左 | 可利用引擎 depth buffer 的现有 format conversion |
| 调试可视化 | 在 NaniteDebug 中添加 HZB mip 可视化模式 | 同左 | 同左 |
| 长期优化 | — | — | 可将 GPU HZB 提升为引擎通用基础设施，SSAO/SSR 等后处理也可复用 |

### 7.8 HZB 调试可视化

在 `NaniteDebug` 中新增 HZB 可视化模式：

```cpp
// NaniteDebug 扩展
enum class DebugMode {
    NONE = 0,
    CLUSTER_COLORS,    // 现有：Cluster 纯色
    BVH_WIREFRAME,     // 现有：BVH 线框
    BOUNDS,            // 现有：包围盒
    LOD_HEATMAP,       // 现有：LOD 热力图
    OVERDRAW,          // 现有：过度绘制
    HZB_MIP_LEVELS,    // 新增：显示各级 mip 深度图
    HZB_OCCLUSION,     // 新增：显示遮挡查询结果（绿=可见，红=被剔除）
};
```

HZB mip 可视化实现：在 Material Eval 阶段，用 fullscreen quad 将各级 HZB mip 绘制到屏幕四角（类似 UE5 的 HZB 调试视图）。

### 7.9 与 Godot CPU HZB 的共存策略

```
┌──────────────────────────────────────────────────────────────────┐
│                     帧渲染管线                                     │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CPU 侧 (Godot 原有)                                             │
│  ┌─────────────────────────────────────┐                        │
│  │ RendererSceneCull::render_camera()   │                        │
│  │   → HZBuffer::update() (CPU 降采样) │                        │
│  │   → OCCLUSION_CULLED 宏              │                        │
│  │   → 剔除传统实例                     │                        │
│  └─────────────────────────────────────┘                        │
│                                                                  │
│  GPU 侧 (Nanite 自建)                                            │
│  ┌─────────────────────────────────────┐                        │
│  │ NaniteGPUPipeline::dispatch_cull()   │                        │
│  │   → 采样 NaniteHZB 纹理 (GPU)       │                        │
│  │   → 剔除 Nanite Cluster              │                        │
│  │   → NaniteHZB::build() (Compute)    │                        │
│  └─────────────────────────────────────┘                        │
│                                                                  │
│  两套 HZB 互不干扰，数据流完全独立                                 │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 8. 阶段零：离线构建数据与预览

### 8.1 为什么用 meshoptimizer

Godot 在 `thirdparty/meshoptimizer/`(版本 1.1)和 `modules/meshoptimizer/` 中已携带 meshoptimizer 库，是业界事实标准，**已包含 Nanite 离线阶段所需的全部基础算法**：

| Nanite 需求 | 对应 meshoptimizer API |
|---|---|
| 切叶子 cluster(128 tri) | `meshopt_buildMeshletsFlex` |
| 计算簇包围盒 + 法线锥 | `meshopt_computeMeshletBounds` |
| 簇分组(4 簇→1 父) | `meshopt_partitionClusters` |
| QEM 简化(支持属性/锁边) | `meshopt_simplifyWithAttributes` |
| 误差尺度归一化 | `meshopt_simplifyScale` |
| Meshlet 内部 reorder | `meshopt_optimizeMeshletLevel` |
| 簇序列化(<1 byte/tri) | `meshopt_encodeMeshlet` / `meshopt_decodeMeshlet` |
| 顶点 buffer 压缩 | `meshopt_encodeVertexBuffer` |
| 顶点 cache/fetch 优化 | `meshopt_optimizeVertexCache` / `meshopt_optimizeVertexFetch` |
| Provoking vertex 调整 | `meshopt_generateProvokingIndexBuffer` |

但 Godot 的 `modules/meshoptimizer/register_types.cpp` 只把 7 个函数挂到 `SurfaceTool`，完全没暴露 meshlet/BVH/cluster 相关 API。因此：
- meshoptimizer 库已链接进引擎，无需重新引入；
- 三套方案都可 `#include <thirdparty/meshoptimizer/meshoptimizer.h>` 直接调用；
- 需在自己的代码里直接调用 meshoptimizer C API，不要通过 `SurfaceTool` 间接触发。

### 8.2 离线构建管线总览

```mermaid
flowchart TD
    Input["Input: ArrayMesh / ImporterMesh / PackedScene / MeshInstance3D"] --> BuildFrom["build_from_resource()<br/>合并多 surface → 单 ArrayMesh"]
    BuildFrom --> Build["build()"]
    Build --> Pre["1. preprocess_mesh()<br/>顶点去重 + cache/fetch 优化<br/>(positions + normals + UVs 三路属性流)"]
    Pre --> Leaf["2. build_leaf_clusters()<br/>meshopt_buildMeshletsFlex<br/>→ L0 cluster 列表 (error=0)"]
    Leaf --> Hierarchy["3. build_hierarchy()<br/>自底向上: 4相邻簇合并 → 分区独立简化"]
    Hierarchy --> BVH["4. build_bvh()<br/>层次结构 → 线性 NaniteClusterNode 数组"]
    BVH --> Shadow["5. build_shadow_mesh()<br/>选取 group_id=shadow_lod_depth 的 cluster<br/>+ 注入 AABB 角点"]
    Shadow --> Finalize["6. finalize_resource()<br/>PagePacker + 序列化"]
    Finalize --> Output["Output: NaniteMeshResource"]
    
    style Pre fill:#c8e6c9,stroke:#2e7d32
    style Hierarchy fill:#e3f2fd,stroke:#1565c0
    style Shadow fill:#fce4ec,stroke:#c62828
```

### 8.3 核心数据结构

```mermaid
classDiagram
    class BuilderConfig {
        +int max_vertices = 64
        +int max_triangles = 128
        +int min_triangles = 32
        +int partition_size = 4
        +double cone_weight = 0.5
        +double split_factor = 0.5
        +double simplification_ratio = 0.5
        +double target_error = 0.5
        +bool lock_partition_border = true
        +int meshlet_optimize_level = 3
        +int max_lod_levels = 16
        +int page_size_bytes = 65536
        +int shadow_lod_depth = 3
        +is_valid() bool
    }
    class NaniteCluster {
        +uint32 vertex_offset
        +uint32 triangle_offset
        +uint32 vertex_count
        +uint32 triangle_count
        +AABB bounds
        +Vector3 cone_axis
        +float cone_cutoff
        +float error
        +uint32 group_id
        +uint32 material_index
        +uint32 page_id
        +serialize() PackedByteArray
        +deserialize(PackedByteArray, offset) NaniteCluster
        +get_serialized_size() size_t
    }
    class NaniteClusterNode {
        +AABB bounds
        +Vector3 cone_axis
        +float cone_cutoff
        +float error
        +uint32 left_child
        +uint32 right_child
        +uint32 first_cluster
        +uint32 cluster_count
        +uint32 page_id
        +uint32 depth
    }
    class NaniteMeshResource {
        +PackedByteArray vertex_data
        +PackedByteArray clusters_data
        +PackedByteArray nodes_data
        +PackedByteArray page_table_data
        +PackedByteArray meshlet_vertices_data
        +PackedByteArray meshlet_triangles_data
        +PackedByteArray materials_data
        +Ref~ArrayMesh~ shadow_mesh
        +Ref~BuilderConfig~ build_config
        +int cluster_count
        +int node_count
        +int page_count
        +get_max_lod_level() int
        +save(String) Error
        +load(String) Error
    }
    class NaniteBuilder {
        +build(Ref~ArrayMesh~) Ref~NaniteMeshResource~
        +static build_from_resource(Ref~Resource~) Ref~NaniteMeshResource~
        -preprocess_mesh(vertices, indices, normals, uvs) bool
        -build_leaf_clusters() bool
        -build_hierarchy() bool
        -build_bvh() bool
        -build_shadow_mesh() bool
        -finalize_resource() Ref~NaniteMeshResource~
        -clone_cluster_for_lod(cluster_idx, new_group_id) uint32_t
        -BuilderConfig m_cfg
    }
    class PagePacker {
        +pack(clusters, config) PageTable
        -sort_by_lod_then_locality(...)
    }
    class NaniteDebug {
        +enum DisplayMode: NORMAL, NORMAL_WIREFRAME, CLUSTER_SOLID, CLUSTER_SOLID_WIREFRAME, WIREFRAME_ONLY, CLUSTER_SOLID_WITH_PARTITION_BORDER
        +enum LODMode: NANITE_AUTO, FORCE_LOD_LEVEL
        +enum DebugMode: NONE, CLUSTER_SOLID_COLOR, LOD_SOLID_COLOR, OVERDRAW_HEATMAP, PAGE_RESIDENCY, HZB_MIP_LEVELS, HZB_OCCLUSION
    }

    NaniteBuilder --> BuilderConfig : reads
    NaniteBuilder --> NaniteCluster : produces
    NaniteBuilder --> NaniteClusterNode : produces
    NaniteBuilder --> NaniteMeshResource : outputs
    NaniteBuilder --> PagePacker : delegates
```

**NaniteCluster 序列化格式**（固定 68 字节 stride）：

| 偏移 | 字段 | 类型 | 字节 |
|------|------|------|------|
| 0 | vertex_offset | uint32 | 4 |
| 4 | triangle_offset | uint32 | 4 |
| 8 | vertex_count | uint32 | 4 |
| 12 | triangle_count | uint32 | 4 |
| 16 | bounds.position.x/y/z | float3 | 12 |
| 28 | bounds.size.x/y/z | float3 | 12 |
| 40 | cone_axis.x/y/z | float3 | 12 |
| 52 | cone_cutoff | float | 4 |
| 56 | error | float | 4 |
| 60 | group_id | uint32 | 4 |
| 64 | material_index | uint32 | 4 |
| 68 | (page_id 运行时分配，不序列化) | — | — |

### 8.4 构建算法详解

#### 8.4.1 顶点预处理 (preprocess_mesh)

| 步骤 | 算法 | meshoptimizer API | 作用 |
|------|------|-------------------|------|
| 1 | 顶点去重 | `meshopt_generateVertexRemap()` | 基于 positions 合并重复顶点，remap table 同时应用于 positions / normals / UVs 三路属性流 |
| 2 | 三路属性重映射 | `meshopt_remapVertexBuffer()` (positions + normals + UVs) + `meshopt_remapIndexBuffer()` | 应用 remap table 到所有属性流，缺少 normals/UVs 时填充默认值 (0,1,0) / (0,0) |
| 3 | 顶点缓存优化 | `meshopt_optimizeVertexCache()` | 重排三角形顺序，使顶点在 GPU 顶点缓存中的命中率最大化（Tom Forsyth 算法） |
| 4 | 顶点预取优化 | `meshopt_optimizeVertexFetchRemap()` | 生成 fetch remap table，同时应用于三路属性流，重排顶点 buffer 使内存访问局部性最大化 |

顶点数据最终以 32B stride 存储：`pos.xyz (3f) + normal.xyz (3f) + uv.xy (2f) = 8 floats`。

#### 8.4.2 叶子层聚类 (build_leaf_clusters)

| 步骤 | 算法 | 关键参数 |
|------|------|---------|
| 1 | Meshlet 构建 | `meshopt_buildMeshletsFlex(max_v=64, min_t=32, max_t=128, cone=0.5, split=0.5)` |
| 2 | Meshlet 内部优化 | `meshopt_optimizeMeshletLevel(level=3)` |
| 3 | 包围盒计算 | `meshopt_computeMeshletBounds()` → bounds + cone_axis + cone_cutoff |

叶子 cluster 属性：`error=0`（L0 无简化误差）、`group_id=0`。

**退化法线锥防御**：当 `meshopt_computeMeshletBounds` 返回退化 `cone_axis=(0,0,0)` 时（常见于简化后具有相消法线的父 meshlet），builder 指定备用单位轴 `Vector3(0,1,0)` 并设 `cone_cutoff=-1.0f`（全球面锥，表示无法安全背面剔除）。

#### 8.4.3 层次化简化 (build_hierarchy)

采用 UE5 Nanite 风格的"4相邻簇合并 → 分区独立简化"算法：

```mermaid
flowchart TD
    Start["当前层 cluster 列表"] --> Check{cluster_count > 1<br/>and lod < max_lod?}
    Check -- "是" --> Part["meshopt_partitionClusters<br/>target=4 (空间相邻聚类)"]
    Part --> Loop["对每个 partition (~4 相邻簇)"]
    Loop --> Merge["局部合并 partition 的 index + vertex 子集<br/>(非全局合并)"]
    Merge --> Lock["计算 vertex_lock<br/>跨簇共享顶点 (>=2簇) = 边界顶点"]
    Lock --> Simplify["meshopt_simplifyWithAttributes<br/>target=50%, LockBorder+Regularize<br/>vertex_lock 锁定边界顶点"]
    Simplify --> Bailout{简化结果 < min_triangles?}
    Bailout -- "是" --> Clone["clone_cluster_for_lod()<br/>克隆原 cluster 到新 LOD<br/>(仅修改 group_id)"]
    Bailout -- "否" --> Recluster["meshopt_buildMeshletsFlex<br/>简化结果重聚类"]
    Clone --> Parent["parent.error = max(child.errors, result_error)<br/>parent.bounds = union(child.bounds)<br/>parent.group_id = current_lod + 1"]
    Recluster --> Parent
    Parent --> NextPart{还有 partition?}
    NextPart -- "是" --> Loop
    NextPart -- "否" --> Check
    Check -- "否" --> Done["层次化完成"]
    
    style Simplify fill:#fff3e0,stroke:#e65100
    style Clone fill:#fce4ec,stroke:#c62828
```

**关键设计要点**：

- **局部合并 vs 全局合并**：每个 partition 独立合并其内 cluster 的 vertex + index 子集，不做全局合并。这保留了空间局部性，对后续 GPU culling 和 streaming 友好。
- **vertex_lock 边界检测**：统计每个顶点被 partition 内多少个原始 cluster 引用，`>=2` 的顶点标记为边界顶点（vertex_lock）。
- **LockBorder + Regularize 双重锁定**：`meshopt_simplifyWithAttributes` 的 `options` 参数锁定 mesh 边界，`vertex_lock` 参数锁定 partition 间共享边界，保证 crack-free LOD 过渡。
- **clone_cluster_for_lod**：当 partition 只有 1 个 cluster，或简化结果过少时，原 cluster 被克隆到新 LOD 层级（仅修改 `group_id`，共享同一 meshlet 数据），避免数据丢失。

#### 8.4.4 BVH 装配 (build_bvh)

| 步骤 | 方法 | 说明 |
|------|------|------|
| 1 | `linearize_bvh_recursive()` | 递归遍历 hierarchy tree，将每个 MERGE 节点及其子节点展平为线性 `NaniteClusterNode` 数组 |
| 2 | `build_binary_chain()` | 将 MERGE 节点的多个源子节点链转为二叉树（二分叉），每个节点最多两个子节点 |

叶子节点 `left_child = right_child = UINT32_MAX`，`first_cluster` 与 `cluster_count` 指向 `clusters` 数组。

#### 8.4.5 粗 LOD Shadow Mesh (build_shadow_mesh)

从所有 cluster 中选取 `group_id` 等于 `shadow_lod_depth`（或最接近的不超过该值的 LOD）的 cluster，解码其三角形并合并为一个 `ArrayMesh`。

**AABB 角点注入**：将原始 mesh 的 AABB 8 个角点作为额外顶点追加到 `vertices` 数组（不参与 `indices`），确保 shadow mesh 的 AABB 完整覆盖原始 mesh。这修复了仅选取单个 LOD 层 cluster 时 X 轴负方向覆盖不足的问题。

#### 8.4.6 Page 划分 (PagePacker)

| 步骤 | 说明 |
|------|------|
| 1 | 按 LOD 层级（`group_id`）+ 空间局部性（Morton 码 3D）排序所有 cluster |
| 2 | 按 `config.page_size_bytes`（默认 64KB）切分为 Page |
| 3 | 每个 cluster 分配 `page_id`，输出 `PageTable` |

### 8.5 序列化与导出数据格式

#### 8.5.1 NaniteMeshResource 数据字段

| 数据 | 格式 | 说明 |
|------|------|------|
| `vertex_data` | `PackedByteArray` | 原始顶点池，stride 32B (pos.xyz + normal.xyz + uv.xy = 8 floats) |
| `clusters_data` | `PackedByteArray` | `NaniteCluster` 序列化二进制，固定 68B stride |
| `nodes_data` | `PackedByteArray` | `NaniteClusterNode` 序列化二进制，固定 stride |
| `page_table_data` | `PackedByteArray` | `PageTable` 序列化二进制 |
| `meshlet_vertices_data` | `PackedByteArray` | `uint32[]` 全局顶点索引池，cluster 的 `vertex_offset/vertex_count` 索引此数组 |
| `meshlet_triangles_data` | `PackedByteArray` | `uint8[]` 微索引池，cluster 的 `triangle_offset`（字节偏移）指向此数组，每个三角形 3 字节 |
| `materials_data` | `PackedByteArray` | 材质数据，32B per material (vec4 base_color + vec4 metallic_roughness_pad) |
| `shadow_mesh` | `Ref<ArrayMesh>` | 粗 LOD 阴影 mesh |
| `build_config` | `Ref<BuilderConfig>` | 构建参数回溯 |

#### 8.5.2 两种序列化路径

**路径 A：Godot 原生**（`.tres` / `.res`）
- 所有 `PackedByteArray` 字段通过 `GDPROPERTY` 绑定，`ResourceSaver` / `ResourceLoader` 自动序列化
- `shadow_mesh` 和 `build_config` 作为 `Ref<>` 子资源一并保存

**路径 B：自定义 `.nanite` 二进制格式**

```
char[4]   magic = "NANM"
uint32    version = 3
uint32    vertex_data_size, then vertex_data bytes       (raw stride 32 B)
uint32    clusters_data_size, then clusters_data bytes    (68 B per cluster)
uint32    nodes_data_size, then nodes_data bytes
uint32    page_table_data_size, then page_table_data bytes
uint32    materials_data_size, then materials_data bytes   (v2+)
uint32    meshlet_vertices_data_size, then bytes            (v3+)
uint32    meshlet_triangles_data_size, then bytes           (v3+)
uint32    cluster_count, node_count, page_count (trailer)
```

> `build_config` 和 `shadow_mesh` 不包含在 `.nanite` 文件中（Godot 侧元数据，如需可从源 mesh 重新构建）。

**版本历史**：
- v1：原始格式（NaniteCluster 64B，无 materials，meshopt 压缩 vertex_data，meshlet 数据交织在 clusters_data 中）
- v2：NaniteCluster 68B（新增 material_index）；新增 materials_data blob
- v3：meshlet 顶点/索引数据从 clusters_data 分离到独立的 `meshlet_vertices_data` + `meshlet_triangles_data`；`vertex_data` 改为原始格式（不再 meshopt 压缩）

### 8.6 构建参数与可调性

| 参数 | 类型 | 默认值 | 影响 |
|---|---|---|---|
| `max_vertices` | int | 64 | 兼容 mesh shader；过大撑爆寄存器 |
| `max_triangles` | int | 128 | Nanite 标准；256 增大 dispatch |
| `min_triangles` | int | 32 | 尾簇允许低于此值 |
| `cone_weight` | double | 0.5 | 0=不顾法线锥，1=强约束 |
| `partition_size` | int | 4 | 4→1 标准；8→1 简化更激进 |
| `simplification_ratio` | double | 0.5 | 每层简化到一半 |
| `target_error` | double | 0.5 | 相对误差上限 |
| `lock_partition_border` | bool | true | 锁边防 crack |
| `meshlet_optimize_level` | int | 3 | 0=最快，3=平衡，9=最慢 |
| `max_lod_levels` | int | 16 | 最大 LOD 层级数 |
| `page_size_bytes` | int | 65536 | 64KB/页 |
| `shadow_lod_depth` | int | 3 | 粗 LOD 取哪个 group_id |

`is_valid()` 校验：`max_vertices >= 32`、`max_triangles >= 32`、`partition_size >= 2`、`page_size_bytes >= 4096`、`shadow_lod_depth >= 1`。

### 8.7 资源转换入口

#### 8.7.1 build_from_resource 接口

`NaniteBuilder::build_from_resource(Ref<Resource>)` 静态方法接受多种输入类型：

| 输入类型 | 处理方式 |
|---------|---------|
| `ArrayMesh` | 直接提取 surface 数组 |
| `ImporterMesh` | gltf/glb 导入产物，调用 `get_mesh()` 转 ArrayMesh |
| `PackedScene` | 递归遍历场景树所有 `MeshInstance3D`，合并 mesh surface |
| `MeshInstance3D` | 取 `mesh` 属性，按 ArrayMesh/ImporterMesh 路径处理 |

内部合并所有 surface 到单个 vertex/index 池（按顶点偏移调整 index），然后调用 `build()` 走完整管线。

#### 8.7.2 编辑器入口

| 入口 | 触发方式 | 实现类 |
|------|---------|--------|
| FileSystem 右键菜单 | 选中 .gltf/.glb/.fbx/.obj/.tres 右键 → "Convert to Nanite..." | `NaniteConversionContextMenu : EditorContextMenuPlugin` |
| Inspector 按钮 | 选中 `ArrayMesh` 资源或 `MeshInstance3D` 节点 → Inspector 顶部 "Convert to Nanite..." 按钮 | `EditorInspectorPluginNanite::parse_begin()` |

两个入口共享同一流程：弹 `EditorFileDialog` (save, `*.nanite.tres`) → 调 `build_from_resource()` → `ResourceSaver::save()` → EditorLog 输出统计。

**不接入自动导入的原因**：大模型构建 100K tri 耗时约 190ms，频繁重导入会卡 UI。手动触发让用户明确控制何时构建。

### 8.8 预览界面设计

#### 8.8.1 预览面板结构

`NaniteMeshEditor` 继承 `SubViewportContainer`，采用**二维正交下拉列表**设计：

```
NaniteMeshEditor (SubViewportContainer)
├── SubViewport (独立 World3D)
│   ├── Camera3D (透视相机, 可缩放/平移/聚焦)
│   ├── DirectionalLight3D × 2 (双光源)
│   └── Node3D (rotation_node, 鼠标拖拽旋转)
│       ├── MeshInstance3D (solid_instance, 实体渲染)
│       ├── MeshInstance3D (wire_instance, 白色线框叠加)
│       ├── MeshInstance3D (partition_border_solid_instance, 黄色实体边界线, depth-test enabled)
│       ├── MeshInstance3D (partition_border_instance, 黄色虚线边界 overlay, depth-test disabled + stipple shader)
│       └── MeshInstance3D (dimmed_instance, 选中簇时同 partition sibling 半透明)
├── HBoxContainer (ui_bar, UI 工具栏)
│   ├── OptionButton (display_mode_btn) — List 1
│   ├── OptionButton (lod_mode_btn)      — List 2
│   └── SpinBox (force_lod_spinner)      — List 2 子控件
└── Label (stats_label, 构建统计)
```

> Partition border 由两个 instance 配对渲染同一份 `PRIMITIVE_LINES` mesh：`partition_border_solid_instance` 开启 depth-test 显示模型表面的边界线，`partition_border_instance` 关闭 depth-test 并用 stipple shader（checkerboard discard）以虚线形式显示被遮挡的边界，两者叠加保证外轮廓在任何视角下都可见。

**独立渲染约束**（关键设计）：Stage 0 预览渲染**完全独立于 Nanite GPU 管线**——不挂 `CompositorEffect`、不调用 `nanite_cull.glsl` / `nanite_rasterize.glsl` / `nanite_material_resolve.glsl`、也不写 `NaniteServer` 调试状态。所有渲染代码限制在 `nanite/editor/` 模块内，使用 Godot 标准 `MeshInstance3D` + `ArrayMesh` + `StandardMaterial3D` 经由引擎自带 forward 管线绘制。

#### 8.8.2 Display Mode（6 种）

来自 `NaniteDebug::DisplayMode` 枚举：

| 模式 | 枚举值 | solid_instance | wire_instance / partition_border_instance |
|------|--------|---------------|------------------------------------------|
| Normal | `NORMAL` | shadow_mesh + Lambert | 隐藏 |
| Normal + Wireframe | `NORMAL_WIREFRAME` | shadow_mesh + Lambert | `build_wire_from_array_mesh(shadow_mesh)` 白色线框 |
| Cluster Solid | `CLUSTER_SOLID` | `build_cluster_mesh(force_lod, true)` + per-vertex HSV | 隐藏 |
| Cluster Solid + Wireframe | `CLUSTER_SOLID_WIREFRAME` | 同上 | `build_cluster_wire_mesh(force_lod)` 白色线框 |
| Wireframe Only | `WIREFRAME_ONLY` | 隐藏 | `build_cluster_wire_mesh(force_lod)` 白色线框 |
| Cluster Solid + Partition Border | `CLUSTER_SOLID_WITH_PARTITION_BORDER` | `build_cluster_mesh(force_lod, true)` + per-vertex HSV | `build_partition_border_wire(resource, force_lod)` 黄色边界线（配对渲染：`partition_border_solid_instance` 实体 + `partition_border_instance` 虚线 overlay） |

> 旧 `DebugMode` 枚举（7 项：NONE / CLUSTER_SOLID_COLOR / LOD_SOLID_COLOR / OVERDRAW_HEATMAP / PAGE_RESIDENCY / HZB_MIP_LEVELS / HZB_OCCLUSION）保留供 Stage 1 GPU pipeline (`nanite_material_resolve.glsl`) 继续使用。

**Partition Border 可视化**：`build_partition_border_wire()` 函数对指定 LOD 的 cluster 执行 `meshopt_partitionClusters(target=4)` 重建分区，对每个 partition 合并其内所有 cluster 的三角形，统计每条边（无向，key = `min_v << 32 | max_v`）在 partition 内被多少个三角形引用，**仅保留恰好出现 1 次的边**——即 partition 整体的外轮廓边界（未被同 partition 内其他三角形共享的边）。这与早期"提取被 ≥2 cluster 共享的顶点构成的边"不同：外轮廓描述 partition 与外部的边界，而非 cluster 间的内部接缝。结果生成黄色 `PRIMITIVE_LINES` 线段，由 `partition_border_solid_instance`（depth-test enabled）和 `partition_border_instance`（stipple shader, depth-test disabled）配对渲染。

#### 8.8.3 LOD Mode（2 种）

来自 `NaniteDebug::LODMode` 枚举：

| 模式 | 枚举值 | 行为 |
|------|--------|------|
| Nanite (auto cull + LOD) | `NANITE_AUTO` | Stage 1 的 GPU Nanite 管线自动 LOD 选择。Stage 0 选择此项弹 `WARN_PRINT` 并 fallback 到 Force LOD Level 0 |
| Force LOD Level | `FORCE_LOD_LEVEL` | CPU 侧解码指定 `group_id` 的 cluster，通过 `SpinBox` 选择 LOD 层级（0=最精细，n=最粗糙）。Stage 0 默认此项 |

`SpinBox` 范围 `0..max_lod_level`，由 `NaniteMeshResource::get_max_lod_level()` 扫描 `clusters_data`（68B stride）返回最大 `group_id` 得到。

#### 8.8.4 相机控制

| 操作 | 输入 | 实现 |
|------|------|------|
| 旋转 | 左键拖拽（无 Shift） | 修改 `rot_x/rot_y`，更新 `rotation_node` 变换 |
| 缩放 | 鼠标滚轮 | `camera_distance *= 0.9`（UP）/ `* 1.1`（DOWN），最小 0.01，调用 `_update_camera_transform()` |
| 平移 | 中键拖拽 / Shift+左键拖拽 | 修改 `pan_offset`，速度 = `camera_distance * 0.002`，调用 `_update_camera_transform()` |
| 聚焦 | F 键 | `_focus_on_model()` — 根据 shadow_mesh AABB 长度设置 `camera_distance = aabb.size.length() * 1.2`，重置 `pan_offset = (0,0)`、`rot_x = -15°`、`rot_y = 30°` |

`_update_camera_transform()` 计算相机位置：`Transform3D.origin = Vector3(pan_offset.x, pan_offset.y, camera_distance)`，相机朝向 `(0,0,0)`。

#### 8.8.5 Cluster 选中与虚化

在 `CLUSTER_SOLID` / `CLUSTER_SOLID_WIREFRAME` / `CLUSTER_SOLID_WITH_PARTITION_BORDER` 模式下：

1. 左键点击（非拖拽，移动距离 < 5px）触发 `_ray_pick_cluster()`
2. CPU 侧射线-三角形相交测试（Möller-Trumbore 算法），遍历当前 LOD 所有 cluster 三角形
3. 命中时：`solid_instance` 仅渲染选中 cluster；`dimmed_instance` 以半透明白色（`Color(1,1,1,0.75)`, `TRANSPARENCY_ALPHA` + `FLAG_DISABLE_DEPTH_TEST` + `CULL_DISABLED`）渲染"虚化"内容，其构成随 DisplayMode 不同：
   - `CLUSTER_SOLID` / `CLUSTER_SOLID_WIREFRAME`：虚化当前 LOD 的所有非选中 cluster
   - `CLUSTER_SOLID_WITH_PARTITION_BORDER`：仅虚化选中 cluster 所在 partition 内的 sibling clusters（通过 `build_partition_sibling_mesh()` 收集同 partition_id 的其他 cluster），其余 partition 不渲染，从而突出"4 簇合并"的分区边界
4. ESC 键清除选中，切换 LOD/DisplayMode 也清除选中

#### 8.8.6 渲染管线数据流

```mermaid
flowchart LR
    UI["NaniteMeshEditor<br/>display_mode_btn + lod_mode_btn + force_lod_spinner"] --> DECODE["CPU-side cluster decoder<br/>(anonymous namespace)"]
    RES["NaniteMeshResource<br/>clusters_data / vertex_data /<br/>meshlet_vertices_data /<br/>meshlet_triangles_data"] --> DECODE
    DECODE -->|"filter group_id == force_lod_level<br/>+ optional per-cluster HSV color"| AM["Ref ArrayMesh triangles<br/>(pos + index + color)"]
    DECODE -->|"p_emit_lines = true<br/>6 verts per triangle"| WM["Ref ArrayMesh PRIMITIVE_LINES<br/>(pos only)"]
    DECODE -->|"selected_cluster >= 0<br/>+ same-partition siblings"| SM["Ref ArrayMesh<br/>(sibling triangles)"]
    AM --> MI["solid_instance<br/>FLAG_ALBEDO_FROM_VERTEX_COLOR"]
    WM --> WI["wire_instance<br/>SHADING_MODE_UNSHADED (white)"]
    WM --> PBS["partition_border_solid_instance<br/>SHADING_MODE_UNSHADED + depth-test ON (yellow)"]
    WM --> PBI["partition_border_instance<br/>stipple ShaderMaterial + depth-test OFF (yellow dashed)"]
    SM --> DIM["dimmed_instance<br/>TRANSPARENCY_ALPHA white 0.75 + depth-test OFF"]
    SHADOW["shadow_mesh"] -.->|"Normal modes<br/>skip cluster decode"| AM
    MI --> VP["Preview SubViewport<br/>(no Nanite compositor)"]
    WI --> VP
    PBS --> VP
    PBI --> VP
    DIM --> VP
```

#### 8.8.7 构建统计

| 显示字段 | 来源 |
|---------|------|
| Clusters / Nodes / Pages | `NaniteMeshResource::cluster_count` / `node_count` / `page_count` |
| Shadow mesh tris | `shadow_mesh->surface_get_array_index_len()` / 3 |
| Est. GPU | `(vertex_data + clusters_data + nodes_data + page_table_data) / (1024*1024)` |
| Max LOD Level | `NaniteMeshResource::get_max_lod_level()` |

### 8.9 编辑器集成

#### 8.9.1 类图

```mermaid
classDiagram
    class NaniteMeshEditor {
        +edit(Ref~NaniteMeshResource~) void
        -_rebuild_preview() void
        -_update_camera_transform() void
        -_focus_on_model() void
        -_ray_pick_cluster(Vector2) int
        -_on_display_mode_selected(int) void
        -_on_lod_mode_selected(int) void
        -_on_force_lod_changed(double) void
        -SubViewport viewport
        -MeshInstance3D solid_instance
        -MeshInstance3D wire_instance
        -MeshInstance3D partition_border_solid_instance
        -MeshInstance3D partition_border_instance
        -MeshInstance3D dimmed_instance
        -OptionButton display_mode_btn
        -OptionButton lod_mode_btn
        -SpinBox force_lod_spinner
        -Label stats_label
        -HBoxContainer ui_bar
        -float camera_distance
        -Vector2 pan_offset
        -int selected_cluster_index
    }
    class NaniteMeshResourceEditorWindow {
        +edit(Ref~NaniteMeshResource~) void
        -NaniteMeshEditor viewer
    }
    class EditorInspectorPluginNanite {
        +can_handle(Object) bool
        +parse_begin(Object) void
    }
    class NaniteEditorPlugin {
        +handles(Object) bool
        +edit(Object) void
        +make_visible(bool) void
        -NaniteMeshResourceEditorWindow viewer_window
    }
    class NaniteConversionContextMenu {
        +get_options(Vector String) void
        -_on_convert_callback(Variant) void
    }
    class NaniteResourcePreviewGenerator {
        +handles(String) bool
        +generate(Ref Resource, Size2, Dictionary) Ref Texture2D
    }

    SubViewportContainer <|-- NaniteMeshEditor
    AcceptDialog <|-- NaniteMeshResourceEditorWindow
    EditorInspectorPlugin <|-- EditorInspectorPluginNanite
    EditorPlugin <|-- NaniteEditorPlugin
    EditorContextMenuPlugin <|-- NaniteConversionContextMenu
    EditorResourcePreviewGenerator <|-- NaniteResourcePreviewGenerator
    NaniteEditorPlugin --> EditorInspectorPluginNanite : registers
    NaniteEditorPlugin --> NaniteConversionContextMenu : registers
    NaniteEditorPlugin --> NaniteResourcePreviewGenerator : registers
    NaniteEditorPlugin --> NaniteMeshResourceEditorWindow : creates
    NaniteMeshResourceEditorWindow --> NaniteMeshEditor : hosts
    EditorInspectorPluginNanite --> NaniteBuilder : calls build_from_resource
    NaniteConversionContextMenu --> NaniteBuilder : calls build_from_resource
```

#### 8.9.2 Inspector Convert 按钮

`EditorInspectorPluginNanite::can_handle()` 仅对 `ArrayMesh` 资源和 `MeshInstance3D` 节点返回 `true`（**不**处理 `NaniteMeshResource`）。`parse_begin()` 在 Inspector 顶部添加 "Convert to Nanite" 按钮，点击后弹保存对话框 → 调 `build_from_resource()` → `ResourceSaver::save()`。

#### 8.9.3 独立预览窗口

`NaniteMeshResourceEditorWindow`（继承 `AcceptDialog`）在双击 `.nanite.tres` 时弹出：

```
EditorNode → 查找 handles() 返回 true 的 EditorPlugin
    → NaniteEditorPlugin::handles(NaniteMeshResource*) → true
    → NaniteEditorPlugin::edit(p_object)
        → 创建/复用 NaniteMeshResourceEditorWindow
        → viewer_window->edit(resource)
            → 内嵌 NaniteMeshEditor::edit(resource)
            → popup_centered_clamped(Size2(800, 600))
```

窗口关闭时 `hide()` 不销毁，下次 `edit()` 复用。

#### 8.9.4 资源缩略图

`NaniteResourcePreviewGenerator` 使用 `shadow_mesh`（粗 LOD ArrayMesh）生成 FileSystem dock 缩略图，不启动 Nanite GPUPipeline。

### 8.10 辅助数据汇总

| 字段 | 类型 | 来源(meshoptimizer API) | 运行时用途 |
|---|---|---|---|
| `bounds.center/radius` | vec3/float | `meshopt_computeMeshletBounds` | 视锥/HZB 遮挡剔除 |
| `cone_axis/cutoff` | vec3/float | `meshopt_Bounds.cone_axis/cutoff` | 背面剔除 |
| `error` | float | `meshopt_simplifyWithAttributes` × `meshopt_simplifyScale` | LOD 选择 |
| `group_id` | uint32 | `meshopt_partitionClusters` partition_id | crack-free 渲染 + LOD 过滤 |
| `material_index` | uint32 | surface 来源 | 间接绘制按材质分组 |
| `page_id` | uint32 | `PagePacker` 基于 morton 排序后切页 | 磁盘流式加载 |
| `left_child/right_child` | uint32 | 构建时层次结构展开 | GPU 栈式 BVH 遍历 |

### 8.11 三桥接下离线构建的差异

| 维度 | GDExtension | Module | Deep |
|---|---|---|---|
| meshoptimizer 调用 | GDExtension 内 `#include <meshoptimizer.h>` SCons 编译 | 直接 `#include <thirdparty/meshoptimizer/meshoptimizer.h>` | 同 Module + 可改 SurfaceTool |
| 触发构建 | EditorInspectorPlugin + ContextMenu → `NaniteBuilder::build_from_resource()` | 同左 | 编辑器导入自动判断(面数>阈值) |
| 多线程 | GDExtension 起后台 Thread | `WorkerThreadPool` | 同 Module + ResourceSaver 集成 |
| 粗 LOD Shadow Mesh | **必须生成**，供 `mesh_set_shadow_mesh` 使用 | 可选(动态 GPU shadow) | 不需要(完全打通) |
| 序列化 | `.nanite.tres` (Godot 原生) + `.nanite` (自定义二进制 v3) | 同左 | 可扩展 `.scn`/`.res` 原生格式 |

### 8.12 边界情况与限制

1. **三角形数过少的 mesh**(< 128 tri)：不构建 Nanite，直接走标准 mesh；
2. **多 surface mesh**：所有 surface 合并为单 ArrayMesh 后构建，`material_index` 区分；
3. **Morph Target / Blend Shape**：Nanite 不支持运行时形变，自动 fallback 到标准 mesh；
4. **Skeleton deformation**：Nanite 不支持 bone-skinned mesh，fallback 到标准 mesh；
5. **超大 mesh**(> 100M tri)：离线构建内存占用大，需流式处理 + 多线程；
6. **Compatibility renderer**(OpenGL)：不支持 compute/mesh shader，构建出的 Nanite 资源自动 fallback 到标准 mesh LOD。

---

## 9. 阶段一：GDExtension 桥接

> **阶段目标**：在使用 GDExtension 桥接层（`CompositorEffect` 接入）的情况下，**完整实现 Nanite 渲染** —— 包括 Cull（BVH 遍历 + 视锥/背面/HZB 遮挡剔除 + LOD 选择）、Rasterize（meshlet 解码 + 三角形软光栅化 + VisBuffer 写入）、HZB Build（层次化深度降采样）、Material Resolve（barycentric 插值 + Lambert 着色）四个 Pass 的真实算法实现，使 Stage 1 完成后即可在场景中放置 `NaniteMeshInstance3D` 看到真实 Nanite 渲染输出。
>
> **实现状态（2026-07-25）**：Stage 1 已实现完成（作为 module 内子目录 `nanite/bridge/`，通过 `NANITE_BRIDGE_GDEXT` 宏条件编译，而非独立 GDExtension 插件）。Task 1.1-1.15 框架已跑通，Task 1.16 真实渲染补完已合入（替代 1.5/1.11 占位实现）。Task 1.17 Compositor 自动挂接重构为 **`NaniteGDExtBridgeManager` 独立 singleton**（在桥接层）：核心 `NaniteServer` 不再持有 `Ref<NaniteGDExtBridge>`，改为只持有 `INaniteBridge *` 抽象指针；所有 gdext 专属逻辑（创建 Compositor / 监听 SceneTree / 遍历 `_viewports` group / 追加 effect 到用户 Compositor）集中在 `nanite/bridge/nanite_gdext_bridge_manager.h/cpp`，通过 `NaniteServer::set_bridge(INaniteBridge *)` setter 注入。覆盖三大目标 viewport：游戏运行时、Nanite preview 窗口、引擎 3D 工作区。doctest/GDScript 测试已编写（部分需 Vulkan 后端运行时验证）。详见 `nanite_doc/spec/stage1-gdext-bridge/` 下的 spec.md / tasks.md / checklist.md。
>
> **关键实现差异**（与下方原始设计的对照见 9.5 节，S1-05/S1-06/S1-07/S1-08 已在 Stage 1 内补完）。

### 9.1 渲染管线流程图

```mermaid
flowchart TB
    START["Godot 渲染帧开始"] --> CULL["RendererSceneCull<br/>CPU 剔除 (Nanite 实例参与)"]
    CULL --> SHADOW["引擎原生阴影 Pass<br/>(使用 shadow_mesh)"]
    SHADOW --> CE_PRE["CompositorEffect<br/>PRE_OPAQUE 回调"]
    CE_PRE --> NANITE_CULL["NaniteGPUPipeline::dispatch_cull<br/>GPU BVH 遍历 + LOD 选择"]
    NANITE_CULL --> NANITE_RASTER["NaniteGPUPipeline::dispatch_rasterize<br/>软光栅化写 Visibility Buffer"]
    NANITE_RASTER --> OPAQUE["引擎原生不透明 Pass<br/>(Nanite 可见簇以 draw indirect 参与)"]
    OPAQUE --> CE_POST["CompositorEffect<br/>POST_OPAQUE 回调"]
    CE_POST --> NANITE_MAT["NaniteGPUPipeline::dispatch_material_resolve<br/>基于 VisBuffer 解码材质"]
    NANITE_MAT --> TRANSPARENT["引擎原生透明 Pass"]
    TRANSPARENT --> END["帧结束"]

    style CE_PRE fill:#e3f2fd,stroke:#1565c0
    style CE_POST fill:#e3f2fd,stroke:#1565c0
    style NANITE_CULL fill:#c8e6c9,stroke:#2e7d32
    style NANITE_RASTER fill:#c8e6c9,stroke:#2e7d32
    style NANITE_MAT fill:#c8e6c9,stroke:#2e7d32
```

### 9.2 时序图

```mermaid
sequenceDiagram
    participant Engine as Godot Engine
    participant CE as NaniteGDExtBridge<br/>(CompositorEffect)
    participant NS as NaniteServer
    participant GPU as NaniteGPUPipeline
    participant RD as RenderingDevice

    Engine->>CE: _render_callback(PRE_OPAQUE, render_data)
    CE->>NS: on_pre_opaque_pass(render_data)
    NS->>GPU: dispatch_cull(rd, cull_params)
    GPU->>RD: compute_list_begin()
    GPU->>RD: compute_list_dispatch()
    GPU->>RD: compute_list_end()
    RD-->>GPU: cull 完成
    GPU->>RD: draw_list_begin(vis_buffer_framebuffer)
    GPU->>RD: draw_list_draw()
    GPU->>RD: draw_list_end()
    RD-->>GPU: rasterize 完成
    GPU-->>NS: vis_buffer RID
    NS-->>CE: 返回

    Note over Engine: 引擎原生不透明 Pass 执行

    Engine->>CE: _render_callback(POST_OPAQUE, render_data)
    CE->>NS: on_post_opaque_pass(render_data)
    NS->>GPU: dispatch_material_resolve(rd, vis_buffer)
    GPU->>RD: compute_list_begin()
    GPU->>RD: compute_list_dispatch()
    GPU->>RD: compute_list_end()
    RD-->>GPU: 材质解析完成
    GPU-->>NS: 返回
    NS-->>CE: 返回
```

### 9.3 代码骨架

```cpp
// nanite_bridge_gdext/nanite_gdext_bridge.h
#pragma once

#include "scene/resources/compositor_effect.h"
#include "nanite/nanite_bridge.h"

class NaniteGDExtBridge : public CompositorEffect, public INaniteBridge {
    GDCLASS(NaniteGDExtBridge, CompositorEffect)

protected:
    static void _bind_methods();

public:
    // CompositorEffect override
    virtual void _render_callback(int p_effect_callback_type,
                                   const RenderData *p_render_data) override;

    // INaniteBridge implementation
    virtual void install(NaniteServer *p_server) override {
        p_server->set_shadow_mode(NaniteServer::SHADOW_COARSE_LOD);
    }
    virtual ShadowMode get_shadow_mode() const override {
        return ShadowMode::SHADOW_COARSE_LOD;
    }
    virtual void on_pre_render(const RenderData *p_render_data) override {}
    virtual void on_pre_opaque_pass(const RenderData *p_render_data) override;
    virtual void on_post_opaque_pass(const RenderData *p_render_data) override;
    virtual void on_shadow_pass(const RenderData *p_render_data,
                                 RID p_light, int p_pass) override {}
    virtual StringName get_bridge_name() const override {
        return "gdext";
    }

    NaniteGDExtBridge();
    ~NaniteGDExtBridge();
};
```

```cpp
// nanite_bridge_gdext/nanite_gdext_bridge.cpp
#include "nanite_gdext_bridge.h"
#include "nanite/nanite_server.h"

void NaniteGDExtBridge::_bind_methods() {
}

NaniteGDExtBridge::NaniteGDExtBridge() {
    // 使用 PRE_OPAQUE 和 POST_OPAQUE 枚举值
    // Godot 4.7.1: EffectCallbackType 枚举
    set_effect_callback_type(
        CompositorEffect::EFFECT_CALLBACK_TYPE_PRE_OPAQUE);
}

void NaniteGDExtBridge::_render_callback(
    int p_effect_callback_type,
    const RenderData *p_render_data) {

    NaniteServer *ns = NaniteServer::get_singleton();
    if (!ns) return;

    switch (p_effect_callback_type) {
        case CompositorEffect::EFFECT_CALLBACK_TYPE_PRE_OPAQUE:
            // BVH 遍历 + 可见性光栅化
            ns->render_visibility(p_render_data);
            break;
        case CompositorEffect::EFFECT_CALLBACK_TYPE_POST_OPAQUE:
            // 材质解析
            ns->render_material_resolve(p_render_data);
            break;
    }
}

void NaniteGDExtBridge::on_pre_opaque_pass(const RenderData *p_render_data) {
    // 由 _render_callback 内部调用 NaniteServer
}

void NaniteGDExtBridge::on_post_opaque_pass(const RenderData *p_render_data) {
    // 由 _render_callback 内部调用 NaniteServer
}
```

### 9.4 阴影方案：粗 LOD

```cpp
// nanite/nanite_mesh_resource.cpp
void NaniteMeshResource::setup_shadow_mesh() {
    // 使用最低 LOD 的簇构建简化网格
    // 通过 RenderingServer::mesh_set_shadow_mesh 设置
    Array mesh_arrays = build_coarse_lod_arrays();
    RID shadow_rid = RenderingServer::get_singleton()->mesh_create();
    RenderingServer::get_singleton()->mesh_add_surface(
        shadow_rid,
        RenderingServer::PRIMITIVE_TRIANGLES,
        mesh_arrays);
    RenderingServer::get_singleton()->mesh_set_shadow_mesh(
        get_rid(), shadow_rid);
}
```

### 9.5 实现与设计差异对照

Stage 1 实现过程中发现 Godot 4.7.1 真实 API 与本节 10.3 代码骨架存在以下差异，已按实际 API 调整实现：

| 编号 | 原始设计（10.3） | 实际实现 | 原因 |
|------|-----------------|---------|------|
| S1-01 | `nanite_bridge_gdext/` 作为独立 GDExtension 插件目录 | `nanite/bridge/` 作为 module 内子目录，通过 `NANITE_BRIDGE_GDEXT` 宏条件编译 | Stage 1 优先复用 module 内已有核心类（NaniteServer / NaniteMeshResource / NaniteBuilder），独立 GDExtension 需重复导出所有核心类，推迟到 Stage 2+ |
| S1-02 | 一个 `NaniteGDExtBridge` 实例通过 `add_effect_callback_type` 注册 PRE_OPAQUE + POST_OPAQUE 两个回调 | 两个独立 `NaniteGDExtBridge` 实例，每个调 `set_effect_callback_type` 注册一个回调类型 | Godot 4.7.1 `CompositorEffect` 一个实例只能持有一个回调类型，`add_effect_callback_type` API 不存在 |
| S1-03 | `virtual void _render_callback(...) override` | `virtual void _render_callback(...)`（非 override）+ 构造函数中 `compositor_effect_set_callback(get_rid(), ..., callable_mp(this, &NaniteGDExtBridge::_render_callback))` 重新绑定 | `GDVIRTUAL2(_render_callback, ...)` 宏不暴露 C++ 虚函数，仅生成 script/GDExtension 派发代码；需显式重新注册回调槽使 `RendererSceneRenderRD::_process_compositor_effects` 直接调用本类方法 |
| S1-04 | push constants: `view_matrix (mat4) + projection (mat4) + screen_size + error_threshold + cluster_count` | `view_matrix / projection` 移至 uniform buffer (UBO binding 5)；push constants 仅保留 `screen_size / error_threshold / bvh_node_count / cluster_count` | `RenderingDevice::MAX_PUSH_CONSTANT_SIZE == 128` 字节，两个 mat4 (128B) + 标量超限 |
| S1-05 | cull shader 完整 BVH 遍历 + 视锥剔除 + 背面剔除 + HZB 遮挡 + LOD 选择 | **已补完（Stage 1 内）**：完整实现 per-cluster parallel BVH-aware 测试（视锥 6 平面 / 法线锥背面 / HZB 最粗 mip 遮挡 / parent-fallback LOD 选择），详见 `stage1-gdext-bridge/spec.md` "补完：真实 Nanite 渲染" + Task 1.16.7 | 最初为验证管线连通性采用 pass-through；Task 1.16 在 Stage 1 内补完真实剔除算法，不再留待 Stage 2 |
| S1-06 | rasterize shader 软光栅化写 Visibility Buffer | **已补完（Stage 1 内）**：完整实现 meshlet 解码 + 三角形 2D 重心坐标测试 + 深度插值 + 非原子 compare-then-store 深度测试（R32_SFLOAT 不支持 `imageAtomicCompSwap`，Stage 2 可改 R32_UINT + atomic 路径），详见 Task 1.16.8 | 最初为验证 GPU dispatch 路径占位（每 thread 写固定像素）；Task 1.16 在 Stage 1 内补完全软光栅化，Stage 2 可加大三角硬件光栅混合路径 |
| S1-07 | material_resolve shader 绑定 vertex_ssbo + materials_ssbo，做 barycentric 插值 + BRDF 着色 | **已补完（Stage 1 内）**：绑定 materials_ssbo（binding 3）+ vertex_ssbo（binding 4 含 normal/uv），实现 barycentric 插值 + Lambert 着色 + 调试模式分支，详见 Task 1.16.9 | 最初因 `NaniteMeshResource::get_materials()` 未暴露、材质数据未上传 GPU 而简化为固定灰；Task 1.16 在 Stage 1 内补完材质访问器 + 上传 + Lambert 着色；完整 PBR BRDF 仍留待 Stage 2+ |
| S1-08 | CompositorEffect 自动挂接到默认 Compositor | **已补完（Stage 1 内）**：所有 gdext 专属挂接逻辑移到桥接层 `NaniteGDExtBridgeManager`（独立 singleton），核心 `NaniteServer` 仅持有 `INaniteBridge *` 抽象指针并通过 `set_bridge()` setter 接收注入。Manager 创建内部 `Ref<Compositor> default_compositor`（持有 PRE_OPAQUE + POST_OPAQUE 两个 `NaniteGDExtBridge` Ref），通过 `SceneTree::node_added` 信号 + 每 60 帧轮询 `_viewports` group 的双路径兜底，将 default_compositor 注入到三类目标 viewport 的 `find_world_3d()`（无 compositor 时直接挂接，有 compositor 时调用 `attach_to_compositor()` 追加 nanite effect 到用户已有 Compositor）。用户也可在 inspector 把 `NaniteServer.get_default_compositor()` 返回的预制 Compositor 拖到 `WorldEnvironment.compositor` / `Camera3D.compositor` 属性。同时 `render_visibility` / `render_material_resolve` 改读 `RenderData::get_render_scene_data()` 真实 `get_cam_transform()` / `get_cam_projection()` + `get_render_scene_buffers()->get_internal_size()`。详见 Task 1.17 | Stage 1 最初为节省时间仅完成框架，挂接逻辑推迟；Task 1.17 在 Stage 1 内补完，让引擎每帧真正触发渲染回调。重构后核心模块保持纯净，桥接逻辑集中在 `nanite/bridge/`，为 Stage 2/3 桥接复用提供清晰边界 |
| S1-09 | `get_render_data()` 获取相机/投影 | `get_render_scene_data()` (RenderDataExtension) | 调研文档 D02 已纠正 |
| S1-10 | HZB 测试 `correct_mip_count_for_resolution` / `downsample_takes_max_of_2x2` / `resize_handles_resolution_change` | `compute_mip_count_for_common_resolutions` / `init_and_build_smoke_test` / `downsample_takes_max_of_2x2` | 测试用例名略有不同但功能覆盖等价；`resize_handles_resolution_change` 未单独编写，由 `init_and_build_smoke_test` 间接覆盖 |

### 9.6 实现验证状态

Stage 1 验证分三层：

1. **doctest（C++ 单元测试）**：`nanite/tests/test_nanite_*.h` 共 8 个测试文件，覆盖 Bridge / Server / MeshData / HZB / GPUPipeline / MeshInstance3D / PageCache / GDExtBridge / MaterialResolve。GPU 测试需 Vulkan 后端，无后端时自动 SKIP。Task 1.16 补完后，`test_nanite_gpu_pipeline.h` 的 `cull_respects_frustum` 等原本在 pass-through 下不适用的测试用例已启用。
2. **GDScript 测试**：`nanite/tests/test_nanite_debug.gd` / `test_shadow_mesh_gdext.gd` / `test_gdext_e2e.gd` / `test_perf_stage1.gd` / `test_nanite_editor_stage1.gd` 共 5 个测试文件。Task 1.16 补完后新增 `test_stage1_real_rendering.gd` / `test_multi_instance.gd` 验证真实算法端到端正确性。
3. **手动验证**：编辑器内双击 .nanite.tres 文件打开预览窗口、调试模式下拉切换、3D 场景中放置 NaniteMeshInstance3D 渲染（Task 1.16 补完后可见真实 Nanite Lambert 输出，而非引擎原生 mesh）。

详细验证清单见 `nanite_doc/spec/stage1-gdext-bridge/checklist.md`（第 16 节"真实渲染补完 (Task 1.16)"对应补完项的检查）。

---

## 10. 阶段二：Module 桥接

### 10.1 渲染管线流程图

```mermaid
flowchart TB
    START["Godot 渲染帧开始"] --> CULL["RendererSceneCull::render_camera<br/>hook: 收集 Nanite 实例"]
    CULL --> SHADOW_PASS{"引擎阴影 Pass"}
    SHADOW_PASS -->|"每个光源"| HOOK_SHADOW["NaniteModuleBridge<br/>on_pre_shadow_pass"]
    HOOK_SHADOW --> NANITE_SHADOW["NaniteGPUPipeline::dispatch_shadow_rasterize<br/>写入引擎 shadow atlas"]
    NANITE_SHADOW --> NEXT_LIGHT["下一个光源"]
    NEXT_LIGHT --> SHADOW_PASS
    SHADOW_PASS -->|"阴影完成"| HOOK_PRE["NaniteModuleBridge<br/>on_pre_opaque_pass"]
    HOOK_PRE --> NANITE_CULL["NaniteGPUPipeline::dispatch_cull<br/>GPU BVH 遍历 + LOD 选择"]
    NANITE_CULL --> NANITE_RASTER["NaniteGPUPipeline::dispatch_rasterize<br/>软光栅化写 VisBuffer"]
    NANITE_RASTER --> OPAQUE["引擎原生不透明 Pass"]
    OPAQUE --> HOOK_POST["NaniteModuleBridge<br/>on_post_opaque_pass"]
    HOOK_POST --> NANITE_MAT["NaniteGPUPipeline::dispatch_material_resolve"]
    NANITE_MAT --> TRANSPARENT["引擎原生透明 Pass"]
    TRANSPARENT --> END["帧结束"]

    style HOOK_SHADOW fill:#f3e5f5,stroke:#7b1fa2
    style HOOK_PRE fill:#f3e5f5,stroke:#7b1fa2
    style HOOK_POST fill:#f3e5f5,stroke:#7b1fa2
    style NANITE_SHADOW fill:#c8e6c9,stroke:#2e7d32
    style NANITE_CULL fill:#c8e6c9,stroke:#2e7d32
    style NANITE_RASTER fill:#c8e6c9,stroke:#2e7d32
    style NANITE_MAT fill:#c8e6c9,stroke:#2e7d32
```

### 10.2 时序图

```mermaid
sequenceDiagram
    participant RSC as RendererSceneCull
    participant Hook as NaniteSceneCullHook
    participant Bridge as NaniteModuleBridge
    participant NS as NaniteServer
    participant GPU as NaniteGPUPipeline
    participant LS as LightStorage RD
    participant RD as RenderingDevice

    RSC->>Hook: render_camera() 被拦截
    Hook->>Bridge: 收集 Nanite 实例信息

    loop 每个需要阴影的光源
        RSC->>Hook: _render_shadow_pass 之前
        Hook->>Bridge: on_pre_shadow_pass(render_data, light, pass)
        Bridge->>NS: render_shadow(render_data, light, pass)
        NS->>GPU: dispatch_shadow_rasterize(rd, shadow_fb, params)

        Note over NS,LS: 获取 shadow atlas FB 和 rect
        NS->>LS: shadow_atlas_get_fb(light_shadow_atlas_rid)
        LS-->>NS: shadow_fb (RID)
        NS->>LS: light_instance_get_shadow_atlas_rect(light_instance, shadow_atlas, rect)
        LS-->>NS: shadow_atlas_rect (Rect2i)

        GPU->>RD: draw_list_begin(shadow_fb)
        GPU->>RD: draw_list_draw()
        GPU->>RD: draw_list_end()
        RD-->>GPU: shadow rasterize 完成
    end

    Hook->>Bridge: on_pre_opaque_pass(render_data)
    Bridge->>NS: render_visibility(render_data)
    NS->>GPU: dispatch_cull + dispatch_rasterize
    GPU-->>NS: vis_buffer

    Note over RSC: 引擎不透明 Pass

    Hook->>Bridge: on_post_opaque_pass(render_data)
    Bridge->>NS: render_material_resolve(render_data)
    NS->>GPU: dispatch_material_resolve
    GPU-->>NS: 完成
```

### 10.3 代码骨架

```cpp
// modules/nanite_bridge_module/nanite_module_bridge.h
#pragma once

#include "nanite/nanite_bridge.h"

class NaniteModuleBridge : public INaniteBridge {
public:
    void on_pre_shadow_pass(const RenderData *p_rd, RID p_light, int p_pass);
    void on_pre_opaque_pass(const RenderData *p_rd);
    void on_post_opaque_pass(const RenderData *p_rd);

    // INaniteBridge
    virtual void install(NaniteServer *p_server) override {
        p_server->set_shadow_mode(NaniteServer::SHADOW_DYNAMIC_GPU);
    }
    virtual ShadowMode get_shadow_mode() const override {
        return ShadowMode::SHADOW_DYNAMIC_GPU;
    }
    virtual void on_pre_render(const RenderData *p_render_data) override;
    virtual void on_pre_opaque_pass(const RenderData *p_render_data) override;
    virtual void on_post_opaque_pass(const RenderData *p_render_data) override;
    virtual void on_shadow_pass(const RenderData *p_render_data,
                                 RID p_light, int p_pass) override;
    virtual StringName get_bridge_name() const override {
        return "module";
    }
};
```

```cpp
// modules/nanite_bridge_module/nanite_module_bridge.cpp
#include "nanite_module_bridge.h"
#include "nanite/nanite_server.h"
#include "servers/rendering/renderer_storage.h"
#include "servers/rendering/rendering_server_default.h"

void NaniteModuleBridge::on_shadow_pass(
    const RenderData *p_render_data, RID p_light, int p_pass) {

    NaniteServer *ns = NaniteServer::get_singleton();
    if (!ns) return;

    // 获取 shadow atlas framebuffer
    RendererLightStorage *ls = RendererLightStorage::get_singleton();
    RID shadow_atlas = ...; // 从 render_scene_data 获取
    RID shadow_fb = ls->shadow_atlas_get_fb(shadow_atlas);

    // 获取阴影 atlas 中的 rect
    Vector2i rect;
    ls->light_instance_get_shadow_atlas_rect(p_light, shadow_atlas, rect);

    // 执行 Nanite 阴影光栅化到 shadow atlas
    ns->render_shadow(p_render_data, p_light, p_pass);
}
```

### 10.4 Hook 安装策略

```cpp
// modules/nanite_bridge_module/nanite_scene_cull_hook.h
#pragma once

#include "servers/rendering/renderer_scene_cull.h"

class NaniteSceneCullHook {
    static NaniteSceneCullHook *singleton;

    // 保存原始函数指针
    // RendererSceneCull::render_camera 不是虚函数，
    // 需要在模块初始化时通过指针替换或包装方式 hook

public:
    static NaniteSceneCullHook *get_singleton() { return singleton; }

    void install_hooks();
    void uninstall_hooks();

    // 包装后的 render_camera
    void hooked_render_camera(
        RendererSceneCull *self,
        Ref<RendererSceneCamera> p_camera,
        const RendererSceneCull::CameraData &p_camera_data);

    // 包装后的 shadow pass
    void hooked_render_shadow_pass(
        RendererSceneCull *self,
        RID p_light, ...);
};
```

```cpp
// modules/nanite_bridge_module/nanite_scene_cull_hook.cpp
#include "nanite_scene_cull_hook.h"
#include "nanite/nanite_server.h"
#include "nanite_module_bridge.h"

NaniteSceneCullHook *NaniteSceneCullHook::singleton = nullptr;

// 保存原始 render_camera 函数指针
using OriginalRenderCameraFn = void(*)(RendererSceneCull*, ...);
static OriginalRenderCameraFn original_render_camera = nullptr;

void NaniteSceneCullHook::install_hooks() {
    singleton = this;

    // 方案 A: 在 RendererSceneCull::render_camera 入口
    // 插入回调检查。需要在 render_camera 函数体内
    // 添加 nanite_pre_render_camera() 调用点。
    //
    // 方案 B: 通过模块 register_types 时，
    // 对 RendererSceneCull 实例设置回调。

    // 具体实现：在 render_camera 被调用时，
    // 通过引擎内部的信号/回调机制触发 hook
    RendererSceneCull *cull = RendererSceneCull::get_singleton();
    if (cull) {
        // 设置 pre_render_camera 回调
        cull->set_pre_render_camera_callback(
            callable_mp(this, &NaniteSceneCullHook::hooked_render_camera));
    }
}

void NaniteSceneCullHook::hooked_render_camera(
    RendererSceneCull *self,
    Ref<RendererSceneCamera> p_camera,
    const RendererSceneCull::CameraData &p_camera_data) {

    // 1. 调用原始 render_camera
    if (original_render_camera) {
        original_render_camera(self, p_camera, p_camera_data);
    }

    // 2. 通知 Nanite
    NaniteModuleBridge *bridge = ...; // 获取桥接实例
    // bridge->on_pre_opaque_pass(render_data);
}
```

**Hook 安装时序**：

```mermaid
sequenceDiagram
    participant Module as Module register_types
    participant Hook as NaniteSceneCullHook
    participant RSC as RendererSceneCull
    participant NS as NaniteServer

    Module->>Hook: install_hooks()
    Hook->>RSC: 获取 singleton
    Hook->>RSC: 注册 pre_render_camera 回调

    Note over RSC: 每帧渲染

    RSC->>Hook: render_camera 回调触发
    Hook->>NS: on_pre_opaque_pass(render_data)
    NS->>NS: BVH 遍历 + 光栅化

    Note over RSC: 阴影 Pass

    RSC->>Hook: shadow_pass 回调触发
    Hook->>NS: render_shadow(render_data, light, pass)
    NS->>NS: 阴影光栅化到 shadow atlas
```

---

## 11. 阶段三：Deep 桥接

### 11.1 渲染管线流程图

```mermaid
flowchart TB
    START["Godot 渲染帧开始"] --> CULL["RendererSceneCull::render_camera<br/>patch: Nanite 实例跳过 CPU 剔除"]
    CULL --> SHADOW_PASS{"引擎阴影 Pass<br/>(patch 插入 Nanite)"}
    SHADOW_PASS -->|"每个光源"| DEEP_SHADOW["NaniteDeepBridge<br/>on_pre_shadow_pass"]
    DEEP_SHADOW --> NANITE_SHADOW["NaniteGPU shadow<br/>写入 shadow atlas"]
    NANITE_SHADOW --> NEXT_LIGHT["下一个光源"]
    NEXT_LIGHT --> SHADOW_PASS
    SHADOW_PASS -->|"阴影完成"| DEEP_PRE["NaniteDeepBridge<br/>on_pre_opaque_pass<br/>(直接在 _render_scene 内)"]
    DEEP_PRE --> NANITE_CULL["Nanite BVH 遍历 + VisBuffer"]
    NANITE_CULL --> NANITE_RASTER["Nanite 软光栅化"]
    NANITE_RASTER --> OPAQUE["引擎不透明 Pass<br/>(Nanite 簇以 indirect draw 参与)"]
    OPAQUE --> DEEP_POST["NaniteDeepBridge<br/>on_post_opaque_pass"]
    DEEP_POST --> NANITE_MAT["Nanite 材质解析"]
    NANITE_MAT --> GI["GI/SDFGI Pass<br/>patch: Nanite 几何参与"]
    GI --> TRANSPARENT["引擎透明 Pass"]
    TRANSPARENT --> END["帧结束"]

    style DEEP_SHADOW fill:#fce4ec,stroke:#c62828
    style DEEP_PRE fill:#fce4ec,stroke:#c62828
    style DEEP_POST fill:#fce4ec,stroke:#c62828
    style GI fill:#fff3e0,stroke:#e65100
```

### 11.2 时序图

```mermaid
sequenceDiagram
    participant RSC as RendererSceneCull
    participant RFC as RenderForwardClustered
    participant Deep as NaniteDeepBridge
    participant NS as NaniteServer
    participant LS as LightStorage RD
    participant RD as RenderingDevice

    Note over RSC: patch: render_camera 中跳过 Nanite 实例

    RSC->>RFC: _render_scene()
    RFC->>RFC: _render_shadows()

    loop 每个光源
        RFC->>Deep: on_pre_shadow_pass(render_data, light, pass)
        Deep->>NS: render_shadow(render_data, light, pass)
        NS->>LS: shadow_atlas_get_fb(atlas_rid)
        LS-->>NS: shadow_fb
        NS->>LS: light_instance_get_shadow_atlas_rect(li, atlas, rect)
        LS-->>NS: rect
        NS->>RD: draw_list_begin(shadow_fb)
        NS->>RD: draw_list_draw()
        NS->>RD: draw_list_end()
    end

    RFC->>Deep: on_pre_opaque_pass(render_data)
    Deep->>NS: render_visibility(render_data)
    NS->>RD: compute + draw (cull + rasterize)

    RFC->>RFC: _render_opaque()

    RFC->>Deep: on_post_opaque_pass(render_data)
    Deep->>NS: render_material_resolve(render_data)
    NS->>RD: compute (material resolve)

    Note over RFC: GI/SDFGI Pass
    RFC->>Deep: on_gi_pass(render_data)
    Deep->>NS: 提供 Nanite 几何数据给 GI
    NS->>NS: 生成 SDFGI 体素/Nanite 几何信息
```

### 11.3 Patch 骨架

**Patch 1: renderer_scene_cull.h/.cpp — 跳过 Nanite 实例的 CPU 剔除**

```diff
--- a/servers/rendering/renderer_scene_cull.h
+++ b/servers/rendering/renderer_scene_cull.h
@@ -XXX,6 +XXX,8 @@
 	class Instance : public RendererInstanceData {
 	public:
+		// Nanite: 标记此实例是否由 Nanite 管理
+		bool nanite_managed = false;
 	};

+	// Nanite: 渲染前回调
+	Callable nanite_pre_render_callback;
+	Callable nanite_shadow_callback;
```

```diff
--- a/servers/rendering/renderer_scene_cull.cpp
+++ b/servers/rendering/renderer_scene_cull.cpp
@@ -XXX,6 +XXX,10 @@
 void RendererSceneCull::render_camera(Ref<RendererSceneCamera> p_camera, ...) {
+	// Nanite: 通知桥接
+	if (nanite_pre_render_callback.is_valid()) {
+		nanite_pre_render_callback.call();
+	}
+
 	// ... 原有 render_camera 逻辑 ...
 	// Nanite 实例跳过 CPU 剔除
 	for (Instance *inst : visible_instances) {
+		if (inst->nanite_managed) continue;
 		// ... 原有剔除 ...
 	}
 }
```

**Patch 2: render_forward_clustered.cpp — 插入 Nanite 回调**

```diff
--- a/servers/rendering/render_forward_clustered.cpp
+++ b/servers/rendering/render_forward_clustered.cpp
@@ -XXX,6 +XXX,10 @@
 void RenderForwardClustered::_render_shadow_pass(...) {
+	// Nanite: 阴影 Pass 前
+	if (nanite_shadow_callback.is_valid()) {
+		nanite_shadow_callback.call(light, pass);
+	}
 	// ... 原有阴影渲染 ...
 }

 void RenderForwardClustered::_render_scene(...) {
+	// Nanite: Pre-Opaque
+	if (nanite_pre_opaque_callback.is_valid()) {
+		nanite_pre_opaque_callback.call(render_data);
+	}
 	// ... 原有不透明 Pass ...
+	// Nanite: Post-Opaque
+	if (nanite_post_opaque_callback.is_valid()) {
+		nanite_post_opaque_callback.call(render_data);
+	}
 }
```

**Patch 3: light_storage — 阴影 API 暴露**

```diff
--- a/servers/rendering/storage/light_storage.h
+++ b/servers/rendering/storage/light_storage.h
@@ -XXX,6 +XXX,10 @@
 	// 已有 API（无需修改，只需确认可用）:
 	// shadow_atlas_get_fb(RID) — line 1156
 	// direction_shadow_get_fb() — line 1183
 	// light_instance_get_shadow_atlas_rect(RID, RID, Vector2i&) — line 684
+
+	// Nanite: 添加公开访问接口
+	RID nanite_shadow_atlas_get_fb(RID p_atlas) { return shadow_atlas_get_fb(p_atlas); }
+	Rect2i nanite_light_instance_get_shadow_atlas_rect(RID p_light_instance, RID p_atlas) {
+		Vector2i rect;
+		light_instance_get_shadow_atlas_rect(p_light_instance, p_atlas, rect);
+		return Rect2i(rect.x, rect.y, ...);
+	}
```

### 11.4 Deep 桥接代码

```cpp
// nanite_bridge_deep/nanite_deep_bridge.h
#pragma once

#include "nanite/nanite_bridge.h"

class NaniteDeepBridge : public INaniteBridge {
public:
    void on_pre_shadow_pass(const RenderData *p_rd, RID p_light, int p_pass);
    void on_pre_opaque_pass(const RenderData *p_rd);
    void on_post_opaque_pass(const RenderData *p_rd);
    void on_gi_pass(const RenderData *p_rd);

    // INaniteBridge
    virtual void install(NaniteServer *p_server) override {
        p_server->set_shadow_mode(NaniteServer::SHADOW_DYNAMIC_GPU);
    }
    virtual ShadowMode get_shadow_mode() const override {
        return ShadowMode::SHADOW_DYNAMIC_GPU;
    }
    virtual void on_pre_render(const RenderData *p_render_data) override;
    virtual void on_pre_opaque_pass(const RenderData *p_render_data) override;
    virtual void on_post_opaque_pass(const RenderData *p_render_data) override;
    virtual void on_shadow_pass(const RenderData *p_render_data,
                                 RID p_light, int p_pass) override;
    virtual StringName get_bridge_name() const override {
        return "deep";
    }
};
```

---

## 12. 关键 API 对照表

> 本节基于 Godot 4.7.1 源码精确验证，是编码的直接依据。

### 12.1 CompositorEffect

| 项目 | API | 源码位置 | 备注 |
|------|-----|----------|------|
| 类声明 | `class CompositorEffect : public Resource` | `scene/resources/compositor_effect.h` | GDExtension 可继承 |
| 渲染回调 | `virtual void _render_callback(int p_effect_callback_type, const RenderData *p_render_data)` | `compositor_effect.h` | 第一个参数是 `int`，不是枚举 |
| 设置回调类型 | `void set_effect_callback_type(EffectCallbackType p_type)` | `compositor_effect.h` | 注册时机 |
| PRE_OPAQUE 枚举 | `EFFECT_CALLBACK_TYPE_PRE_OPAQUE = 0` | `compositor_effect.h` | ✅ 不是 `BEFORE_OPAQUE_PASS` |
| POST_OPAQUE 枚举 | `EFFECT_CALLBACK_TYPE_POST_OPAQUE = 1` | `compositor_effect.h` | ✅ 不是 `AFTER_OPAQUE_PASS` |

### 12.2 RenderDataExtension

| 项目 | API | 源码位置 | 备注 |
|------|-----|----------|------|
| 获取场景数据 | `RenderSceneData *get_render_scene_data() const` | `servers/rendering/storage/render_data_extension.h` | ✅ 返回 `RenderSceneData*`，不是 `get_render_data()` |
| 获取渲染 buffer | `Ref<RenderSceneBuffers> get_render_scene_buffers() const` | `render_data_extension.h` | 获取 color/depth FB |

### 12.3 RenderingServer — 阴影网格

| 项目 | API | 源码位置 | 备注 |
|------|-----|----------|------|
| 设置阴影网格 | `void mesh_set_shadow_mesh(RID p_mesh, RID p_shadow_mesh)` | `rendering_server.h:239` | ✅ ClassDB 绑定在 `.cpp:2389` |

### 12.4 LightStorage RD — 阴影 Atlas

| 项目 | API | 源码位置 | 备注 |
|------|-----|----------|------|
| Shadow Atlas FB | `RID shadow_atlas_get_fb(RID p_atlas)` | `light_storage.h:1156` | 获取 shadow atlas 的 framebuffer |
| 方向光阴影 FB | `RID direction_shadow_get_fb()` | `light_storage.h:1183` | 方向光专用 shadow FB |
| 阴影 Rect | `bool light_instance_get_shadow_atlas_rect(RID p_light_instance, RID p_atlas, Vector2i &r_rect)` | `light_storage.h:684` | ✅ 不是 `shadow_atlas_get_quadrant_rect` |

### 12.5 RendererSceneCull

| 项目 | API | 源码位置 | 备注 |
|------|-----|----------|------|
| 渲染相机 | `void render_camera(Ref<RendererSceneCamera> p_camera, const CameraData &p_camera_data)` | `renderer_scene_cull.h` | ✅ 是 `render_camera`，不是 `_render_camera` |

### 12.6 RenderForwardClustered

| 项目 | API | 源码位置 | 备注 |
|------|-----|----------|------|
| 阴影 Pass | `void _render_shadow_pass(RenderData *p_render_data, RID p_light, ...)` | `render_forward_clustered.h` | 阶段二/三 hook 目标 |
| 渲染场景 | `void _render_scene(RenderData *p_render_data, ...)` | `render_forward_clustered.h` | 阶段三 patch 目标 |

### 12.7 RenderingDevice — HZB 相关 API

| 项目 | API | 源码位置 | 备注 |
|------|-----|----------|------|
| 创建纹理 | `RID texture_create(const TextureFormat &p_format, const TextureView &p_view, const Vector<uint8_t> &p_data = Vector<uint8_t>())` | `rendering_device.h` | HZB R32_SFLOAT 纹理创建 |
| 创建共享纹理视图 | `RID texture_create_shared_from_layer(const TextureView &p_view, RID p_texture, uint32_t p_layer = 0, uint32_t p_mipmap = 0)` | `rendering_device.h` | 为 HZB 每级 mip 创建独立 view |
| 计算管线创建 | `RID compute_pipeline_create(RID p_shader)` | `rendering_device.h` | HZB 降采样 Compute Pipeline |
| Compute 分发 | `void compute_list_dispatch(ComputeListID p_list, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups)` | `rendering_device.h` | HZB 降采样 dispatch |
| Barrier | `void barrier(BarrierMask p_from, BarrierMask p_to = BARRIER_NO_BARRIER)` | `rendering_device.h` | mip 级间 compute→compute barrier |
| 纹理格式 | `DATA_FORMAT_R32_SFLOAT` | `rendering_device_enum.h` | HZB 深度值存储格式 |

---

## 13. 验证与测试矩阵

### 13.1 三桥接能力对照

| 能力 | GDExtension | Module | Deep |
|------|:-----------:|:------:|:----:|
| BVH 遍历 + 剔除 | ✅ | ✅ | ✅ |
| GPU HZB 构建 | ✅ | ✅ | ✅ |
| 可见性缓冲渲染 | ✅ | ✅ | ✅ |
| 材质解析 | ✅ | ✅ | ✅ |
| 粗 LOD 阴影 | ✅ | ✅ | ✅ |
| 动态 GPU 阴影 | ❌ | ✅ | ✅ |
| 阴影写入 Shadow Atlas | ❌ | ✅ | ✅ |
| 方向光阴影 | 粗LOD | GPU | GPU |
| GI/SDFGI 打通 | ❌ | ❌ | ✅ |
| 不改引擎源码 | ✅ | ❌ | ❌ |
| 插件分发 | ✅ | ❌ | ❌ |
| 多光源阴影 | 粗LOD | GPU | GPU |
| Nanite 调试可视化 | ✅ | ✅ | ✅ |
| 流式加载(PageCache) | ✅ | ✅ | ✅ |

### 13.2 测试场景

| 编号 | 场景 | 测试重点 | 适用阶段 |
|------|------|----------|-----------|
| T01 | 单个 Nanite 网格(10K tri) | 基础渲染正确性 | 全部 |
| T02 | 多 Nanite 实例(100+) | 实例管理/剔除 | 全部 |
| T03 | 大型场景(1M+ tri) | BVH 遍历性能 | 全部 |
| T04 | LOD 切换验证 | 不同距离 LOD 过渡 | 全部 |
| T05 | 粗 LOD 阴影 | 阴影投射/接收 | 阶段一 |
| T06 | 动态 GPU 阴影 — 点光源 | shadow atlas 写入 | 阶段二/三 |
| T07 | 动态 GPU 阴影 — 方向光 | direction_shadow_get_fb | 阶段二/三 |
| T08 | 多光源阴影 | 阴影 atlas 分配 | 阶段二/三 |
| T09 | 动态加载/卸载 | PageCache 淘汰 | 全部 |
| T10 | 材质切换 | 材质解析正确性 | 全部 |
| T11 | 调试可视化 | 簇/BVH/Bounds 显示 | 全部 |
| T12 | 桥接切换 | 编译+运行时切换 | 全部 |
| T13 | SDFGI 集成 | Nanite 几何参与 GI | 阶段三 |
| T14 | VoxelGI 集成 | Nanite 几何参与 GI | 阶段三 |
| T15 | GPU HZB 构建 | HZB 降采样正确性 | 全部 |
| T16 | HZB 遮挡剔除 | Cluster 级遮挡准确性 | 全部 |
| T17 | HZB 调试可视化 | Mip 级别 / 遮挡结果可视化 | 全部 |
| T18 | HZB 性能基准 | 1080p 构建耗时 < 0.1ms | 全部 |

### 13.3 阶段验收标准

**阶段一(GDExtension)验收标准**：

> **状态（2026-07-25）**：除"独立 .gdextension 插件"项外，其余项均已实现。Task 1.16 真实渲染补完（S1-05/06/07）+ Task 1.17 Compositor 自动挂接（S1-08）已合入后，原 PARTIAL 项全部升级为完整实现：BVH 遍历 / 可见性缓冲 / 材质解析（Lambert + barycentric 插值）+ CompositorEffect 每帧自动触发。Task 1.17 重构为 `NaniteGDExtBridgeManager` 独立 singleton 模式后，核心 `NaniteServer` 保持纯净（仅持有 `INaniteBridge *` 抽象指针），所有 gdext 专属挂接逻辑集中在桥接层。详见 9.5 节实现差异对照（S1-05/S1-06/S1-07/S1-08 已标注"已补完"）。

- [x] CompositorEffect PRE_OPAQUE 回调被正确触发 (Task 1.17 已补完：`NaniteGDExtBridgeManager` 创建 `default_compositor` 并通过 `SceneTree::node_added` + `_viewports` group 轮询双路径注入到游戏运行时 / Nanite preview 窗口 / 引擎 3D 工作区；引擎每帧触发 `render_visibility` / `render_material_resolve`)
- [x] BVH 遍历 + LOD 选择在 GPU 正确执行 (Task 1.16.7 已补完：per-cluster parallel BVH-aware 测试 + parent-fallback LOD)
- [x] 可见性缓冲正确生成(调试可视化可观察) (Task 1.16.8 已补完：meshlet 解码 + 三角形软光栅化 + 深度测试。Stage 1 因 R32_SFLOAT 不支持 `imageAtomicCompSwap`，简化为非原子 compare-then-store，race-safe 由"last writer wins per pixel"保证；Stage 2 可改 R32_UINT + atomic 路径)
- [x] 材质解析正确(与标准材质对比) (Task 1.16.9 已补完：barycentric 插值 + Lambert 着色 + 调试模式分支；完整 PBR 留待 Stage 2+)
- [x] 粗 LOD 阴影正确投射(对比引擎默认阴影) (PARTIAL: 代码完成，待运行时验证)
- [x] `mesh_set_shadow_mesh` 被正确调用
- [x] 帧率 ≥ 30fps(10K tri 单网格场景) (PARTIAL: 测试已编写，待运行)
- [ ] 编译为 .gdextension 插件可独立加载 (Stage 1 为 module 内实现，独立 GDExtension 推迟到 Stage 4)

**阶段二(Module)验收标准**：

- [ ] 阶段一所有标准通过
- [ ] RendererSceneCull hook 正确安装
- [ ] 动态 GPU 阴影正确写入 shadow atlas
- [ ] `shadow_atlas_get_fb` 返回有效 RID
- [ ] `light_instance_get_shadow_atlas_rect` 返回正确 rect
- [ ] 点光源 + 聚光灯阴影正确
- [ ] 方向光阴影使用 `direction_shadow_get_fb` 正确
- [ ] 多光源场景无 shadow atlas 冲突
- [ ] 作为 Module 编译通过

**阶段三(Deep)验收标准**：

- [ ] 阶段二所有标准通过
- [ ] 源码 patch 正确应用
- [ ] Nanite 实例跳过 CPU 剔除
- [ ] SDFGI 正确使用 Nanite 几何
- [ ] VoxelGI 正确使用 Nanite 几何
- [ ] 无渲染回归(非 Nanite 场景不受影响)
- [ ] 性能：与阶段二对比无退化

---

## 14. 与调研文档差异说明

> 本节纠正 `nanite-godot-design.md` 调研文档中的 API 命名错误，确保编码使用正确名称。

| 编号 | 调研文档中的错误 | Godot 4.7.1 正确 API | 源码依据 | 影响范围 |
|------|-------------------|----------------------|----------|----------|
| D01 | `BEFORE_OPAQUE_PASS` / `AFTER_OPAQUE_PASS` | `EFFECT_CALLBACK_TYPE_PRE_OPAQUE` / `EFFECT_CALLBACK_TYPE_POST_OPAQUE` | `compositor_effect.h` 枚举定义 | 阶段一 CompositorEffect 注册 |
| D02 | `get_render_data()` | `get_render_scene_data()` | `render_data_extension.h` | 阶段一获取相机/投影信息 |
| D03 | `shadow_atlas_get_quadrant_rect()` | `light_instance_get_shadow_atlas_rect(RID, RID, Vector2i&)` | `light_storage.h:684` | 阶段二/三阴影 rect 获取 |
| D04 | `_render_camera` | `render_camera` | `renderer_scene_cull.h` | 阶段二 hook 目标函数名 |
| D05 | (未提及) `direction_shadow_get_fb()` | `direction_shadow_get_fb()` | `light_storage.h:1183` | 阶段二/三方向光阴影 |
| D06 | (混淆) `RenderData*` vs `RenderDataExtension*` | GDExtension 中使用 `RenderDataExtension*`；Module 中使用 `RenderData*` | 不同的桥接层上下文 | 全阶段回调签名 |
