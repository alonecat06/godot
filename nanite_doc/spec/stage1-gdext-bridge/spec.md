# Stage 1 — GDExtension 桥接（CompositorEffect + GPU 渲染管线）Spec

> 基于 [nanite-overall-design.md 第 10 章](../nanite-overall-design.md) 与 [nanite-implementation-tasks.md 阶段 1](../nanite-implementation-tasks.md)
> 在 Stage 0 离线构建模块基础上扩展 GPU 渲染管线。

---

## Why

Stage 0 已产出可序列化的 `NaniteMeshResource`（含 cluster/bvh/page blobs + shadow_mesh），但运行时没有任何渲染路径消费这些数据 —— `NaniteMeshInstance3D` 节点尚未存在，GPU 没有 cluster buffer，没有 Cull/Raster/HZB/Material 管线，引擎渲染管线也不知道 Nanite 的存在。

Stage 1 的目标是用最小侵入方式接入 Godot 渲染管线：通过 `CompositorEffect` 的 `PRE_OPAQUE` / `POST_OPAQUE` 回调执行 Nanite GPU 管线，使用 `mesh_set_shadow_mesh` 走粗 LOD 阴影方案。本阶段不依赖任何引擎源码 patch，全部在 `nanite/` module 内完成。

## What Changes

- **新增** `INaniteBridge` 抽象接口（`nanite/core/nanite_bridge.h`）— 桥接层接口，为 Stage 2/3 预留扩展点
- **新增** `NaniteServer` 单例（`nanite/core/nanite_server.h/.cpp`）— 管理实例注册、bridge 生命周期、GPU pipeline 调度
- **新增** `NaniteMeshInstance3D` 场景节点（`nanite/scene/nanite_mesh_instance_3d.h/.cpp`）— 用户在场景中放置 Nanite 网格的入口
- **新增** `NaniteMeshData` GPU buffer 管理（`nanite/gpu/nanite_mesh_data.h/.cpp`）— 将 `NaniteMeshResource` 的 blobs 上传到 SSBO
- **新增** `NaniteGPUPipeline`（`nanite/gpu/nanite_gpu_pipeline.h/.cpp`）— Cull + Rasterize + HZB Build + Material Resolve 的 RD pipeline 管理
- **新增** `NaniteHZB`（`nanite/gpu/nanite_hzb.h/.cpp`）— GPU 层次化深度缓冲
- **新增** `NanitePageCache`（`nanite/core/nanite_page_cache.h/.cpp`）— 占位实现（Stage 1 全常驻，不做流式加载，但接口就位）
- **新增** `NaniteDebug`（`nanite/core/nanite_debug.h/.cpp`）— 调试可视化模式枚举与状态
- **新增** `NaniteGDExtBridge`（`nanite/bridge/nanite_gdext_bridge.h/.cpp`）— `CompositorEffect` 子类，注册 PRE_OPAQUE + POST_OPAQUE 回调
- **新增** GLSL shaders：`nanite_cull.glsl` / `nanite_rasterize.glsl` / `nanite_hzb_downsample.glsl` / `nanite_material_resolve.glsl`
- **修改** `nanite/register_types.cpp` — 注册所有新类，注册 `NaniteServer` 为 singleton
- **修改** `nanite/SCsub` — 加入新源文件通配（gpu/, scene/, bridge/, shaders/）
- **修改** `SConstruct` — 添加 `nanite_bridge=gdext|module|deep|all` 开关（Stage 1 默认 `gdext`，编译 `NaniteGDExtBridge`）
- **修改** `NaniteMeshResource` — 新增 `get_materials()` 访问器，材质数组在 Stage 0 已有占位但未暴露
- **修改** `NaniteMeshEditor` — 预览视口内使用真实 `NaniteMeshInstance3D` 替代当前占位的 `MeshInstance3D`（Stage 0 用 shadow_mesh 预览，Stage 1 切到真实 Nanite 渲染）
- **BREAKING**（内部）：Stage 0 的 `NaniteServer` 是空壳占位（`NOTIFICATION_FOCUS_EXIT` 空实现），Stage 1 替换为真实实现。由于 Stage 0 未对外暴露 NaniteServer API，不破坏用户代码。

## Impact

- **Affected specs**：
  - [stage0-offline-build/spec.md](../stage0-offline-build/spec.md) — `NaniteMeshResource` 数据布局、`NaniteMeshEditor` 预览组件行为
- **Affected code**：
  - `nanite/register_types.cpp` — 新增类注册
  - `nanite/SCsub` — 新增源文件通配
  - `nanite/core/nanite_resource.h/.cpp` — 可能新增 material 访问器
  - `nanite/editor/nanite_mesh_editor.cpp` — 预览切换到真实 Nanite 渲染
  - `SConstruct` — 新增 `nanite_bridge` 开关
  - 新增目录：`nanite/gpu/` / `nanite/scene/` / `nanite/bridge/` / `nanite/shaders/`

## ADDED Requirements

### Requirement: NaniteServer 单例与桥接调度

系统 SHALL 提供 `NaniteServer : Object` 单例（通过 `ClassDB::register_class` + `Engine::get_singleton()->add_singleton` 注册），作为 Nanite 运行时与引擎之间的协调中心。

#### Scenario: 启动时创建并安装桥接

- **WHEN** 引擎初始化到 `MODULE_INITIALIZATION_LEVEL_SERVERS`（早于 EDITOR，因为渲染相关）
- **THEN** `NaniteServer` 单例被创建
- **AND** 读取 `ProjectSettings` 的 `nanite/bridge/active`（默认 `"gdext"`）
- **AND** 若 `NANITE_BRIDGE_GDEXT` 宏已定义且 active=`"gdext"`，则实例化 `NaniteGDExtBridge` 并调 `bridge->install(this)`
- **AND** 若桥接不可用，`ERR_PRINT` 警告但引擎继续启动

#### Scenario: 实例注册与注销

- **WHEN** `NaniteMeshInstance3D` 进入 SceneTree（`NOTIFICATION_ENTER_TREE`）
- **THEN** 调用 `NaniteServer::register_instance(this)`，instance_map 按 ObjectID 记录
- **WHEN** 节点退出 SceneTree（`NOTIFICATION_EXIT_TREE`）
- **THEN** 调用 `NaniteServer::unregister_instance(this)`

#### Scenario: mesh 资源注册为 GPU 资源

- **WHEN** `NaniteMeshInstance3D::set_nanite_mesh(resource)` 被调用且 resource 非空
- **THEN** 调用 `NaniteServer::register_mesh(resource)` 返回一个 `RID`（内部维护 `HashMap<RID, NaniteMeshData*>`）
- **WHEN** 同一 resource 被多个 instance 引用
- **THEN** mesh_data 引用计数 +1，不重复上传
- **WHEN** resource 被替换为 null 或节点销毁
- **THEN** 引用计数 -1，归零时释放 GPU buffer

---

### Requirement: NaniteMeshInstance3D 场景节点

系统 SHALL 提供 `NaniteMeshInstance3D : MeshInstance3D`（继承而非 `VisualInstance3D`，以便复用 Godot 的 mesh RID 管理和 shadow 注册），作为用户在场景树中放置 Nanite 网格的入口。

#### Scenario: 资源绑定与 RID 创建

- **WHEN** `set_nanite_mesh(Ref<NaniteMeshResource>)` 被调用
- **THEN** 内部 `Ref<ArrayMesh>` 占位 mesh 被替换为 `resource->get_shadow_mesh()` 用于引擎原生渲染（这样引擎视锥剔除、阴影投射、材质系统都正常工作）
- **AND** `NaniteServer::register_mesh(resource)` 返回的 RID 存到 `mesh_rid`
- **AND** 通过 `RenderingServer::mesh_set_shadow_mesh(get_mesh_rid(), shadow_mesh_rid)` 设置粗 LOD 阴影 mesh

#### Scenario: 强制 LOD 与相对屏幕尺寸

- **WHEN** `set_forced_lod(int)` 被设为非负值（默认 -1 = 自动）
- **THEN** 该实例在 GPU Cull 时跳过 LOD 误差计算，直接使用指定 LOD 层
- **WHEN** `forced_lod = -1` 且 `relative_screen_size > 0`
- **THEN** LOD 选择按 `screen_size / mesh_bounds_projected_size` 比例调整误差阈值

#### Scenario: nanite_enabled 开关

- **WHEN** `set_nanite_enabled(false)`
- **THEN** 实例从 `NaniteServer` 注销 GPU mesh，恢复为纯 `MeshInstance3D`（用 shadow_mesh 渲染）
- **WHEN** `set_nanite_enabled(true)`
- **THEN** 重新注册到 `NaniteServer`

---

### Requirement: NaniteMeshData GPU Buffer 管理

系统 SHALL 提供 `NaniteMeshData` 类，将 `NaniteMeshResource` 的 blobs 上传到 GPU SSBO，供 GPU pipeline 采样。

#### Scenario: 首次上传

- **WHEN** `NaniteServer::register_mesh(resource)` 发现 resource 未在 mesh_map 中
- **THEN** 创建 `NaniteMeshData`，调用 `upload_to_gpu(rd, resource)`
- **AND** 创建 4 个 SSBO：`cluster_ssbo`（clusters_data）/ `vertex_ssbo`（vertex_data）/ `bvh_ssbo`（nodes_data）/ `page_ssbo`（page_table_data）
- **AND** 每个SSBO 大小等于对应 blob 字节数
- **AND** `gpu_uploaded = true`

#### Scenario: GPU 资源释放

- **WHEN** 引用计数归零
- **THEN** 调用 `free_gpu_resources(rd)` 释放所有 SSBO
- **AND** `gpu_uploaded = false`

#### Scenario: 上传数据完整性

- **WHEN** `upload_to_gpu` 完成
- **THEN** `rd->buffer_get_data(cluster_ssbo)` 的字节内容 == `resource->get_clusters_data()`
- **AND** 同样对 vertex/bvh/page 三个 SSBO 成立

---

### Requirement: NaniteGPUPipeline — Cull + Rasterize

系统 SHALL 提供 `NaniteGPUPipeline` 类，通过 RD compute/draw 调用执行 Nanite 的核心 GPU 管线。

#### Scenario: pipeline 初始化

- **WHEN** `NaniteServer::init()` 创建 `NaniteGPUPipeline` 并调用 `init(rd)`
- **THEN** 编译 4 个 shader（cull/rasterize/hzb_downsample/material_resolve），创建对应 RD pipeline
- **AND** 内部 `NaniteHZB hzb` 成员被 `init(rd)`

#### Scenario: Cull Pass — BVH 遍历 + 三重剔除 + LOD 选择

- **WHEN** `dispatch_cull(rd, params)` 被调用，params 包含 view_matrix / projection / screen_size / hzb_texture
- **THEN** compute shader 执行栈式 BVH 遍历：
  - 对每个 BVH 节点做视锥剔除（节点 AABB 6 个面 vs frustum 6 个面）
  - 对叶子节点做背面剔除（cluster.cone_axis · view_dir > cone_cutoff）
  - 对可见节点做 HZB 遮挡剔除（投影 AABB 到屏幕，采样 HZB mip 比较深度）
  - 对可见 cluster 做 LOD 选择（cluster.error vs 屏幕空间误差阈值）
- **AND** 输出一个 `visible_clusters_buffer`（uint32 array：cluster_index 列表）
- **AND** 输出 `visible_count_buffer`（uint32：可见 cluster 总数）

#### Scenario: Rasterize Pass — Visibility Buffer 生成

- **WHEN** `dispatch_rasterize(rd, visible_buffer, mesh_data)` 被调用
- **THEN** 对每个可见 cluster：
  - 解码 meshlet（meshopt_decodeMeshlet）
  - 对每个三角形做软光栅化（compute shader 内）：计算覆盖的像素，记录 `(cluster_id << 8 | triangle_id)` 到 vis_buffer
- **AND** 输出 `vis_buffer`（R32_UINT 纹理，每像素 32 位）
- **AND** 输出 `depth_buffer`（R32_SFLOAT，软光栅化的深度值，供 HZB build 使用）

#### Scenario: 空场景处理

- **WHEN** `visible_count == 0`
- **THEN** rasterize pass 跳过，vis_buffer 清零
- **AND** 不产生 GPU 错误

---

### Requirement: NaniteHZB — GPU 层次化深度缓冲

系统 SHALL 提供 `NaniteHZB` 类，从 Nanite 光栅化的 depth buffer 构建 mip 链，供下一帧 Cull Pass 采样。

#### Scenario: 初始化与 resize

- **WHEN** `init(rd)` 被调用
- **THEN** 编译 `nanite_hzb_downsample.glsl` compute shader，创建 pipeline
- **WHEN** `resize(rd, screen_size)` 被调用
- **THEN** 计算 mip_count = `ceil(log2(max(width, height)))`
- **AND** 创建 R32_SFLOAT 纹理（mip_count 级，每级 size = screen_size >> level）
- **AND** 为每级 mip 创建独立 image view（`texture_create_shared_from_layer` 或逐级 view）

#### Scenario: HZB 构建

- **WHEN** `build(rd, depth_texture)` 被调用
- **THEN** 对 mip 1..mip_count-1 循环：
  - 绑定 src = mip_{i-1}, dst = mip_i
  - dispatch compute（workgroup 8x8），每个线程取 2x2 深度的 MAX
  - 插入 `BARRIER_COMPUTE_TO_COMPUTE`
- **AND** 完成后 `get_hzb_texture()` 返回完整 mip 链纹理

#### Scenario: 屏幕尺寸变化

- **WHEN** `RenderSceneBuffers` 尺寸变化
- **THEN** 下一帧 `build` 检测到尺寸不匹配，触发 `resize`
- **AND** 旧纹理被 `rd->free_rid` 释放

---

### Requirement: NaniteGDExtBridge — CompositorEffect 接入

系统 SHALL 提供 `NaniteGDExtBridge : CompositorEffect`，实现 `INaniteBridge`，通过 `set_effect_callback_type` 注册 PRE_OPAQUE 和 POST_OPAQUE 回调。

#### Scenario: 桥接安装

- **WHEN** `NaniteServer::init()` 创建 `NaniteGDExtBridge`
- **THEN** 构造函数调用 `set_effect_callback_type(EFFECT_CALLBACK_TYPE_PRE_OPAQUE)` 和 `set_effect_callback_type(EFFECT_CALLBACK_TYPE_POST_OPAQUE)`（通过 `add_effect_callback_type` API）
- **AND** 调用 `install(NaniteServer*)` 设置 shadow_mode = `SHADOW_COARSE_LOD`

#### Scenario: PRE_OPAQUE 回调

- **WHEN** 引擎在渲染帧的不透明 Pass 前调用 `_render_callback(EFFECT_CALLBACK_TYPE_PRE_OPAQUE, render_data)`
- **THEN** 从 `render_data->get_render_scene_data()` 获取相机 view/projection
- **AND** 从 `render_data->get_render_scene_buffers()` 获取 color/depth target 尺寸
- **AND** 调用 `NaniteServer::render_visibility(render_data)`：
  - 遍历所有注册的 NaniteMeshInstance3D
  - 对每个实例的 mesh_data 调 `dispatch_cull` → `dispatch_rasterize`
  - 调 `dispatch_hzb_build` 从 rasterize 的 depth 构建 HZB
- **AND** 将 vis_buffer 与 HZB 保存到 server 状态，供 POST_OPAQUE 使用

#### Scenario: POST_OPAQUE 回调

- **WHEN** 引擎在不透明 Pass 后调用 `_render_callback(EFFECT_CALLBACK_TYPE_POST_OPAQUE, render_data)`
- **THEN** 调用 `NaniteServer::render_material_resolve(render_data)`：
  - 执行 `dispatch_material_resolve` 从 vis_buffer 解码材质
  - 输出到 render_data 的 color buffer（通过 blending 或 fullscreen quad 覆盖 Nanite 像素）

#### Scenario: 无 Nanite 实例时的早退

- **WHEN** `NaniteServer::get_instance_count() == 0`
- **THEN** PRE_OPAQUE 与 POST_OPAQUE 直接 return，不发起任何 RD 调用

---

### Requirement: 粗 LOD 阴影方案

系统 SHALL 通过 `RenderingServer::mesh_set_shadow_mesh` 让引擎原生阴影 Pass 使用 NaniteBuilder 在 Stage 0 生成的粗 LOD shadow mesh 投射阴影。

#### Scenario: shadow mesh 设置

- **WHEN** `NaniteMeshInstance3D::set_nanite_mesh(resource)` 执行
- **THEN** 获取 `resource->get_shadow_mesh()` 返回的 ArrayMesh RID
- **AND** 调用 `RenderingServer::get_singleton()->mesh_set_shadow_mesh(get_mesh_rid(), shadow_mesh_rid)`
- **AND** 引擎原生阴影 Pass 自动使用 shadow mesh 渲染深度

#### Scenario: shadow mesh 缺失

- **WHEN** `resource->get_shadow_mesh()` 为 null（构建失败或老资源）
- **THEN** 不调用 `mesh_set_shadow_mesh`，引擎回退到用主 mesh 投射阴影
- **AND** `WARN_PRINT` 一次（避免每帧刷屏）

---

### Requirement: NaniteDebug 调试可视化

系统 SHALL 提供 `NaniteDebug` 类与调试模式枚举，通过 `NaniteServer::set_debug_mode()` 全局切换。

#### Scenario: 调试模式枚举

- **WHEN** 系统初始化
- **THEN** 暴露枚举 `NaniteDebugMode { NONE=0, CLUSTER_SOLID_COLOR, LOD_SOLID_COLOR, OVERDRAW_HEATMAP, PAGE_RESIDENCY, HZB_MIP_LEVELS, HZB_OCCLUSION }`
- **AND** 通过 `ClassDB::bind_enum_constant` 暴露到 GDScript

#### Scenario: NONE 模式（默认）

- **WHEN** debug_mode == NONE
- **THEN** Material Resolve shader 正常输出材质颜色
- **AND** 不渲染任何调试覆盖层

#### Scenario: CLUSTER_SOLID_COLOR 模式

- **WHEN** debug_mode == CLUSTER_SOLID_COLOR
- **THEN** Material Resolve shader 根据 cluster_index 哈希生成颜色，覆盖材质颜色
- **AND** 相邻 cluster 颜色明显不同（便于肉眼区分）

#### Scenario: HZB_MIP_LEVELS 模式

- **WHEN** debug_mode == HZB_MIP_LEVELS
- **THEN** POST_OPAQUE 额外渲染 fullscreen quad，将 HZB 各级 mip 显示在屏幕四角（类似 UE5）

#### Scenario: 全局生效与隔离

- **WHEN** `NaniteServer::set_debug_mode(mode)` 被调用
- **THEN** 全局所有 NaniteMeshInstance3D 都使用该模式（包括预览视口内的）
- **WHEN** 预览视口失焦
- **THEN** 自动恢复为 NONE（Stage 0 的 `NOTIFICATION_FOCUS_EXIT` 逻辑保持）

---

### Requirement: GLSL Shader 实现

系统 SHALL 提供 4 个 GLSL shader 文件，位于 `nanite/shaders/`，通过 RD shader include 机制编译。

#### Scenario: nanite_cull.glsl

- **WHEN** cull shader 被 dispatch
- **THEN** 输入：cluster_ssbo / bvh_ssbo / view_matrix / projection / hzb_texture / screen_size / error_threshold
- **AND** 输出：visible_clusters_buffer（append 方式写入）/ visible_count_buffer（atomic add）
- **AND** 算法：从 BVH 根节点开始 DFS，对每个节点做视锥/背面/HZB 遮挡剔除，对可见叶子 cluster 做 LOD 选择

#### Scenario: nanite_rasterize.glsl

- **WHEN** rasterize shader 被 dispatch
- **THEN** 输入：visible_clusters_buffer / cluster_ssbo / vertex_ssbo / meshlet_triangles
- **AND** 输出：vis_buffer（R32_UINT 纹理）/ depth_buffer（R32_SFLOAT 纹理）
- **AND** 算法：对每个可见 cluster 解码 meshlet，对每个三角形软光栅化，记录覆盖像素的 (cluster_id << 8 | triangle_id) 与深度

#### Scenario: nanite_hzb_downsample.glsl

- **WHEN** downsample shader 被 dispatch（已在总体设计文档 7.4 节定义）
- **THEN** 输入：src_depth（上一级 mip 或原始 depth）/ params（src_size, mip_level）
- **AND** 输出：dst_mip（image2D）
- **AND** 算法：每个线程取 2x2 深度的 MAX，写入 dst_mip

#### Scenario: nanite_material_resolve.glsl

- **WHEN** material resolve shader 被 dispatch
- **THEN** 输入：vis_buffer / cluster_ssbo / vertex_ssbo / materials
- **AND** 输出：color_buffer（写入 render_data 的 color target）
- **AND** 算法：对每个像素从 vis_buffer 解码 (cluster_id, triangle_id)，用 barycentric 插值顶点属性，查材质表着色

---

### Requirement: ProjectSettings 配置项

系统 SHALL 在 ProjectSettings 中注册 `nanite/bridge/active` 配置项，支持运行时桥接切换。

#### Scenario: 首次启动注册默认值

- **WHEN** `NaniteServer::init()` 检测到 `nanite/bridge/active` 不存在
- **THEN** 设置默认值 `"gdext"`
- **AND** 调用 `set_custom_property_info` 添加 enum hint：`"gdext,module,deep"`（Stage 1 仅 `gdext` 可用，其他会触发 ERR_PRINT）

#### Scenario: 用户在 ProjectSettings 中切换

- **WHEN** 用户在 ProjectSettings UI 修改 `nanite/bridge/active`
- **THEN** 下次引擎启动时生效（运行时不切换，避免 GPU 资源泄漏）

---

## MODIFIED Requirements

### Requirement: NaniteMeshEditor 预览组件

Stage 0 的 `NaniteMeshEditor` 使用 `MeshInstance3D` + shadow_mesh 预览。Stage 1 切换为 `NaniteMeshInstance3D`，使用真实 Nanite GPU 管线渲染。

#### Scenario: 切换为真实 Nanite 渲染

- **WHEN** `NaniteMeshEditor::edit(resource)` 被调用
- **THEN** 内部 `preview_instance` 类型由 `MeshInstance3D*` 改为 `NaniteMeshInstance3D*`
- **AND** `preview_instance->set_nanite_mesh(resource)` 触发 NaniteServer 注册 + GPU 上传
- **AND** SubViewport 渲染时 NaniteGDExtBridge 的 PRE_OPAQUE/POST_OPAQUE 回调被触发

#### Scenario: SubViewport 独立渲染上下文

- **WHEN** 预览视口渲染
- **THEN** NaniteGPUPipeline 在 SubViewport 的 RD 上下文中执行（不污染主视口）
- **AND** HZB 尺寸匹配 SubViewport 而非主窗口

---

### Requirement: NaniteMeshResource 材质数组访问

Stage 0 的 `NaniteMeshResource` 已有 `materials` 字段但未暴露。Stage 1 暴露给 Material Resolve shader 使用。

#### Scenario: 暴露 materials 访问器

- **WHEN** `NaniteServer::register_mesh(resource)` 被调用
- **THEN** 将 `resource->get_materials()` 的每个材质的 RID 收集到 `NaniteMeshData::material_rids` 数组
- **AND** Material Resolve shader 通过 `material_index` 间接寻址

---

## REMOVED Requirements

（本阶段不删除任何已有需求。）

---

## 实现约束与决策

### 1. 不作为独立 GDExtension 分发

虽然总体设计文档第 10 章描述为独立 `.gdextension` 插件，但实际实现选择在 `nanite/` module 内完成。理由：
- Stage 0 已是 module 的一部分，复用 `NaniteMeshResource` 等类无需跨二进制边界
- 当前 `nanite_bridge=gdext` SCons 开关仅控制是否编译 `NaniteGDExtBridge`，不影响分发方式
- 未来若需独立分发，可将 nanite/ 抽离为 GDExtension 项目，类代码无需改动

### 2. shader 编译

GLSL shader 通过 Godot 的 RD shader include 机制（`#include` 与 `.glslinc`）编译。Stage 1 暂不使用 GLSL 嵌入式字符串，shader 文件单独存放便于迭代。

### 3. 测试依赖

所有 GPU 测试需要 Vulkan 后端（`RenderingDevice::get_singleton()` 非 null）。OpenGL 后端不支持，测试自动 SKIP 而非 FAIL。

### 4. 性能目标

- 单个 10K tri Nanite 网格在 1080p 下 ≥ 30fps
- BVH 遍历 + 三重剔除 dispatch 耗时 < 1ms
- HZB build 耗时 < 0.1ms
- Material Resolve 耗时 < 0.5ms

---

## 验收标准（对应 tasks.md 1.10）

1. [ ] `nanite_bridge=gdext` 编译开关工作，默认编译 `NaniteGDExtBridge`
2. [ ] `NaniteServer` 单例在 `MODULE_INITIALIZATION_LEVEL_SERVERS` 注册
3. [ ] `NaniteMeshInstance3D` 可在场景中放置，set_nanite_mesh 后实例注册到 server
4. [ ] `NaniteMeshData` 上传 4 个 SSBO，大小匹配 resource blobs
5. [ ] GPU Cull shader 正确执行 BVH 遍历 + 三重剔除 + LOD 选择
6. [ ] GPU Rasterize shader 正确生成 vis_buffer + depth_buffer
7. [ ] GPU HZB 从 depth buffer 正确构建 mip 链（取 MAX）
8. [ ] Material Resolve shader 输出非空 color buffer
9. [ ] 粗 LOD 阴影通过 `mesh_set_shadow_mesh` 正常工作
10. [ ] NaniteDebug 7 种模式可切换（NONE / CLUSTER_SOLID_COLOR / LOD_SOLID_COLOR / OVERDRAW_HEATMAP / PAGE_RESIDENCY / HZB_MIP_LEVELS / HZB_OCCLUSION）
11. [ ] ProjectSettings `nanite/bridge/active` 配置项存在，默认 `"gdext"`
12. [ ] 单个 10K tri 场景 ≥ 30fps（1080p）
13. [ ] 所有 doctest + GDScript 测试通过
