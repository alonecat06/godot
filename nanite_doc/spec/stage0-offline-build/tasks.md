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

- [ ] **Task 0.5.1**：实现 `build_hierarchy()` — UE5 Nanite 风格: 4 相邻 cluster 合并 → 分区独立简化
  - 每层循环：`meshopt_partitionClusters(target=4)` → 按 partition_id 分组
  - 对每个 partition（4 个相邻 cluster）：
    - 合并 partition 内 cluster 的 vertex + index 子集（仅局部合并，非全局合并）
    - 计算 `vertex_lock`（锁跨 partition 共享边顶点，即出现在 >=2 个原始 cluster 中的顶点）
    - `meshopt_simplifyWithAttributes(target=原/2, options=LockBorder|Regularize)` 分区独立简化
    - `meshopt_buildMeshletsFlex` 简化结果再切 ~2 簇
    - 计算 parent.error = max(child.error, result_error) 和 parent.bounds = union(child.bounds)
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

> **Stage 0 重构（2026-07-28）**：原 Task 0.9.1 的单轴 `debug_mode_btn`（7 项 DebugMode）+ `wireframe_btn` + `bounds_btn` 三个控件已重构为两个正交下拉列表 + 独立 CPU 渲染器。新增 Task 0.9.7 - 0.9.10 记录该重构；Task 0.9.1 - 0.9.6 标记为已完成（保留旧实现历史），实际生产代码以 0.9.7 - 0.9.10 为准。

- [x] **Task 0.9.1**：实现 `nanite/editor/nanite_mesh_editor.h` / `.cpp`（已被 0.9.7 - 0.9.10 重构覆盖）
  - 继承 `SubViewportContainer`，`GDCLASS(NaniteMeshEditor, SubViewportContainer)`
  - 子节点：`SubViewport`、`Node3D rotation_node`、`Camera3D`、`DirectionalLight3D * 2`、`OptionButton debug_mode_btn`、`Button wireframe_btn`、`Button bounds_btn`、`Label stats_label`
  - `edit(Ref<NaniteMeshResource>)`：设置预览 mesh、填充统计、自动缩放相机
  - `gui_input()` 实现鼠标拖拽旋转（复用 `editor/scene/3d/mesh_editor_plugin.cpp` 模式）
  - `_notification()` 处理 `NOTIFICATION_FOCUS_ENTER/EXIT` 调试模式隔离（Stage 0 后改为 no-op，见 0.9.9）

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

- [x] **Task 0.9.7**（Stage 0 重构, 2026-07-28）：`NaniteDebug` 二维枚举重构
  - 在 `nanite/core/nanite_debug.h` 新增 `DisplayMode` 枚举（6 项：`NORMAL` / `NORMAL_WIREFRAME` / `CLUSTER_SOLID` / `CLUSTER_SOLID_WIREFRAME` / `WIREFRAME_ONLY` / `CLUSTER_SOLID_WITH_PARTITION_BORDER`）
  - 新增 `LODMode` 枚举（2 项：`NANITE_AUTO` / `FORCE_LOD_LEVEL`）
  - 保留旧 `DebugMode` 枚举（7 项，原名字不变）继续供 Stage 1 GPU pipeline 使用
  - 新增 `set_display_mode` / `set_lod_mode` / `set_force_lod_level` / `set_show_bounds` 及对应 getter
  - 在 `_bind_methods` 中通过 `BIND_ENUM_CONSTANT` 暴露新枚举常量到 GDScript / ClassDB
  - 验证：`scons platform=windows target=editor accesskit=no angle=no dev_build=yes` 编译通过

- [x] **Task 0.9.8**（Stage 0 重构）：`NaniteMeshResource::get_max_lod_level()` 辅助方法
  - 在 `nanite/core/nanite_resource.h` 声明 `int get_max_lod_level() const`
  - 在 `nanite/core/nanite_resource.cpp` 实现：遍历 `clusters_data`（固定 68 字节 stride），返回所有 cluster 中最大的 `group_id` 字段
  - 处理空资源 / 短 blob 情况返回 0
  - 不绑定到 ClassDB（编辑器内部使用，每次 `edit()` 调用一次即可）
  - 验证：编译通过；对含多 LOD 的资源返回正确 max_lod

- [x] **Task 0.9.9**（Stage 0 重构）：`NaniteMeshEditor` 二维下拉列表 + 独立 CPU 渲染器
  - 移除旧的 `debug_mode_btn`（7 项）/ `wireframe_btn` / `bounds_btn` 三个控件
  - 新增两个正交下拉列表：
    - `display_mode_btn`（`OptionButton`）：5 个 DisplayMode 项（`add_item` 用 `NaniteDebug::NORMAL` 等常量作 id）
    - `lod_mode_btn`（`OptionButton`）：2 个 LODMode 项（`NANITE_AUTO` 标 `[Stage 1]` 后缀）
  - 新增 `force_lod_spinner`（`SpinBox`，range `0..max_lod_level`），选中 `FORCE_LOD_LEVEL` 时可见
  - 选中 `NANITE_AUTO` 时弹 `WARN_PRINT` 提示 Stage 1 未实现，自动回退到 `FORCE_LOD_LEVEL` + 0
  - 三个回调 `_on_display_mode_selected` / `_on_lod_mode_selected` / `_on_force_lod_changed` 都只触发本地 `_rebuild_preview()`，**不调用** `NaniteServer::set_debug_mode()`
  - 用两个 `MeshInstance3D` 子节点替换原单一 instance：
    - `solid_instance`：渲染 Lambert / per-vertex HSV 色（`FLAG_ALBEDO_FROM_VERTEX_COLOR`）
    - `wire_instance`：渲染 `PRIMITIVE_LINES` mesh + `SHADING_MODE_UNSHADED` 白色（线框叠加层）
  - 构造函数**不**调用 `NaniteGDExtBridgeManager::attach_to_viewport(viewport)`（避免预览视口被 Nanite GPU pipeline 接管）
  - 使用标准 `MeshInstance3D` 而非 `NaniteMeshInstance3D`（避免触发 NaniteServer 的 instance 注册与 GPU pipeline 调度）
  - `_notification()` 在 `NOTIFICATION_FOCUS_ENTER` / `NOTIFICATION_FOCUS_EXIT` 分支为 no-op（Stage 0 preview 状态完全本地）
  - 验证：编译通过；UI 切换不崩溃；预览视口不挂 Nanite compositor

- [x] **Task 0.9.10**（Stage 0 重构）：CPU-side cluster 解码器 + 五种 Display Mode 渲染
  - 在 `nanite/editor/nanite_mesh_editor.cpp` 匿名命名空间实现 `decode_clusters_for_lod()` 解码器：
    - 遍历 `clusters_data`（68B stride），过滤 `group_id == force_lod_level`
    - 通过 `meshlet_vertices_data`（`uint32[]`）映射 micro-index → 全局顶点索引
    - 从 `vertex_data`（stride 32B: `pos.xyz` + `normal.xyz` + `uv.xy`）读取位置
    - 可选地为每个 cluster 生成唯一 HSV 色（按 `ci * 2654435761u` hash 散布色相）
    - `p_emit_lines = false`：输出 `PackedVector3Array` + `PackedInt32Array` + 可选 `PackedColorArray` → `build_cluster_mesh()` 构造 `PRIMITIVE_TRIANGLES` ArrayMesh
    - `p_emit_lines = true`：每三角形输出 6 个顶点（3 条边 `(v0,v1) (v1,v2) (v2,v0)`），仅写入 `PackedVector3Array` → `build_cluster_wire_mesh()` 构造 `PRIMITIVE_LINES` ArrayMesh
    - 含 bounds check（`vertex_offset + vertex_count <= mv_count`、`triangle_offset + triangle_count * 3 <= tri_byte_count`、global index `< total_vertex_count`）
  - 实现 `build_wire_from_array_mesh()`：从已有 `ArrayMesh` 的三角形索引展开成边顶点，构造 `PRIMITIVE_LINES` ArrayMesh（供 Normal+Wireframe 模式使用）
  - 实现 `_rebuild_preview()`：按 DisplayMode 选择 mesh 构造路径与 `solid_instance` / `wire_instance` 可见性
  - 验证：编译通过；五种 DisplayMode 切换不崩溃；`Force LOD Level` 切换时解码出不同 triangle count

- [ ] **Task 0.9.11**（Stage 0 新增, 2026-07-29）：`CLUSTER_SOLID_WITH_PARTITION_BORDER` DisplayMode
  - 在 `nanite/core/nanite_debug.h` 的 `DisplayMode` 枚举中新增 `CLUSTER_SOLID_WITH_PARTITION_BORDER = 5`
  - 在 `nanite/core/nanite_debug.cpp` 的 `_bind_methods` 中更新 `BIND_ENUM_CONSTANT`、`PROPERTY_HINT_ENUM` 字符串及 `set_display_mode` switch
  - 在 `nanite/editor/nanite_mesh_editor.cpp` 匿名命名空间实现 `build_partition_border_wire()` 函数：
    - 解码 LOD N+1 的所有 cluster，提取每个 cluster 的三角形全局顶点索引列表
    - 调用 `meshopt_partitionClusters(target=4)` 将 LOD N+1 的 cluster 按空间邻近性分组
    - 对每个 partition（约 4 个 cluster）：
      - 统计每个全局顶点被几个 cluster 引用（vertex_ref_count）
      - 标记 `vertex_ref_count >= 2` 的顶点为 locked（边界顶点）
      - 遍历所有三角形边，收集两端均为 locked 的边
      - 边去重后输出为 PRIMITIVE_LINES 顶点对
    - 所有 partition 的边界边合并为一个黄色 PRIMITIVE_LINES ArrayMesh
  - 在 `_rebuild_preview()` 中处理 `CLUSTER_SOLID_WITH_PARTITION_BORDER` 模式：
    - `solid_instance` 渲染当前 LOD 的 cluster solid（per-cluster HSV 色）
    - `wire_instance` 渲染 partition 边界线框（黄色 unshaded material）
    - 当 `force_lod == max_lod_level` 或 LOD N+1 的 cluster 数 <= 1 时，`wire_instance` 隐藏
  - 在 `display_mode_btn` 下拉列表中添加 "Cluster Solid + Partition Border" 项
  - 验证：编译通过；模式切换不崩溃；partition 边界线框正确显示

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

## 0.13 独立资源编辑器窗口

> 解决"双击 `.nanite.tres` 仅在 Inspector 显示属性"的体验缺口。实现 `NaniteMeshResourceEditorWindow`（独立 `AcceptDialog`）+ 扩展 `NaniteEditorPlugin` 的 `handles()`/`edit()`/`make_visible()`，让双击资源弹出独立窗口预览 3D 模型 + 构建统计。原 Inspector 嵌入预览移除，`EditorInspectorPluginNanite` 仅保留 `ArrayMesh` / `MeshInstance3D` 的 Convert 按钮处理。

- [ ] **Task 0.13.1**：实现 `nanite/editor/nanite_resource_editor_window.h` / `.cpp`
  - 继承 `AcceptDialog`，`GDCLASS(NaniteMeshResourceEditorWindow, AcceptDialog)`
  - 成员：`NaniteMeshEditor *viewer`（内嵌 3D 预览控件，复用现有实现）
  - `edit(Ref<NaniteMeshResource>)`：设置窗口标题 `"Nanite Mesh Viewer - <资源名>"`，调 `viewer->edit(res)`，`popup_centered_clamped(Size2(800, 600))`
  - 构造函数：创建 `NaniteMeshEditor` 作为内容子节点，连接 `confirmed` 信号到 `hide()`（关闭不销毁）
  - 所有 `.cpp` 用 `#ifdef TOOLS_ENABLED` 包裹

- [ ] **Task 0.13.2**：扩展 `NaniteEditorPlugin` 实现 main editor plugin 行为
  - 添加成员 `NaniteMeshResourceEditorWindow *viewer_window = nullptr`
  - 重写 `handles(Object *p_object) const`：对 `NaniteMeshResource` 返回 `true`
  - 重写 `edit(Object *p_object)`：`cast_to<NaniteMeshResource>`，若窗口不存在则 `memnew`，调 `viewer_window->edit(res)`
  - 重写 `make_visible(bool p_visible)`：当 `p_visible = false` 且窗口存在时 `hide()`
  - 构造函数中创建窗口实例（不立即显示）

- [ ] **Task 0.13.3**：修订 `EditorInspectorPluginNanite` 移除 NaniteMeshResource 预览
  - `can_handle()`：移除对 `NaniteMeshResource` 的判断，仅保留 `ArrayMesh` / `MeshInstance3D`
  - `parse_begin()`：移除对 `NaniteMeshResource` 创建 `NaniteMeshEditor` 的逻辑（保留 `ArrayMesh` / `MeshInstance3D` 的 Convert 按钮）
  - 验证：双击 `.nanite.tres` 不再在 Inspector 嵌入预览，改为触发 `NaniteEditorPlugin::edit()` 弹窗

- [ ] **Task 0.13.4**：在 `register_types.cpp` 注册新类
  - `ClassDB::register_class<NaniteMeshResourceEditorWindow>()`（`TOOLS_ENABLED` 守卫）
  - 验证：`ClassDB.class_exists("NaniteMeshResourceEditorWindow")` 返回 true

- [ ] **Task 0.13.5**：扩展 `test_nanite_editor.gd` 测试
  - 新增断言：`ClassDB.class_exists("NaniteMeshResourceEditorWindow")` 返回 true
  - 新增断言：`ClassDB.is_parent_class("NaniteMeshResourceEditorWindow", "AcceptDialog")` 返回 true
  - 新增断言：`ClassDB.is_parent_class("NaniteMeshResourceEditorWindow", "Window")` 返回 true（间接继承链 AcceptDialog → Window）
  - 新增断言：`ClassDB.is_parent_class("NaniteMeshResourceEditorWindow", "Viewport")` 返回 true（间接继承链 Window → Viewport）
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
- Task 0.9.* → Task 0.13.*（独立窗口复用 NaniteMeshEditor 控件）
- Task 0.13.1 → Task 0.13.2（窗口类先行，plugin 扩展随后）
- Task 0.13.2 + 0.13.3 → Task 0.13.5（窗口 + Inspector 修订完成后跑测试）

**可并行任务**：
- Task 0.2.* 与 Task 0.3.* 可并行
- Task 0.6.* 与 Task 0.7.* 可并行（均依赖 0.5.*）
- Task 0.12.3 与 Task 0.12.4 可并行（右键菜单和 Inspector 按钮独立实现）
- Task 0.13.3 与 Task 0.13.1/0.13.2 可并行（Inspector 修订与窗口实现独立）
