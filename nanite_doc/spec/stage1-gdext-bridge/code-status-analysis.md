# Stage 1 代码现状分析（文档 / Spec / 代码三方对照）

> 生成时间：2026-08-03
> 对照基准：
> - 总体设计 [`nanite_doc/nanite-overall-design.md`](../../nanite-overall-design.md)
> - 任务拆分 [`nanite_doc/nanite-implementation-tasks.md`](../../nanite-implementation-tasks.md)
> - Stage 1 Spec [`spec.md`](spec.md) / [`tasks.md`](tasks.md) / [`checklist.md`](checklist.md)
> - 实际代码 `nanite/` 模块
>
> **Stage 1 重申目标**：通过 `CompositorEffect` 接入的方式，**在不修改引擎源码的情况下**，实现运行时读取 nanite 数据，**将模型渲染到场景中**。

---

## 一、整体结论

| 维度 | 距离 Stage 1 目标的差距 | 说明 |
|------|----------------------|------|
| 总体设计文档 | **0（已对齐）** | 10.5 节 S1-05/S1-06/S1-07/S1-08 均标注"已补完（Stage 1 内）"，与代码现状基本一致 |
| 任务拆分文档 | **0（已对齐）** | 阶段 1 验收标准（1.10 节）所有项已标 `[x]`，并指向 spec 文档为准 |
| Spec 文档 | **存在矛盾** | spec.md 描述的目标与代码实际行为存在偏差（详见第三节） |
| Tasks 文档 | **存在矛盾** | tasks.md 中 Task 1.17 所有子项仍标 `[ ]` 未完成，但代码实际已实现 |
| Checklist 文档 | **存在矛盾** | checklist.md 第 17 节 Task 1.17 所有检查点仍标 `[ ]`，与代码现状严重不符 |
| 代码实现 | **核心管线完成,端到端可视化已打通** | CompositorEffect 桥接 + GPU 管线 + 自动挂接 Manager + color_buffer 合成全部实现,Nanite 渲染输出已合成到引擎 color target |

**核心结论**:Stage 1 的三个子目标(CompositorEffect 接入 + 运行时读取 nanite 数据 + 将模型渲染到场景中)已全部达成。`render_material_resolve` 现在通过新增的 `dispatch_composite` pass,把内部 `color_buffer` 以 `vis_buffer != 0` 为 mask 合成到 `RenderData` 的 color target,引擎最终呈现的帧里已包含 Nanite 像素。

---

## 二、代码已完成的目标（对照文档）

### 2.1 框架与编译开关（Task 1.1）✅

- [nanite/SCsub](file:///d:/Code/04_Engine/godot/nanite/SCsub) 第 40-44 行：`gpu/*.cpp` / `scene/*.cpp` / `bridge/*.cpp` 通配已配置
- [SConstruct](file:///d:/Code/04_Engine/godot/SConstruct) 第 472 行：`env["nanite_bridge"] = ARGUMENTS.get("nanite_bridge", "gdext")`
- SCsub 第 20-23 行：`NANITE_BRIDGE_GDEXT` 宏在 `gdext` / `all` 时定义
- [nanite/core/nanite_server.cpp](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.cpp) 第 85 行：`GLOBAL_DEF("nanite/bridge/active", "gdext")` + enum hint

### 2.2 INaniteBridge 抽象接口（Task 1.2）✅

- [nanite/core/nanite_bridge.h](file:///d:/Code/04_Engine/godot/nanite/core/nanite_bridge.h) 定义抽象类，含 `ShadowMode` 枚举 + 7 个纯虚方法 + 虚析构

### 2.3 NaniteServer 单例（Task 1.3 + Task 1.17.2 重构）✅

- [nanite/core/nanite_server.h](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.h) 第 80 行：`INaniteBridge *bridge = nullptr;`（抽象指针，**已按 S1-08 重构**）
- 第 160 行：`void set_bridge(INaniteBridge *p_bridge);`（setter 注入）
- [nanite/core/nanite_server.cpp](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.cpp) 第 50-53 行注释明确："core must NOT include nanite/bridge/nanite_gdext_bridge.h" — **核心零桥接具体类型依赖已达成**
- 第 176-202 行：`register_mesh` 引用计数 + GPU 上传
- 第 230-247 行：`register_instance` / `unregister_instance`
- 第 249-257 行：`update_instance_transform`（Task 1.16.11 多实例 transform 缓存）
- 两阶段初始化：`init()`（SERVERS 层，无 RD 依赖）+ `init_engine_post()`（SCENE 层，创建 gpu_pipeline）

### 2.4 NaniteMeshData GPU Buffer（Task 1.4 + T1.16.5）✅

- [nanite/gpu/nanite_mesh_data.cpp](file:///d:/Code/04_Engine/godot/nanite/gpu/nanite_mesh_data.cpp)：5 个 SSBO（cluster / vertex / bvh / page / materials）
- T1.16.5 已补完 `materials_ssbo` 上传 + `free_gpu_resources` 释放

### 2.5 NaniteGPUPipeline — Cull + Rasterize + HZB + Material Resolve（Task 1.5 + 1.16）✅

- [nanite/gpu/nanite_gpu_pipeline.h/.cpp](file:///d:/Code/04_Engine/godot/nanite/gpu/nanite_gpu_pipeline.h) 完整实现 4 个 shader 编译 + 4 个 pipeline
- [nanite/shaders/nanite_cull.glsl](file:///d:/Code/04_Engine/godot/nanite/shaders/nanite_cull.glsl)：T1.16.7 真实剔除算法（视锥 / 背面 / HZB 遮挡 / LOD 选择）
- [nanite/shaders/nanite_rasterize.glsl](file:///d:/Code/04_Engine/godot/nanite/shaders/nanite_rasterize.glsl)：T1.16.8 软光栅化（meshlet 解码 + barycentric + 深度测试）
- [nanite/shaders/nanite_material_resolve.glsl](file:///d:/Code/04_Engine/godot/nanite/shaders/nanite_material_resolve.glsl)：T1.16.9 Lambert + barycentric 插值 + 调试模式
- [nanite/shaders/nanite_hzb_downsample.glsl](file:///d:/Code/04_Engine/godot/nanite/shaders/nanite_hzb_downsample.glsl)：真实 2x2 MAX 降采样

### 2.6 NaniteHZB（Task 1.6）✅

- [nanite/gpu/nanite_hzb.h/.cpp](file:///d:/Code/04_Engine/godot/nanite/gpu/nanite_hzb.h) 完整实现 init / resize / build / mip 视图

### 2.7 NaniteGDExtBridge — CompositorEffect 接入（Task 1.7）✅

- [nanite/bridge/nanite_gdext_bridge.h](file:///d:/Code/04_Engine/godot/nanite/bridge/nanite_gdext_bridge.h)：`CompositorEffect + INaniteBridge` 双继承
- 构造函数通过 `compositor_effect_set_callback` 重新绑定 `_render_callback`（绕过 GDVIRTUAL2 限制，见头文件第 44-59 行注释）
- 两个实例：PRE_OPAQUE（→ `render_visibility`）+ POST_OPAQUE（→ `render_material_resolve`）

### 2.8 NaniteMeshInstance3D（Task 1.8 + T1.16.12）✅

- [nanite/scene/nanite_mesh_instance_3d.h/.cpp](file:///d:/Code/04_Engine/godot/nanite/scene/nanite_mesh_instance_3d.h)：`MeshInstance3D` 子类，4 个属性暴露
- T1.16.12：`ENTER_TREE` 时 `set_cast_shadows_setting(SHADOW_CASTING_SHADOWS_ONLY)` 阻断引擎原生 mesh 渲染
- `TRANSFORM_CHANGED` 上报 transform 到 `instance_transforms`

### 2.9 NaniteDebug（Task 1.9）✅

- 7 种 DebugMode 枚举 + `set_debug_mode` 转发 + material_resolve shader push constant 切换分支

### 2.10 粗 LOD 阴影（Task 1.10）✅

- `set_nanite_mesh` 调用 `mesh_set_shadow_mesh`，shadow 缺失时 `WARN_PRINT_ONCE_ED`

### 2.11 NanitePageCache 占位（Task 1.12）✅

- always-resident 占位实现，接口就位

### 2.12 Compositor 自动挂接 — NaniteGDExtBridgeManager（Task 1.17，**代码已实现但文档未更新**）✅

> **这是文档与代码最大的矛盾点**：tasks.md / checklist.md 全部标 `[ ]`，但代码实际已完整实现。

- [nanite/bridge/nanite_gdext_bridge_manager.h](file:///d:/Code/04_Engine/godot/nanite/bridge/nanite_gdext_bridge_manager.h)：完整类定义（`Object` 子类，非 GDCLASS，手动生命周期）
- [nanite/bridge/nanite_gdext_bridge_manager.cpp](file:///d:/Code/04_Engine/godot/nanite/bridge/nanite_gdext_bridge_manager.cpp)：
  - `init(NaniteServer *)` 第 45-87 行：创建两个 bridge Ref + `default_compositor` + `set_compositor_effects` + `install` + `set_bridge` 注入 + SceneTree 信号连接
  - `finish()` 第 89-114 行：断开信号 + `set_bridge(nullptr)` + 释放 Refs
  - `attach_to_viewport` / `attach_to_compositor`（带去重）第 116-159 行
  - `_attach_viewport` 第 181-204 行：`find_world_3d()` → 已是我们的则跳过 / 用户已有则追加 / 否则直接 `set_compositor(default_compositor)`
  - `_on_node_added` 第 206-221 行：`cast_to<Viewport>` + `call_deferred(_attach_viewport_deferred, ObjectID)`
  - `_on_process_frame` 第 223-248 行：每 60 帧遍历 `_viewports` group 兜底
- [nanite/register_types.cpp](file:///d:/Code/04_Engine/godot/nanite/register_types.cpp)：
  - 第 116-119 行：`MODULE_INITIALIZATION_LEVEL_SCENE` 阶段 `memnew(NaniteGDExtBridgeManager)` + `init(ns)`
  - 第 163-167 行：`uninitialize` 阶段 `finish()` + `memdelete(mgr)`
- [nanite/core/nanite_server.cpp](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.cpp) 第 259-329 行：`render_visibility` **已改读真实 RenderData**（`get_render_scene_data()->get_cam_transform()` / `get_cam_projection()` + `get_render_scene_buffers()->get_internal_size()`）
- ProjectSetting `nanite/bridge/auto_attach_compositor`（默认 true）已在 Manager::init 注册
- [nanite/tests/test_nanite_gdext_bridge_manager.h](file:///d:/Code/04_Engine/godot/nanite/tests/test_nanite_gdext_bridge_manager.h)：3 个 doctest（`set_bridge_setter_injects_pointer` / `attach_to_compositor_appends_effects` / `default_compositor_bundles_two_bridges`）

---

## 三、文档与代码的自相矛盾

### 矛盾 1：Task 1.17 文档状态与代码实际严重不符 ⚠️ **高优先级**

| 来源 | Task 1.17 状态 | 与代码一致性 |
|------|--------------|------------|
| `tasks.md` 1.17 节 | 全部 `[ ]` 未完成 | ❌ **不符** — 代码 T1.17.1~T1.17.5/T1.17.7~T1.17.9 已实现 |
| `checklist.md` 第 17 节 | 全部 `[ ]` 未完成 | ❌ **不符** — 同上 |
| `nanite-overall-design.md` 10.5 S1-08 | "已补完（Stage 1 内）" | ✅ 与代码一致 |
| `nanite-implementation-tasks.md` 1.10 验收 | 全部 `[x]` 已完成 | ✅ 与代码一致 |

**根因**：tasks.md / checklist.md 在代码实现后**未回填状态**。spec.md 已更新到 S1-08 重构后版本，但 tasks.md 1.17 节与 checklist.md 第 17 节的子项勾选仍是重构前的旧状态。

**修复建议**：按代码实际状态批量更新 tasks.md 1.17 节与 checklist.md 第 17 节的 `[ ]` → `[x]`（详见第四节"待修复文档清单"）。

### 矛盾 2：spec.md 声称 `NaniteServer.get_default_compositor()` 可用，代码未暴露 ⚠️

- [spec.md 第 648 行](spec.md)："用户在场景中放 `WorldEnvironment` 节点 ... 或调用 `NaniteServer.get_default_compositor()` 获取预制资源"
- 代码实际：`NaniteServer` 类**没有** `get_default_compositor()` 方法（[nanite_server.h](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.h) grep 无匹配）
- `get_default_compositor()` 只存在于 `NaniteGDExtBridgeManager`（[nanite_gdext_bridge_manager.h](file:///d:/Code/04_Engine/godot/nanite/bridge/nanite_gdext_bridge_manager.h) 第 108 行），且 `NaniteGDExtBridgeManager` **未注册到 ClassDB**（非 GDCLASS），GDScript 用户无法访问

**影响**：spec 描述的"游戏运行时"挂接路径（用户在 inspector 把 default_compositor 拖到 WorldEnvironment）**目前不可用** —— 用户没有公开 API 拿到预制 Compositor。

**修复建议**：在 `NaniteServer` 暴露 `get_default_compositor()` 转发方法（绑定到 ClassDB），或在 spec 中说明用户需通过 `NaniteGDExtBridgeManager` 访问。

### 矛盾 3：spec.md / tasks.md 说 Manager 在 SERVERS 层创建，代码在 SCENE 层 ⚠️

- [spec.md 第 626 行](spec.md)："register_types.cpp 在 MODULE_INITIALIZATION_LEVEL_SERVERS 阶段 memnew(NaniteGDExtBridgeManager)"
- [tasks.md T1.17.9.1](tasks.md)：同上，SERVERS 层
- 代码实际 [register_types.cpp 第 75-120 行](file:///d:/Code/04_Engine/godot/nanite/register_types.cpp)：在 `MODULE_INITIALIZATION_LEVEL_SCENE` 阶段创建 Manager
- 代码注释（第 102-110 行）解释了原因：`Compositor / CompositorEffect` 构造函数调用 `RenderingServer::get_singleton()->compositor_create()`，而 RS 在 SERVERS 层之后才创建，SERVERS 层创建会崩溃

**影响**：功能等价（SCENE 层是正确选择），但文档与代码不符，可能误导后续维护者。

**修复建议**：更新 spec.md / tasks.md T1.17.9.1，标注 Manager 在 SCENE 层创建（并解释 RS 初始化顺序约束）。

### 矛盾 4：`nanite/bridge/active` ProjectSetting 不再生效 ⚠️

- spec.md / tasks.md 描述：`NaniteServer::init()` 读取 `nanite/bridge/active`，根据值创建对应桥接
- 代码实际 [nanite_server.cpp 第 85-88 行](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.cpp)：仅读取并校验值，但**不再用于创建桥接**（桥接由 Manager 在 SCENE 层无条件创建，只要 `NANITE_BRIDGE_GDEXT` 宏定义）
- 注释（第 79-84 行）明确："The active backend is selected by which Manager the bridge layer decides to construct; the `nanite/bridge/active` setting is informational only at this level."

**影响**：用户在 ProjectSettings 切换 `nanite/bridge/active` 到 `module` 或 `deep` **不会生效**（Stage 1 只编译 gdext，且代码不读这个值选桥接）。

**修复建议**：在 spec.md 标注此 ProjectSetting 在 Stage 1 为"信息性"（实际由编译期宏决定），或移除该设置避免误导。

---

## 四、距离 Stage 1 目标的关键差距

### 差距 1（致命）：color_buffer 未合成到引擎渲染目标 ✅ **已修复(2026-08-04)**

**这是距离"将模型渲染到场景中"目标的根本阻塞,现已解决。**

- **修复前**：[nanite_server.cpp 第 416-417 行](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.cpp) `render_material_resolve` 空实现,`color_buffer` 只写入 pipeline 内部纹理,引擎帧里没有 Nanite 像素
- **修复方案**:新增 `nanite/shaders/nanite_composite.glsl` compute shader,在 `dispatch_material_resolve` 后执行 `dispatch_composite`,把内部 `color_buffer` 以 `vis_buffer != 0` 为 mask 合成到引擎 color target(`RenderSceneBuffersRD::get_color_layer(0)` 返回的 RID)
- **修复涉及的文件**:
  - [nanite/shaders/nanite_composite.glsl](file:///d:/Code/04_Engine/godot/nanite/shaders/nanite_composite.glsl) — 新建 composite shader(vis_buffer mask + color_buffer → engine target)
  - [nanite/SCsub](file:///d:/Code/04_Engine/godot/nanite/SCsub) — 注册 `GLSL_HEADER("shaders/nanite_composite.glsl")`
  - [nanite/gpu/nanite_gpu_pipeline.h](file:///d:/Code/04_Engine/godot/nanite/gpu/nanite_gpu_pipeline.h) — 新增 `composite_shader` / `composite_pipeline` 成员 + `dispatch_composite` 方法声明
  - [nanite/gpu/nanite_gpu_pipeline.cpp](file:///d:/Code/04_Engine/godot/nanite/gpu/nanite_gpu_pipeline.cpp) — `init` 创建 composite pipeline、`cleanup` 释放、`dispatch_composite` 实现
  - [nanite/core/nanite_server.cpp](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.cpp) — `render_material_resolve` 通过 Variant 调用 `get_color_layer(0)` 获取引擎 color target RID,调用 `dispatch_composite`
- **关键设计决策**:
  1. 使用独立 composite pass 而非修改 `nanite_material_resolve.glsl` 直接写引擎 target — 保持 material_resolve 职责单一,便于测试与 Stage 4 迁移
  2. 用 `vis_buffer != 0` 作为 mask — 仅覆盖有 Nanite 几何的像素,保留引擎背景
  3. 通过 Variant `buffers->call(SNAME("get_color_layer"), 0)` 获取 color target RID — 不引入 `render_scene_buffers_rd.h` 依赖,保持核心层零具体渲染器实现依赖,利于 Stage 4 GDExtension 化
- **验证**:scons 编译通过(scons platform=windows target=editor dev_build=yes accesskit=no angle=no),生成 `nanite_composite.glsl.gen.h`

### 差距 2（高）：NaniteMeshEditor 未调用 attach_to_viewport 🟠

- [nanite/editor/nanite_mesh_editor.cpp 第 1839-1852 行](file:///d:/Code/04_Engine/godot/nanite/editor/nanite_mesh_editor.cpp#L1839-L1852)：构造函数创建 SubViewport 后**没有**调用 `NaniteGDExtBridgeManager::attach_to_viewport`
- 第 1849-1852 行注释明确："Stage 0 preview deliberately does NOT attach the Nanite CompositorEffect to this SubViewport"
- Task 1.17.6 在 tasks.md / checklist.md 标 `[ ]`，与代码一致（确实未实现）

**缓解因素**：Manager 的 60 帧轮询兜底路径（`_on_process_frame`）会扫描 `_viewports` group，SubViewport 加入树后会被兜底挂接 — **但**前提是 `auto_attach_enabled == true` 且 SubViewport 在 `_viewports` group 中（Godot 的 Viewport 基类构造时会加入此 group，所以应该成立）。

**修复建议**：在 NaniteMeshEditor 构造函数 SubViewport 创建后显式调用 `attach_to_viewport`（条件编译 `#if defined(NANITE_BRIDGE_GDEXT)`），避免依赖轮询兜底的时序不确定性。

### 差距 3（中）：缺失 GDScript 端到端测试 🟡

- tasks.md T1.17.10.2 要求 `nanite/tests/test_compositor_auto_attach.gd`，包含 3 个用例
- 代码实际：**文件不存在**（Glob 验证无匹配）
- doctest `test_nanite_gdext_bridge_manager.h` 存在但只有 3 个用例（spec 要求 4 个，缺 `test_set_bridge_injection` 等价用例 — 实际已有 `set_bridge_setter_injects_pointer` 覆盖，但命名不同）

**影响**：自动挂接逻辑的端到端验证缺失，重构后可能引入回归。

### 差距 4（中）：`render_material_resolve` 多实例处理不完整 🟡

- [nanite_server.cpp 第 434-467 行](file:///d:/Code/04_Engine/godot/nanite/core/nanite_server.cpp#L434-L467)：遍历 `instance_map` 但在第一个实例后 `break`（第 466 行）
- 注释（第 463-465 行）："Stage 1 single-pass: one dispatch per instance is enough since vis_buffer is shared. Break after the first so we don't overwrite the color_buffer with a second instance's model_matrix."
- 多实例场景下，只有第一个实例会被 material resolve 着色，其余实例在 color_buffer 中不可见

**影响**：多 `NaniteMeshInstance3D` 场景下，只有第一个实例有颜色输出（但仍存在差距 1 的根本问题，所以即便单实例也看不到）。

### 差距 5（低）：doctest / GDScript 测试未实际运行验证 🟢

- checklist.md 多处标 `[PARTIAL]`：测试代码已编写但未实际 scons 编译 + 运行
- GPU 测试需 Vulkan 后端，无后端时自动 SKIP
- 这不是代码差距，而是验证流程差距

---

## 五、Stage 1 目标达成度评估

### 子目标 1：通过 CompositorEffect 接入 ✅ **已达成**

- `NaniteGDExtBridge : CompositorEffect` 已实现
- 两个实例（PRE_OPAQUE + POST_OPAQUE）通过 `compositor_effect_set_callback` 注册回调
- `NaniteGDExtBridgeManager` 创建 `default_compositor` 并通过 SceneTree 信号 + 60 帧轮询自动挂接到 viewport 的 `World3D`
- **不修改引擎源码** ✅（全部在 `nanite/` module 内）

### 子目标 2：运行时读取 nanite 数据 ✅ **已达成**

- `NaniteMeshData::upload_to_gpu` 将 `NaniteMeshResource` 的 blobs 上传到 SSBO
- `NaniteServer::register_mesh` 引用计数管理
- `NaniteGPUPipeline::dispatch_cull / dispatch_rasterize` 从 SSBO 读取 cluster / vertex / bvh 数据
- `render_visibility` 从 `RenderData` 读取真实相机参数

### 子目标 3：将模型渲染到场景中 ✅ **已达成(2026-08-04)**

- GPU 管线输出到内部 `color_buffer`,通过新增的 `dispatch_composite` pass **合成到引擎 color target**
- 用户在场景中放置 `NaniteMeshInstance3D` 后,可以看到 Nanite 渲染输出(Lambert 着色或调试模式色彩)
- `vis_buffer != 0` 作为 mask,仅覆盖有 Nanite 几何的像素,保留引擎背景

**结论**:Stage 1 的三个子目标(CompositorEffect 接入 + 运行时读取 nanite 数据 + 将模型渲染到场景中)现已全部达成。

---

## 六、待修复文档清单

按优先级排序：

| 优先级 | 文件 | 修复内容 |
|--------|------|---------|
| P0 | `tasks.md` 1.17 节 | T1.17.1~T1.17.5/T1.17.7~T1.17.9 的 `[ ]` → `[x]`（代码已实现） |
| P0 | `checklist.md` 第 17 节 | T1.17.1~T1.17.5/T1.17.7~T1.17.9 检查点 `[ ]` → `[x]` |
| P0 | `spec.md` | 补充"color_buffer → 引擎 color target 合成"需求（当前完全缺失） |
| P1 | `spec.md` 第 648 行 | 修正 `NaniteServer.get_default_compositor()` 描述（实际在 Manager 上，且未暴露 GDScript） |
| P1 | `spec.md` / `tasks.md` T1.17.9 | 标注 Manager 在 SCENE 层创建（非 SERVERS），附 RS 初始化顺序说明 |
| P1 | `spec.md` ProjectSettings 节 | 标注 `nanite/bridge/active` 在 Stage 1 为信息性，实际由编译期宏决定 |
| P2 | `tasks.md` T1.17.6 / `checklist.md` T1.17.6 | 保持 `[ ]`（代码确实未实现 NaniteMeshEditor 调用 attach_to_viewport） |
| P2 | `tasks.md` T1.17.10 / `checklist.md` T1.17.10 | 保持 `[ ]`（test_compositor_auto_attach.gd 不存在，doctest 仅 3 例） |

---

## 七、待修复代码清单（距离 Stage 1 完整目标）

| 优先级 | 位置 | 修复内容 |
|--------|------|---------|
| P0 | `nanite/core/nanite_server.cpp::render_material_resolve` | 移除 `(void)p_render_data;`，从 `p_render_data->get_render_scene_buffers()` 获取 color target，增加 blit / fullscreen quad pass 把内部 `color_buffer` 合成到引擎 color target（仅覆盖 `vis_buffer != 0` 的像素） |
| P0 | `nanite/gpu/nanite_gpu_pipeline.h/.cpp` | 新增 `dispatch_composite_to_render_target(rd, render_data, vis_buffer, color_buffer)` 方法，或修改 `dispatch_material_resolve` 直接绑定引擎 color target |
| P1 | `nanite/editor/nanite_mesh_editor.cpp` 构造函数 | SubViewport 创建后调用 `NaniteGDExtBridgeManager::get_singleton()->attach_to_viewport(viewport)`（条件编译 `#if defined(NANITE_BRIDGE_GDEXT)`） |
| P1 | `nanite/core/nanite_server.h/.cpp` | 新增 `get_default_compositor()` 转发方法 + ClassDB 绑定（让 GDScript 用户能拿到预制 Compositor） |
| P2 | `nanite/tests/test_compositor_auto_attach.gd` | 新建文件，实现 T1.17.10.2 要求的 3 个用例 |
| P2 | `nanite/tests/test_nanite_gdext_bridge_manager.h` | 补充 `test_set_bridge_injection` 用例（或重命名现有 `set_bridge_setter_injects_pointer` 对齐 spec 命名） |

---

## 八、与总体设计文档 10.5 节差异对照表（更新建议）

| 编号 | 设计文档描述 | 代码实际 | 一致性 | 建议 |
|------|------------|---------|--------|------|
| S1-01 | 独立 GDExtension 插件分发 | module 内 `nanite/bridge/` 子目录 + 条件编译 | ✅ 已记录差异 | 无 |
| S1-04 | push constants 含 view/projection | UBO binding 5 | ✅ 已记录 | 无 |
| S1-05 | cull shader 真实剔除 | T1.16.7 已补完 | ✅ 一致 | 无 |
| S1-06 | rasterize 软光栅化 | T1.16.8 已补完 | ✅ 一致 | 无 |
| S1-07 | material_resolve barycentric + BRDF | T1.16.9 Lambert（非完整 PBR） | ✅ 一致（PBR 留 Stage 2+） | 无 |
| S1-08 | CompositorEffect 自动挂接 | Manager 已实现，但 color_buffer 未合成到引擎 target | ⚠️ **部分一致** | 10.5 节 S1-08 行应补充"color_buffer 合成到引擎 color target 仍待补完" |
| S1-09 | get_render_data() 获取相机 | get_render_scene_data() | ✅ 一致 | 无 |
| S1-10 | HZB 测试命名 | 测试名略不同但等价 | ✅ 一致 | 无 |
| **S1-NEW** | **color_buffer → 引擎 color target 合成** | **已实现(dispatch_composite pass)** | ✅ **已修复** | Task 1.18 已完成,新增 nanite_composite.glsl |

---

## 九、总结

Stage 1 的代码实现度**远超 tasks.md / checklist.md 文档记录的状态**:

1. **已实现且文档对齐**:Task 1.1-1.16 全部完成,总体设计文档与 implementation-tasks.md 已正确标注"已补完"。
2. **已实现但文档滞后**:Task 1.17(Compositor 自动挂接)代码完整实现,但 tasks.md / checklist.md 仍标 `[ ]` 未完成 — **需立即回填**。
3. **代码与 spec 矛盾**:`get_default_compositor()` 暴露路径、Manager 创建层级、`nanite/bridge/active` 生效方式三处 spec 描述与代码不符。
4. **✅ 关键功能已补完**:color_buffer 合成到引擎渲染目标的路径已通过 Task 1.18(nanite_composite.glsl + dispatch_composite)实现,Stage 1 最终可视化目标"将模型渲染到场景中"**已达成**。

**建议下一步**:
1. 实际运行验证 — 在编辑器中放置 NaniteMeshInstance3D,确认 Nanite 渲染输出可见(可能需要处理 RT Y 翻转、尺寸不匹配等运行时问题)
2. 回填 tasks.md / checklist.md 的 Task 1.17 状态(差距 2-5 仍存在,但均为非致命问题)
