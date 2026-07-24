# Nanite 阶段 0：离线构建模块 Spec

> 基于 [总体设计文档](../../nanite_doc/nanite-overall-design.md) 第 8 章 与 [实施任务文档](../../nanite_doc/nanite-implementation-tasks.md) 阶段 0。

---

## Why

Nanite 的 GPU 渲染管线（阶段 1+）必须依赖一份预先构建好的**层次化 Cluster + BVH** 资源才能运行。Godot 4.7.1 当前没有任何 Nanite 数据生成能力——`modules/meshoptimizer/` 仅暴露 7 个 `SurfaceTool` 工具函数，未开放 meshlet/BVH/cluster 相关 API。

阶段 0 的目标是补齐这一前置依赖：在**不引入任何 GPU 渲染**的前提下，构建纯 CPU 数据管线 + 编辑器 UI，将原始 `ArrayMesh` 转化为可序列化的 `NaniteMeshResource`，并可在 Inspector 中预览与调试。该阶段完成后即可独立交付价值（mesh 优化、LOD 资产生产），同时为后续三阶段桥接提供统一的资源基础。

---

## What Changes

- **新增** `nanite/` 核心库目录（与 `modules/`、`servers/` 同级），由 SCons 顶层构建系统调用
- **新增** `NaniteCluster` / `NaniteClusterNode` 数据结构（POD + 序列化）
- **新增** `BuilderConfig`（`RefCounted` 子类，Inspector 可编辑）
- **新增** `NaniteBuilder` 离线构建器，直接调用 `thirdparty/meshoptimizer/meshoptimizer.h` C API（绕过 `SurfaceTool`），实现：
  - 顶点预处理（`meshopt_generateVertexRemap` / `optimizeVertexCache` / `optimizeVertexFetch`）
  - 叶子层聚类（`meshopt_buildMeshletsFlex` + `optimizeMeshletLevel` + `computeMeshletBounds`）
  - 自底向上层次化简化（`meshopt_partitionClusters` + `simplifyWithAttributes` + 再聚类）
  - BVH 线性化装配
  - Page 划分（按 LOD + Morton 排序后切页）
  - 粗 LOD Shadow Mesh 生成（供阶段 1 `mesh_set_shadow_mesh` 使用）
- **新增** `PagePacker` 负责按 `page_size_bytes` 切页
- **新增** `NaniteMeshResource`（`Resource` 子类），包含：
  - 顶点 / cluster / node / page-table 序列化字节流
  - `Ref<ArrayMesh> shadow_mesh` 引用
  - `Ref<BuilderConfig>` 构建参数回溯
  - Godot 原生 `ResourceSaver` / `ResourceLoader` 集成
- **新增** meshlet 编解码（`meshopt_encodeMeshlet` / `decodeMeshlet`）+ 顶点池编码（`meshopt_encodeVertexBuffer`）
- **新增** `nanite/editor/` 编辑器扩展：
  - `NaniteMeshEditor`（继承 `SubViewportContainer`，3D 旋转预览 + 构建统计 + 调试模式切换）
  - `EditorInspectorPluginNanite` + `NaniteEditorPlugin`
  - `NaniteResourcePreviewGenerator`（使用 `shadow_mesh` 生成 FileSystem 缩略图）
  - `NaniteConversionContextMenu`（继承 `EditorContextMenuPlugin`，在 FileSystem 对 `.gltf`/`.glb`/`.fbx`/`.obj`/`.tres` mesh 资源提供右键 "Convert to Nanite" 入口）
- **新增** `NaniteBuilder::build_from_resource(Ref<Resource>)` 静态方法，接受 `ArrayMesh` / `PackedScene` / `MeshInstance3D`，内部合并所有 surface 到单个 ArrayMesh 再调 `build()`
- **扩展** `EditorInspectorPluginNanite`：当被选中对象是 `ArrayMesh` 资源或 `MeshInstance3D` 节点时，`parse_begin()` 在 Inspector 顶部显示 "Convert to Nanite" 按钮
- **新增** `register_types.h/cpp` 模块注册（仅注册 `NaniteMeshResource` / `BuilderConfig`，**不注册** `NaniteMeshInstance3D`——该节点属于阶段 1）
- **新增** SCons 编译开关 `nanite_bridge=gdext|module|deep|all`，阶段 0 仅启用 `nanite/` 核心库编译（无桥接）
- **新增** C++ 单元测试（doctest）+ 编辑器集成测试（GUT）+ 性能基准测试脚手架

不修改任何 Godot 引擎源码——本阶段产物是**附加模块**，无 BREAKING 变更。

---

## Impact

- **Affected specs**: 阶段 1（GDExtension 桥接）依赖本阶段产物 `NaniteMeshResource`；阶段 2/3 复用同一资源格式
- **Affected code**:
  - 新增：`nanite/`（核心库）、`nanite/editor/`（编辑器扩展）、`nanite/tests/`（测试）
  - 修改：`SConstruct`（顶层添加 `nanite_bridge=` 开关与 `nanite/SConscript` 调用）
  - 不修改：`servers/`、`scene/`、`modules/meshoptimizer/`、`thirdparty/meshoptimizer/`
- **Affected docs**: `nanite_doc/nanite-implementation-tasks.md` 阶段 0 各子任务完成后勾选

---

## ADDED Requirements

### Requirement: Nanite 核心库目录与构建集成

系统 SHALL 在 `nanite/` 目录下组织所有 Nanite 核心代码，并通过 SCons 顶层 `nanite_bridge=` 开关控制桥接编译；阶段 0 仅编译核心库（`nanite/SConscript`），不引入任何桥接。

#### Scenario: 顶层 SConstruct 集成
- **WHEN** 开发者执行 `scons platform=windows target=editor`（未指定 `nanite_bridge`）
- **THEN** 默认 `nanite_bridge=gdext`，但因阶段 0 未实现桥接，仅 `nanite/SConscript` 被调用并编译核心库
- **AND** 编译产物包含 `NaniteBuilder`、`NaniteMeshResource`、`BuilderConfig` 等 ClassDB 类

#### Scenario: include 路径配置
- **WHEN** `nanite/SConscript` 执行
- **THEN** 编译环境的 `env.Append(CPPPATH=['#thirdparty/meshoptimizer'])` 被设置
- **AND** 核心库源码可 `#include <meshoptimizer.h>` 直接调用 meshoptimizer C API

---

### Requirement: NaniteCluster 与 NaniteClusterNode 数据结构

系统 SHALL 提供 `NaniteCluster` 与 `NaniteClusterNode` 两个 POD 结构体作为 Nanite 资源的最小数据单元，并支持二进制序列化往返。

#### Scenario: NaniteCluster 默认初始化
- **WHEN** 默认构造 `NaniteCluster c`
- **THEN** `c.vertex_count == 0`、`c.triangle_count == 0`、`c.error == 0.0f`、`c.group_id == 0`、`c.bounds == AABB()`

#### Scenario: NaniteClusterNode 叶子哨兵
- **WHEN** 默认构造 `NaniteClusterNode n`
- **THEN** `n.left_child == UINT32_MAX`、`n.right_child == UINT32_MAX`、`n.cluster_count == 0`

#### Scenario: 序列化往返一致性
- **WHEN** 任一 `NaniteCluster` / `NaniteClusterNode` 通过 `serialize()` 写入 `PackedByteArray` 后 `deserialize()` 回读
- **THEN** 所有字段（含 `AABB`、`Vector3`、`float`、`uint32_t`）数值精确匹配原值（浮点使用 `is_equal_approx` 比较）

---

### Requirement: BuilderConfig 参数定义与校验

系统 SHALL 提供 `BuilderConfig : public RefCounted`，包含 13 个可调参数（`max_vertices`、`max_triangles`、`partition_size`、`cone_weight`、`split_factor`、`simplification_ratio`、`target_error`、`max_lod_levels`、`page_size_bytes`、`lock_partition_border`、`meshlet_optimize_level`、`shadow_lod_depth`、`min_triangles`），并暴露 `is_valid()` 校验方法供 Inspector 编辑时反馈。

#### Scenario: 默认参数合法
- **WHEN** 默认构造 `BuilderConfig cfg`
- **THEN** `cfg.is_valid() == true`
- **AND** 默认值符合设计文档（`max_vertices=64`、`max_triangles=128`、`partition_size=4`、`page_size_bytes=65536`、`shadow_lod_depth=3`）

#### Scenario: 越界参数被拒绝
- **WHEN** `cfg.max_triangles = 0` 或 `cfg.max_vertices = 0` 或 `cfg.partition_size = 1`
- **THEN** `cfg.is_valid() == false`

#### Scenario: Inspector 可编辑
- **WHEN** 用户在 Inspector 中修改 `BuilderConfig` 子资源
- **THEN** Godot 通过 `GDCLASS` 绑定的 setter 触发 `is_valid()` 校验，非法值在 UI 上以红色高亮

---

### Requirement: NaniteBuilder 叶子层聚类

系统 SHALL 在 `NaniteBuilder::build_leaf_clusters()` 中按顺序调用 meshoptimizer：`meshopt_generateVertexRemap` → `meshopt_optimizeVertexCache` → `meshopt_optimizeVertexFetch` → `meshopt_buildMeshletsFlex` → 逐簇 `meshopt_optimizeMeshletLevel` + `meshopt_computeMeshletBounds`，产出 `LocalVector<NaniteCluster>`。

#### Scenario: 顶点去重
- **WHEN** 输入 mesh 含重复顶点
- **THEN** `preprocess_mesh()` 输出 `out_verts.size()` 小于输入 `vertices.size()`
- **AND** `out_indices.size()` 与输入 `indices.size()` 相同

#### Scenario: Cluster 满足约束
- **WHEN** 对 2000 tri 球体 mesh 调用 `build_leaf_clusters()`
- **THEN** 产出 clusters 数量 ≥ 8
- **AND** 每个 cluster 满足 `vertex_count <= cfg.max_vertices`、`triangle_count <= cfg.max_triangles`
- **AND** 除尾簇外每个 cluster 满足 `triangle_count >= cfg.min_triangles`（meshopt `meshopt_buildMeshletsFlex` 明确允许尾簇 `triangle_count < min_triangles`，避免最后一簇稀疏被强制补满）
- **AND** 每个 cluster 的 `error == 0.0f`（L0 叶子误差为 0）
- **AND** 每个 cluster 的 `bounds.has_volume() == true`

#### Scenario: 法线锥有效
- **WHEN** 叶子层构建完成
- **THEN** 每个 cluster 的 `cone_axis.length() > 0.9f`（单位向量）
- **AND** `cone_cutoff ∈ [-1.0, 1.0]`

#### Scenario: 退化法线锥的防御回退
- **WHEN** `meshopt_computeMeshletBounds` 返回退化的 `cone_axis = (0,0,0)`（length_squared == 0），常见于简化后具有相消法线的父 meshlet
- **THEN** builder SHALL 指定备用单位轴 `Vector3(0, 1, 0)` 到 `NaniteCluster::cone_axis`
- **AND** SHALL 设置 `NaniteCluster::cone_cutoff = -1.0f`（全球面锥，表示无法安全背面剔除）
- **AND** SHALL NOT 修改 bounds AABB（独立于法线锥计算）

> 该 Scenario 同时适用于 `build_leaf_clusters()` 与 `build_hierarchy()` 中的 cone_axis 赋值块。修复落地于 `nanite_builder.cpp:307-321`（leaf）与 `nanite_builder.cpp:739-753`（hierarchy）。（原 `.trae-cn/specs/fix-nanite-cone-axis-degenerate/` 修复 spec 已并入本 spec 并清理。）


---

### Requirement: NaniteBuilder 层次化简化与 BVH 装配

系统 SHALL 在 `NaniteBuilder::build_hierarchy()` 中自底向上循环：`meshopt_partitionClusters(target=4)` → 合并 partition → 计算 `vertex_lock` → `meshopt_simplifyWithAttributes(target=原/2, options=LockBorder|Regularize)` → `meshopt_buildMeshletsFlex` 再聚类 → 计算父节点 error/bounds；并在 `build_bvh()` 中将层次结构展开为线性 `NaniteClusterNode` 数组（`left_child`/`right_child` 用数组下标，叶子为 `UINT32_MAX`）。

#### Scenario: 多层 BVH 生成
- **WHEN** 对 8000 tri 球体（segments=64）调用 `builder.build(sphere)`
- **THEN** 返回的 `nodes.size() >= 3`
- **AND** 根节点（`nodes[0]`）的 `error` 大于等于所有其他节点的 `error`

#### Scenario: 父节点 bounds 包含子节点
- **WHEN** 遍历 BVH 内部节点
- **THEN** `nodes[n.left_child].bounds` 与 `nodes[n.right_child].bounds` 均被 `n.bounds` 包含（`is_inside()`）

#### Scenario: BVH 根节点 bounds 包含整个 mesh
- **WHEN** 取 `sphere->get_aabb()` 与 `nodes[0].bounds` 比较
- **THEN** mesh AABB 完全位于 BVH 根 bounds 内

#### Scenario: 简化降低三角形数
- **WHEN** 统计叶子层 vs 父层 cluster 的三角形总数
- **THEN** 叶子层三角形数 > 父层三角形数

#### Scenario: 父节点 error 单调
- **WHEN** 遍历内部节点 `n`
- **THEN** `n.error >= max(children.error)`（允许 0.001 容差）

---

### Requirement: PagePacker 切页

系统 SHALL 在 `PagePacker::pack()` 中按 LOD + 空间局部性（Morton 码）对 cluster 排序后，按 `page_size_bytes` 切页，为每个 cluster 分配 `page_id`。

#### Scenario: 所有 cluster 分配 page_id
- **WHEN** 输入 100 个 cluster
- **THEN** 每个 cluster 的 `page_id < table.page_count`

#### Scenario: Page 大小不超限
- **WHEN** 设置 `cfg.page_size_bytes = 4096`，输入 200 个 cluster
- **THEN** `table.page_count > 1`
- **AND** 每个 page 实际字节数 `<= cfg.page_size_bytes * 2`（允许少量溢出用于边界 cluster）

#### Scenario: 同 LOD cluster 倾向同页
- **WHEN** 输入 50 个同 `group_id=0` 的 cluster
- **THEN** 被分配到的不同 `page_id` 数量 ≤ 3

---

### Requirement: 粗 LOD Shadow Mesh 生成

系统 SHALL 在 `NaniteBuilder::build_shadow_mesh()` 中从 BVH 第 `shadow_lod_depth` 层取 cluster，展开 meshlet micro-index 为标准 triangle list，构造 `Ref<ArrayMesh>`。

#### Scenario: 三角形数减少
- **WHEN** 对 segments=64 球体构建并调用 `build_shadow_mesh(clusters, nodes)`
- **THEN** shadow mesh 三角形数 < 原 mesh 三角形数
- **AND** shadow mesh 三角形数 > 0

#### Scenario: Shadow mesh bounds 包含原 mesh
- **WHEN** 比较 `shadow->get_aabb()` 与 `sphere->get_aabb()`
- **THEN** 原 mesh AABB 完全位于 shadow AABB 内（容差 0.01）

> 修复记录：`build_shadow_mesh()` 仅选取单个 LOD 层 cluster 时，X 轴负方向覆盖不足导致测试失败。修复方案是在 `m_shadow_mesh->add_surface_from_arrays(...)` 之前向 `vertices` 数组追加原 mesh AABB 的 8 个角点作为"保底顶点"（不参与 `indices`，仅扩展 bounds）。落地于 `nanite_builder.cpp:1101-1136`。（原 `.trae-cn/specs/fix-nanite-stage0-validation-failures/` 修复 spec 已并入本 spec 并清理。）

#### Scenario: 可创建 RID
- **WHEN** 用 `RenderingServer::mesh_create()` 创建 RID 并添加 surface
- **THEN** RID 有效，不崩溃，可被 `mesh_set_shadow_mesh` 接受

---

### Requirement: NaniteMeshResource 序列化

系统 SHALL 提供 `NaniteMeshResource : public Resource`，支持两种序列化路径：(a) Godot 原生 `ResourceSaver`/`ResourceLoader`（`.tres` / `.res`）；(b) 自定义 `.nanite` 二进制格式（meshopt 编码）。

#### Scenario: Godot 资源往返
- **WHEN** `ResourceSaver::save(res, "res://test.tres")` 后 `ResourceLoader::load("res://test.tres")`
- **THEN** `loaded->cluster_count == res->cluster_count`
- **AND** `loaded->node_count == res->node_count`
- **AND** `loaded->page_count == res->page_count`

#### Scenario: .nanite 二进制往返
- **WHEN** `res->save("user://test.nanite")` 后 `loaded->load("user://test.nanite")`
- **THEN** `vertex_data` / `clusters_data` / `nodes_data` / `page_table_data` 字节长度精确匹配

#### Scenario: Meshlet 编解码一致
- **WHEN** 用 `meshopt_encodeMeshlet` 编码一个 cluster 后 `meshopt_decodeMeshlet` 解码
- **THEN** 解码后的 vertex / triangle 数据与原值精确匹配（顶点位置 `is_equal_approx`）

---

### Requirement: 编辑器预览与调试可视化

系统 SHALL 提供 `NaniteMeshEditor`（继承 `SubViewportContainer`）、`EditorInspectorPluginNanite`、`NaniteEditorPlugin`、`NaniteResourcePreviewGenerator`，使 `NaniteMeshResource` 在 Inspector 中可旋转预览、显示构建统计、切换调试模式。

#### Scenario: InspectorPlugin 识别资源
- **WHEN** 在 Inspector 中选中 `NaniteMeshResource`
- **THEN** `EditorInspectorPluginNanite::can_handle()` 返回 `true`
- **AND** Inspector 顶部出现 `NaniteMeshEditor` 预览面板

#### Scenario: 预览面板渲染不崩溃
- **WHEN** 调用 `NaniteMeshEditor::edit(res)`（res 为有效资源）
- **THEN** 不崩溃
- **AND** `stats_label` 显示 `cluster_count` / `node_count` / `page_count` / shadow mesh 三角形数 / 估算内存

#### Scenario: 资源缩略图生成
- **WHEN** FileSystem 面板需要 `NaniteMeshResource` 缩略图
- **THEN** `NaniteResourcePreviewGenerator::handles("NaniteMeshResource")` 返回 `true`
- **AND** `generate()` 使用 `shadow_mesh` 渲染缩略图（不启动 Nanite GPUPipeline）

#### Scenario: 焦点隔离
- **WHEN** `NaniteMeshEditor` 失去焦点
- **THEN** 全局 `NaniteServer::set_debug_mode(NONE)` 被调用，避免影响场景渲染

---

### Requirement: 阶段 0 端到端流程

系统 SHALL 支持完整的 导入 → 构建 → 预览 → 保存/加载 流程，可在不进入阶段 1 的前提下独立验证。

#### Scenario: 完整构建管线
- **WHEN** 在 GDScript 中 `BuilderConfig.new()` → `NaniteBuilder.new(cfg)` → `builder.build(mesh)`
- **THEN** 返回的 `NaniteMeshResource` 非空
- **AND** `cluster_count > 0`、`node_count > 0`、`page_count > 0`
- **AND** `shadow_mesh != null`

#### Scenario: 50K tri 大网格构建
- **WHEN** 输入约 50000 tri 测试 mesh
- **THEN** `cluster_count > 100`
- **AND** `node_count > 10`
- **AND** 构建过程在合理时间内完成（无硬性 ms 上限，但需可观察进度）

---

### Requirement: 资源转换接口与编辑器对接

系统 SHALL 提供从任意 Godot mesh 资源（`ArrayMesh` / `PackedScene` 即 gltf/glb/fbx 导入产物 / `MeshInstance3D` 节点）到 `NaniteMeshResource` 的转换入口，无需用户编写命令行脚本。转换由用户在编辑器中主动触发（不接入自动 import 流程，避免每次重新导入都重跑构建）。

#### Scenario: build_from_resource 接受 ArrayMesh
- **WHEN** GDScript 调用 `NaniteBuilder.build_from_resource(arr_mesh)`
- **AND** 输入是单 surface 的 `ArrayMesh`
- **THEN** 返回的 `NaniteMeshResource` 非空
- **AND** `cluster_count > 0`、`node_count > 0`、`page_count > 0`、`shadow_mesh != null`

#### Scenario: build_from_resource 合并多 surface
- **WHEN** 输入 `ArrayMesh` 有 N 个 surface (N ≥ 2)
- **THEN** 所有 surface 被合并到单个 vertex/index 池（按顶点偏移调整 index）
- **AND** 合并后的 vertex 数 == 各 surface vertex 数之和
- **AND** 合并后的 triangle 数 == 各 surface triangle 数之和
- **AND** 返回的 `NaniteMeshResource` 反映合并后的几何

#### Scenario: build_from_resource 接受 PackedScene
- **WHEN** 输入是 gltf/glb/fbx 导入后的 `PackedScene` 资源
- **THEN** 递归遍历场景树所有 `MeshInstance3D`
- **AND** 收集所有 mesh 的所有 surface 合并
- **AND** 调用 `build()` 返回 `NaniteMeshResource`

#### Scenario: build_from_resource 接受 MeshInstance3D 节点
- **WHEN** 输入是场景中的 `MeshInstance3D` 节点
- **THEN** 取 `mesh` 属性按 ArrayMesh 路径处理
- **AND** 不修改场景树本身（只读 mesh 数据）

#### Scenario: FileSystem 右键菜单转换
- **WHEN** 用户在 FileSystem Dock 右键 `.gltf` / `.glb` / `.fbx` / `.obj` / mesh `.tres` 文件
- **THEN** 上下文菜单出现 "Convert to Nanite" 项
- **AND** 点击后弹文件保存对话框（默认同名 `.nanite.tres`）
- **AND** 确认后调 `build_from_resource()` 构建并保存
- **AND** 进度条显示构建耗时（大模型 > 100ms 时必须有可见反馈，不能假装冻结）

#### Scenario: Inspector Convert 按钮
- **WHEN** 用户选中 `ArrayMesh` 资源或 `MeshInstance3D` 节点
- **THEN** Inspector 顶部出现 "Convert to Nanite" 按钮
- **AND** 点击后弹文件保存对话框
- **AND** 确认后构建并保存为 `.nanite.tres`
- **AND** 完成后输出到 EditorLog（cluster/node/page 数 + 耗时）

#### Scenario: 构建失败反馈
- **WHEN** 输入 mesh 为空（0 顶点或 0 索引）或合并后无几何
- **THEN** 弹错误对话框 "Nanite build failed: <reason>"
- **AND** 不写入文件
- **AND** EditorLog 输出错误详情

---

## MODIFIED Requirements

无。阶段 0 是新增模块，不修改任何现有 Godot 行为。

---

## REMOVED Requirements

无。
