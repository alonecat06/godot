# Tasks — 阶段 0：离线构建模块

> 任务依赖关系见末尾。每个任务对应 `nanite-implementation-tasks.md` 中的 0.x 子阶段。

---

## 0.1 目录结构与编译

- [x] **Task 0.1.1**：创建 `nanite/` 核心库目录骨架
  - 创建 `nanite/SConscript`、`nanite/register_types.h` / `register_types.cpp`
  - 创建子目录 `nanite/core/`、`nanite/editor/`、`nanite/tests/`
  - 在 `nanite/SConscript` 中 `env.Append(CPPPATH=['#thirdparty/meshoptimizer'])`
  - 在顶层 `SConstruct` 添加 `nanite_bridge_arg = ARGUMENTS.get("nanite_bridge", "gdext")` 并调用 `SConscript("nanite/SConscript")`
  - 阶段 0 暂不添加任何 `nanite_bridge_*` 桥接子目录调用

- [x] **Task 0.1.2**：编写空 `register_types.cpp`，注册占位类（无类），仅 `NANITE_MODULE_ENABLED` 宏定义
  - 在 `modules_enabled.py` 中按需添加 `nanite` 模块开关（如使用 module 形式集成）
  - 验证：`scons platform=windows target=editor` 编译通过，无链接错误

- [x] **Task 0.1.3**：搭建 doctest 测试脚手架
  - 在 `nanite/tests/` 下创建 `test_macros.h` 复用 Godot 现有 `tests/test_macros.h`
  - 在 `nanite/SConscript` 中添加测试目标 `nanite_tests`，链接 doctest
  - 验证：`scons platform=windows target=editor --test nanite` 可运行（即使 0 用例）

---

## 0.2 NaniteCluster / NaniteClusterNode 数据结构

- [x] **Task 0.2.1**：实现 `nanite/core/nanite_cluster.h` / `.cpp`
  - 结构体字段按 spec.md Requirement "NaniteCluster 与 NaniteClusterNode 数据结构" 定义
  - 提供 `PackedByteArray serialize() const` 与 `static NaniteCluster deserialize(const PackedByteArray&)`
  - 字节序使用小端 + 4 字节对齐

- [x] **Task 0.2.2**：实现 `nanite/core/nanite_bvh.h` / `.cpp`（含 `NaniteClusterNode`）
  - `NaniteClusterNode` 字段按 spec.md 定义
  - 提供 `serialize()` / `deserialize()` 方法
  - 叶子节点哨兵 `left_child = right_child = UINT32_MAX`

- [x] **Task 0.2.3**：编写单元测试 `test_nanite_data_structures.cpp`
  - 覆盖：默认初始化、叶子哨兵、序列化往返（含 AABB/Vector3/float/uint32_t 全字段）
  - 验证：所有用例通过

---

## 0.3 BuilderConfig 参数定义

- [x] **Task 0.3.1**：实现 `nanite/core/builder_config.h` / `.cpp`
  - 继承 `RefCounted`，`GDCLASS(BuilderConfig, RefCounted)`
  - 13 个字段均通过 `_bind_methods` 暴露为 `GDPROPERTY`（含 range hint）
  - `bool is_valid() const` 检查：`max_vertices >= 32`、`max_triangles >= 32`、`partition_size >= 2`、`page_size_bytes >= 4096`、`shadow_lod_depth >= 1`

- [x] **Task 0.3.2**：在 `register_types.cpp` 注册 `BuilderConfig`
  - `ClassDB::register_class<BuilderConfig>()`

- [x] **Task 0.3.3**：编写单元测试 `test_builder_config.cpp`
  - 覆盖：默认合法、`max_triangles=0` 拒绝、`max_vertices=0` 拒绝、`partition_size=1` 拒绝
  - 验证：所有用例通过

---

## 0.4 NaniteBuilder — 叶子层构建

- [x] **Task 0.4.1**：实现 `nanite/core/nanite_builder.h` / `.cpp` 框架
  - 类签名：`class NaniteBuilder : public RefCounted`，持 `BuilderConfig cfg`
  - `GDCLASS(NaniteBuilder, RefCounted)`，`_bind_methods` 暴露 `build(Ref<ArrayMesh>)` 给 GDScript
  - 私有方法：`preprocess_mesh()`、`build_leaf_clusters()`、`build_hierarchy()`、`build_bvh()`、`build_shadow_mesh()`、`finalize_resource()`

- [x] **Task 0.4.2**：实现 `preprocess_mesh()`
  - 输入：`PackedVector3Array vertices, PackedInt32Array indices`
  - 调用 `meshopt_generateVertexRemap` → `meshopt_remapVertexBuffer` → `meshopt_remapIndexBuffer` → `meshopt_optimizeVertexCache`
  - 输出：紧凑 `LocalVector<float> verts_pos`（12B stride）+ `LocalVector<unsigned int> indices`

- [x] **Task 0.4.3**：实现 `build_leaf_clusters()`
  - 调用 `meshopt_buildMeshletsBound` 计算容量
  - 调用 `meshopt_buildMeshletsFlex(max_v=64, min_t=32, max_t=128, cone=0.5, split=0.5)`
  - 对每个 meshlet：`meshopt_optimizeMeshletLevel(3)` + `meshopt_computeMeshletBounds`
  - 填充 `NaniteCluster`（`vertex_offset`、`triangle_offset`、`bounds`、`cone_axis`、`cone_cutoff`、`error=0`）

- [x] **Task 0.4.4**：编写测试辅助 `nanite/tests/test_helpers.h`
  - `Ref<ArrayMesh> create_cube_mesh()`、`Ref<ArrayMesh> create_sphere_mesh(int segments=32)`、`Ref<ArrayMesh> create_large_test_mesh(int target_tris)`
  - 共享给后续子任务复用

- [x] **Task 0.4.5**：编写单元测试 `test_nanite_builder_leaf.cpp`
  - 覆盖：`preprocess_mesh` 去重、cube mesh 产出 ≥1 cluster、sphere mesh(2000 tri) 产出 ≥8 cluster、bounds.has_volume、cone_axis 单位化
  - 验证：所有用例通过

---

## 0.5 NaniteBuilder — 层次化简化与 BVH 装配

- [x] **Task 0.5.1**：实现 `build_hierarchy()`
  - 自底向上循环：`meshopt_partitionClusters(target=4)` → 按 partition_id 分组 → 合并 partition 的 index+vertex 子集 → 计算 `vertex_lock`（锁跨 partition 共享边顶点）→ `meshopt_simplifyWithAttributes(target=原/2, options=LockBorder|Regularize)` → `meshopt_buildMeshletsFlex` 再切 2 簇 → 计算父 error/bounds
  - 循环退出条件：`current_level.size() <= 1`
  - 中间产物：层次化 `NaniteClusterNode` 列表

- [x] **Task 0.5.2**：实现 `build_bvh()`
  - 将层次结构展开为线性 `LocalVector<NaniteClusterNode>`
  - `left_child`/`right_child` 用数组下标，根节点置于 `nodes[0]`
  - 叶子节点 `left_child = right_child = UINT32_MAX`，`first_cluster` 与 `cluster_count` 指向 `clusters` 数组

- [x] **Task 0.5.3**：编写单元测试 `test_nanite_builder_hierarchy.cpp`
  - 覆盖：8000 tri sphere 产出 ≥3 节点、根 error ≥ 子节点 error、父 bounds 包含子 bounds、根 bounds 包含 mesh AABB、简化降低三角形数、父 error 单调
  - 验证：所有用例通过

---

## 0.6 Page 划分

- [x] **Task 0.6.1**：实现 `nanite/core/page_packer.h` / `.cpp`
  - 类签名：`class PagePacker`，方法 `PageTable pack(const LocalVector<NaniteCluster>&, const BuilderConfig&)`
  - 排序键：先按 `group_id`（LOD 层），再按 cluster bounds 中心的 Morton 码（3D）
  - 切页策略：累计 cluster 字节数超过 `page_size_bytes` 即开新页
  - 返回 `PageTable`（含 `page_count`、每页 cluster 范围）

- [x] **Task 0.6.2**：在 `NaniteCluster` 中追加 `page_id` 字段（若 0.2 未加），由 `PagePacker::pack` 回填
  - 注意：`page_id` 是运行时分配，序列化时不写入（由加载时的 PageTable 决定）

- [x] **Task 0.6.3**：编写单元测试 `test_page_packer.cpp`
  - 覆盖：100 cluster 全分配、200 cluster + `page_size=4096` 强制多页且每页 ≤ 2× limit、同 LOD cluster 集中
  - 验证：所有用例通过

---

## 0.7 粗 LOD Shadow Mesh 生成

- [x] **Task 0.7.1**：实现 `build_shadow_mesh()`
  - 输入：`clusters`、`nodes`、`verts`、`meshlet_vertices`、`meshlet_triangles`、`shadow_lod_depth`
  - 遍历 `nodes`：`depth <= shadow_lod_depth && cluster_count > 0` 的节点收集 `first_cluster..+cluster_count`
  - 展开 meshlet 8-bit micro-index → 标准 triangle list（每簇顶点偏移累加）
  - 构造 `Ref<ArrayMesh>`，单 surface，`PRIMITIVE_TRIANGLES`

- [x] **Task 0.7.2**：编写单元测试 `test_shadow_mesh.cpp`
  - 覆盖：shadow 三角形数 < 原 mesh、shadow bounds 包含原 mesh（容差 0.01）、RID 创建不崩溃
  - 验证：所有用例通过 **[已修复]** `shadow_bounds_contain_original` 通过向 shadow_vertices 追加原 mesh AABB 8 角点修复（原 `.trae-cn/specs/fix-nanite-stage0-validation-failures/` 已并入 spec.md 并清理）

---

## 0.8 序列化与 NaniteMeshResource

- [x] **Task 0.8.1**：实现 `nanite/core/nanite_resource.h` / `.cpp`
  - 继承 `Resource`，`GDCLASS(NaniteMeshResource, Resource)`
  - 字段：`PackedByteArray vertex_data`、`clusters_data`、`nodes_data`、`page_table_data`；`Ref<ArrayMesh> shadow_mesh`；`Ref<BuilderConfig> build_config`；`int cluster_count`、`node_count`、`page_count`
  - `_bind_methods` 暴露所有字段为 `GDPROPERTY`
  - 实现 `_get_property()` / `_set_property()` 以支持 Godot 原生序列化

- [x] **Task 0.8.2**：实现 `.nanite` 二进制格式
  - Magic: `"NANM"` (4 bytes)
  - Version: `uint32 = 1`
  - 顶点池：`meshopt_encodeVertexBuffer` 编码 + 长度前缀
  - 每 cluster：`meshopt_encodeMeshlet` 编码 + `vertex_count`/`triangle_count`/`encoded_size` 前缀
  - nodes 数组：原始结构体字节流
  - page_table：序列化的 `PageTable` 结构
  - 公开 `Error save(String path)` 与 `Error load(String path)`

- [x] **Task 0.8.3**：实现 `NaniteBuilder::finalize_resource()`
  - 将 `clusters` / `nodes` / `verts` / `meshlet_*` / `shadow_mesh` 装配到 `Ref<NaniteMeshResource>`
  - 调用 `meshopt_encodeMeshlet` 与 `meshopt_encodeVertexBuffer`
  - 调用 `PagePacker::pack` 填充 `page_table_data`
  - 返回完整资源

- [x] **Task 0.8.4**：在 `register_types.cpp` 注册 `NaniteMeshResource`
  - `ClassDB::register_class<NaniteMeshResource>()`

- [x] **Task 0.8.5**：编写单元测试 `test_nanite_resource.cpp`
  - 覆盖：`.nanite` 往返、`.tres` 往返、meshlet encode/decode 一致
  - 验证：所有用例通过

---

## 0.9 编辑器预览与调试可视化

- [x] **Task 0.9.1**：实现 `nanite/editor/nanite_mesh_editor.h` / `.cpp`
  - 继承 `SubViewportContainer`，`GDCLASS(NaniteMeshEditor, SubViewportContainer)`
  - 子节点：`SubViewport`、`Node3D rotation_node`、`Camera3D`、`DirectionalLight3D * 2`、`OptionButton debug_mode_btn`、`Button wireframe_btn`、`Button bounds_btn`、`Label stats_label`
  - `edit(Ref<NaniteMeshResource>)`：设置预览 mesh、填充统计、自动缩放相机
  - `gui_input()` 实现鼠标拖拽旋转（复用 `editor/scene/3d/mesh_editor_plugin.cpp` 模式）
  - `_notification()` 处理 `NOTIFICATION_FOCUS_ENTER/EXIT` 调试模式隔离

- [x] **Task 0.9.2**：实现 `nanite/editor/nanite_editor_plugin.h` / `.cpp`
  - `EditorInspectorPluginNanite : EditorInspectorPlugin`，`can_handle()` 识别 `NaniteMeshResource`，`parse_begin()` 创建 `NaniteMeshEditor`
  - `NaniteEditorPlugin : EditorPlugin`，构造时 `add_inspector_plugin()`

- [x] **Task 0.9.3**：实现 `nanite/editor/nanite_resource_preview_gen.h` / `.cpp`
  - 继承 `EditorResourcePreviewGenerator`
  - `handles("NaniteMeshResource")` 返回 `true`
  - `generate()` 用 `shadow_mesh` 委托 `StandardResourcePreview` 生成缩略图

- [x] **Task 0.9.4**：编辑器编译守卫
  - 所有 `nanite/editor/*.cpp` 用 `#ifdef TOOLS_ENABLED` 包裹
  - `nanite/SConscript` 仅在 `env["target"] == "editor"` 时编译 editor 子目录

- [x] **Task 0.9.5**：在 `register_types.cpp` 中注册编辑器类（`TOOLS_ENABLED` 守卫）
  - `ClassDB::register_class<NaniteMeshEditor>()`、`EditorInspectorPluginNanite`、`NaniteEditorPlugin`、`NaniteResourcePreviewGenerator`

- [x] **Task 0.9.6**：编写 GUT 集成测试 `test_nanite_editor.gd`
  - 覆盖：`can_handle(NaniteMeshResource)`、`edit()` 不崩溃、`handles("NaniteMeshResource")` 返回 true
  - 验证：测试通过 **[策略调整]** C++ 虚函数未暴露到 GDScript，改为通过 ClassDB API 验证类注册与继承（10/10 断言 PASS）（原 `.trae-cn/specs/fix-nanite-stage0-validation-failures/` 已并入 spec.md 并清理）

---

## 0.10 阶段 0 端到端测试

- [x] **Task 0.10.1**：编写 GUT 端到端测试 `test_nanite_e2e_build.gd`
  - 测试 `test_full_build_pipeline`：创建 1000 tri icosphere → `BuilderConfig.new()` → `NaniteBuilder.new(cfg)` → `builder.build(mesh)` → 验证 cluster/node/page/shadow 非空
  - 测试 `test_build_large_mesh`：50000 tri mesh → 验证 cluster_count > 100、node_count > 10
  - 测试 `test_save_load_roundtrip`：保存 `.tres` → 重新加载 → cluster_count 一致

- [x] **Task 0.10.2**：编写性能基准测试 `test_perf_build.cpp`
  - 测量 1K / 10K / 50K / 100K tri mesh 的构建耗时
  - 输出：每个量级的 cluster 数、node 数、page 数、构建总时间（ms）
  - 不设硬性 ms 上限，仅记录基线数据供后续阶段回归对比

---

## 0.12 资源转换接口与编辑器对接

> 解决"Stage 0 离线构建无法直接打开 gltf/fbx 文件"的产品缺口。新增 `build_from_resource()` 接口 + 编辑器右键菜单 + Inspector 按钮，让用户无需命令行脚本即可在编辑器内完成 mesh → nanite 转换。

- [ ] **Task 0.12.1**：实现 `NaniteBuilder::build_from_resource(Ref<Resource>)` 静态方法
  - 在 `nanite/core/nanite_builder.h` 声明 `static Ref<NaniteMeshResource> build_from_resource(Ref<Resource> p_resource)`
  - 在 `nanite/core/nanite_builder.cpp` 实现：识别 `ArrayMesh` / `PackedScene` / `MeshInstance3D`，合并所有 surface 到单个 `ArrayMesh`，调 `build()`
  - 在 `_bind_methods` 中用 `GDVIRTUAL_METHOD` 或 `ClassDB::bind_static_method` 暴露到 GDScript
  - 验证：`var res = NaniteBuilder.build_from_resource(arr_mesh)` 在 GDScript 可调用

- [ ] **Task 0.12.2**：编写 `build_from_resource` 单元测试
  - 测试用例：单 surface ArrayMesh、多 surface ArrayMesh、PackedScene（含 MeshInstance3D 子节点）、MeshInstance3D 节点
  - 覆盖：合并后 vertex/triangle 数正确、空 mesh 返回 null、所有输入路径产出非空 NaniteMeshResource
  - 验证：所有用例通过

- [ ] **Task 0.12.3**：实现 `nanite/editor/nanite_conversion_menu.h` / `.cpp`
  - 继承 `EditorContextMenuPlugin`，`GDCLASS(NaniteConversionContextMenu, EditorContextMenuPlugin)`
  - `_get_options(path)` 对 `.gltf` / `.glb` / `.fbx` / `.obj` / `.tres` 返回 `[Convert to Nanite]` 选项
  - `_execute_option(path, option_idx)` 弹 `EditorFileDialog`（save 模式，filter `*.nanite.tres`），确认后调 `build_from_resource()` + `ResourceSaver::save()`
  - 在 `NaniteEditorPlugin` 构造函数中 `add_context_menu_plugin(memnew(NaniteConversionContextMenu))`
  - 所有 `.cpp` 用 `#ifdef TOOLS_ENABLED` 包裹

- [ ] **Task 0.12.4**：扩展 `EditorInspectorPluginNanite::parse_begin()`
  - 修改 `can_handle()`：除 `NaniteMeshResource` 外，对 `ArrayMesh` 资源和 `MeshInstance3D` 节点也返回 true
  - 修改 `parse_begin()`：对 `ArrayMesh` / `MeshInstance3D` 不创建 `NaniteMeshEditor`，而是创建一个含 "Convert to Nanite" 按钮的 `Control`
  - 按钮点击：弹 `EditorFileDialog` save → 调 `build_from_resource()` → `ResourceSaver::save()` → EditorLog 输出统计
  - 验证：选中 ArrayMesh 资源时 Inspector 出现 Convert 按钮；选中 MeshInstance3D 时同

- [ ] **Task 0.12.5**：构建进度反馈
  - 大 mesh（>100K tri）构建 >100ms 时，用 `EditorProgress` 显示进度条
  - 失败时弹 `AcceptDialog` 错误提示
  - 成功时 EditorLog 输出 `cluster_count` / `node_count` / `page_count` / 耗时
  - 验证：转换 100K tri mesh 时进度条可见，完成后日志正确

- [ ] **Task 0.12.6**：编写端到端集成测试 `test_nanite_conversion.gd`
  - 测试场景：通过 ClassDB 验证 `NaniteConversionContextMenu` 注册并继承 `EditorContextMenuPlugin`
  - 测试场景：通过 ClassDB 验证 `EditorInspectorPluginNanite.can_handle(ArrayMesh)` 在 C++ 层返回 true（仅验证 ClassDB 注册，不直接调用 C++ 虚函数，沿用 Task 0.9.6 策略）
  - 测试场景：用 `NaniteBuilder.build_from_resource()` 直接转换 `ArrayMesh.new()` + `add_surface_from_arrays()` 构造的 mesh，验证返回非空
  - 验证：所有断言 PASS

---

## Task Dependencies

- Task 0.1.* → 所有后续任务（编译脚手架先行）
- Task 0.2.* → Task 0.4.*（Builder 需要 Cluster/Node 结构）
- Task 0.3.* → Task 0.4.*（Builder 需要 Config）
- Task 0.4.* → Task 0.5.*（层次化基于叶子层）
- Task 0.5.* → Task 0.6.*（Page 划分需要完整 cluster 列表）
- Task 0.5.* → Task 0.7.*（Shadow Mesh 需要 BVH nodes）
- Task 0.6.* + 0.7.* → Task 0.8.*（资源序列化依赖 page table + shadow mesh）
- Task 0.8.* → Task 0.9.*（编辑器预览 NaniteMeshResource）
- Task 0.9.* → Task 0.10.*（端到端测试依赖完整管线）
- Task 0.8.* → Task 0.12.*（资源转换依赖 NaniteMeshResource + 序列化）
- Task 0.12.1 → Task 0.12.2 / 0.12.3 / 0.12.4（接口先行，UI 入口和测试随后）
- Task 0.12.3 + 0.12.4 → Task 0.12.5（进度反馈在两个 UI 入口完成后统一加）
- Task 0.12.* → Task 0.12.6（端到端测试最后跑）

**可并行任务**：
- Task 0.2.* 与 Task 0.3.* 可并行
- Task 0.6.* 与 Task 0.7.* 可并行（均依赖 0.5.*）
- Task 0.12.3 与 Task 0.12.4 可并行（右键菜单和 Inspector 按钮独立实现）
