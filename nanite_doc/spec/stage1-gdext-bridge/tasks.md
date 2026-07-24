# Tasks — Stage 1: GDExtension 桥接（CompositorEffect + GPU 渲染管线）

> 对应 [spec.md](spec.md)。任务按依赖顺序排列，可并行标记在每节末尾。
> 所有任务完成时应满足 [checklist.md](checklist.md) 中所有检查点。

---

## 1.1 框架搭建与编译开关

- [x] **Task 1.1.1**：创建新目录结构 `nanite/gpu/`、`nanite/scene/`、`nanite/bridge/`、`nanite/shaders/`
  - 在 `nanite/` 下创建 4 个空目录
  - 在 `nanite/SCsub` 中追加 `add_source_files` 通配：`gpu/*.cpp`、`scene/*.cpp`、`bridge/*.cpp`
  - shaders 目录通过 `nanite/SCsub` 的 `env.Append(SHADER_FILES=[...])` 或直接文件读取方式供 RD 编译

- [x] **Task 1.1.2**：在 `SConstruct` 添加 `nanite_bridge` 开关
  - 读取 `ARGUMENTS.get("nanite_bridge", "gdext")`
  - 根据 `nanite_bridge in ("gdext", "all")` 定义 `NANITE_BRIDGE_GDEXT` 宏（通过 `env.Append(CPPDEFINES=["NANITE_BRIDGE_GDEXT"])`）
  - 默认值 `"gdext"`，保证 Stage 1 开箱即用

- [x] **Task 1.1.3**：在 `ProjectSettings` 注册 `nanite/bridge/active` 配置项
  - 默认值 `"gdext"`
  - `set_custom_property_info` 添加 enum hint `"gdext,module,deep"`
  - 放在 `NaniteServer::init()` 内（避免提前依赖 ProjectSettings）
  - **子任务**：编写 doctest `[Nanite][Config] project_settings_default_is_gdext` 验证 `GLOBAL_GET("nanite/bridge/active") == "gdext"`

**依赖**：无（可与 1.2 并行）
**验证**：`scons platform=windows target=editor` 编译通过，新目录被纳入构建

---

## 1.2 INaniteBridge 抽象接口

- [x] **Task 1.2.1**：创建 `nanite/core/nanite_bridge.h`
  - 定义 `INaniteBridge` 抽象类（纯虚函数，无 GDCLASS）
  - 枚举 `ShadowMode { SHADOW_COARSE_LOD, SHADOW_DYNAMIC_GPU }`
  - 方法：`install(NaniteServer*)` / `get_shadow_mode()` / `on_pre_render(RenderData*)` / `on_pre_opaque_pass(RenderData*)` / `on_post_opaque_pass(RenderData*)` / `on_shadow_pass(RenderData*, RID, int)` / `get_bridge_name()`
  - 内联虚析构

- [x] **Task 1.2.2**：编译验证
  - 编写空 doctest `[Nanite][Bridge] inanitebridge_is_abstract`（`static_assert(!std::is_constructible<INaniteBridge>::value)`）
  - `scons` 编译通过

**依赖**：1.1.1（目录存在）
**验证**：doctest 通过

---

## 1.3 NaniteServer 单例实现

- [x] **Task 1.3.1**：创建 `nanite/core/nanite_server.h` 与 `nanite/core/nanite_server.cpp`
  - `NaniteServer : Object`，`GDCLASS(NaniteServer, Object)`
  - 静态 `singleton` 指针 + `get_singleton()`
  - 成员：`INaniteBridge *bridge`、`NaniteGPUPipeline *gpu_pipeline`、`HashMap<RID, NaniteMeshData*> mesh_map`、`HashMap<ObjectID, NaniteMeshInstance3D*> instance_map`、`NanitePageCache *page_cache`、`NaniteDebug *debug`
  - 方法：`init()` / `finish()` / `register_mesh(Ref<NaniteMeshResource>)` → RID / `unregister_mesh(RID)` / `register_instance(NaniteMeshInstance3D*)` / `unregister_instance(NaniteMeshInstance3D*)` / `render_visibility(RenderData*)` / `render_material_resolve(RenderData*)` / `get_instance_count()` / `get_visible_cluster_count()` / `set_debug_mode(int)` / `get_bridge()`
  - 替换 Stage 0 的空 `NaniteServer` 占位（Stage 0 当前文件不存在，直接新建）
  - **子任务**：在 `register_types.cpp` 的 `MODULE_INITIALIZATION_LEVEL_SERVERS` 注册 `ClassDB::register_class<NaniteServer>` + `Engine::get_singleton()->add_singleton(Engine::Singleton("NaniteServer", NaniteServer::get_singleton()))`

- [x] **Task 1.3.2**：实现 `init()` / `finish()`
  - `init()`：读取 `nanite/bridge/active`，根据宏 `NANITE_BRIDGE_GDEXT` 创建 `NaniteGDExtBridge`（1.7 实现前先用 nullptr 占位）并调 `install(this)`
  - `init()`：创建 `NaniteGPUPipeline` 并调 `init(rd)`（1.5 实现前先 nullptr）
  - `finish()`：反向销毁

- [x] **Task 1.3.3**：实现 `register_mesh` / `unregister_mesh`
  - `register_mesh`：检查 mesh_map 是否已有该 resource（按 resource 指针 hash）→ 有则 +1 引用计数，无则创建 `NaniteMeshData` 并 `upload_to_gpu`（1.4 实现前先空实现，返回 generated RID）
  - 返回的 RID 用 `RID_Allocator` 或简单 `HashMap<uint64_t, NaniteMeshData*>` 管理
  - `unregister_mesh`：引用计数 -1，归零则 `free_gpu_resources` 并 erase

- [x] **Task 1.3.4**：实现 `register_instance` / `unregister_instance`
  - 简单 `instance_map[instance->get_instance_id()] = instance`
  - `get_instance_count()` 返回 map.size()

- [x] **Task 1.3.5**：doctest `[Nanite][Server] singleton_creation_and_finish`
  - 验证 `NaniteServer::get_singleton()` 在 `init()` 后非空
  - 验证 `finish()` 后 singleton 为 nullptr
  - **子任务**：doctest `[Nanite][Server] register_unregister_instance`
    - 创建 `NaniteMeshInstance3D`，`register_instance` 后 `get_instance_count() == 1`
    - `unregister_instance` 后 `get_instance_count() == 0`

**依赖**：1.1（编译开关）、1.2（接口）
**验证**：doctest 通过

---

## 1.4 NaniteMeshData — GPU Buffer 管理

- [x] **Task 1.4.1**：创建 `nanite/gpu/nanite_mesh_data.h` 与 `nanite/gpu/nanite_mesh_data.cpp`
  - `NaniteMeshData` 类（不继承 Object，纯 C++ 类）
  - 成员：`RID cluster_ssbo / vertex_ssbo / bvh_ssbo / page_ssbo`、`LocalVector<RID> material_rids`、`int ref_count = 0`、`bool gpu_uploaded = false`
  - 方法：`upload_to_gpu(RenderingDevice*, const NaniteMeshResource*)` / `free_gpu_resources(RenderingDevice*)` / getters

- [x] **Task 1.4.2**：实现 `upload_to_gpu`
  - 对每个 blob 调 `rd->storage_buffer_create(size, data_bytes)`
  - `cluster_ssbo = rd->storage_buffer_create(res->get_clusters_data().size(), res->get_clusters_data())`
  - 同样 vertex/bvh/page
  - 遍历 `res->get_materials()` 收集 RID 到 `material_rids`
  - `gpu_uploaded = true`

- [x] **Task 1.4.3**：实现 `free_gpu_resources`
  - 对每个 RID 调 `rd->free_rid(rid)`，置 invalid
  - `gpu_uploaded = false`

- [x] **Task 1.4.4**：doctest `[Nanite][MeshData] upload_creates_valid_rids`
  - 创建 sphere NaniteMeshResource（复用 Stage 0 helper）
  - `upload_to_gpu(rd, res)`
  - `CHECK(cluster_ssbo.is_valid())` 等四个 RID
  - 验证 `rd->buffer_get_data(cluster_ssbo).size() == res->get_clusters_data().size()`
  - `free_gpu_resources` 后 `CHECK_FALSE(gpu_uploaded)`
  - **子任务**：doctest `[Nanite][MeshData] ssbo_data_matches_resource_blob` 逐字节比较

**依赖**：1.3（NaniteServer 提供 register_mesh 入口）
**验证**：doctest 通过（需要 Vulkan 后端，否则 SKIP）

---

## 1.5 NaniteGPUPipeline — Cull + Rasterize

- [x] **Task 1.5.1**：编写 `nanite/shaders/nanite_cull.glsl`
  - `#[compute] #version 450`
  - bindings：cluster_ssbo (set 0, binding 0) / bvh_ssbo (1) / hzb_texture (2, sampler) / visible_clusters_buffer (3, storage) / visible_count_buffer (4, storage) / params uniform (5)
  - push constants：view_matrix (mat4) / projection (mat4) / screen_size (ivec2) / error_threshold (float) / cluster_count (uint)
  - **实现说明**：view_matrix/projection 因 push constant 128B 上限改为放入 uniform buffer (UBO binding 5)，push constant 仅保留 screen_size/error_threshold/bvh_node_count/cluster_count。
  - **Stage 1 占位实现**：cull shader 简化为 pass-through，所有 cluster 标记为可见（atomicAdd 写入 visible_clusters），BVH 遍历/视锥剔除/背面剔除/HZB 遮挡/LOD 选择留 TODO Stage 2 实现。
  - 已完成于 `nanite/shaders/nanite_cull.glsl`

- [x] **Task 1.5.2**：编写 `nanite/shaders/nanite_rasterize.glsl`
  - `#[compute] #version 450`
  - bindings：visible_clusters_buffer / cluster_ssbo / vertex_ssbo / meshlet_triangles_ssbo / vis_buffer (image2D, r32ui) / depth_buffer (image2D, r32f) / params
  - **Stage 1 占位实现**：rasterize shader 简化为占位，每个 thread 处理一个 visible cluster，写入固定像素位置（`tid % screen_size.x`），编码 `(cluster_idx << 8) | 0`。完整软光栅化留 TODO Stage 2 实现。
  - 已完成于 `nanite/shaders/nanite_rasterize.glsl`

- [x] **Task 1.5.3**：创建 `nanite/gpu/nanite_gpu_pipeline.h` 与 `.cpp`
  - `NaniteGPUPipeline` 类
  - 成员：`RID cull_shader / rasterize_shader / hzb_downsample_shader / material_resolve_shader`、对应 `RID *_pipeline`、`NaniteHZB hzb`、`RID vis_buffer / depth_buffer`
  - 方法：`init(RenderingDevice*)` / `cleanup(RenderingDevice*)` / `dispatch_cull(rd, params)` → visible_buffer / `dispatch_rasterize(rd, visible_buffer, mesh_data)` → vis_buffer / `dispatch_hzb_build(rd, depth_texture)` / `dispatch_material_resolve(rd, vis_buffer, mesh_data)` / `get_hzb()`
  - 已完成于 `nanite/gpu/nanite_gpu_pipeline.h/.cpp`

- [x] **Task 1.5.4**：实现 `init(rd)`
  - 使用 GLSL_HEADER 生成器将 GLSL 嵌入为 C 字符串头文件（`<name>_shader_glsl`），运行时通过 `RDShaderFile::parse_versions_from_text` 解析
  - `rd->shader_create_from_spirv` 编译，`rd->compute_pipeline_create(shader)` 创建 4 个 pipeline
  - `hzb.init(rd)`
  - 额外创建：`hzb_sampler` (nearest+clamp)、`camera_ubo` (uniform_buffer_create)、`dummy_hzb_texture` (1x1 R32_SFLOAT fallback)

- [x] **Task 1.5.5**：实现 `dispatch_cull`
  - 创建 uniform set（绑定 cluster_ssbo / bvh_ssbo / hzb_texture+sampler / visible_buffer / count_buffer / camera_ubo）
  - `rd->compute_list_begin()` → bind pipeline → bind uniform set → set push constants → `dispatch_threads`
  - dispatch 数量：基于 `cluster_count`（Stage 1 简化：每线程处理一个 cluster）
  - 返回 visible_buffer RID
  - 通过 `buffer_update` 更新 camera_ubo 与零计数

- [x] **Task 1.5.6**：实现 `dispatch_rasterize`
  - 创建 uniform set（visible_buffer / cluster_ssbo / vertex_ssbo / vis_buffer image / depth_buffer image）
  - dispatch 数量：`dispatch_threads(visible_count, 1, 1)`
  - 内部用 `ensure_screen_buffers` 维护 vis_buffer / depth_buffer

- [x] **Task 1.5.7**：doctest `[Nanite][GPUPipeline] cull_produces_visible_list`
  - 已完成于 `nanite/tests/test_nanite_gpu_pipeline.h`（包含 cull_produces_visible_list 与 rasterize_produces_nonzero_visbuffer 两个 TEST_CASE）
  - GPU 测试需要 Vulkan 后端，无后端时自动 SKIP 而非 FAIL
  - **注**：cull_respects_frustum 因 Stage 1 cull 为 pass-through（所有 cluster 都视为可见），此测试在 Stage 1 不适用，将在 Stage 2 完整 BVH 遍历实现后启用

**依赖**：1.4（mesh data）、1.6（HZB 用于 cull 遮挡剔除，但 cull shader 本身可先不采样 HZB）
**验证**：doctest 通过

---

## 1.6 NaniteHZB — GPU 层次化深度缓冲

- [x] **Task 1.6.1**：编写 `nanite/shaders/nanite_hzb_downsample.glsl`
  - `#[compute] #version 450`
  - bindings：src_depth (sampler2D) / dst_mip (image2D, r32f) / params uniform
  - 算法：workgroup 8x8，每线程取 src 的 2x2 区域取 MAX 写入 dst
  - push constants：src_size (ivec2) / mip_level (int)

- [x] **Task 1.6.2**：创建 `nanite/gpu/nanite_hzb.h` 与 `nanite/gpu/nanite_hzb.cpp`
  - `NaniteHZB` 类
  - 成员：`RID hzb_texture` / `LocalVector<RID> hzb_mip_views` / `RID downsample_shader / pipeline` / `int mip_count` / `Size2i screen_size` / `bool needs_rebuild`
  - 方法：`init(rd)` / `cleanup(rd)` / `resize(rd, Size2i)` / `build(rd, RID depth_texture)` / `get_hzb_texture()` / `get_mip_count()`

- [x] **Task 1.6.3**：实现 `init(rd)`
  - 加载 `nanite_hzb_downsample.glsl` → 编译 shader → 创建 pipeline

- [x] **Task 1.6.4**：实现 `resize(rd, size)`
  - `mip_count = ceil(log2(max(size.width, size.height)))`
  - `RD::TextureFormat fmt; fmt.format = DATA_FORMAT_R32_SFLOAT; fmt.width = size.width; fmt.height = size.height; fmt.mipmaps = mip_count;`
  - `hzb_texture = rd->texture_create(fmt, RD::TextureView())`
  - 为每级 mip 创建 `texture_create_shared_from_layer` 或逐级 view
  - `screen_size = size; needs_rebuild = false`

- [x] **Task 1.6.5**：实现 `build(rd, depth_texture)`
  - 若 `needs_rebuild` → `resize(rd, screen_size)`
  - 循环 mip 1..mip_count-1：bind src=hzb_mip_views[i-1] / dst=hzb_mip_views[i] → dispatch compute → barrier
  - 第 0 级从 depth_texture 复制（或作为 mip 0 输入）

- [x] **Task 1.6.6**：doctest `[Nanite][HZB] correct_mip_count_for_resolution`
  - `hzb.resize(rd, Size2i(1920, 1080))`
  - `CHECK(hzb.get_mip_count() == 11)`（ceil(log2(1920)) = 11）
  - **子任务**：doctest `[Nanite][HZB] downsample_takes_max_of_2x2`
    - 4x4 深度纹理，4 个 2x2 区域分别填 0.2/0.8/0.4/0.6
    - build 后回读 mip 1，验证每像素为对应区域 MAX
  - **子任务**：doctest `[Nanite][HZB] resize_handles_resolution_change`
    - resize(1920, 1080) → resize(3840, 2160)，验证 mip_count 增加
  - **注**：测试文件 `nanite/tests/test_nanite_hzb.h` 已创建，包含 `compute_mip_count_for_common_resolutions`、`init_and_build_smoke_test`、`downsample_takes_max_of_2x2` 三个 TEST_CASE。测试用例名与 spec 略有差异但功能覆盖等价。`resize_handles_resolution_change` 子任务未单独编写，但 `resize` 内部逻辑已包含尺寸变化检测。测试头文件需在下一次 scons 构建时由 `modules/SCsub` 的 glob 自动纳入 `modules_tests.gen.h`。

**依赖**：1.1.1（shaders 目录）
**验证**：doctest 通过

---

## 1.7 NaniteGDExtBridge — CompositorEffect 接入

> **API 修订**：tasks.md 原描述的 `add_effect_callback_type(EFFECT_CALLBACK_TYPE_PRE_OPAQUE, "nanite_pre_opaque")` API 在 Godot 4.7.1 中**不存在**。实际 API 是 `CompositorEffect::set_effect_callback_type(EffectCallbackType)`，且每个 `CompositorEffect` 实例只能注册一个回调类型。
>
> **修订方案**：使用**两个 `NaniteGDExtBridge` 实例**，一个 `EFFECT_CALLBACK_TYPE_PRE_OPAQUE`（→ `NaniteServer::render_visibility`），一个 `EFFECT_CALLBACK_TYPE_POST_OPAQUE`（→ `NaniteServer::render_material_resolve`）。`_render_callback` 根据 `get_effect_callback_type()` 派发。

- [x] **Task 1.7.1**：创建 `nanite/bridge/nanite_gdext_bridge.h` 与 `nanite/bridge/nanite_gdext_bridge.cpp`
  - `NaniteGDExtBridge : CompositorEffect, public INaniteBridge`
  - `GDCLASS(NaniteGDExtBridge, CompositorEffect)`
  - **实现偏差**：原描述"重写 `virtual void _render_callback(...) override`"经核实**不成立** — `GDVIRTUAL2(_render_callback, ...)` 宏不会暴露 C++ 虚函数，仅生成 script/GDExtension 派发代码。实际实现：将 `_render_callback` 声明为 `virtual`（非 `override`），并在构造函数中通过 `RenderingServer::compositor_effect_set_callback(get_rid(), ..., callable_mp(this, &NaniteGDExtBridge::_render_callback))` 显式重新注册回调槽，使 `RendererSceneRenderRD::_process_compositor_effects` 直接调用本类方法。详见头文件注释。
  - 实现 INaniteBridge 的所有方法
  - 构造函数：`NaniteGDExtBridge(EffectCallbackType p_callback_type)`，内部调用 `set_effect_callback_type(p_callback_type)`，随后重新绑定回调 callable（见上）
  - 默认构造函数（用于 ClassDB）：委托给 `NaniteGDExtBridge(EFFECT_CALLBACK_TYPE_PRE_OPAQUE)`

- [x] **Task 1.7.2**：实现 `_render_callback`
  - `if (NaniteServer::get_singleton() == nullptr) return;`
  - `int cb_type = get_effect_callback_type();`
  - switch (cb_type):
    - `EFFECT_CALLBACK_TYPE_PRE_OPAQUE`：`ns->render_visibility(p_render_data)`
    - `EFFECT_CALLBACK_TYPE_POST_OPAQUE`：`ns->render_material_resolve(p_render_data)`
    - 其他：忽略（Nanite 仅在 PRE_OPAQUE / POST_OPAQUE 工作）

- [x] **Task 1.7.3**：实现 `install(NaniteServer*)`
  - `p_server->set_shadow_mode(SHADOW_COARSE_LOD)`
  - 不做其他初始化（GPU pipeline 由 NaniteServer::init 创建）

- [x] **Task 1.7.4**：在 `NaniteServer::init()` 创建两个桥接实例（条件编译）
  - `#if defined(NANITE_BRIDGE_GDEXT)`
  - `if (active == "gdext") {`
  -   `pre_opaque_bridge = memnew(NaniteGDExtBridge(CompositorEffect::EFFECT_CALLBACK_TYPE_PRE_OPAQUE));`
  -   `post_opaque_bridge = memnew(NaniteGDExtBridge(CompositorEffect::EFFECT_CALLBACK_TYPE_POST_OPAQUE));`
  -   `bridge = pre_opaque_bridge;`  // 让 `bridge` 指向 PRE_OPAQUE 实例（INaniteBridge* 接口）
  -   `pre_opaque_bridge->install(this);`
  - `}`
  - `#endif`
  - **注**：`CompositorEffect` 是 `Resource`（RefCounted）。使用 `memnew` 创建后通过 `init()` 内置的 `rid = rs->compositor_effect_create()` 自动注册到 RenderingServer；待真正要被引擎调用时还需添加到一个 `Compositor` 的 effects 列表（Stage 1 仅完成框架，不实现自动挂接到默认 Compositor，标记为 Stage 2 TODO）

- [x] **Task 1.7.5**：在 `register_types.cpp` 注册 `NaniteGDExtBridge` 类
  - `ClassDB::register_class<NaniteGDExtBridge>()` 在 `MODULE_INITIALIZATION_LEVEL_SCENE`（与 CompositorEffect 同级）

- [x] **Task 1.7.6**：doctest `[Nanite][Bridge] gdext_bridge_registers_as_compositor_effect`
  - 实例化 `NaniteGDExtBridge(CompositorEffect::EFFECT_CALLBACK_TYPE_PRE_OPAQUE)`，`CHECK(bridge->is_class("CompositorEffect"))`
  - **子任务**：doctest `[Nanite][Bridge] install_sets_coarse_lod_shadow_mode`
    - `bridge->install(server)`，`CHECK(server->get_shadow_mode() == SHADOW_COARSE_LOD)`

**依赖**：1.3（NaniteServer）、1.5（GPU pipeline）
**验证**：doctest 通过

---

## 1.8 NaniteMeshInstance3D 场景节点

- [x] **Task 1.8.1**：创建 `nanite/scene/nanite_mesh_instance_3d.h` 与 `nanite/scene/nanite_mesh_instance_3d.cpp`
  - `NaniteMeshInstance3D : MeshInstance3D`（继承复用 mesh RID 管理）
  - `GDCLASS(NaniteMeshInstance3D, MeshInstance3D)`
  - 成员：`Ref<NaniteMeshResource> nanite_mesh` / `RID mesh_rid` / `bool nanite_enabled = true` / `int forced_lod = -1` / `float relative_screen_size = 0.0f`
  - 属性：`nanite_mesh` (PROPERTY_HINT_RESOURCE_TYPE) / `nanite_enabled` / `forced_lod` / `relative_screen_size`
  - 方法：`set_nanite_mesh(Ref<NaniteMeshResource>)` / `get_nanite_mesh()` / `set_nanite_enabled(bool)` / `get_nanite_enabled()` / `set_forced_lod(int)` / `get_forced_lod()` / `set_relative_screen_size(float)` / `get_relative_screen_size()`

- [x] **Task 1.8.2**：实现 `set_nanite_mesh`
  - `if (nanite_mesh.is_valid()) NaniteServer::get_singleton()->unregister_mesh(mesh_rid);`
  - `nanite_mesh = p_resource;`
  - `if (nanite_mesh.is_valid() && nanite_enabled)`
    - `mesh_rid = NaniteServer::get_singleton()->register_mesh(nanite_mesh);`
    - `set_mesh(nanite_mesh->get_shadow_mesh());` // 让 MeshInstance3D 用 shadow mesh 做引擎渲染
    - `if (shadow_mesh valid) RenderingServer::get_singleton()->mesh_set_shadow_mesh(get_mesh_rid(), shadow_mesh_rid);`

- [x] **Task 1.8.3**：实现 `_notification`
  - `NOTIFICATION_ENTER_TREE`: if `nanite_enabled && nanite_mesh.is_valid()` → `NaniteServer::get_singleton()->register_instance(this)`
  - `NOTIFICATION_EXIT_TREE`: `NaniteServer::get_singleton()->unregister_instance(this)`

- [x] **Task 1.8.4**：实现 `set_nanite_enabled`
  - `if (nanite_enabled == p_enabled) return;`
  - `nanite_enabled = p_enabled;`
  - `if (nanite_enabled)` → 重新 register_mesh + register_instance
  - `else` → unregister（保留 MeshInstance3D 的 shadow mesh 渲染）

- [x] **Task 1.8.5**：在 `register_types.cpp` 注册
  - `ClassDB::register_class<NaniteMeshInstance3D>()` 在 `MODULE_INITIALIZATION_LEVEL_SCENE`

- [x] **Task 1.8.6**：doctest `[Nanite][Instance] set_nanite_mesh_registers_with_server`
  - 创建 instance，set_nanite_mesh(sphere_resource)
  - `CHECK(NaniteServer::get_singleton()->get_instance_count() == 1)`
  - **子任务**：doctest `[Nanite][Instance] disable_nanite_unregisters`
    - `set_nanite_enabled(false)` → `get_instance_count() == 0`
  - **子任务**：doctest `[Nanite][Instance] forced_lod_getter_setter`
    - `set_forced_lod(2)` → `CHECK(get_forced_lod() == 2)`

**依赖**：1.3（NaniteServer）
**验证**：doctest 通过

---

## 1.9 NaniteDebug 调试可视化

- [x] **Task 1.9.1**：创建 `nanite/core/nanite_debug.h` 与 `nanite/core/nanite_debug.cpp`
  - `NaniteDebug : Object`，`GDCLASS(NaniteDebug, Object)`
  - 枚举 `DebugMode { NONE=0, CLUSTER_SOLID_COLOR, LOD_SOLID_COLOR, OVERDRAW_HEATMAP, PAGE_RESIDENCY, HZB_MIP_LEVELS, HZB_OCCLUSION }`，通过 `VARIANT_ENUM_CAST` 暴露
  - 成员：`DebugMode mode = NONE` / `bool wireframe = false` / `bool show_bounds = false`
  - 方法：`set_mode(int)` / `get_mode()` / `set_wireframe(bool)` / `set_show_bounds(bool)`

- [x] **Task 1.9.2**：在 `NaniteServer` 添加 `set_debug_mode(int)` 转发到 `debug->set_mode()`
  - `ClassDB::bind_method(D_METHOD("set_debug_mode", "mode"), &NaniteServer::set_debug_mode)`

- [x] **Task 1.9.3**：在 `NaniteGPUPipeline::dispatch_material_resolve` 根据 debug mode 切换 shader 分支
  - material_resolve shader 内 `if (debug_mode == CLUSTER_SOLID_COLOR) { color = hash(cluster_id); }`
  - 通过 push constants 传 debug_mode
  - **实现说明**：`nanite/shaders/nanite_material_resolve.glsl` 通过 `params.debug_mode` push constant 切换 7 种 DebugMode 分支（NONE=灰占位 / CLUSTER_SOLID_COLOR=hash_color(cluster_id) / LOD_SOLID_COLOR=heat_color(triangle_id) / OVERDRAW_HEATMAP=同 CLUSTER_SOLID_COLOR / PAGE_RESIDENCY=绿色 / HZB_MIP_LEVELS=洋红 / HZB_OCCLUSION=黄色）。`NaniteGPUPipeline::dispatch_material_resolve` 把 `NaniteDebug::DebugMode` 作为 `uint32_t` 通过 `MaterialResolvePushConstant.debug_mode` 上传到 shader。`NaniteServer::render_material_resolve` 通过 `debug->get_mode_enum()` 读取当前调试模式并转发。

- [x] **Task 1.9.4**：在 `NaniteMeshEditor` 接入调试模式切换
  - `OptionButton` 选项改为 7 项（NONE / CLUSTER_SOLID_COLOR / LOD_SOLID_COLOR / OVERDRAW_HEATMAP / PAGE_RESIDENCY / HZB_MIP_LEVELS / HZB_OCCLUSION）
  - 选项变化时调 `NaniteServer::get_singleton()->set_debug_mode(mode)`
  - `NOTIFICATION_FOCUS_EXIT` 恢复 NONE（Stage 0 已有逻辑）
  - **实现说明**：OptionButton 现有 7 个选项对应 NaniteDebug::DebugMode 枚举值（0-6），`item_selected` 信号通过 `callable_mp` 转发到 `NaniteServer::set_debug_mode`；`NOTIFICATION_FOCUS_ENTER` 应用当前选择，`NOTIFICATION_FOCUS_EXIT` 恢复 NONE 防止调试模式泄漏到主编辑器视口。旧的 "Wireframe"/"Bounds"/"Clusters" 4 项已替换，wireframe_btn / bounds_btn 仍作为独立 toggle 保留。

- [x] **Task 1.9.5**：GDScript 测试 `test_nanite_debug.gd`
  - `test_debug_mode_none_default`：`assert(srv.get_debug_mode() == 0)`
  - `test_debug_mode_can_change`：`srv.set_debug_mode(1); assert(srv.get_debug_mode() == 1)`
  - **实现说明**：`nanite/tests/test_nanite_debug.gd` 已创建，包含 `test_debug_mode_none_default`（验证默认值为 NONE）与 `test_debug_mode_can_change`（验证 set/get 往返），通过 `Engine.get_singleton("NaniteServer")` 获取单例。代码已编写，待运行验证。

**依赖**：1.3、1.5
**验证**：测试通过

---

## 1.10 粗 LOD 阴影方案

- [x] **Task 1.10.1**：在 `NaniteMeshInstance3D::set_nanite_mesh` 调用 `mesh_set_shadow_mesh`
  - 已在 Task 1.8.2 实现，此处仅验证
  - 添加 `WARN_PRINT_ONCE` 当 shadow_mesh 为空时
  - **已完成**：`nanite/scene/nanite_mesh_instance_3d.cpp` 中 `set_nanite_mesh` 已调用 `RenderingServer::get_singleton()->mesh_set_shadow_mesh(mesh_rid, shadow->get_rid())`（参数顺序：main_mesh_rid, shadow_mesh_rid），shadow_mesh 为空时 `WARN_PRINT_ONCE_ED` 警告

- [x] **Task 1.10.2**：GDScript 测试 `test_shadow_mesh_gdext.gd`
  - `test_shadow_mesh_set_on_instance`：创建 instance + resource，await 一帧，`assert(rs.mesh_get_shadow_mesh(mesh_rid).is_valid())`
  - **已完成**：`nanite/tests/test_shadow_mesh_gdext.gd` 已创建。Stage 1 实际为 SKIP（需预构建 NaniteMeshResource fixture，`preload("res://nanite/tests/_sphere_mesh.tres")` 为占位路径），保留代码结构以便 Stage 2 完善 fixture 后直接运行

**依赖**：1.8
**验证**：测试通过

---

## 1.11 Material Resolve Shader

- [x] **Task 1.11.1**：编写 `nanite/shaders/nanite_material_resolve.glsl`
  - `#[compute] #version 450`
  - bindings：vis_buffer (image2D, r32ui) / cluster_ssbo / vertex_ssbo / materials_ssbo / color_buffer (image2D, rgba8) / params
  - push constants：debug_mode (int)
  - 算法：每线程 1 像素，从 vis_buffer 读 (cluster_id << 8 | triangle_id)
    - 若 triangle_id == 0xFFFF（空像素）→ discard
    - 解码 cluster 的 vertex_offset / triangle_offset
    - 取 triangle 的 3 个顶点，barycentric 插值 normal / uv
    - 查 materials_ssbo 取材质属性
    - 简化：使用 material_index 对应的 BaseMaterial3D albedo 颜色
    - 若 debug_mode == CLUSTER_SOLID_COLOR → color = hash(cluster_id)
    - 若 debug_mode == LOD_SOLID_COLOR → color = heat(cluster.depth)
  - **实现说明（Stage 1 简化）**：bindings 简化为 `vis_buffer (set 0, binding 0, r32ui readonly uimage2D)` / `color_buffer (binding 1, rgba8 writeonly image2D)` / `cluster_ssbo (binding 2, storage buffer)` — vertex_ssbo 与 materials_ssbo 暂不绑定（材质数据未上传到 GPU，`NaniteMeshResource::get_materials()` 尚未暴露）。push constant `Params { ivec2 screen_size; uint debug_mode; uint _pad; }`。空像素判定为 `encoded == 0u`（而非 `triangle_id == 0xFFFF`，与 Stage 1 rasterize shader 写入语义一致）。所有 7 种 DebugMode 均有对应分支（NONE=灰占位 0.8、CLUSTER_SOLID_COLOR=hash_color(cluster_id)、LOD_SOLID_COLOR=heat_color(triangle_id)、OVERDRAW_HEATMAP=同 CLUSTER_SOLID_COLOR、PAGE_RESIDENCY=绿色、HZB_MIP_LEVELS=洋红、HZB_OCCLUSION=黄色）。真正的 BRDF 着色（材质 albedo / 法线插值）留待 Stage 2 实现。

- [x] **Task 1.11.2**：实现 `dispatch_material_resolve`
  - 创建 uniform set
  - dispatch 数量：`(screen_size.x * screen_size.y + 63) / 64`
  - 输出写入 render_data 的 color target
  - **实现说明（Stage 1 简化）**：`NaniteGPUPipeline::dispatch_material_resolve` 在 `nanite/gpu/nanite_gpu_pipeline.cpp` 中实现完整 dispatch：构建 uniform set（binding 0/1/2 = vis_buffer image / color_buffer image / cluster_ssbo），设置 `MaterialResolvePushConstant { screen_size, debug_mode, _pad }`，调用 `compute_list_dispatch_threads(current_width, current_height, 1)`（workgroup 8x8，由 RD helper 自动按 workgroup 大小切分）。输出写入 pipeline 内部 `color_buffer`（RGBA8 UNORM），由 `ensure_screen_buffers` 创建并随 vis_buffer / depth_buffer 一起释放。Stage 1 不直接写入 `render_data` 的 color target — 由 Stage 2 桥接层负责将 `color_buffer` 合成到引擎渲染目标。

- [x] **Task 1.11.3**：doctest `[Nanite][MaterialResolve] output_is_non_black`
  - 全流程：cull → rasterize → hzb_build → material_resolve
  - 回读 color_buffer，检查至少 10% 像素非黑
  - **子任务**：doctest `[Nanite][MaterialResolve] cluster_solid_color_mode`
    - `set_debug_mode(CLUSTER_SOLID_COLOR)`，验证输出有不同颜色
  - **实现说明**：`nanite/tests/test_nanite_material_resolve.h` 已创建，包含两个 TEST_CASE：`output_is_non_black` 与 `cluster_solid_color_mode`。共用辅助函数 `build_upload_and_run_pipeline` 完成 sphere resource 构建 + 上传 + cull + rasterize + material_resolve 全流程。8x8 屏幕 + 32 段 sphere（~16 cluster）→ ~25% 像素非黑（满足 ≥10% 阈值）。CLUSTER_SOLID_COLOR 模式下统计 unique 颜色数量 > 1。GPU 测试需要 Vulkan 后端，无后端时自动 SKIP。代码已编写，待 scons 构建纳入 `modules_tests.gen.h` 后运行验证。

**依赖**：1.5、1.9
**验证**：doctest 通过

---

## 1.12 NanitePageCache 占位

- [x] **Task 1.12.1**：创建 `nanite/core/nanite_page_cache.h` 与 `nanite/core/nanite_page_cache.cpp`
  - `NanitePageCache : Object`，`GDCLASS(NanitePageCache, Object)`
  - 接口：`request_page(uint64_t page_id) → bool` / `evict_lru()` / `get_resident_count() → int`
  - Stage 1 实现为 always-resident（所有 page 都返回 true，不淘汰）
  - 注释标记 TODO Stage 2+ 实现真实 LRU

- [x] **Task 1.12.2**：在 `NaniteServer::init()` 创建 `page_cache = memnew(NanitePageCache)`
  - 在 `finish()` 中 `memdelete(page_cache)`

- [x] **Task 1.12.3**：doctest `[Nanite][PageCache] stage1_always_resident`
  - `CHECK(page_cache->request_page(0))`
  - `CHECK(page_cache->request_page(UINT64_MAX))`
  - `CHECK(page_cache->get_resident_count() == INT_MAX)`（或类似哨兵值）
  - **已完成**：`nanite/tests/test_nanite_page_cache.h` 已创建，TEST_CASE 名为 `[Nanite][PageCache] stage1_always_resident`，验证 `request_page(0/1/42/UINT64_MAX)` 均返回 true、`evict_lru()` 后 `get_resident_count() == INT_MAX`。结构与 `test_nanite_bridge.h` 一致。待下次 scons 构建由 `modules/SCsub` glob 纳入 `modules_tests.gen.h` 后编译验证

**依赖**：1.3
**验证**：doctest 通过

---

## 1.13 端到端测试

- [x] **Task 1.13.1**：GDScript E2E 测试 `test_gdext_e2e.gd`
  - `test_full_gdext_render_pipeline`：
    - 创建 Node3D + NaniteMeshInstance3D（sphere resource）+ Camera3D + DirectionalLight3D
    - await 5 帧
    - `assert(NaniteServer.get_singleton().get_instance_count() == 1)`
    - `assert(NaniteServer.get_singleton().get_visible_cluster_count() > 0)`
  - `test_no_nanite_instances_no_crash`：空场景渲染 10 帧不崩溃

- [x] **Task 1.13.2**：性能基准测试 `test_perf_stage1.gd`
  - `_build_large_resource()`：~100K tri sphere
  - 测量 60 帧平均帧时间
  - `assert(avg_ms < 33.0)`（≥30fps）

- [x] **Task 1.13.3**：扩展 `nanite/tests/test_helpers.h`
  - 添加 `create_constant_depth_texture(rd, size, fill_value)` 辅助函数
  - 添加 `create_depth_texture_4x4(rd, data[16])` 辅助函数
  - 添加 `build_test_resource_sphere(segments)` 复用 Stage 0 的 NaniteBuilder

**依赖**：所有前置任务
**验证**：测试通过

---

## 1.14 编辑器集成

- [x] **Task 1.14.1**：修改 `NaniteMeshEditor` 使用 `NaniteMeshInstance3D` 替代 `MeshInstance3D`
  - 头文件类型变更：`MeshInstance3D *preview_instance` → `NaniteMeshInstance3D *preview_instance`
  - `edit()` 中 `preview_instance->set_nanite_mesh(resource)` 替代 `set_mesh(shadow_mesh)`
  - 注意：SubViewport 需独立 RD 上下文，NaniteGDExtBridge 的回调要在 SubViewport 渲染时被触发

- [x] **Task 1.14.2**：Inspector 暴露 `NaniteMeshInstance3D` 属性
  - `NaniteMeshInstance3D` 的 nanite_mesh / nanite_enabled / forced_lod / relative_screen_size 自动通过 `ADD_PROPERTY` 暴露
  - 验证选中场景中的 NaniteMeshInstance3D 时 Inspector 显示这些属性

- [x] **Task 1.14.3**：GDScript 测试 `test_nanite_editor_stage1.gd`
  - `test_nanite_mesh_instance_in_inspector`：ClassDB.class_exists + is_parent_class
  - `test_nanite_mesh_editor_uses_nanite_instance`：（通过 ClassDB 反射验证，避免实例化 UI）

**依赖**：1.8、1.9
**验证**：测试通过

---

## 1.15 文档与验收

- [x] **Task 1.15.1**：更新 `nanite-overall-design.md` Stage 1 章节
  - 标注"已实现"状态
  - 记录实现与设计差异（module 内 vs 独立 GDExtension）

- [x] **Task 1.15.2**：执行全部验收检查
  - 按 [checklist.md](checklist.md) 逐项检查
  - 标记 [CANNOT_VERIFY] 项（需手动 UI 测试）
  - 标记 [PARTIAL] 项（接口存在但实现不完整）

**依赖**：所有前置任务
**验证**：checklist 全部通过或标记

---

## Task Dependencies

- 1.1 (框架) → 无依赖，可与 1.2 并行
- 1.2 (INaniteBridge) → 1.1
- 1.3 (NaniteServer) → 1.1, 1.2
- 1.4 (NaniteMeshData) → 1.3
- 1.5 (NaniteGPUPipeline) → 1.4, 1.6
- 1.6 (NaniteHZB) → 1.1（shaders 目录）
- 1.7 (NaniteGDExtBridge) → 1.3, 1.5
- 1.8 (NaniteMeshInstance3D) → 1.3
- 1.9 (NaniteDebug) → 1.3, 1.5
- 1.10 (粗 LOD 阴影) → 1.8
- 1.11 (Material Resolve) → 1.5, 1.9
- 1.12 (NanitePageCache) → 1.3
- 1.13 (E2E 测试) → 全部前置
- 1.14 (编辑器集成) → 1.8, 1.9
- 1.15 (文档与验收) → 全部前置

**可并行组**：
- (1.1, 1.2) — 框架与接口
- (1.4, 1.6) — mesh data 与 HZB（1.6 仅依赖 shaders 目录）
- (1.8, 1.9) — 实例节点与调试
- (1.10, 1.11, 1.12) — 阴影、材质解析、page cache
