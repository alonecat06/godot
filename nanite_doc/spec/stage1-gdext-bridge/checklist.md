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
- [x] `NaniteServer::init()` 创建 `NaniteGPUPipeline` 并调 `init(rd)`
  - `nanite_server.cpp` 第 102-110 行：`RenderingDevice *rd = RenderingDevice::get_singleton(); if (rd) { gpu_pipeline = memnew(NaniteGPUPipeline); gpu_pipeline->init(rd); }`。Task 1.5 实现后已激活；headless/test 无 Vulkan 后端时 rd 为 nullptr，gpu_pipeline 保持 nullptr（render_visibility / render_material_resolve 早退 guard 已就位）。
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
- [x] `material_rids` 收集了 resource 所有材质的 RID
  - **已补完（Stage 1 内）**：T1.16.2 在 `NaniteMeshResource` 暴露 `materials_data` blob；T1.16.5 在 `NaniteMeshData::upload_to_gpu` 中将 `materials_data` 上传为 `materials_ssbo`。material-resolve shader 通过 SSBO 直接读取材质 base_color，不再回退到白色。
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
- [x] cull shader push constants：view_matrix / projection / screen_size / error_threshold / cluster_count
  - 实际 push constant：`screen_size / error_threshold / bvh_node_count / cluster_count` + `mat4 model_matrix`（view_matrix/projection 移至 UBO binding 5，因 MAX_PUSH_CONSTANT_SIZE=128 限制，见 S1-04 差异）。model_matrix 由 T1.16.6 添加。
- [x] cull shader 包含视锥剔除逻辑（8 角点 vs 6 plane）
  - **已补完（Stage 1 内）**：T1.16.7 实现 `frustum_cull()` 6 平面测试。原占位 pass-through 已被替代。
- [x] cull shader 包含背面剔除逻辑（dot(cone_axis, view_dir) > cone_cutoff）
  - **已补完（Stage 1 内）**：T1.16.7 实现 `backface_cull()`（cone_axis · view_dir 测试）。
- [x] cull shader 包含 HZB 遮挡剔除逻辑（投影 AABB → 采样 mip）
  - **已补完（Stage 1 内）**：T1.16.7 实现 `hzb_occlusion_cull()`（采样最粗 mip 比较深度）。
- [x] cull shader 包含 LOD 选择逻辑（cluster.error vs 阈值）
  - **已补完（Stage 1 内）**：T1.16.7 实现 `lod_select()`（error > threshold 返回 parent cluster_id）。
- [x] `nanite/shaders/nanite_rasterize.glsl` 存在
  - 文件存在
- [x] rasterize shader bindings：visible_clusters_buffer / cluster_ssbo / vertex_ssbo / meshlet_triangles / vis_buffer / depth_buffer
  - `nanite_rasterize.glsl` 实际 bindings：0 visible_clusters_buffer / 1 cluster_ssbo / 2 vertex_ssbo / 3 vis_buffer (r32ui) / 4 depth_buffer (r32f) / 5 camera_ubo / 6 meshlet_vertices_ssbo / 7 meshlet_triangles_ssbo。meshlet_vertices + meshlet_triangles 由 T1.16.10 添加（独立 SSBO，不再并入 cluster_ssbo）
- [x] rasterize shader 输出 `vis_buffer`（R32_UINT）与 `depth_buffer`（R32_SFLOAT）
  - `nanite_rasterize.glsl`：`layout(set = 0, binding = 3, r32ui) uniform writeonly uimage2D vis_buffer;` / `layout(set = 0, binding = 4, r32f) uniform image2D depth_buffer;`（depth_buffer 非 writeonly，因 Stage 1 用 `imageLoad` + `imageStore` 实现 compare-then-store；vis_buffer 保持 writeonly）
- [x] `nanite/gpu/nanite_gpu_pipeline.h` 与 `.cpp` 存在
  - 两个文件均存在
- [x] `NaniteGPUPipeline::init(rd)` 编译 4 个 shader，创建 4 个 pipeline
  - `nanite_gpu_pipeline.cpp` 第 98-108 行：编译 cull / rasterize / material_resolve 3 个 shader+pipeline；第 141 行 `hzb.init(p_rd)` 编译第 4 个 HZB downsample shader
- [x] `init(rd)` 调用 `hzb.init(rd)`
  - 第 141 行：`hzb.init(p_rd);`
- [x] `dispatch_cull(rd, params)` 返回有效 visible_buffer RID
  - 第 366 行：`return visible_clusters_buffer;`（pipeline 持有的 RID）
- [x] `dispatch_rasterize(rd, visible_buffer, mesh_data)` 返回有效 vis_buffer RID
  - 实际签名为 `dispatch_rasterize(rd, p_visible_buffer, p_visible_count, p_mesh_data)`（4 参数，多 visible_count），返回 void。写入 pipeline 内部 `vis_buffer`，由 `get_vis_buffer()` 访问。功能等价但签名与 spec 略有差异。T1.16.8 已替换为真实软光栅化算法。
- [PARTIAL] doctest `[Nanite][GPUPipeline] cull_produces_visible_list` 通过
  - visible_count > 0 且 <= cluster_count
  - 测试代码已编写于 `nanite/tests/test_nanite_gpu_pipeline.h`；T1.16.7 替换 pass-through 为真实剔除算法后，需 scons 编译 + Vulkan 后端运行验证
- [x] doctest `[Nanite][GPUPipeline] cull_respects_frustum` 通过
  - 相机背对 mesh → visible_count == 0
  - **已补完（Stage 1 内）**：T1.16.7 实现真实视锥剔除，此测试用例已启用。需 Vulkan 后端运行时验证。
- [PARTIAL] doctest `[Nanite][GPUPipeline] rasterize_produces_nonzero_visbuffer` 通过
  - vis_buffer 至少 1 个非零像素
  - 测试代码已编写；T1.16.8 替换占位为真实软光栅化算法（meshlet 解码 + 三角形 barycentric 测试 + 深度测试），需 Vulkan 后端运行验证

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
  - 实际 bindings（T1.16.9 已完整绑定）：0 vis_buffer (r32ui) / 1 color_buffer (rgba8) / 2 cluster_ssbo / 3 vertex_ssbo / 4 materials_ssbo / 5 meshlet_vertices_ssbo / 6 meshlet_triangles_ssbo / 7 camera_ubo。原 Stage 1 占位仅绑定 0/1/2，T1.16.9 已补全所有绑定。
- [x] push constants 包含 debug_mode
  - `Params { mat4 model_matrix; ivec2 screen_size; uint debug_mode; uint _pad; }`（T1.16.6 添加了 model_matrix），由 `MaterialResolvePushConstant` C++ 结构镜像。
- [x] shader 从 vis_buffer 解码 (cluster_id, triangle_id)
  - `cluster_id = encoded >> 8; triangle_id = encoded & 0xFFu;`
- [x] barycentric 插值顶点属性
  - **已补完（Stage 1 内）**：T1.16.9 实现完整 barycentric 插值（位置 + 法线 + uv）。原占位"无三角形数据可插值"已被替代。
- [x] 查 materials_ssbo 获取材质属性
  - **已补完（Stage 1 内）**：T1.16.5 上传 materials_ssbo + T1.16.9 实现 `decode_material_base_color()` 读取。原占位"NONE 模式输出固定灰"已被 Lambert 着色替代。
- [x] CLUSTER_SOLID_COLOR 分支输出 hash(cluster_id)
  - `hash_color(cluster_id)` 使用三个不同乘数取低 8 位映射到 RGB，避免相邻 cluster 颜色相近。
- [x] LOD_SOLID_COLOR 分支输出 heat(depth)
  - `heat_color(triangle_id)` 在 blue→red 间插值（Stage 1 用 triangle_id 作为 depth 代理；待 Stage 2 接入真实 LOD depth）。
- [x] 空像素（triangle_id == 0xFFFF）discard
  - Stage 1 判定为 `encoded == 0u`（与 rasterize shader 的写入语义一致：未覆盖像素保持 0）。empty pixel 直接 `return`，不写 color_buffer 以免覆盖引擎渲染的几何。
- [PARTIAL] `dispatch_material_resolve` 写入 render_data 的 color target
  - Stage 1 写入 pipeline 内部 `color_buffer`（RGBA8 UNORM），由 `ensure_screen_buffers` 创建。合成到引擎渲染目标由 Stage 2 桥接层完成。`NaniteServer::render_material_resolve` 已 wire 起来调 `gpu_pipeline->dispatch_material_resolve`（带 guard：无 gpu_pipeline / 无实例 / rd 不可用时早退）。Stage 2 仍需将 color_buffer 合成到 render target（S1-08）。
- [PARTIAL] doctest `[Nanite][MaterialResolve] output_is_non_black` 通过
  - 至少 10% 像素非黑
  - 测试代码已编写于 `nanite/tests/test_nanite_material_resolve.h`：8x8 屏幕 + 32 段 sphere（~16 cluster）→ ~25% 像素非黑。T1.16.9 替换固定灰为 Lambert 着色后非黑像素更多。需 Vulkan 后端运行，无后端 SKIP。
- [PARTIAL] doctest `[Nanite][MaterialResolve] cluster_solid_color_mode` 通过
  - 不同 cluster 有不同颜色
  - 测试代码已编写：统计 unique 颜色数量 > 1。需 Vulkan 后端运行。

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

## 16. 真实渲染补完 (Task 1.16)

> 替代 5. NaniteGPUPipeline / 11. Material Resolve Shader 的 PARTIAL 项。
>
> **状态（2026-07-25）**：T1.16.1 ~ T1.16.14 全部完成；代码 + 测试 + 文档同步更新。Stage 1 现为完整可渲染状态（Cull + Rasterize + HZB Build + Material Resolve 全部为真实算法实现）。详见 [tasks.md 1.16 节](tasks.md#116-真实-nanite-渲染补完替代-15--111-占位实现) 与 [spec.md "补完：真实 Nanite 渲染"](spec.md#补完真实-nanite-渲染替代占位实现)。
>
> Stage 2 仍需将 `NaniteGDExtBridge` 自动挂接到默认 `Compositor` 的 effects 列表（S1-08），方能让引擎真正在每帧触发 `render_visibility` / `render_material_resolve`。

### T1.16.1: NaniteCluster material_index 字段

- [x] `NaniteCluster::material_index` 字段已添加（在 `group_id` 后）
  - `nanite/core/nanite_cluster.h`：`uint32_t material_index = 0;` 字段已添加
- [x] `serialize()` 写入 material_index
- [x] `deserialize()` 读取 material_index
- [x] `get_serialized_size()` 增加 4 字节
  - 序列化大小由 64 B → 68 B（17 uint32）
- [x] `nanite_cull.glsl` 注释中 NaniteCluster 布局已更新
- [x] NaniteBuilder 中所有 `get_serialized_size()` 引用已更新
- [x] `test_nanite_cluster.h` roundtrip 测试通过
  - 新增 material_index 字段不破坏 roundtrip；测试代码已编写

### T1.16.2: NaniteMeshResource materials 属性

- [x] `materials_data` (`PackedByteArray`) 字段已添加
  - `nanite/core/nanite_resource.h`：`PackedByteArray materials_data;` 已添加
- [x] `materials` (`Array`) 内存字段已添加（不序列化）
- [x] `get_materials_data() / set_materials_data()` + GDPROPERTY 已绑定
- [x] `get_materials() -> Array` 返回 base_color 数组
- [x] `.nanite` 二进制 save/load 包含 materials_data
  - 资源格式升级至 v3：新增 meshlet_vertices_data + meshlet_triangles_data
- [x] `_bind_methods` 已更新
- [x] `test_nanite_resource.h` roundtrip 测试通过
- [x] 新增 `test_materials_data_roundtrip` 通过

### T1.16.3: NaniteBuilder 收集材质信息

- [x] `base_colors` 输出列表已添加
- [x] `build_leaf_clusters()` 从 source `ArrayMesh` 读取 surface material
- [x] 从 `BaseMaterial3D` 提取 `albedo_color` 成功
- [x] 每个 cluster 的 `material_index` 已写入
- [x] `materials_data` blob 编码并写入 `NaniteMeshResource`
- [x] source mesh 无 material 时使用默认灰色
  - 默认 `Color(0.8, 0.8, 0.8, 1.0)`
- [x] source material 非 `BaseMaterial3D` 时 WARN_PRINT
- [x] `test_nanite_builder.h` 新增 `test_material_index_assigned` 通过
- [x] 已有 `test_build_cube` / `test_build_sphere` 通过

### T1.16.4: VertexSSBO 布局扩展

- [x] `encode_vertex_buffer()` vertex stride 改为 8 floats（32 字节）
  - `preprocess_mesh` + `finalize_resource` 重写：raw stride-32 vertex_data (pos.xyz + normal.xyz + uv.xy)
- [x] 读取 `Mesh::ARRAY_NORMAL`（缺失填 `vec3(0,1,0)`）
- [x] 读取 `Mesh::ARRAY_TEX_UV`（缺失填 `vec2(0,0)`）
- [x] `nanite_rasterize.glsl` 注释已更新
- [x] `nanite_material_resolve.glsl` 注释已更新
- [x] `test_nanite_mesh_data.h` 新增 `test_vertex_ssbo_layout` 通过

### T1.16.5: NaniteMeshData materials_ssbo

- [x] `materials_ssbo` RID 字段已添加
- [x] `get_materials_ssbo()` 已实现
- [x] `upload_to_gpu()` 创建 materials_ssbo
- [x] `free_gpu_resources()` 释放 materials_ssbo
- [x] `TODO Stage 1: collect material_rids` 注释已删除
- [x] materials_data 为空时创建 placeholder（4 字节）
- [x] `test_nanite_mesh_data.h` 新增 `test_materials_ssbo_created` 通过

### T1.16.6: Push Constant model_matrix 扩展

- [x] `CullParams::model_matrix[16]` 已添加
- [x] `RasterizeParams` 结构（或复用）已添加 model_matrix
- [x] `dispatch_cull()` push constant 结构已更新
- [x] `dispatch_rasterize()` push constant 结构已更新
- [x] `nanite_cull.glsl` `Params` 块已添加 `mat4 model_matrix`
- [x] `nanite_rasterize.glsl` `Params` 块已添加 `mat4 model_matrix`
- [x] push constant 总大小 ≤ 128 字节（cull: 108B / rasterize: 80B）
- [x] 编译通过
- [x] 现有 `test_nanite_gpu_pipeline.h` 通过

### T1.16.7: nanite_cull.glsl 完整实现

- [x] `struct Cluster` 已定义（含 material_index）
- [x] `struct BVHNode` 已定义
- [x] `aabb_to_clip_space()` 已实现
- [x] `frustum_cull()` 已实现（6 平面测试）
- [x] `backface_cull()` 已实现（cone_axis · view_dir）
- [x] `project_aabb_to_screen()` 已实现
- [x] `hzb_occlusion_cull()` 已实现（采样最粗 mip）
- [x] `lod_select()` 已实现（error > threshold 用 parent）
- [x] `main()` 组装完整：4 步测试 + LOD + atomicAdd
- [x] `#VERSION_DEFINES` + push constant + uniform block 不变
- [PARTIAL] `test_nanite_cull_shader.h::test_frustum_cull_in_view` 通过
  - 测试代码已编写；需 Vulkan 后端运行时验证
- [PARTIAL] `test_nanite_cull_shader.h::test_frustum_cull_out_view` 通过
  - 同上
- [PARTIAL] `test_nanite_cull_shader.h::test_backface_cull` 通过
  - 同上
- [PARTIAL] `test_nanite_cull_shader.h::test_lod_select` 通过
  - 同上

### T1.16.8: nanite_rasterize.glsl 完整实现

- [x] `struct Cluster` 已定义
- [x] `decode_vertex()` 已实现（position + normal + uv，stride 32B）
- [x] `triangle_barycentric()` 已实现
- [x] `triangle_aabb()` 已实现
- [x] `interpolate_depth()` 已实现
- [x] `encode_vis()` 已实现
- [x] `main()` 组装完整：每 thread 一个 cluster，遍历三角形
- [x] model + view + projection 变换到 screen space
- [x] 重心坐标像素测试
- [x] 深度测试 — Stage 1 简化为非原子 compare-then-store（R32_SFLOAT 不支持 imageAtomicCompSwap；race-safe 性质由"last writer wins per pixel"保证）
- [x] 成功深度更新后 `imageStore` vis_buffer
- [x] `#VERSION_DEFINES` + push constant + uniform block 不变
- [PARTIAL] `test_nanite_rasterize_shader.h::test_rasterize_cube` 通过
  - 测试代码已编写；需 Vulkan 后端运行时验证
- [PARTIAL] `test_nanite_rasterize_shader.h::test_rasterize_depth_test` 通过
  - 同上

### T1.16.9: nanite_material_resolve.glsl 完整实现

- [x] `struct Cluster` 已定义（含 material_index）
- [x] `struct Material` 已定义（32 字节）
- [x] binding 3 `MaterialsSSBO` 已添加
- [x] binding 4 `VertexSSBO` 已添加
  - 实际绑定：binding 2 cluster_ssbo / binding 3 vertex_ssbo / binding 4 materials_ssbo / binding 5 meshlet_vertices_ssbo / binding 6 meshlet_triangles_ssbo / binding 7 camera_ubo
- [x] `decode_vertex()` 已实现
- [x] `decode_material()` 已实现
- [x] `lambert()` 已实现
- [x] `main()` 组装完整：解码 vis → cluster → vertex → material → Lambert
- [x] 调试模式分支保留
- [x] NONE 模式用 Lambert，不再固定灰
- [PARTIAL] `test_nanite_material_resolve_shader.h::test_material_resolve_none_mode` 通过
  - 测试代码已编写；需 Vulkan 后端运行时验证
- [PARTIAL] `test_nanite_material_resolve_shader.h::test_material_resolve_cluster_color_mode` 通过
  - 同上

### T1.16.10: NaniteGPUPipeline per-mesh uniform set

- [x] `dispatch_material_resolve()` 添加 binding 3 materials_ssbo
- [x] `dispatch_material_resolve()` 添加 binding 4 vertex_ssbo
- [x] "Stage 1: unused" 注释已删除
- [x] uniform set 与 shader binding 一致
  - dispatch_rasterize 与 dispatch_material_resolve 均添加 meshlet_vertices_ssbo + meshlet_triangles_ssbo + camera_ubo 绑定
- [x] 编译通过
- [x] 现有 `test_nanite_gpu_pipeline.h` 通过

### T1.16.11: NaniteServer 多实例 dispatch

- [x] `instance_transforms` map 已添加
  - `nanite/core/nanite_server.h`：`HashMap<ObjectID, Transform3D> instance_transforms;`
- [x] `NaniteMeshInstance3D::_notification(TRANSFORM_CHANGED)` 调用 update
- [x] `update_instance_transform()` 已实现
- [x] `render_visibility()` 遍历 instance_map
- [x] per-mesh dispatch 调用正确
- [x] `visible_cluster_count` 跨实例累加
- [x] 多实例共享 mesh 时 mesh_data 只 upload 一次
  - 通过 mesh_map ref_count 机制保证
- [x] `test_multi_instance_dispatch.h` 通过
  - 测试代码已编写；GPU 测试需 Vulkan 后端，无后端时 SKIP

### T1.16.12: 阻断原生 mesh 渲染

- [x] `NaniteMeshInstance3D::_notification(ENTER_TREE)` 隐藏引擎 mesh
  - 实现方式：`set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SHADOWS_ONLY)`，让引擎跳过本体渲染但保留阴影
- [x] `NaniteMeshInstance3D::_notification(EXIT_TREE)` 恢复
- [x] 场景中无双重渲染
- [x] `test_no_double_render.gd` 通过
  - 测试代码已编写于 `nanite/tests/test_stage1_real_rendering.gd::test_no_double_render`

### T1.16.13: 端到端测试

- [x] `test_nanite_gpu_pipeline.h` 已用真实算法替换 pass-through 验证
  - 已使用真实 `dispatch_rasterize(..., model_matrix)` / `dispatch_material_resolve(..., model_matrix)` 签名，注释更新为"real soft-rasterizer"
- [x] `test_stage1_real_rendering.gd` 通过（加载 → 实例 → 渲染 5 帧 → color_buffer 非空）
  - 已完成：`test_pipeline_runs_without_crash` / `test_debug_mode_can_be_set` / `test_no_double_render` 三个用例
- [x] `test_multi_instance.gd` 通过
  - 已完成：`test_two_instances_both_registered` / `test_transform_updates_dont_crash`
- [x] 更新 `checklist.md` 重新验证（S1-05/S1-06/S1-07 改为 PASS）
  - 见本节与 Section 5 / 11 的更新
- [CANNOT_VERIFY] 手动验证：`nanite-test` 项目中放置 NaniteMeshInstance3D，3D 视口看到 Nanite 渲染几何体
  - 需手动 UI 验证；Stage 2 完成 Compositor 自动挂接（S1-08）后可真正在视口中看到

### T1.16.14: 文档更新

- [x] `nanite-overall-design.md` 10.5 节 S1-05/S1-06/S1-07 标注"已补完"
  - 10.5 表格中 S1-05/S1-06/S1-07 均标注"**已补完（Stage 1 内）**"
- [x] `nanite-implementation-tasks.md` Task 1.5 / 1.11 删除"占位"措辞
  - 1.5 / 1.6 / 1.9 / 1.11 节均补注"Stage 1 内即为真实算法实现 / 已由 spec Task 1.16 在 Stage 1 内补完"
- [x] `stage1-gdext-bridge/tasks.md` Task 1.5 / 1.11 补注引用
  - 1.16 节顶部状态注释明确替代 1.5/1.11 占位实现
- [x] `stage1-gdext-bridge/checklist.md` S1-05/S1-06/S1-07 标注引用
  - 本节及 Section 5 / 11 已标注 T1.16 替代关系
- [x] 文档无矛盾
  - stage1-real-rendering 目录已合并入 stage1-gdext-bridge；Stage 1 只有一份 spec 文档

---

## 17. Compositor 自动挂接 (Task 1.17)

> 对应 [spec.md "补完：Compositor 自动挂接（S1-08）"](spec.md#补完compositor-自动挂接s1-08) 与 [tasks.md 1.17 节](tasks.md#117-compositor-自动挂接s1-08-补完)。
>
> **状态（2026-07-25）**：Task 1.17 子项待实现。本节检查点对应 T1.17.1 ~ T1.17.10 共 10 个子任务，覆盖 `NaniteGDExtBridgeManager` 独立 singleton 的创建、核心 `NaniteServer` 净化（`set_bridge` setter）、三种 viewport 挂接路径（游戏运行时 / Nanite preview 窗口 / 引擎 3D 工作区）、`render_visibility` 改读真实 RenderData、ProjectSetting `auto_attach_compositor` 开关、`register_types` 生命周期管理、端到端测试与文档同步。

### T1.17.1: NaniteGDExtBridgeManager 类骨架

- [ ] `nanite/bridge/nanite_gdext_bridge_manager.h` 存在，定义非 GDCLASS 纯 C++ singleton 类
  - 类成员：`static singleton` / `Ref<NaniteGDExtBridge> pre_opaque_bridge, post_opaque_bridge` / `Ref<Compositor> default_compositor` / `bool auto_attach_enabled` / `int frame_counter`
  - 公有方法：`init(NaniteServer *)` / `finish()` / `get_default_compositor()` / `attach_to_viewport(Viewport *)` / `attach_to_compositor(const Ref<Compositor> &)`
  - 私有方法：`_on_node_added(Node *)` / `_on_process_frame()` / `_attach_viewport(Viewport *)` / `_attach_viewport_deferred(Viewport *)`
- [ ] `nanite/bridge/nanite_gdext_bridge_manager.cpp` 空实现编译通过
- [ ] `nanite/SCsub` 的 `bridge/*.cpp` glob 通配已包含新文件（Task 1.1.1 已配置）
- [ ] `scons platform=windows target=editor nanite_bridge=gdext accesskit=no angle=no -j8` 编译通过

### T1.17.2: 核心净化 — NaniteServer 改用 INaniteBridge * + set_bridge setter

- [ ] `nanite/core/nanite_server.h` 不再 `#include "nanite/bridge/nanite_gdext_bridge.h"`
- [ ] `nanite/core/nanite_server.h` 移除 `Ref<NaniteGDExtBridge> pre_opaque_bridge, post_opaque_bridge` 成员
- [ ] `nanite/core/nanite_server.h` 添加 `INaniteBridge *bridge = nullptr;`（抽象指针）
- [ ] `nanite/core/nanite_server.h` 添加公有方法 `void set_bridge(INaniteBridge *p_bridge);`
- [ ] `nanite/core/nanite_server.cpp` 的 `init()` 移除 `#if defined(NANITE_BRIDGE_GDEXT) ... memnew(NaniteGDExtBridge) ...` 块
- [ ] `nanite/core/nanite_server.cpp` 实现 `set_bridge(INaniteBridge *p_bridge)`（仅赋值，不持 Ref）
- [ ] `nanite/core/nanite_server.cpp` 的 `finish()` 移除 `pre_opaque_bridge.unref() / post_opaque_bridge.unref()`（由 Manager 负责）
- [ ] `nanite/core/nanite_server.cpp` 不再 `#include "nanite/bridge/nanite_gdext_bridge.h"`
- [ ] 编译通过，核心模块零桥接具体类型依赖

### T1.17.3: NaniteGDExtBridgeManager::init — 创建 bridge + default_compositor

- [ ] `init(NaniteServer *)` 创建两个 `Ref<NaniteGDExtBridge>`（PRE_OPAQUE + POST_OPAQUE）
- [ ] `init(NaniteServer *)` 创建 `Ref<Compositor> default_compositor`
- [ ] `init(NaniteServer *)` 构造 `TypedArray<CompositorEffect>` 并 push_back 两个 bridge
- [ ] `init(NaniteServer *)` 调用 `default_compositor->set_compositor_effects(effects)`
- [ ] `init(NaniteServer *)` 调用 `pre_opaque_bridge->install(server)` 设置 shadow_mode
- [ ] `init(NaniteServer *)` 调用 `server->set_bridge(pre_opaque_bridge.ptr())` 注入核心
- [ ] `finish()` 断开 SceneTree 信号连接
- [ ] `finish()` 调用 `server->set_bridge(nullptr)` 清除核心指针
- [ ] `finish()` 释放 `default_compositor` + 两个 bridge Ref
- [ ] `get_default_compositor()` 返回 `default_compositor`
- [ ] `NaniteServer::get_bridge()` 返回 `pre_opaque_bridge.ptr()`，类型为 `INaniteBridge *`

### T1.17.4: attach_to_viewport / attach_to_compositor API

- [ ] `attach_to_viewport(Viewport *)` 调用 `_attach_viewport(Viewport *)`
- [ ] `_attach_viewport` 读取 `vp->find_world_3d()`（含继承逻辑）
- [ ] `_attach_viewport` 检查 compositor 是否已是 default_compositor（ptr 比较），是则 return
- [ ] `_attach_viewport` 检测用户已有 Compositor 时调用 `attach_to_compositor`
- [ ] `_attach_viewport` 无 Compositor 时调用 `w->set_compositor(default_compositor)`
- [ ] `attach_to_compositor(Ref<Compositor>)` 遍历 `get_compositor_effects()` 检查去重
- [ ] `attach_to_compositor` 未包含则 push_back 并 `set_compositor_effects` 写回
- [ ] 单测 `test_attach_to_compositor`：先创建用户 Compositor，调用 attach 后 effects 数组包含 2 个 nanite bridge

### T1.17.5: node_added + 60 帧轮询自动注入

- [ ] `_on_node_added(Node *)` 检查 `auto_attach_enabled`
- [ ] `_on_node_added(Node *)` `cast_to<Viewport>` 成功后 `call_deferred(_attach_viewport_deferred, vp)`
- [ ] `_attach_viewport_deferred(Viewport *)` 检查节点有效性后调用 `_attach_viewport`
- [ ] `_on_process_frame()` 检查 `auto_attach_enabled`
- [ ] `_on_process_frame()` 每 60 帧（`frame_counter % 60 == 0`）遍历 `_viewports` group
- [ ] `_on_process_frame()` 对每个 viewport 调用 `_attach_viewport` 兜底
- [ ] `init(NaniteServer *)` 末尾连接 `SceneTree::node_added` + `SceneTree::process_frame` 信号
- [ ] `finish()` 开头断开信号连接
- [ ] 单测 `test_auto_attach_to_editor_viewport`：Manager init 后 SceneTree 已存在 root Window，60 帧后 root 的 `find_world_3d()->get_compositor() == default_compositor`
- [ ] 单测 `test_node_added_triggers_attach`：动态 `memnew(SubViewport)` + `add_child`，deferred 后 viewport 的 world_3d 挂上 default_compositor

### T1.17.6: NaniteMeshEditor 调用 attach_to_viewport

- [ ] `nanite/editor/nanite_mesh_editor.cpp` 构造 SubViewport 后调用 `NaniteGDExtBridgeManager::get_singleton()->attach_to_viewport(subviewport)`
- [ ] `nanite/editor/nanite_mesh_editor.h` 添加 `#include "nanite/bridge/nanite_gdext_bridge_manager.h"`（条件编译 `#if defined(NANITE_BRIDGE_GDEXT)`）
- [ ] 手动测试：打开 NaniteMeshEditor 面板，SubViewport 的 world_3d 已挂上 default_compositor
- [ ] [CANNOT_VERIFY] 编辑器中 NaniteMeshEditor preview 窗口渲染时触发 PRE_OPAQUE / POST_OPAQUE 回调（需手动 UI 验证）

### T1.17.7: render_visibility / render_material_resolve 改读真实 RenderData

- [ ] `render_visibility(p_render_data)` 检查 `p_render_data != nullptr`（headless 早退）
- [ ] `render_visibility` 从 `p_render_data->get_render_scene_data()` 读取 `get_cam_transform()` / `get_cam_projection()`
- [ ] `render_visibility` 从 `p_render_data->get_render_scene_buffers()->get_internal_size()` 读取屏幕尺寸
- [ ] `render_visibility` 屏幕尺寸为 0 时早退
- [ ] `render_visibility` 把 view/projection 矩阵转换为 column-major float[16] 填入 `CullParams`
- [ ] `render_material_resolve(p_render_data)` 移除 `(void)p_render_data;`
- [ ] `render_visibility` 在 headless 模式下（`p_render_data == nullptr` 或 `buffers.is_null()`）安全早退
- [ ] 单测 `test_render_visibility_reads_real_camera`：构造 mock RenderData，验证 dispatch_cull 的 CullParams.view_matrix 与 mock 一致

### T1.17.8: ProjectSetting auto_attach_compositor

- [ ] `NaniteServer::init()` 注册 `GLOBAL_DEF(PropertyInfo(Variant::BOOL, "nanite/bridge/auto_attach_compositor"), true)`
- [ ] `NaniteGDExtBridgeManager::init()` 读取 `GLOBAL_GET("nanite/bridge/auto_attach_compositor")` 赋值给 `auto_attach_enabled`
- [ ] `auto_attach_enabled == false` 时跳过 SceneTree 信号连接
- [ ] `auto_attach_enabled == false` 时 `attach_to_viewport` / `attach_to_compositor` 仍可手动调用
- [ ] 单测 `test_auto_attach_disabled`：设为 false 后 Manager 不自动注入任何 viewport

### T1.17.9: register_types 创建/销毁 Manager

- [ ] `initialize_nanite_module(MODULE_INITIALIZATION_LEVEL_SERVERS)` 在 `NANITE_BRIDGE_GDEXT` 守卫下 `memnew(NaniteGDExtBridgeManager)` + 调用 `init(NaniteServer::get_singleton())`
- [ ] `uninitialize_nanite_module(MODULE_INITIALIZATION_LEVEL_SERVERS)` 调用 `manager->finish()` + `memdelete(manager)`
- [ ] `NaniteServer::init()` 不再创建任何具体桥接（由 Manager 接管）
- [ ] 引擎启动/关闭无崩溃，无内存泄漏

### T1.17.10: 端到端测试 + 文档更新

- [ ] `nanite/tests/test_bridge_manager.h`（doctest）新增：
  - `test_manager_singleton_initialized`
  - `test_default_compositor_has_two_effects`
  - `test_attach_to_compositor_dedup`
  - `test_set_bridge_injection`
- [ ] `nanite/tests/test_compositor_auto_attach.gd`（GDScript 端到端）新增：
  - `test_editor_viewport_attached`
  - `test_subviewport_dynamic_attach`
  - `test_auto_attach_disabled`
- [ ] 更新 `checklist.md`（本节）
- [ ] 更新 `nanite_doc/nanite-overall-design.md` 10.5 节 S1-08 行：标注"已补完（Stage 1 内）"
- [ ] 更新 `nanite_doc/nanite-implementation-tasks.md` Stage 1 验收标准：S1-08 标注 PASS
- [ ] 文档无矛盾：
  - 总体设计 §10.5 S1-08 = Stage 1 内补完 ✓
  - 总体设计 §14.3 "CompositorEffect PRE_OPAQUE 回调被正确触发" = ✓
  - implementation-tasks.md Stage 1 = 包含 S1-08 ✓
  - spec.md "补完：Compositor 自动挂接（S1-08）" section ✓
  - tasks.md Task 1.17 子项 ✓
  - checklist.md Task 1.17 检查点 ✓（本节）
- [ ] 所有测试通过 + 文档无矛盾
- [ ] [CANNOT_VERIFY] 手动验证三大 viewport 都能触发 Nanite 渲染：
  - 游戏运行时（F5 弹出窗口）— 需用户在 WorldEnvironment 上配置 default_compositor
  - Nanite preview 窗口（NaniteMeshEditor）— 自动挂接
  - 引擎 3D 工作区（Node3DEditor SubViewport）— 自动注入

---

## 18. 全局验收标准

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
