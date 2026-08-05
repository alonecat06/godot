# Stage 4 GDExtension 插件化可行性调研与设计草案

> 生成时间:2026-08-04
> 调研范围:`nanite/` 模块当前依赖、godot-cpp 4.x 能力边界、Stage 0/2/3 对 Stage 4 的影响
> 对照基准:
> - 总体设计 [`nanite_doc/nanite-overall-design.md`](../../nanite-overall-design.md)
> - 任务拆分 [`nanite_doc/nanite-implementation-tasks.md`](../../nanite-implementation-tasks.md)
> - Stage 1 Spec [`spec.md`](spec.md) / [`tasks.md`](tasks.md) / [`checklist.md`](checklist.md)
> - Stage 1 代码现状 [`code-status-analysis.md`](code-status-analysis.md)
>
> **核心问题**:Stage 1 当前作为 in-engine module 编译,大量引用引擎内部头文件。若推迟到 Stage 4 再做 GDExtension 插件化,是否会"积重难返"?Stage 1 或 Stage 4 哪个节点输出独立分发的 GDExtension 插件更合理?

---

## 一、核心结论速览

**Stage 4 实现 GDExtension 插件化在技术上完全可行,且不会"积重难返"。** 关键依据:

1. **godot-cpp 4.x 的能力远超此前评估** —— `CompositorEffect` 子类化 + `GDVIRTUAL2(_render_callback)` 路径在 GDExtension 中原生支持,此前担心的"私有回调重绑"阻塞实际不存在
2. **Stage 0 与 Stage 1 的依赖绝大多数都在 godot-cpp 中有对应实现** —— 连 `RID_PtrOwner`、`callable_mp`、`memnew` 都有同名等价物
3. **Stage 2/3 的 Module/Deep 桥接是独立编译路径**,不会污染 GDExtension 化的代码,且与 GDExtension 互斥(无法在 GDExtension 中实现)
4. **`INaniteBridge` 抽象接口已为核心与桥接解耦铺好路**,Stage 4 能顺利抽取 GDExt 桥接 + 核心

**推荐路径**:Stage 1 维持 in-engine,专注可视化目标;Stage 2/3 完成三桥接切换机制验证;Stage 4 做一次性 GDExtension 化迁移,预期 4-7 天,核心业务逻辑零改动。

---

## 二、godot-cpp 4.x 能力边界调研

### 2.1 渲染相关类能力清单

| 类 | 状态 | 说明 |
|---|---|---|
| `RenderingDevice` | ✅ 可用 | `texture_create` / `compute_pipeline_create` / `buffer_update` / `compute_list_begin/end` / `uniform_set_create` / `shader_create_from_spirv` 全部通过 `Ref<RDTextureFormat>` 等包装类暴露 |
| `RenderingServer` | ✅ 可用 | `compositor_create` / `compositor_effect_create` / `compositor_effect_set_callback` / `mesh_set_shadow_mesh` 全部 ClassDB 绑定 |
| `CompositorEffect` | ✅ 可用 | `Resource` 子类,可被 GDExtension 子类化;`GDVIRTUAL2(_render_callback, int, RenderData*)` 虚方法可在 godot-cpp 中 override |
| `Compositor` | ✅ 可用 | `set_compositor_effects` / `get_compositor_effects` / `get_rid` 全部绑定 |
| `RenderData` | ✅ 可用 | 抽象基类已暴露;`get_render_scene_buffers()` / `get_render_scene_data()` 绑定 |
| `RenderSceneData` | ✅ 可用 | `get_cam_transform()` / `get_cam_projection()` / `get_view_count()` / `get_view_projection()` 全部绑定 |
| `RenderSceneBuffers` | ⚠️ 部分可用 | 基类只绑定 `configure(config)`;需 `cast_to<RenderSceneBuffersRD>` 访问纹理 |
| `RenderSceneBuffersRD` | ✅ 可用 | `get_color_layer` / `get_depth_layer` / `get_color_texture` / `get_depth_texture` 全部绑定,**关键纹理访问途径** |
| `RenderSceneBuffersConfiguration` | ✅ 可用 | `get_internal_size` / `get_target_size` / `get_view_count` 绑定 |

### 2.2 场景/节点/编辑器类

| 类 | 状态 | 说明 |
|---|---|---|
| `MeshInstance3D` | ⚠️ 部分可用 | `set_mesh` / `get_mesh` / `set_cast_shadows_setting` 可用;`get_mesh_rid` 不存在,绕过:`get_mesh()->get_rid()` 或 `RenderingServer::instance_get_base()` |
| `SceneTree` | ✅ 可用 | `node_added` / `node_removed` / `tree_changed` 信号已绑定,GDExtension 可 `connect` |
| `Viewport` | ✅ 可用 | `set_compositor` / `find_world_3d` / `get_world_3d` 绑定 |
| `Window` / `SubViewport` | ✅ 可用 | 标准暴露类 |
| `EditorPlugin` / `EditorInspectorPlugin` | ✅ 可用 | 需在 `MODULE_INITIALIZATION_LEVEL_EDITOR` 级别注册 |

### 2.3 CompositorEffect 在 GDExtension 中的工作路径(确认可行)

**结论:`CompositorEffect` 可在 GDExtension 中子类化并工作,无阻塞性问题。**

工作链路:
```
引擎基类 CompositorEffect 构造 → 自动注册 callable_mp 到 RS
→ 引擎渲染时调用 _call_render_callback(type, render_data)
→ GDVIRTUAL_CALL(_render_callback, ...)
→ 分派到 GDExtension 子类的 override (✅ 这是 GDVIRTUAL 的设计用途)
```

具体步骤:
1. **子类化**:`class MyEffect : public CompositorEffect { GDCLASS(MyEffect, CompositorEffect); ... }`
2. **Override `_render_callback`**:声明 `GDVIRTUAL2(_render_callback, int, RenderData*)` 并实现 `void _render_callback(int p_type, RenderData *p_data) override`
3. **回调接线由引擎完成**:引擎基类构造函数自动调用 `rs->compositor_effect_set_callback(rid, type, callable_mp(this, &CompositorEffect::_call_render_callback))`,GDExtension 子类**无需**手动接线
4. **访问纹理**:`p_data->get_render_scene_buffers()` → `Object::cast_to<RenderSceneBuffersRD>(buffers.ptr())` → `get_color_layer(0)` / `get_depth_layer(0)`
5. **执行 compute**:`RenderingServer::get_singleton()->get_rendering_device()` 获取 RD,用 `compute_list_begin` / `compute_pipeline_create` / `buffer_update` 等

**关键反转**:当前 [nanite/bridge/nanite_gdext_bridge.h:44-59](file:///d:/Code/04_Engine/godot/nanite/bridge/nanite_gdext_bridge.h#L44-L59) 注释记录的"GDVIRTUAL2 不会派发到 in-engine C++ 子类,必须通过 `compositor_effect_set_callback` 重绑" —— **这是 in-engine 子类的问题**,而 GDExtension 子类**正是 GDVIRTUAL2 的设计目标**。在 GDExtension 化时,当前的重绑 hack **反而需要移除**。

### 2.4 引擎内部 API 的 godot-cpp 等价物

| 引擎内部 API | godot-cpp 等价 | 状态 |
|---|---|---|
| `memnew` / `memdelete` | `godot_cpp/core/memory.hpp` | ✅ 同名同语义 |
| `callable_mp` | `godot_cpp/variant/callable_method_pointer.hpp` | ✅ 同名 |
| `callable_custom` | `godot_cpp/variant/callable_custom.hpp` | ✅ 可用 |
| `RID_PtrOwner<T>` | `godot_cpp/templates/rid_owner.hpp` | ✅ 完全一致 |
| `HashMap` / `LocalVector` / `Vector` / `List` / `HashSet` / `RBMap` / `RBSet` | `godot_cpp/templates/` | ✅ 齐全 |
| `ShaderRD` / `GLSL_HEADER` | ❌ 不在 godot-cpp | ⚠️ 需改用 `RDShaderFile` 运行时加载 `.glsl` + `RenderingDevice::shader_create_from_spirv`,或预编译 SPIR-V 内嵌 |
| `GLOBAL_DEF` 宏 | ❌ 不在 godot-cpp | ⚠️ 用 `ProjectSettings::has_setting` + `set_setting` 手写"无则设默认"逻辑 |
| `Engine::add_singleton` | ❌ 不在 godot-cpp | ⚠️ GDExtension 不能注册引擎级单例;用 `GDREGISTER_CLASS` + Autoload 配置替代 |
| `ClassDB::register_class` | `GDREGISTER_CLASS` | ✅ 等价 |

### 2.5 GDExtension 限制与支持

| 能力 | 状态 | 说明 |
|---|---|---|
| 多初始化级别 | ✅ 完全支持 | `MODULE_INITIALIZATION_LEVEL_CORE` / `SERVERS` / `SCENE` / `EDITOR` |
| SCENE 级别创建 Compositor | ✅ 可以 | `Compositor` / `CompositorEffect` 是 `Resource` 子类,SCENE 级注册;RenderingServer 在 SERVERS 级已就绪 |
| hook `SceneTree::node_added` 信号 | ✅ 可以 | `Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop())` → `connect("node_added", callable)` |
| Godot 4.5+ 新增能力 | ✅ | `register_startup_callback` / `register_frame_callback` / `register_shutdown_callback`(main_loop 回调,无需手动拿 SceneTree) |

---

## 三、当前 nanite/ 模块依赖分析

### 3.1 头文件依赖分类

基于对 `nanite/` 目录下所有 `.h` / `.cpp` 文件的搜索,主要依赖分类如下:

**渲染核心头文件**(全部在 godot-cpp 有等价):
- `servers/rendering/rendering_device.h` → `<godot_cpp/classes/rendering_device.hpp>`
- `servers/rendering_server.h` → `<godot_cpp/classes/rendering_server.hpp>`
- `scene/resources/compositor.h` → `<godot_cpp/classes/compositor.hpp>` + `<godot_cpp/classes/compositor_effect.hpp>`

**场景节点头文件**(全部在 godot-cpp 有等价):
- `scene/3d/mesh_instance_3d.h` → `<godot_cpp/classes/mesh_instance_3d.hpp>`
- `scene/main/scene_tree.h` → `<godot_cpp/classes/scene_tree.hpp>`
- `scene/main/window.h` → `<godot_cpp/classes/window.hpp>`

**核心容器头文件**(全部在 godot-cpp 有等价):
- `core/templates/rid.h` → `<godot_cpp/variant/rid.hpp>`
- `core/templates/hash_map.h` → `<godot_cpp/templates/hash_map.hpp>`
- `core/templates/local_vector.h` → `<godot_cpp/templates/local_vector.hpp>`
- `core/templates/vector.h` → `<godot_cpp/templates/vector.hpp>`

**核心对象头文件**(全部在 godot-cpp 有等价):
- `core/object/object.h` → `<godot_cpp/classes/object.hpp>`
- `core/object/class_db.h` → `<godot_cpp/core/class_db.hpp>`
- `core/config/engine.h` → `<godot_cpp/classes/engine.hpp>`
- `core/config/project_settings.h` → `<godot_cpp/classes/project_settings.hpp>`
- `core/io/file_access.h` → `<godot_cpp/classes/file_access.hpp>`
- `core/variant/variant.h` → `<godot_cpp/variant/variant.hpp>`
- `core/variant/typed_array.h` → `<godot_cpp/variant/typed_array.hpp>`
- `core/variant/callable.h` → `<godot_cpp/variant/callable.hpp>`
- `core/variant/callable_method_pointer.h` → `<godot_cpp/variant/callable_method_pointer.hpp>`
- `core/error/error_macros.h` → `<godot_cpp/core/error_macros.hpp>`

### 3.2 私有 API 依赖分析

**CompositorEffect 回调重绑**(关键 hack):
- 位置:[nanite/bridge/nanite_gdext_bridge.cpp](file:///d:/Code/04_Engine/godot/nanite/bridge/nanite_gdext_bridge.cpp) 构造函数
- 当前做法:通过 `RenderingServer::compositor_effect_set_callback` 重新绑定 `callable_mp(this, &NaniteGDExtBridge::_render_callback)` 到 effect 的回调槽
- 原因:in-engine C++ 子类没有 script instance,`GDVIRTUAL2` 分派不会到达
- **GDExtension 化后**:此 hack 需**移除**,改为纯 `GDVIRTUAL2(_render_callback, int, RenderData*) override`,因为 GDExtension 子类正是 GDVIRTUAL2 的设计目标

**ShaderRD / GLSL_HEADER 编译路径**:
- 位置:[nanite/SCsub:30-34](file:///d:/Code/04_Engine/godot/nanite/SCsub#L30-L34) `env_nanite.GLSL_HEADER("shaders/nanite_hzb_downsample.glsl")`
- 当前做法:引擎编译期将 `.glsl` 转为头文件
- **GDExtension 化后**:改用 `RDShaderFile` 运行时加载 `.glsl`,或预编译 SPIR-V 字节流嵌入二进制

**GLOBAL_DEF 宏**:
- 位置:[nanite/core/nanite_server.cpp:85](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.cpp#L85) `GLOBAL_DEF("nanite/bridge/active", "gdext")`
- **GDExtension 化后**:改为 `ProjectSettings::has_setting` + `set_setting` 手写

**Engine::add_singleton**:
- 位置:[nanite/register_types.cpp](file:///d:/Code/04_Engine/godot/nanite/register_types.cpp) `Engine::get_singleton()->add_singleton(Engine::Singleton("NaniteServer", ns))`
- **GDExtension 化后**:改为 `GDREGISTER_CLASS` + Autoload 配置

### 3.3 依赖改造难度评估

| 依赖类别 | 出现频率 | GDExtension 化难度 | 说明 |
|---|---|---|---|
| 引擎内部头文件 | 约 30-40 处 | 🟢 易 | 直接替换为 `<godot_cpp/...>` |
| `memnew` / `memdelete` | 全模块使用 | 🟢 易 | godot-cpp 同名提供 |
| `callable_mp` | bridge/ 目录 | 🟢 易 | godot-cpp 同名提供 |
| `RID_PtrOwner` / `HashMap` / `LocalVector` | core/ 目录 | 🟢 易 | godot-cpp 同名提供 |
| `GDCLASS` / `ClassDB::register_class` | 所有暴露类 | 🟢 易 | 改为 `GDREGISTER_CLASS` |
| `ShaderRD` / `GLSL_HEADER` | SCsub + shaders/ | 🟡 中 | 需改为 `RDShaderFile` 运行时编译 |
| `GLOBAL_DEF` | core/nanite_server.cpp | 🟢 易 | 改为 `ProjectSettings` 手写 |
| `Engine::add_singleton` | register_types.cpp | 🟢 易 | 改为 Autoload |
| `CompositorEffect` 重绑 hack | bridge/nanite_gdext_bridge.cpp | 🟡 中 | 需移除 hack,改用纯 GDVIRTUAL2 override |
| `register_types.cpp` 入口签名 | register_types.cpp | 🟡 中 | 拆分为 module 版 + gdext 版 |

---

## 四、Stage 0 对 Stage 4 的影响分析

### 4.1 Stage 0 不是 GDExtension 化的阻塞

Stage 0 与 Stage 1 的契约是 **`.nanite` 二进制文件格式 v4**。解耦路径清晰:

| Stage 0 类 | 是否随 Stage 4 进入 GDExtension | 原因 |
|---|---|---|
| `NaniteMeshResource` | ✅ 必须 | 运行时反序列化需要 |
| `BuilderConfig` | ✅ 必须 | Inspector 编辑构建参数 |
| `NaniteBuilder` / `NaniteCluster` / `NaniteBVH` / `PagePacker` | ❌ 不需要 | 离线构建工具,保留引擎内或单独分发 |
| `NaniteMeshEditor` / `NaniteEditorPlugin` | ⚠️ 可选 | 作为编辑器 GDExtension 独立分发 |

### 4.2 关键解耦点

- **Stage 0 的 meshoptimizer 依赖** 不污染 GDExtension —— 构建工具留在引擎内,GDExtension 只需 `NaniteMeshResource` 的反序列化逻辑
- **Stage 0 的编辑器 UI** 可作为独立的 EditorPlugin GDExtension 分发,与运行时 GDExtension 解耦
- **Stage 1 只需 `NaniteMeshResource` 的反序列化逻辑**,这部分代码量很小

### 4.3 Stage 0 间接影响点

1. **`NaniteMeshResource` 序列化格式与引擎内部 API 的耦合**:如果序列化使用了 `FileAccess`、`FileAccessMemory` 等内部 API,需替换为 godot-cpp 的 `FileAccess`(已暴露)
2. **`NaniteMeshEditor` 作为 GDExtension 分发的额外难点**:编辑器类需要 `EditorPlugin`、`EditorInspectorPlugin` 等 godot-cpp 支持的类,但 SubViewport + 相机交互逻辑(滚轮缩放、中键平移、F 聚焦、Cluster 射线选择)中的 `RenderingServer` 直调需替换为 godot-cpp 等价调用

---

## 五、Stage 2/3 对 Stage 4 的影响分析(关键判断)

### 5.1 三桥接是独立编译路径

[nanite-overall-design.md:171, 264](file:///d:/Code/04_Engine/godot/nanite_doc/nanite-overall-design.md#L171) 明确:

```
nanite/
├── nanite_bridge_deep/       # 阶段三桥接 (源码 Patch)
│   └── patches/
└── register_types.h/.cpp
```

三桥接各自有独立 Manager + 独立目录,**通过 `INaniteBridge` 抽象接口与核心解耦**。

### 5.2 Stage 2/3 的依赖不会污染 Stage 4

| 桥接 | 接入方式 | 是否影响 Stage 4 |
|---|---|---|
| Stage 2 Module | `RendererSceneCull::render_camera` hook | ❌ **本身就不能作为 GDExtension 分发** —— `RendererSceneCull` 是引擎私有类,GDExtension 无法访问。Stage 2 只能以 in-engine module 形式存在 |
| Stage 3 Deep | 源码 Patch 直接修改 `_render_scene` | ❌ **本质就是修改引擎源码**,与 GDExtension 互斥 |

**关键判断**:Stage 2/3 与 Stage 4 **是互斥的并行路径,不是递进依赖**。

- Stage 2/3 的代码在 `nanite/bridge/nanite_module_bridge_*` 和 `nanite/bridge/nanite_deep_bridge_*` 目录下,**永远不会进入 GDExtension 分发包**
- Stage 4 的 GDExtension 分发包**只包含**:
  - `nanite/core/` (NaniteServer + NaniteMeshResource + NaniteDebug)
  - `nanite/gpu/` (GPUPipeline + HZB + MeshData)
  - `nanite/scene/` (NaniteMeshInstance3D)
  - `nanite/bridge/nanite_gdext_bridge*` (仅 GDExt 桥接)
  - `nanite/shaders/` (改用 `RDShaderFile` 运行时编译)

### 5.3 INaniteBridge 抽象接口是关键保险

[nanite/core/nanite_bridge.h](file:///d:/Code/04_Engine/godot/nanite/core/nanite_bridge.h) 的抽象接口设计,**已经为核心与桥接解耦铺好路**:

```cpp
class INaniteBridge {
public:
    virtual void install(NaniteServer *p_server) = 0;
    virtual void on_pre_opaque_pass(const RenderData *) = 0;
    // ...
};
```

[nanite/core/nanite_server.cpp:50-53](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.cpp#L50-L53) 注释:"core must NOT include nanite/bridge/nanite_gdext_bridge.h"。**核心零桥接具体类型依赖已达成**,这正是 Stage 4 能独立抽取 GDExt 桥接 + 核心的根本前提。

---

## 六、"积重难返"风险评估

| 风险源 | 实际风险 | 依据 |
|---|---|---|
| 引擎内部头文件依赖积累 | 🟢 低 | 绝大多数在 godot-cpp 有等价(见第二节) |
| Stage 2/3 深化引擎依赖 | 🟢 无 | 互斥路径,代码物理隔离 |
| CompositorEffect 私有 API 重绑 hack | 🟡 中 | 当前 in-engine hack 在 GDExtension 化时**反而需移除**(因为 GDVIRTUAL2 在 GDExtension 中直接工作) |
| GLSL 编译路径 | 🟡 中 | 需从 `env_nanite.GLSL_HEADER` 改为 `RDShaderFile` 运行时加载,但这是 Stage 4 内的局部改造,不涉及业务逻辑 |
| `Engine::add_singleton` 不可用 | 🟢 低 | 用 Autoload 替代,这是 GDExtension 标准实践 |
| Stage 0 的 `NaniteMeshEditor` 相机交互直调 RS | 🟢 低 | RS 方法在 godot-cpp 已绑定,只是调用方式略变 |

**结论:不存在"积重难返"风险。** 真正需要改造的只有三处薄层:

1. **入口点** `register_types.cpp` → GDExtension 入口签名 + `GDREGISTER_CLASS`
2. **Shader 编译** `SCsub GLSL_HEADER` → `RDShaderFile` 运行时编译
3. **CompositorEffect 回调** 移除当前 in-engine 的 `compositor_effect_set_callback` 重绑 hack,改用纯 `GDVIRTUAL2` override

---

## 七、Stage 4 设计草案

### 7.1 核心策略

**维持当前 in-engine 编译模式不变,在 Stage 4 做一次性的 GDExtension 化迁移。**

- Stage 1-3 期间:维持 [nanite/SCsub](file:///d:/Code/04_Engine/godot/nanite/SCsub) 的 `GODOT_MODULE` 宏定义
- 继续用 `ShaderRD` / `GLSL_HEADER` / `GLOBAL_DEF` / `Engine::add_singleton`
- Stage 2 的 Module 桥接按原计划开发
- Stage 3 的 Deep 桥接按原计划开发
- Stage 4 做一次性迁移

### 7.2 Stage 4 GDExtension 分发包结构

```
nanite_gdext/                           # 独立 GDExtension 插件根目录
├── nanite.gdextension                  # GDExtension 配置文件
├── bin/
│   ├── libnanite.windows.release.x86_64.dll
│   ├── libnanite.linux.release.x86_64.so
│   └── libnanite.macos.release.framework
├── shaders/                            # GLSL 源码(运行时编译)
│   ├── nanite_cull.glsl
│   ├── nanite_rasterize.glsl
│   ├── nanite_hzb_downsample.glsl
│   └── nanite_material_resolve.glsl
├── src/
│   ├── core/                           # 从 nanite/core/ 迁移
│   │   ├── nanite_server.h/.cpp        # 改为 Autoload
│   │   ├── nanite_resource.h/.cpp     # 反序列化逻辑
│   │   ├── nanite_debug.h/.cpp
│   │   ├── nanite_bridge.h             # INaniteBridge 抽象接口(零改动)
│   │   └── nanite_page_cache.h/.cpp    # 占位实现
│   ├── gpu/                            # 从 nanite/gpu/ 迁移
│   │   ├── nanite_gpu_pipeline.h/.cpp  # 零业务逻辑改动
│   │   ├── nanite_hzb.h/.cpp
│   │   └── nanite_mesh_data.h/.cpp
│   ├── scene/                          # 从 nanite/scene/ 迁移
│   │   └── nanite_mesh_instance_3d.h/.cpp
│   ├── bridge/                         # 仅 GDExt 桥接
│   │   ├── nanite_gdext_bridge.h/.cpp  # 移除重绑 hack
│   │   └── nanite_gdext_bridge_manager.h/.cpp
│   └── register_types.cpp              # GDExtension 入口
└── addon/                              # 可选:编辑器集成
    └── nanite_editor/                  # 独立的编辑器 GDExtension
        ├── nanite_mesh_editor.h/.cpp
        └── nanite_editor_plugin.h/.cpp
```

**关键点**:Stage 2/3 的 `nanite_module_bridge_*` 和 `nanite_deep_bridge_*` 目录**完全不进入** GDExtension 分发包。

### 7.3 实施步骤

#### 步骤 1:薄层条件编译(2-3 天)

在所有源文件头加 `#ifdef GODOT_MODULE ... #else ... #endif`,替换约 30-40 处引擎内部头文件为 `<godot_cpp/...>`。

示例:
```cpp
// nanite/core/nanite_server.h
#ifdef GODOT_MODULE
#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/templates/rid.h"
#else
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/rid.hpp>
#endif
```

#### 步骤 2:Shader 路径迁移(1-2 天)

`nanite/shaders/*.glsl` 改为运行时通过 `RDShaderFile` 加载:

```cpp
// 替换 ShaderRD 编译期路径
Ref<RDShaderFile> shader_file;
shader_file.instantiate();
shader_file->set_source_code(SourceShader::SHADER_VERSION_COMPUTE, load_glsl("nanite_cull.glsl"));
RID shader = rd->shader_create_from_spirv(shader_file->get_spirv());
```

或预编译为 SPIR-V 字节流嵌入二进制:
```cpp
static const uint32_t cull_shader_spirv[] = { /* 预编译字节流 */ };
RID shader = rd->shader_create_from_spirv(cull_shader_spirv, sizeof(cull_shader_spirv));
```

#### 步骤 3:移除 CompositorEffect 重绑 hack(0.5 天)

[nanite/bridge/nanite_gdext_bridge.cpp](file:///d:/Code/04_Engine/godot/nanite/bridge/nanite_gdext_bridge.cpp) 构造函数中删除 `compositor_effect_set_callback` 调用,改为纯 `GDVIRTUAL2(_render_callback, int, RenderData*) override`:

```cpp
// GDExtension 版本
class NaniteGDExtBridge : public CompositorEffect, public INaniteBridge {
    GDCLASS(NaniteGDExtBridge, CompositorEffect);
    
    // 纯 GDVIRTUAL2 override,无需重绑
    GDVIRTUAL2(_render_callback, int, RenderData*);
    void _render_callback(int p_type, RenderData *p_data) override {
        // 业务逻辑零改动
    }
};
```

#### 步骤 4:单例机制改造(0.5 天)

`NaniteServer` 改为 Autoload + `GDREGISTER_CLASS`:
- 移除 `Engine::get_singleton()->add_singleton(...)` 调用
- 在 `project.godot` 配置 Autoload:
```
[autoload]
NaniteServer="*res://addons/nanite_gdext/nanite_server.gd"
```
- `NaniteServer::get_singleton()` 改为静态指针 + 自动注册

#### 步骤 5:ProjectSetting 注册(0.5 天)

`GLOBAL_DEF` 改为 `ProjectSettings::has_setting` + `set_setting` 手写:

```cpp
// 替换 GLOBAL_DEF("nanite/bridge/active", "gdext")
if (!ProjectSettings::get_singleton()->has_setting("nanite/bridge/active")) {
    ProjectSettings::get_singleton()->set_setting("nanite/bridge/active", "gdext");
}
```

#### 步骤 6:入口点改造(0.5 天)

`register_types.cpp` 拆分为 module 版 + gdext 版:

```cpp
// gdext/register_types.cpp (GDExtension 入口)
extern "C" GDExtensionBool nanite_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization) {
    
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    
    init_obj.register_initializer(initialize_nanite_module);
    init_obj.register_terminator(uninitialize_nanite_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SERVERS);
    
    return init_obj.init();
}

void initialize_nanite_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
        GDREGISTER_CLASS(NaniteServer);
        GDREGISTER_CLASS(NaniteMeshResource);
        // ...
    }
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        GDREGISTER_CLASS(NaniteMeshInstance3D);
        GDREGISTER_CLASS(NaniteGDExtBridge);
        // 创建 NaniteGDExtBridgeManager
    }
}
```

### 7.4 时间预估

| 步骤 | 预估时间 | 改动范围 |
|---|---|---|
| 薄层条件编译 | 2-3 天 | 所有源文件头 |
| Shader 路径迁移 | 1-2 天 | SCsub + shaders/ + gpu/ |
| 移除重绑 hack | 0.5 天 | bridge/nanite_gdext_bridge.cpp |
| 单例机制改造 | 0.5 天 | core/nanite_server.h/.cpp + project.godot |
| ProjectSetting 注册 | 0.5 天 | core/nanite_server.cpp |
| 入口点改造 | 0.5 天 | register_types.cpp |
| **总计** | **5-8 天** | **核心业务逻辑零改动** |

---

## 八、Stage 1 与 Stage 4 输出独立 GDExtension 的对比

| 维度 | Stage 1 直接 GDExtension 化 | Stage 4 GDExtension 化(推荐) |
|---|---|---|
| 技术可行性 | ✅ 可行 | ✅ 可行 |
| 改造成本 | 中(需立即替换 ShaderRD / GLOBAL_DEF / 入口点) | 中(同左,但延后到 Stage 4) |
| 风险 | 中(Stage 2/3 仍需 in-engine,双路径维护成本) | 低(Stage 2/3 完成后再迁移,无双路径) |
| 阻塞 Stage 1 可视化目标 | 🟠 是(分散精力,推迟 color_buffer 合成) | 🟢 否(Stage 1 专注可视化,不分散) |
| 验证三桥接切换机制 | 🟠 难(GDExtension 化后 Stage 2/3 无法测试) | 🟢 易(Stage 2/3 完成后验证切换机制) |
| 对 INaniteBridge 抽象的纪律要求 | 高(需立即严格遵守) | 中(可渐进完善) |

**关键判断**:Stage 1 直接 GDExtension 化**会阻塞** Stage 1 的核心可视化目标(color_buffer 合成),且 Stage 2/3 的 Module/Deep 桥接**无法**在 GDExtension 中验证(因为它们依赖引擎私有 API)。Stage 4 化则是**所有桥接完成后的自然收尾**。

---

## 九、最终建议

### 9.1 阶段性建议

1. **Stage 1 当前**:专注补完 color_buffer → 引擎 color target 合成路径,达成"将模型渲染到场景中"的可视化目标。**不做 GDExtension 化**。

2. **Stage 2/3 期间**:按原计划开发 Module/Deep 桥接,验证三桥接切换机制(`nanite/bridge/active` ProjectSetting)。**保持 in-engine 编译**。

3. **Stage 4**:做一次性 GDExtension 化迁移,按第七节路径执行。**预期 5-8 天**,核心业务逻辑零改动。

4. **保持 INaniteBridge 抽象接口的纪律**:这是 Stage 4 能顺利抽取 GDExt 桥接 + 核心的根本保险。当前 [nanite/core/nanite_server.cpp:50-53](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.cpp#L50-L53) 已严格遵守"core 不 include 具体 bridge 头文件",**继续保持**。

### 9.2 纪律性要求

为确保 Stage 4 迁移顺利,Stage 1-3 期间需遵守以下纪律:

1. **核心层(`nanite/core/`)零桥接具体类型依赖** —— 只通过 `INaniteBridge` 抽象接口访问桥接
2. **桥接层(`nanite/bridge/nanite_gdext_bridge*`)与 Module/Deep 桥接物理隔离** —— 不共享源文件,不互相 include
3. **Shader 代码(`nanite/shaders/*.glsl`)保持纯 GLSL** —— 不依赖引擎 `ShaderRD` 特有宏
4. **业务逻辑与引擎内部 API 分离** —— Cull/Rasterize/HZB/Material Resolve 的算法实现不直接调用引擎私有 API,通过 `RenderingDevice` / `RenderingServer` 的公开 API

### 9.3 最终结论

**Stage 4 实现 GDExtension 插件化完全可行,不会积重难返。** 当前架构(`INaniteBridge` 抽象 + Manager 独立目录)已为 Stage 4 铺好路,关键依赖在 godot-cpp 都有等价物。建议维持原计划:Stage 1 专注可视化,Stage 4 做 GDExtension 化收尾。

---

## 十、附录:关键源文件参考

### 10.1 引擎源码参考(均在 `godotengine/godot` 仓库)

- `scene/resources/compositor.h` — `CompositorEffect` / `Compositor` 类定义
- `servers/rendering/storage/render_data.h` / `render_scene_data.h` / `render_scene_buffers.h` — 抽象基类
- `servers/rendering/storage/render_data_extension.h` — Extension 子类
- `servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h` / `.cpp` — RD 实现 + "exposed to extensions" 方法
- `servers/register_server_types.cpp` — `GDREGISTER_CLASS` 注册清单
- `servers/rendering/rendering_server.cpp` — `ClassDB::bind_method` 绑定
- `servers/rendering/rendering_device.h` — `RenderingDevice` 方法签名

### 10.2 godot-cpp 仓库参考

- `include/godot_cpp/core/memory.hpp` — `memnew` / `memdelete`
- `include/godot_cpp/variant/callable_method_pointer.hpp` — `callable_mp`
- `include/godot_cpp/templates/rid_owner.hpp` — `RID_PtrOwner`
- `include/godot_cpp/templates/hash_map.hpp` / `local_vector.hpp` / `vector.hpp` — 容器
- `include/godot_cpp/godot.hpp` — `ModuleInitializationLevel`
- `gdextension/extension_api-{4-3,4-4,4-5,4-6}.json` — 各版本暴露 API 清单

### 10.3 当前 nanite 模块关键文件

- [nanite/SCsub](file:///d:/Code/04_Engine/godot/nanite/SCsub) — 编译脚本,`GODOT_MODULE` 宏定义
- [nanite/register_types.cpp](file:///d:/Code/04_Engine/godot/nanite/register_types.cpp) — 模块注册入口
- [nanite/core/nanite_server.h](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.h) — `NaniteServer` 核心类
- [nanite/core/nanite_bridge.h](file:///d:/Code/04_Engine/godot/nanite/core/nanite_bridge.h) — `INaniteBridge` 抽象接口
- [nanite/bridge/nanite_gdext_bridge.h](file:///d:/Code/04_Engine/godot/nanite/bridge/nanite_gdext_bridge.h) — `CompositorEffect` 子类(含重绑 hack 注释)
- [nanite/bridge/nanite_gdext_bridge_manager.h](file:///d:/Code/04_Engine/godot/nanite/bridge/nanite_gdext_bridge_manager.h) — 自动挂接 Manager
- [nanite/gpu/nanite_gpu_pipeline.h](file:///d:/Code/04_Engine/godot/nanite/gpu/nanite_gpu_pipeline.h) — GPU 管线管理
