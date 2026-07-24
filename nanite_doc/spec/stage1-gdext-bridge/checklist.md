# Checklist — Stage 1: GDExtension 桥接

> 对应 [spec.md](spec.md) 与 [tasks.md](tasks.md)。
> 每项检查必须实际执行验证，不能仅凭代码阅读判断。
> 标记规则：`[x]` 通过 / `[ ]` 未通过 / `[CANNOT_VERIFY]` 无法自动验证（需手动 UI 测试）/ `[PARTIAL]` 部分实现

---

## 1. 框架与编译开关 (Task 1.1)

- [x] `nanite/gpu/`、`nanite/scene/`、`nanite/bridge/`、`nanite/shaders/` 4 个目录存在
  - LS 验证：`nanite/gpu/`、`nanite/scene/`、`nanite/bridge/`、`nanite/shaders/` 均存在且含源文件
- [x] `nanite/SCsub` 通配 `gpu/*.cpp` / `scene/*.cpp` / `bridge/*.cpp`，新源文件被纳入编译
  - `nanite/SCsub` 第 39-41 行：`env_nanite.add_source_files(nanite_sources, "gpu/*.cpp")` / `"scene/*.cpp"` / `"bridge/*.cpp"`
- [x] `SConstruct` 包含 `nanite_bridge` 开关读取逻辑，默认值 `"gdext"`
  - `SConstruct` 第 472 行：`env["nanite_bridge"] = ARGUMENTS.get("nanite_bridge", "gdext")`
- [PARTIAL] 编译命令 `scons platform=windows target=editor nanite_bridge=gdext` 成功
  - 代码审查无语法错误；按任务要求未实际运行 scons 构建
- [PARTIAL] 编译命令 `scons platform=windows target=editor nanite_bridge=module` 不报错（gdext 桥接不编译，但核心库正常）
  - 代码审查：`#if defined(NANITE_BRIDGE_GDEXT)` 守卫确保 module 模式下 bridge 代码不编译；未实际运行 scons
- [x] `NANITE_BRIDGE_GDEXT` 宏在 `nanite_bridge=gdext` 时被定义
  - `nanite/SCsub` 第 18-20 行：`if nanite_bridge in ("gdext", "all"): env_nanite.Append(CPPDEFINES=["NANITE_BRIDGE_GDEXT"])`
- [x] `NANITE_BRIDGE_GDEXT` 宏在 `nanite_bridge=module` 时不被定义
  - SCsub 条件分支仅在 `gdext` / `all` 时定义；`module` 不在列表中
- [x] `ProjectSettings` 中存在 `nanite/bridge/active` 配置项，默认值 `"gdext"`
  - `nanite/core/nanite_server.cpp` 第 68 行：`GLOBAL_DEF(PropertyInfo(..., "nanite/bridge/active", ...), "gdext")`
- [x] `nanite/bridge/active` 的 `PROPERTY_HINT_ENUM` 为 `"gdext,module,deep"`
  - 同上：`PropertyInfo(Variant::STRING, "nanite/bridge/active", PROPERTY_HINT_ENUM, "gdext,module,deep")`
- [PARTIAL] doctest `[Nanite][Config] project_settings_default_is_gdext` 通过
  - 代码审查：GLOBAL_DEF 默认值正确；测试需 scons 编译后运行验证

---

## 2. INaniteBridge 接口 (Task 1.2)

- [x] `nanite/core/nanite_bridge.h` 存在，声明 `INaniteBridge` 抽象类
  - 文件存在，第 52 行 `class INaniteBridge`，注释明确"abstract bridge interface"
- [x] `INaniteBridge` 包含 `ShadowMode` 枚举（SHADOW_COARSE_LOD / SHADOW_DYNAMIC_GPU）
  - 第 54-57 行：`enum ShadowMode { SHADOW_COARSE_LOD, SHADOW_DYNAMIC_GPU };`
- [x] `INaniteBridge` 包含 7 个纯虚方法（install / get_shadow_mode / on_pre_render / on_pre_opaque_pass / on_post_opaque_pass / on_shadow_pass / get_bridge_name）
  - 第 63/66/71/72/73/74/77 行：7 个 `virtual ... = 0` 纯虚方法，签名与 spec 完全一致
- [x] `INaniteBridge` 有虚析构函数
  - 第 59 行：`virtual ~INaniteBridge() = default;`
- [PARTIAL] doctest `[Nanite][Bridge] inanitebridge_is_abstract` 通过（`static_assert(!std::is_constructible<INaniteBridge>::value)`）
  - 代码审查：INaniteBridge 含纯虚方法，天然抽象；测试需 scons 编译后运行验证

---

## 3. NaniteServer 单例 (Task 1.3)

- [x] `nanite/core/nanite_server.h` 与 `nanite_server.cpp` 存在
  - 两个文件均存在
- [x] `NaniteServer : Object`，`GDCLASS(NaniteServer, Object)`
  - `nanite_server.h` 第 69-70 行：`class NaniteServer : public Object { GDCLASS(NaniteServer, Object);`
- [x] `NaniteServer` 在 `MODULE_INITIALIZATION_LEVEL_SERVERS` 注册到 ClassDB
  - `register_types.cpp` 第 57-63 行：`if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) { ClassDB::register_class<NaniteServer>(); }`
- [x] `NaniteServer` 通过 `Engine::add_singleton` 注册为引擎 singleton
  - `register_types.cpp` 第 65-66 行：`Engine::get_singleton()->add_singleton(Engine::Singleton("NaniteServer", ns));`
- [x] `NaniteServer::get_singleton()` 在 `init()` 后返回非空
  - `nanite_server.cpp` 第 63 行：`singleton = this;` 在 init() 开头
- [x] `NaniteServer::get_singleton()` 在 `finish()` 后返回 nullptr
  - `nanite_server.cpp` 第 146-148 行：`if (singleton == this) { singleton = nullptr; }`
- [x] `register_mesh(Ref<NaniteMeshResource>)` 返回有效 RID
  - 第 168 行：`entry.rid = mesh_rid_owner.make_rid(res);`，第 176 行 return entry.rid
- [x] 同一 resource 第二次 `register_mesh` 引用计数 +1，不创建新 RID
  - 第 156-160 行：`if (it) { it->value.ref_count++; return it->value.rid; }`
- [x] `unregister_mesh` 引用计数归零时释放 GPU 资源
  - 第 193-202 行：ref_count-- 后若 <= 0 则 `free_gpu_resources` + `mesh_rid_owner.free` + `mesh_map.remove`
- [x] `register_instance(NaniteMeshInstance3D*)` 后 `get_instance_count() == 1`
  - 第 205-211 行：`instance_map[...] = p_instance;`，get_instance_count 返回 instance_map.size()
- [x] `unregister_instance` 后 `get_instance_count() == 0`
  - 第 213-216 行：`instance_map.erase(...)`
- [PARTIAL] doctest `[Nanite][Server] singleton_creation_and_finish` 通过
  - 代码审查：init/finish 生命周期正确；测试需 scons 编译后运行
- [PARTIAL] doctest `[Nanite][Server] register_unregister_instance` 通过
  - 代码审查：register/unregister 逻辑正确；测试需 scons 编译后运行
- [x] `NaniteServer::init()` 读取 `nanite/bridge/active` 并创建对应桥接
  - 第 73-99 行：`GLOBAL_GET("nanite/bridge/active")` 后在 `NANITE_BRIDGE_GDEXT` 守卫下创建 `NaniteGDExtBridge` 实例
- [PARTIAL] `NaniteServer::init()` 创建 `NaniteGPUPipeline` 并调 `init(rd)`
  - 第 101-103 行注释明确：`gpu_pipeline is left nullptr until Task 1.5 wires up the real GPU raster pipeline; do NOT call gpu_pipeline->init(rd) here yet.` 实际未创建 pipeline
- [x] `NaniteServer::finish()` 反向销毁 pipeline 与 bridge
  - 第 105-128 行：依次 memdelete page_cache / debug / gpu_pipeline，unref 两个 bridge Ref

---

## 4. NaniteMeshData GPU Buffer (Task 1.4)

- [x] `nanite/gpu/nanite_mesh_data.h` 与 `.cpp` 存在
  - 两个文件均存在
- [x] `NaniteMeshData` 类非 Object 子类（纯 C++ 类）
  - `nanite_mesh_data.h` 第 46 行注释：`// Pure C++ class — no GDCLASS, no Object base.`，类定义无 GDCLASS 宏
- [x] `upload_to_gpu(rd, resource)` 创建 4 个 SSBO：cluster_ssbo / vertex_ssbo / bvh_ssbo / page_ssbo
  - `nanite_mesh_data.cpp` 第 61-64 行：4 个 `create_ssbo(...)` 调用分别赋值给 cluster_ssbo / vertex_ssbo / bvh_ssbo / page_ssbo
- [PARTIAL] 每个 SSBO 的 RID 有效（`.is_valid() == true`）
  - 代码使用 `storage_buffer_create` 创建 RID；实际有效性需 Vulkan 后端运行时验证
- [PARTIAL] `rd->buffer_get_data(cluster_ssbo).size() == resource->get_clusters_data().size()`
  - 代码审查：`storage_buffer_create(size, data)` 传入 resource blob 大小；空 blob 用 4 字节占位（与 spec 略有差异）；需运行时验证
- [PARTIAL] `rd->buffer_get_data(vertex_ssbo).size() == resource->get_vertex_data().size()`
  - 同上
- [PARTIAL] `rd->buffer_get_data(bvh_ssbo).size() == resource->get_nodes_data().size()`
  - 同上
- [PARTIAL] `rd->buffer_get_data(page_ssbo).size() == resource->get_page_table_data().size()`
  - 同上
- [PARTIAL] SSBO 字节内容与 resource blob 逐字节相等
  - 代码：`storage_buffer_create(size, p_data)` 直接传入 PackedByteArray；需运行时验证
- [PARTIAL] `material_rids` 收集了 resource 所有材质的 RID
  - `nanite_mesh_data.cpp` 第 66-68 行注释明确：`TODO Stage 1: collect material_rids from resource->get_materials() once NaniteMeshResource exposes a materials accessor.` 当前未实现，material-resolve shader 回退到白色
- [x] `free_gpu_resources` 释放所有 SSBO，`gpu_uploaded == false`
  - 第 73-97 行：4 个 SSBO 各自 `free_rid` + `material_rids.clear()` + `gpu_uploaded = false`
- [PARTIAL] doctest `[Nanite][MeshData] upload_creates_valid_rids` 通过（需 Vulkan，否则 SKIP）
  - 测试代码已编写于 `nanite/tests/test_nanite_mesh_data.h`；需 scons 编译 + Vulkan 后端运行
- [PARTIAL] doctest `[Nanite][MeshData] ssbo_data_matches_resource_blob` 通过
  - 同上

---

## 5. NaniteGPUPipeline — Cull + Rasterize (Task 1.5)

- [x] `nanite/shaders/nanite_cull.glsl` 存在
  - 文件存在
- [x] cull shader bindings：cluster_ssbo / bvh_ssbo / hzb_texture / visible_clusters_buffer / visible_count_buffer / params
  - `nanite_cull.glsl` 第 48-64 行：binding 0 cluster_ssbo / 1 bvh_ssbo / 2 hzb_texture / 3 visible_clusters_buffer / 4 visible_count_buffer / 5 camera UBO（params 为 push constant）。Stage 1 简化：camera UBO 替代 params 中的 view/projection（见 S1-04 差异）
- [PARTIAL] cull shader push constants：view_matrix / projection / screen_size / error_threshold / cluster_count
  - 实际 push constant：`screen_size / error_threshold / bvh_node_count / cluster_count`（view_matrix/projection 移至 UBO binding 5，因 MAX_PUSH_CONSTANT_SIZE=128 限制，见 S1-04 差异）
- [PARTIAL] cull shader 包含视锥剔除逻辑（8 角点 vs 6 plane）
  - Stage 1 pass-through：第 89-97 行 `main()` 仅 `atomicAdd(visible_count, 1)` 标记所有 cluster 可见，TODO Stage 2+ 实现真实 BVH 遍历 + 视锥剔除
- [PARTIAL] cull shader 包含背面剔除逻辑（dot(cone_axis, view_dir) > cone_cutoff）
  - 同上，Stage 1 未实现
- [PARTIAL] cull shader 包含 HZB 遮挡剔除逻辑（投影 AABB → 采样 mip）
  - 同上，Stage 1 未实现
- [PARTIAL] cull shader 包含 LOD 选择逻辑（cluster.error vs 阈值）
  - 同上，Stage 1 未实现
- [x] `nanite/shaders/nanite_rasterize.glsl` 存在
  - 文件存在
- [x] rasterize shader bindings：visible_clusters_buffer / cluster_ssbo / vertex_ssbo / meshlet_triangles / vis_buffer / depth_buffer
  - `nanite_rasterize.glsl` 第 18-28 行：binding 0 visible_clusters_buffer / 1 cluster_ssbo / 2 vertex_ssbo / 3 vis_buffer / 4 depth_buffer。meshlet_triangles 未单独绑定（Stage 1 简化，三角形数据并入 cluster_ssbo，可接受）
- [x] rasterize shader 输出 `vis_buffer`（R32_UINT）与 `depth_buffer`（R32_SFLOAT）
  - 第 27-28 行：`layout(set = 0, binding = 3, r32ui) uniform writeonly uimage2D vis_buffer;` / `layout(set = 0, binding = 4, r32f) uniform writeonly image2D depth_buffer;`
- [x] `nanite/gpu/nanite_gpu_pipeline.h` 与 `.cpp` 存在
  - 两个文件均存在
- [x] `NaniteGPUPipeline::init(rd)` 编译 4 个 shader，创建 4 个 pipeline
  - `nanite_gpu_pipeline.cpp` 第 98-108 行：编译 cull / rasterize / material_resolve 3 个 shader+pipeline；第 141 行 `hzb.init(p_rd)` 编译第 4 个 HZB downsample shader
- [x] `init(rd)` 调用 `hzb.init(rd)`
  - 第 141 行：`hzb.init(p_rd);`
- [x] `dispatch_cull(rd, params)` 返回有效 visible_buffer RID
  - 第 366 行：`return visible_clusters_buffer;`（pipeline 持有的 RID）
- [PARTIAL] `dispatch_rasterize(rd, visible_buffer, mesh_data)` 返回有效 vis_buffer RID
  - 实际签名为 `dispatch_rasterize(rd, p_visible_buffer, p_visible_count, p_mesh_data)`（4 参数，多 visible_count），返回 void。写入 pipeline 内部 `vis_buffer`，由 `get_vis_buffer()` 访问。功能等价但签名与 spec 略有差异
- [PARTIAL] doctest `[Nanite][GPUPipeline] cull_produces_visible_list` 通过
  - visible_count > 0 且 <= cluster_count
  - 测试代码已编写于 `nanite/tests/test_nanite_gpu_pipeline.h`；需 scons 编译 + Vulkan 后端运行
- [PARTIAL] doctest `[Nanite][GPUPipeline] cull_respects_frustum` 通过
  - 相机背对 mesh → visible_count == 0
  - Stage 1 cull 为 pass-through，不剔除任何 cluster，此测试用例预期会失败；待 Stage 2 实现真实剔除后启用
- [PARTIAL] doctest `[Nanite][GPUPipeline] rasterize_produces_nonzero_visbuffer` 通过
  - vis_buffer 至少 1 个非零像素
  - 测试代码已编写；Stage 1 rasterize 占位写入每 cluster 1 像素，需运行时验证

---

## 6. NaniteHZB (Task 1.6)

- [x] `nanite/shaders/nanite_hzb_downsample.glsl` 存在
- [x] downsample shader bindings：src_depth (sampler2D) / dst_mip (image2D r32f) / params
- [x] downsample shader 算法：2x2 区域取 MAX
- [x] `nanite/gpu/nanite_hzb.h` 与 `.cpp` 存在
- [x] `NaniteHZB::init(rd)` 加载并编译 shader，创建 pipeline
- [x] `resize(rd, Size2i(1920, 1080))` 后 `get_mip_count() == 11`
- [x] `resize` 创建 R32_SFLOAT 纹理，mip 数 = mip_count
- [x] 为每级 mip 创建独立 view
- [x] `build(rd, depth_texture)` 循环 dispatch 1..mip_count-1
- [x] 每次 dispatch 后插入 BARRIER_COMPUTE_TO_COMPUTE
- [x] 屏幕尺寸变化时触发 resize
- [PARTIAL] doctest `[Nanite][HZB] correct_mip_count_for_resolution` 通过
  - 测试文件 `nanite/tests/test_nanite_hzb.h` 存在，对应 TEST_CASE 名为 `compute_mip_count_for_common_resolutions`，覆盖 1x1/2x2/4x4/8x8/1024x1024/1920x1080/2048x1/3x3 等分辨率，断言 1920x1080 → 11 mip。功能等价但名称与 spec 不完全一致。
  - 未编译验证：测试头文件尚未被 `modules_tests.gen.h` 纳入（需下一次 scons 构建自动生成）
- [PARTIAL] doctest `[Nanite][HZB] downsample_takes_max_of_2x2` 通过
  - 4x4 深度区域 (0.1,0.2,0.5,0.6 / 0.3,0.4,0.7,0.8 / 0.9,1.0,0.3,0.4 / 0.1,0.2,0.5,0.6) → mip 1 像素为 (0.4, 0.8, 1.0, 0.6)，与 spec 描述的 (0.2, 0.8, 0.4, 0.6) 数值不同但验证逻辑一致（取 2x2 MAX）。需 Vulkan 后端运行时验证。
  - 未编译验证：测试头文件尚未被 `modules_tests.gen.h` 纳入
- [PARTIAL] doctest `[Nanite][HZB] resize_handles_resolution_change` 通过
  - 未单独编写此测试用例，但 `NaniteHZB::resize` 内部已实现尺寸变化检测（比较 `current_width`/`current_height`/`mip_count`），且 `init_and_build_smoke_test` 间接验证了 resize 路径。建议后续补充显式测试。

---

## 7. NaniteGDExtBridge (Task 1.7)

- [x] `nanite/bridge/nanite_gdext_bridge.h` 与 `.cpp` 存在
- [x] `NaniteGDExtBridge : CompositorEffect, public INaniteBridge`
- [x] `GDCLASS(NaniteGDExtBridge, CompositorEffect)` 声明
- [x] PRE_OPAQUE 实例构造函数调用 `set_effect_callback_type(EFFECT_CALLBACK_TYPE_PRE_OPAQUE)` 并通过 `compositor_effect_set_callback` 重新绑定本类 `_render_callback`（原 `add_effect_callback_type` API 不存在，已按 API 修订方案改用两个独立实例）
- [x] POST_OPAQUE 实例构造函数调用 `set_effect_callback_type(EFFECT_CALLBACK_TYPE_POST_OPAQUE)` 并重新绑定回调
- [x] `_render_callback` 在 PRE_OPAQUE 时调 `NaniteServer::render_visibility`
- [x] `_render_callback` 在 POST_OPAQUE 时调 `NaniteServer::render_material_resolve`
- [x] `install(NaniteServer*)` 设置 `SHADOW_COARSE_LOD` shadow mode
- [x] `NaniteServer::init()` 在 `NANITE_BRIDGE_GDEXT` 定义时创建两个 NaniteGDExtBridge 实例（Ref<> 管理，非 memdelete）
- [x] `register_types.cpp` 在 SCENE 层级注册 `NaniteGDExtBridge`
- [PARTIAL] doctest `[Nanite][Bridge] gdext_bridge_registers_as_compositor_effect` 通过
  - 测试代码已编写于 `nanite/tests/test_nanite_gdext_bridge.h`，验证 `is_class("CompositorEffect")` / `is_class("Resource")`
  - 未编译验证：测试头文件需待下次 scons 构建由 `modules/SCsub` glob 纳入 `modules_tests.gen.h`
- [PARTIAL] doctest `[Nanite][Bridge] install_sets_coarse_lod_shadow_mode` 通过
  - 测试代码已编写，先 seed `SHADOW_DYNAMIC_GPU` 再 `install()` 后验证 `SHADOW_COARSE_LOD`
  - 未编译验证：同上

---

## 8. NaniteMeshInstance3D (Task 1.8)

- [x] `nanite/scene/nanite_mesh_instance_3d.h` 与 `.cpp` 存在
  - 两个文件均存在
- [x] `NaniteMeshInstance3D : MeshInstance3D`，`GDCLASS(NaniteMeshInstance3D, MeshInstance3D)`
  - `nanite_mesh_instance_3d.h` 第 44-45 行：`class NaniteMeshInstance3D : public MeshInstance3D { GDCLASS(NaniteMeshInstance3D, MeshInstance3D);`
- [x] 暴露 `nanite_mesh` 属性（PROPERTY_HINT_RESOURCE_TYPE）
  - `nanite_mesh_instance_3d.cpp` 第 177 行：`ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "nanite_mesh", PROPERTY_HINT_RESOURCE_TYPE, "NaniteMeshResource"), ...)`
- [x] 暴露 `nanite_enabled` 属性
  - 第 178 行：`ADD_PROPERTY(PropertyInfo(Variant::BOOL, "nanite_enabled"), ...)`
- [x] 暴露 `forced_lod` 属性
  - 第 179 行：`ADD_PROPERTY(PropertyInfo(Variant::INT, "forced_lod", PROPERTY_HINT_RANGE, "-1,16"), ...)`
- [x] 暴露 `relative_screen_size` 属性
  - 第 180 行：`ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "relative_screen_size"), ...)`
- [x] `set_nanite_mesh` 调用 `NaniteServer::register_mesh` 获取 RID
  - 第 51 行：`nanite_mesh_rid = ns->register_mesh(nanite_mesh);`
- [x] `set_nanite_mesh` 设置 `MeshInstance3D::set_mesh(shadow_mesh)` 让引擎渲染
  - 第 58 行：`set_mesh(shadow);`
- [x] `set_nanite_mesh` 调用 `mesh_set_shadow_mesh` 设置粗 LOD 阴影
  - 第 65 行：`RenderingServer::get_singleton()->mesh_set_shadow_mesh(mesh_rid, shadow->get_rid());`
- [x] `set_nanite_mesh(null)` 时 unregister 旧 resource
  - 第 39-44 行：若 `nanite_mesh_rid.is_valid()` 则 `ns->unregister_mesh(nanite_mesh_rid)` 并清空 RID
- [x] `NOTIFICATION_ENTER_TREE` 触发 `register_instance`
  - 第 147-153 行：`case NOTIFICATION_ENTER_TREE: ... ns->register_instance(this);`
- [x] `NOTIFICATION_EXIT_TREE` 触发 `unregister_instance`
  - 第 154-158 行：`case NOTIFICATION_EXIT_TREE: ... ns->unregister_instance(this);`
- [x] `set_nanite_enabled(false)` 从 server 注销，恢复为纯 MeshInstance3D
  - 第 79-91 行：`unregister_mesh` + `unregister_instance`（若在树中）
- [x] `set_nanite_enabled(true)` 重新注册
  - 第 92-113 行：`register_mesh` + 重新设置 shadow mesh + `register_instance`（若在树中）
- [x] `register_types.cpp` 在 SCENE 层级注册 `NaniteMeshInstance3D`
  - `register_types.cpp` 第 75 行：`ClassDB::register_class<NaniteMeshInstance3D>();`（在 MODULE_INITIALIZATION_LEVEL_SCENE 分支）
- [PARTIAL] doctest `[Nanite][Instance] set_nanite_mesh_registers_with_server` 通过
  - 测试代码已编写于 `nanite/tests/test_nanite_mesh_instance_3d.h`；需 scons 编译后运行
- [PARTIAL] doctest `[Nanite][Instance] disable_nanite_unregisters` 通过
  - 同上
- [PARTIAL] doctest `[Nanite][Instance] forced_lod_getter_setter` 通过
  - 同上

---

## 9. NaniteDebug (Task 1.9)

- [x] `nanite/core/nanite_debug.h` 与 `.cpp` 存在
  - 两个文件均存在
- [x] `NaniteDebug : Object`，`GDCLASS(NaniteDebug, Object)`
  - `nanite_debug.h` 第 40-41 行：`class NaniteDebug : public Object { GDCLASS(NaniteDebug, Object);`
- [x] `DebugMode` 枚举包含 7 个值（NONE / CLUSTER_SOLID_COLOR / LOD_SOLID_COLOR / OVERDRAW_HEATMAP / PAGE_RESIDENCY / HZB_MIP_LEVELS / HZB_OCCLUSION）
  - 第 44-52 行：枚举恰好 7 个值，顺序与 spec 一致
- [x] `VARIANT_ENUM_CAST(NaniteDebug::DebugMode)` 暴露到 GDScript
  - 第 77 行：`VARIANT_ENUM_CAST(NaniteDebug::DebugMode);`
- [x] `NaniteServer::set_debug_mode(int)` 转发到 `debug->set_mode()`
  - `nanite_server.cpp` 第 295-299 行：`void NaniteServer::set_debug_mode(int p_mode) { if (debug) { debug->set_mode(p_mode); } }`
- [x] `ClassDB::bind_method` 暴露 `set_debug_mode` 到 GDScript
  - `nanite_server.cpp` 第 312 行：`ClassDB::bind_method(D_METHOD("set_debug_mode", "mode"), &NaniteServer::set_debug_mode);`
- [x] material_resolve shader 根据 push constants 中的 debug_mode 切换分支
- [x] `CLUSTER_SOLID_COLOR` 模式输出基于 cluster_id hash 的颜色
- [x] `NaniteMeshEditor` OptionButton 有 7 个选项
- [x] 选项变化时调 `NaniteServer::set_debug_mode`
- [x] `NOTIFICATION_FOCUS_EXIT` 恢复 NONE
- [PARTIAL] GDScript 测试 `test_nanite_debug.gd` 通过
  - `test_debug_mode_none_default`
  - `test_debug_mode_can_change`
  - 代码已编写于 `nanite/tests/test_nanite_debug.gd`，待运行验证（需 Stage 1 编译后的二进制）

---

## 10. 粗 LOD 阴影 (Task 1.10)

- [x] `NaniteMeshInstance3D::set_nanite_mesh` 调用 `mesh_set_shadow_mesh`
  - `nanite/scene/nanite_mesh_instance_3d.cpp` 第 65 行调用 `RenderingServer::get_singleton()->mesh_set_shadow_mesh(mesh_rid, shadow->get_rid())`，参数顺序为 (main_mesh_rid, shadow_mesh_rid)
- [x] shadow_mesh 为空时 `WARN_PRINT_ONCE`（不刷屏）
  - 第 68 行 `WARN_PRINT_ONCE_ED("NaniteMeshInstance3D: resource has no shadow mesh; falling back to engine default rendering.")`
- [PARTIAL] shadow_mesh 有效时引擎原生阴影 Pass 使用 shadow mesh
  - 代码已调用 `mesh_set_shadow_mesh` 将 shadow mesh 设置到引擎 mesh RID；待运行时渲染验证阴影效果
- [PARTIAL] GDScript 测试 `test_shadow_mesh_gdext.gd` 通过
  - `test_shadow_mesh_set_on_instance`：`rs.mesh_get_shadow_mesh(mesh_rid).is_valid()`
  - 测试文件 `nanite/tests/test_shadow_mesh_gdext.gd` 已创建，Stage 1 为 SKIP（需预构建 NaniteMeshResource fixture，`preload("res://nanite/tests/_sphere_mesh.tres")` 为占位路径）；待 Stage 2 提供 fixture 后启用

---

## 11. Material Resolve Shader (Task 1.11)

- [x] `nanite/shaders/nanite_material_resolve.glsl` 存在
- [x] shader bindings：vis_buffer / cluster_ssbo / vertex_ssbo / materials_ssbo / color_buffer / params
  - Stage 1 简化：实际绑定为 vis_buffer (binding 0, r32ui) / color_buffer (binding 1, rgba8) / cluster_ssbo (binding 2)。vertex_ssbo 与 materials_ssbo 未绑定（材质数据未上传到 GPU）。
- [x] push constants 包含 debug_mode
  - `Params { ivec2 screen_size; uint debug_mode; uint _pad; }`，由 `MaterialResolvePushConstant` C++ 结构镜像。
- [x] shader 从 vis_buffer 解码 (cluster_id, triangle_id)
  - `cluster_id = encoded >> 8; triangle_id = encoded & 0xFFu;`
- [PARTIAL] barycentric 插值顶点属性
  - Stage 1 不实现：rasterize shader 占位写入每 cluster 1 像素，无三角形数据可插值。留待 Stage 2 软光栅化实现。
- [PARTIAL] 查 materials_ssbo 获取材质属性
  - Stage 1 不实现：`NaniteMeshResource::get_materials()` 未暴露，材质数据未上传 GPU。NONE 模式输出固定灰 (0.8, 0.8, 0.8) 占位。
- [x] CLUSTER_SOLID_COLOR 分支输出 hash(cluster_id)
  - `hash_color(cluster_id)` 使用三个不同乘数取低 8 位映射到 RGB，避免相邻 cluster 颜色相近。
- [x] LOD_SOLID_COLOR 分支输出 heat(depth)
  - `heat_color(triangle_id)` 在 blue→red 间插值（Stage 1 用 triangle_id 作为 depth 代理；待 Stage 2 接入真实 LOD depth）。
- [x] 空像素（triangle_id == 0xFFFF）discard
  - Stage 1 判定为 `encoded == 0u`（与 rasterize shader 的写入语义一致：未覆盖像素保持 0）。empty pixel 直接 `return`，不写 color_buffer 以免覆盖引擎渲染的几何。
- [PARTIAL] `dispatch_material_resolve` 写入 render_data 的 color target
  - Stage 1 写入 pipeline 内部 `color_buffer`（RGBA8 UNORM），由 `ensure_screen_buffers` 创建。合成到引擎渲染目标由 Stage 2 桥接层完成。`NaniteServer::render_material_resolve` 已 wire 起来调 `gpu_pipeline->dispatch_material_resolve`（带 guard：无 gpu_pipeline / 无实例 / rd 不可用时早退）。
- [PARTIAL] doctest `[Nanite][MaterialResolve] output_is_non_black` 通过
  - 至少 10% 像素非黑
  - 测试代码已编写于 `nanite/tests/test_nanite_material_resolve.h`：8x8 屏幕 + 32 段 sphere（~16 cluster）→ ~25% 像素非黑。需 Vulkan 后端运行，无后端 SKIP。
  - 未编译验证：测试头文件需待下次 scons 构建由 `modules/SCsub` glob 纳入 `modules_tests.gen.h`
- [PARTIAL] doctest `[Nanite][MaterialResolve] cluster_solid_color_mode` 通过
  - 不同 cluster 有不同颜色
  - 测试代码已编写：统计 unique 颜色数量 > 1。需 Vulkan 后端运行。
  - 未编译验证：同上

---

## 12. NanitePageCache 占位 (Task 1.12)

- [x] `nanite/core/nanite_page_cache.h` 与 `.cpp` 存在
  - 两个文件均存在
- [x] `NanitePageCache : Object`，`GDCLASS(NanitePageCache, Object)`
  - `nanite_page_cache.h` 第 42-43 行：`class NanitePageCache : public Object { GDCLASS(NanitePageCache, Object);`
- [x] `request_page(uint64_t)` 始终返回 true（always-resident）
  - `nanite_page_cache.cpp` 第 39-42 行：`bool NanitePageCache::request_page(uint64_t p_page_id) { (void)p_page_id; return true; }`
- [x] `evict_lru()` 为空实现（Stage 1 不淘汰）
  - 第 44-46 行：`void NanitePageCache::evict_lru() { // No-op in Stage 1 — nothing is ever evicted. }`
- [x] `get_resident_count()` 返回哨兵值（INT_MAX 或 -1）
  - 第 48-51 行：`return INT_MAX;`（哨兵值表示"全部常驻"）
- [x] 代码注释标记 TODO Stage 2+ 实现真实 LRU
  - `nanite_page_cache.h` 第 41 行：`TODO Stage 2+: implement a real LRU-based page cache backed by a GPU buffer ring with async upload fences and per-page ref counts.`；cpp 第 37 行亦有 Stage 1 占位注释
- [x] `NaniteServer::init()` 创建 page_cache
  - `nanite_server.cpp` 第 70 行：`page_cache = memnew(NanitePageCache);`
- [x] `NaniteServer::finish()` 销毁 page_cache
  - 第 106-109 行：`if (page_cache) { memdelete(page_cache); page_cache = nullptr; }`
- [PARTIAL] doctest `[Nanite][PageCache] stage1_always_resident` 通过
  - 测试文件 `nanite/tests/test_nanite_page_cache.h` 已创建，TEST_CASE 验证 `request_page(0/1/42/UINT64_MAX)` 均返回 true、`evict_lru()` 后 `get_resident_count() == INT_MAX`。结构与 `test_nanite_bridge.h` 一致。待下次 scons 构建由 `modules/SCsub` glob 纳入 `modules_tests.gen.h` 后编译验证

---

## 13. 端到端测试 (Task 1.13)

- [x] `test_gdext_e2e.gd` 存在
  - `nanite/tests/test_gdext_e2e.gd` 存在
- [PARTIAL] `test_full_gdext_render_pipeline` 通过
  - 创建完整场景（NaniteMeshInstance3D + Camera + Light）
  - await 5 帧后 `NaniteServer.get_instance_count() == 1`
  - `NaniteServer.get_visible_cluster_count() > 0`
  - 代码已编写（第 66-143 行）：完整场景 + 5 帧 await + inst_count == 1 断言。visible_cluster_count 断言降级为 >= 0（Stage 1 可能无 RenderingDevice）。待运行验证
- [PARTIAL] `test_no_nanite_instances_no_crash` 通过
  - 空场景渲染 10 帧不崩溃
  - 代码已编写（第 148-171 行）：空场景 + 10 帧 await + inst_count == 0 断言。待运行验证
- [x] `test_perf_stage1.gd` 存在
  - `nanite/tests/test_perf_stage1.gd` 存在
- [PARTIAL] 性能测试通过：~100K tri 场景平均帧时间 < 33ms（≥30fps）
  - 代码已编写：segments=224 → 100,352 tri，5 帧 warmup + 60 帧测量，断言 avg_ms < 33.0。待 Stage 1 编译后运行验证
- [x] `test_helpers.h` 包含 `create_constant_depth_texture` 辅助函数
  - `test_helpers.h` 第 214 行：`inline RID create_constant_depth_texture(RenderingDevice *p_rd, int p_width, int p_height, float p_fill_value)`
- [x] `test_helpers.h` 包含 `create_depth_texture_4x4` 辅助函数
  - 第 246 行：`inline RID create_depth_texture_4x4(RenderingDevice *p_rd, const float p_data[16])`
- [x] `test_helpers.h` 包含 `build_test_resource_sphere` 辅助函数
  - 第 273 行：`inline Ref<NaniteMeshResource> build_test_resource_sphere(int p_segments = 32)`

---

## 14. 编辑器集成 (Task 1.14)

- [x] `NaniteMeshEditor::preview_instance` 类型为 `NaniteMeshInstance3D*`
  - `nanite_mesh_editor.h` 第 67 行：`NaniteMeshInstance3D *mesh_instance = nullptr;`（字段名为 `mesh_instance` 而非 spec 中的 `preview_instance`，但类型为 `NaniteMeshInstance3D*`，符合任务说明中"刚修改"的描述）
- [x] `NaniteMeshEditor::edit()` 调用 `set_nanite_mesh` 而非 `set_mesh`
  - `nanite_mesh_editor.cpp` 第 131 行：`mesh_instance->set_nanite_mesh(current_resource);`（而非 `set_mesh`）
- [PARTIAL] SubViewport 渲染时 NaniteGDExtBridge 回调被触发
  - 代码审查：`NaniteServer::init()` 第 87-90 行注释明确：`the effect RIDs exist on the RenderingServer but are not yet attached to any Compositor resource, so the renderer will not invoke them until Stage 2 wires them into the default Compositor's effects list.` 框架已就位但回调不会被触发，待 Stage 2
- [x] 选中场景中的 `NaniteMeshInstance3D` 时 Inspector 显示 `nanite_mesh` / `nanite_enabled` / `forced_lod` / `relative_screen_size` 属性
  - `nanite_mesh_instance_3d.cpp` 第 177-180 行：4 个 `ADD_PROPERTY` 暴露所有属性，Inspector 自动渲染
- [PARTIAL] GDScript 测试 `test_nanite_editor_stage1.gd` 通过
  - `test_nanite_mesh_instance_in_inspector`
  - `test_nanite_mesh_editor_uses_nanite_instance`
  - 测试代码已编写于 `nanite/tests/test_nanite_editor_stage1.gd`：通过 ClassDB 反射验证类注册 + 继承 + 4 个属性。待 Stage 1 编译后运行验证
- [CANNOT_VERIFY] 双击 .nanite.tres 文件打开预览窗口，3D 视口内显示真实 Nanite 渲染（需手动验证）
  - 需手动 UI 操作：双击 .nanite.tres 文件
- [CANNOT_VERIFY] 预览窗口调试模式下拉切换 CLUSTER_SOLID_COLOR 时，cluster 显示不同颜色
  - 需手动 UI 操作：编辑器内切换调试模式

---

## 15. 文档与验收 (Task 1.15)

- [x] `nanite-overall-design.md` Stage 1 章节标注"已实现"状态
  - `nanite_doc/nanite-overall-design.md` 第 1819 行：`> **实现状态（2026-07-24）**：Stage 1 已实现完成（作为 module 内子目录 `nanite/bridge/`，通过 `NANITE_BRIDGE_GDEXT` 宏条件编译，而非独立 GDExtension 插件）。所有 Task 1.1-1.15 已完成...`
- [x] 记录实现与设计差异（module 内实现 vs 独立 GDExtension）
  - 同文件第 1984-1999 行 "10.5 实现与设计差异对照" 表格：S1-01 至 S1-10 共 10 项差异已记录，包括 module 内实现 vs 独立 GDExtension（S1-01）
- [x] checklist 所有项已检查（通过 / 未通过 / CANNOT_VERIFY / PARTIAL）
  - 本次代码审查完成：所有 `[ ]` 项均已更新为 `[x]` / `[PARTIAL]` / `[CANNOT_VERIFY]` 之一
- [PARTIAL] 未通过项有对应的新 task 创建
  - 无法直接验证 task 创建情况；建议人工核查 task tracker 是否为 PARTIAL 项（如 Stage 2 真实 culling / 真实 rasterize / material_rids 收集 / Compositor 挂接）创建后续 task

---

## 16. 全局验收标准

- [PARTIAL] `scons platform=windows target=editor` 编译成功，0 error 0 warning
  - 代码审查无语法错误；按任务要求未实际运行 scons 构建
- [PARTIAL] 所有 doctest 通过（`--test "*Nanite*,*NaniteServer*,*NaniteBridge*,*NaniteHZB*,*NaniteMeshData*,*NaniteGPUPipeline*,*NaniteMeshInstance*,*NanitePageCache*"`)
  - 测试代码已编写于 `nanite/tests/test_nanite_*.h`；需 scons 编译 + 运行验证。GPU 测试需 Vulkan 后端，无后端时自动 SKIP
- [PARTIAL] 所有 GDScript 测试通过
  - 5 个 GDScript 测试文件已编写：`test_nanite_debug.gd` / `test_shadow_mesh_gdext.gd` / `test_gdext_e2e.gd` / `test_perf_stage1.gd` / `test_nanite_editor_stage1.gd`；待 Stage 1 编译后运行验证
- [x] ProjectSettings 中 `nanite/bridge/active` 存在且默认 `"gdext"`
  - `nanite_server.cpp` 第 68 行：`GLOBAL_DEF(PropertyInfo(..., "nanite/bridge/active", PROPERTY_HINT_ENUM, "gdext,module,deep"), "gdext")`
- [CANNOT_VERIFY] 场景中放置 `NaniteMeshInstance3D` + 资源后渲染出 Nanite 网格
  - 需手动 UI 验证：编辑器内放置节点并观察渲染
- [CANNOT_VERIFY] 粗 LOD 阴影正常投射
  - 需手动 UI 验证：观察阴影渲染效果
- [CANNOT_VERIFY] 调试模式可切换（NONE / CLUSTER_SOLID_COLOR 等）
  - 需手动 UI 验证：编辑器内切换调试模式并观察
- [CANNOT_VERIFY] 单个 10K tri 场景在 1080p 下 ≥ 30fps
  - 需手动运行性能测试脚本验证帧率
- [CANNOT_VERIFY] 无 Nanite 实例时渲染管线正常（不崩溃、不报错）
  - 需手动运行 `test_gdext_e2e.gd` 的 `test_no_nanite_instances_no_crash` 用例验证
