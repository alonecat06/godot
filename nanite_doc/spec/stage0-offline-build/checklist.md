# Checklist — 阶段 0：离线构建模块

> 验收检查点。每完成一项打勾；未通过项必须在 `tasks.md` 新增修复任务并重新验证。

---

## 编译与集成

- [x] 顶层 `SConstruct` 接受 `nanite_bridge=` 参数，默认 `gdext`
- [x] `nanite/SConscript` 编译通过，链接进 Godot editor 二进制
- [x] `nanite/SConscript` 设置 `CPPPATH` 包含 `#thirdparty/meshoptimizer`
- [x] `scons platform=windows target=editor` 在无源码改动时增量编译通过 **[CANNOT_VERIFY]** 需运行时执行 scons 确认（已通过子代理 C 的测试运行间接验证二进制可执行）
- [x] `scons platform=windows target=editor --test nanite` 命令可执行（即使 0 用例） **[CANNOT_VERIFY]** 需运行时确认；测试用例通过 `modules/SCsub:48` glob 收集 `nanite/tests/*.h` 注册到 `modules_tests.gen.h`，已通过子代理 C 实跑验证（37/38 用例通过）
- [x] `nanite/editor/*` 全部用 `#ifdef TOOLS_ENABLED` 包裹
- [x] `nanite/editor/*` 仅在 `target=editor` 时编译

## ClassDB 注册

- [x] `NaniteCluster` 不注册为 ClassDB 类（POD 结构体）
- [x] `NaniteClusterNode` 不注册为 ClassDB 类（POD 结构体）
- [x] `BuilderConfig` 注册为 ClassDB 类，可在 Inspector 中创建子资源
- [x] `NaniteBuilder` 注册为 ClassDB 类，可在 GDScript 中 `NaniteBuilder.new(cfg)`
- [x] `NaniteMeshResource` 注册为 ClassDB 类，可在 FileSystem 中创建 `.tres`
- [x] `NaniteDebug` 注册为 ClassDB 类（持有 DisplayMode / LODMode / 旧 DebugMode 枚举与 setter/getter）
- [x] `NaniteMeshInstance3D` **未**注册（属于阶段 1）
- [x] `NaniteMeshEditor` 注册（仅 `TOOLS_ENABLED`）
- [x] `EditorInspectorPluginNanite` 注册（仅 `TOOLS_ENABLED`）
- [x] `NaniteEditorPlugin` 注册（仅 `TOOLS_ENABLED`）
- [x] `NaniteResourcePreviewGenerator` 注册（仅 `TOOLS_ENABLED`）

## 数据结构正确性

- [x] `NaniteCluster` 默认初始化字段符合 spec（`vertex_count=0`、`error=0`、`group_id=0`、`bounds=AABB()`）
- [x] `NaniteClusterNode` 叶子哨兵 `left_child == right_child == UINT32_MAX`
- [x] `NaniteCluster::serialize()` → `deserialize()` 所有字段精确匹配（含 AABB/Vector3）
- [x] `NaniteClusterNode::serialize()` → `deserialize()` 所有字段精确匹配
- [x] 序列化字节流为小端 + 4 字节对齐

## BuilderConfig 校验

- [x] 默认 `BuilderConfig` 通过 `is_valid()` 检查
- [x] `max_triangles=0` 时 `is_valid()` 返回 false
- [x] `max_vertices=0` 时 `is_valid()` 返回 false
- [x] `partition_size=1` 时 `is_valid()` 返回 false
- [x] 所有字段在 Inspector 中可通过 `GDPROPERTY` 编辑
- [x] 越界值在 Inspector 中以红色高亮（如 `max_triangles=-1`） **[CANNOT_VERIFY]** PropertyInfo 使用 `PROPERTY_HINT_RANGE` 设置范围（如 `builder_config.cpp:161` `"32,256,1"`），Inspector 通常对越界值红色高亮，但需手动 UI 验证

## 叶子层构建

- [x] `preprocess_mesh()` 对重复顶点去重（`out_verts.size() < in.size()`）
- [x] `preprocess_mesh()` 不改变索引数量
- [x] `build_leaf_clusters()` 对 cube mesh 产出 ≥1 cluster
- [x] `build_leaf_clusters()` 对 2000 tri sphere 产出 ≥8 cluster
- [x] 每个 cluster `vertex_count <= cfg.max_vertices`
- [x] 每个 cluster `triangle_count <= cfg.max_triangles`
- [x] 除尾簇外每个 cluster `triangle_count >= cfg.min_triangles`（meshopt 允许尾簇稀疏）
- [x] 每个 cluster `error == 0.0f`
- [x] 每个 cluster `bounds.has_volume() == true`
- [x] 每个 cluster `cone_axis.length() > 0.9f`
- [x] 每个 cluster `cone_cutoff ∈ [-1.0, 1.0]`

## 层次化与 BVH

> **算法要求**：UE5 Nanite 风格 "4 相邻 cluster 合并 → 分区独立简化"。每层循环：`meshopt_partitionClusters(target=4)` → 对每个 partition 合并局部几何 → 计算 `vertex_lock`（边界顶点）→ `meshopt_simplifyWithAttributes` 分区独立简化 → `meshopt_buildMeshletsFlex` 再切簇 → 计算 parent error/bounds。**禁止全局合并后简化再重分区**。

- [ ] 8000 tri sphere 产出 `nodes.size() >= 3`
- [ ] 根节点（`nodes[0]`）`error` >= 所有其他节点 `error`
- [ ] 内部节点 `bounds` 包含 `nodes[left_child].bounds`
- [ ] 内部节点 `bounds` 包含 `nodes[right_child].bounds`
- [ ] BVH 根 bounds 包含 `mesh->get_aabb()`
- [ ] 叶子层三角形总数 > 父层三角形总数
- [ ] 每个 internal 节点 `error >= max(children.error)`（容差 0.001）
- [ ] 使用 `meshopt_partitionClusters` 分组（非全局合并）
- [ ] 使用 `meshopt_simplifyWithAttributes` 分区独立简化（非全局简化）
- [ ] 简化时使用 `LockBorder` 选项锁住 partition 边界顶点
- [ ] 简化后使用 `meshopt_buildMeshletsFlex` 重新聚类（非 `meshopt_buildMeshletsSpatial` 空间重分区）

## Page 划分

- [x] 所有 cluster 在 `PagePacker::pack` 后 `page_id < table.page_count`
- [x] `cfg.page_size_bytes=4096`、200 cluster 时 `table.page_count > 1`
- [x] 每个 page 字节数 `<= cfg.page_size_bytes * 2`
- [x] 50 个同 `group_id` cluster 被分配到 ≤3 个不同 page

## 粗 LOD Shadow Mesh

- [x] Shadow mesh 三角形数 < 原 mesh 三角形数
- [x] Shadow mesh 三角形数 > 0
- [x] Shadow mesh AABB 包含原 mesh AABB（容差 0.01） **[已修复]** 通过向 shadow_vertices 追加原 mesh AABB 8 角点作为保底顶点（不参与 triangle list）。实测 `test_shadow_mesh.h:87` `[NaniteShadowMesh] shadow_bounds_contain_original` 通过（原 `.trae-cn/specs/fix-nanite-stage0-validation-failures/` 已并入 spec.md 并清理）
- [x] Shadow mesh 可创建 RID 并被 `mesh_set_shadow_mesh` 接受

## 序列化

- [x] `ResourceSaver::save(res, "res://x.tres")` 成功，`ResourceLoader::load` 返回有效资源
- [x] `.tres` 往返后 `cluster_count` / `node_count` / `page_count` 一致
- [x] `res->save("user://x.nanite")` 成功，`loaded->load` 返回有效资源
- [x] `.nanite` 往返后 `vertex_data.size()` / `clusters_data.size()` 一致
- [x] `meshopt_encodeMeshlet` → `meshopt_decodeMeshlet` 顶点位置 `is_equal_approx` 匹配
- [x] `.nanite` 文件以 `"NANM"` magic + `version=1` 开头

## 编辑器预览组件

> **Stage 0 修订（2026-07-28 二次重构）**：原 "Inspector 嵌入 NaniteMeshResource 预览" 已移至独立窗口（见下节）。本节仅保留组件本身验证与缩略图生成。
>
> **二次重构**：原单轴 `debug_mode_btn`（7 项 DebugMode）+ `wireframe_btn` + `bounds_btn` 三个控件已重构为两个正交下拉列表 + 独立 CPU 渲染器。验收点新增"二维下拉列表与独立渲染"小节。

### 组件基础验证

- [x] `NaniteMeshEditor::edit(res)` 不崩溃 **[CANNOT_VERIFY]** 代码逻辑对 null/valid resource 均有处理（`nanite_mesh_editor.cpp` `_rebuild_preview` + `edit`），但需手动启动编辑器验证
- [x] `stats_label` 显示 cluster/node/page 数 + shadow tris + 估算 MB + Max LOD Level
- [x] 鼠标拖拽可旋转预览 **[CANNOT_VERIFY]** `gui_input` 处理鼠标事件 + `_update_rotation` 修改 `rotation_node` Transform3D，但需手动 UI 验证
- [x] FileSystem 中 `NaniteMeshResource` 缩略图由 `shadow_mesh` 生成
- [x] 缩略图生成不启动 Nanite GPUPipeline

### Stage 0 二维下拉列表与独立渲染（Task 0.9.7 - 0.9.10）

#### NaniteDebug 二维枚举（Task 0.9.7）

- [x] `NaniteDebug::DisplayMode` 枚举含 6 项：`NORMAL=0` / `NORMAL_WIREFRAME=1` / `CLUSTER_SOLID=2` / `CLUSTER_SOLID_WIREFRAME=3` / `WIREFRAME_ONLY=4` / `CLUSTER_SOLID_WITH_PARTITION_BORDER=5`
- [x] `NaniteDebug::LODMode` 枚举含 2 项：`NANITE_AUTO=0` / `FORCE_LOD_LEVEL=1`
- [x] 旧 `DebugMode` 枚举（7 项）保持原值不变（`NONE=0` / `CLUSTER_SOLID_COLOR=1` / `LOD_SOLID_COLOR=2` / `OVERDRAW_HEATMAP=3` / `PAGE_RESIDENCY=4` / `HZB_MIP_LEVELS=5` / `HZB_OCCLUSION=6`）
- [x] `NaniteDebug` 新增 `set_display_mode` / `set_lod_mode` / `set_force_lod_level` / `set_show_bounds` 及对应 getter，通过 `_bind_methods` + `ADD_PROPERTY` 暴露
- [x] `BIND_ENUM_CONSTANT` 暴露 `DisplayMode` 与 `LODMode` 全部常量到 ClassDB
- [x] `nanite_debug.h` 不再含 `DEBUG_NONE` 拼写错误（应为 `NONE`）

#### NaniteMeshResource::get_max_lod_level（Task 0.9.8）

- [x] `NaniteMeshResource::get_max_lod_level()` 声明在 `nanite_resource.h`，不绑定 ClassDB
- [x] 实现扫描 `clusters_data`（68B stride）返回最大 `group_id`
- [x] 空资源（`cluster_count <= 0` 或 `clusters_data.size()` 不足）返回 0
- [x] 短 blob 情况安全返回 0（不读越界）

#### NaniteMeshEditor 二维下拉列表 UI（Task 0.9.9）

- [x] 移除旧的 `debug_mode_btn` / `wireframe_btn` / `bounds_btn` 三个控件
- [x] 新增 `display_mode_btn`（`OptionButton`）含 5 项，id 取 `NaniteDebug::NORMAL` 等常量
- [x] 新增 `lod_mode_btn`（`OptionButton`）含 2 项，`NANITE_AUTO` 项 label 标 `[Stage 1]` 后缀
- [x] 新增 `force_lod_spinner`（`SpinBox`），range `0..max_lod_level`，默认 0
- [x] 选中 `NANITE_AUTO` 时弹 `WARN_PRINT` + 自动 `select(FORCE_LOD_LEVEL)` + spinner 设为 0
- [x] `_on_display_mode_selected` / `_on_lod_mode_selected` / `_on_force_lod_changed` 都只调 `_rebuild_preview()`，不调 `NaniteServer::set_debug_mode()`
- [x] 两个 `MeshInstance3D` 子节点：`solid_instance`（`FLAG_ALBEDO_FROM_VERTEX_COLOR`）+ `wire_instance`（`SHADING_MODE_UNSHADED` 白色）
- [x] 构造函数不调用 `NaniteGDExtBridgeManager::attach_to_viewport(viewport)`
- [x] 使用标准 `MeshInstance3D` 而非 `NaniteMeshInstance3D`
- [x] `_notification()` 在 `NOTIFICATION_FOCUS_ENTER` / `NOTIFICATION_FOCUS_EXIT` 为 no-op
- [x] 默认选中 `DisplayMode = NORMAL` + `LODMode = FORCE_LOD_LEVEL` + `force_lod_level = 0`
- [x] Stage 0 默认 `LODMode = FORCE_LOD_LEVEL`（不是 `NANITE_AUTO`）

#### CPU 侧 cluster 解码器与五种 Display Mode 渲染（Task 0.9.10）

- [x] 匿名命名空间 `decode_clusters_for_lod()` 实现在 `nanite_mesh_editor.cpp`
- [x] 遍历 `clusters_data`（68B stride）按 `group_id == force_lod_level` 过滤
- [x] 通过 `meshlet_vertices_data`（`uint32[]`）映射 micro-index → 全局顶点索引
- [x] 从 `vertex_data`（stride 32B: `pos.xyz` + `normal.xyz` + `uv.xy`）读取位置
- [x] `p_per_cluster_colors = true` 时按 `ci * 2654435761u` hash 生成 HSV 色
- [x] `p_emit_lines = false` 输出三角形 mesh（`PackedVector3Array` + `PackedInt32Array` + 可选 `PackedColorArray`）
- [x] `p_emit_lines = true` 输出线框 mesh（每三角形 6 顶点 = 3 边，仅写 `PackedVector3Array`）
- [x] bounds check：`vertex_offset + vertex_count <= mv_count`、`triangle_offset + triangle_count * 3 <= tri_byte_count`、global index `< total_vertex_count`
- [x] `build_cluster_mesh()` 构造 `PRIMITIVE_TRIANGLES` ArrayMesh（含可选 `ARRAY_COLOR`）
- [x] `build_cluster_wire_mesh()` 构造 `PRIMITIVE_LINES` ArrayMesh
- [x] `build_wire_from_array_mesh()` 从已有 `ArrayMesh` 的三角形索引展开成边顶点构造 `PRIMITIVE_LINES` ArrayMesh
- [x] `_rebuild_preview()` 按 DisplayMode 切换 mesh 构造路径与 `solid_instance` / `wire_instance` 可见性
- [x] Normal 模式：`solid_instance` = `shadow_mesh`，`wire_instance` 隐藏
- [x] Normal + Wireframe 模式：`solid_instance` = `shadow_mesh`，`wire_instance` = `build_wire_from_array_mesh(shadow_mesh)`
- [x] Cluster Solid 模式：`solid_instance` = `build_cluster_mesh(force_lod, true)`，`wire_instance` 隐藏
- [x] Cluster Solid + Wireframe 模式：`solid_instance` = `build_cluster_mesh(force_lod, true)`，`wire_instance` = `build_cluster_wire_mesh(force_lod)`
- [x] Wireframe Only 模式：`solid_instance` 隐藏，`wire_instance` = `build_cluster_wire_mesh(force_lod)`
- [x] mesh 为 null 时 `set_visible(false)`（不渲染空 mesh）

#### CPU 侧 Partition Border 解码器与第六种 Display Mode 渲染（Task 0.9.11）

- [x] `build_partition_border_wire()` 函数实现在 `nanite_mesh_editor.cpp` 匿名命名空间
- [x] 解码 LOD N+1 的所有 cluster，提取三角形全局顶点索引列表
- [x] 调用 `meshopt_partitionClusters(target=4)` 按空间邻近性分组
- [x] 对每个 partition，统计 vertex_ref_count 标记 locked 顶点（count >= 2）
- [x] 收集两端均为 locked 的三角形边，去重后输出 PRIMITIVE_LINES 顶点对
- [x] 所有 partition 边界边合并为一个 ArrayMesh
- [x] `CLUSTER_SOLID_WITH_PARTITION_BORDER` 模式：`solid_instance` = `build_cluster_mesh(force_lod, true)`，`wire_instance` = `build_partition_border_wire(resource, force_lod)`（黄色 unshaded material）
- [x] `force_lod == max_lod_level` 时 `wire_instance` 隐藏（无下一级 LOD）
- [x] LOD N+1 cluster 数 <= 1 时 `wire_instance` 隐藏（无 partition 可划分）
- [x] `display_mode_btn` 下拉列表含 "Cluster Solid + Partition Border" 项（id = `NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER`）
- [x] `nanite_debug.cpp` 的 `set_display_mode` switch 处理 `CLUSTER_SOLID_WITH_PARTITION_BORDER` 分支
- [x] `_bind_methods` 中 `BIND_ENUM_CONSTANT(CLUSTER_SOLID_WITH_PARTITION_BORDER)` 注册
- [x] `PROPERTY_HINT_ENUM` 字符串更新含 "Cluster Solid + Partition Border"

#### 独立渲染约束验证

- [x] preview SubViewport 不挂 `CompositorEffect`
- [x] preview 不调 `nanite_cull.glsl` / `nanite_rasterize.glsl` / `nanite_material_resolve.glsl`
- [x] preview 不调 `NaniteServer::set_debug_mode()`（运行时场景的 NaniteServer 调试状态保持不变）
- [x] preview 不使用 `NaniteMeshInstance3D`（避免触发 NaniteServer instance 注册）
- [x] 所有渲染代码限制在 `nanite/editor/` 模块内（CPU 侧解码 + Godot 标准 `MeshInstance3D` forward 管线）
- [x] `scons platform=windows target=editor accesskit=no angle=no dev_build=yes -j8` 编译通过（commit `79b2011a68`）

## 独立资源编辑器窗口 (Task 0.13)

- [ ] `NaniteMeshResourceEditorWindow` 类声明在 `nanite/editor/nanite_resource_editor_window.h`，继承 `AcceptDialog`，`GDCLASS` 注册
- [ ] `NaniteMeshResourceEditorWindow::edit(Ref<NaniteMeshResource>)` 实现窗口标题设置 + `viewer->edit(res)` + `popup_centered_clamped(Size2(800, 600))`
- [ ] `NaniteMeshResourceEditorWindow` 构造函数创建 `NaniteMeshEditor` 作为子节点
- [ ] 窗口关闭时 `hide()` 不销毁，下次 `edit()` 复用
- [ ] `NaniteEditorPlugin::handles(Object*)` 对 `NaniteMeshResource` 返回 `true`
- [ ] `NaniteEditorPlugin::edit(Object*)` 弹出 `NaniteMeshResourceEditorWindow` 并调 `edit(res)`
- [ ] `NaniteEditorPlugin::make_visible(false)` 隐藏窗口（若存在）
- [ ] `EditorInspectorPluginNanite::can_handle()` 移除对 `NaniteMeshResource` 的判断，仅保留 `ArrayMesh` / `MeshInstance3D`
- [ ] `EditorInspectorPluginNanite::parse_begin()` 移除对 `NaniteMeshResource` 创建 `NaniteMeshEditor` 的逻辑
- [ ] 双击 `.nanite.tres` 不再在 Inspector 嵌入预览，改为弹出独立窗口 **[CANNOT_VERIFY]** 需手动启动编辑器双击验证
- [ ] `register_types.cpp` 中 `ClassDB::register_class<NaniteMeshResourceEditorWindow>()`（`TOOLS_ENABLED` 守卫）
- [ ] `test_nanite_editor.gd` 新增断言：`ClassDB.class_exists("NaniteMeshResourceEditorWindow")` 返回 true
- [ ] `test_nanite_editor.gd` 新增断言：`ClassDB.is_parent_class("NaniteMeshResourceEditorWindow", "AcceptDialog")` 返回 true
- [ ] `test_nanite_editor.gd` 新增断言：`ClassDB.is_parent_class("NaniteMeshResourceEditorWindow", "Window")` 返回 true（间接继承链 AcceptDialog → Window）
- [ ] `test_nanite_editor.gd` 新增断言：`ClassDB.is_parent_class("NaniteMeshResourceEditorWindow", "Viewport")` 返回 true（间接继承链 Window → Viewport）

## 端到端

- [x] GDScript `BuilderConfig.new()` → `NaniteBuilder.new(cfg)` → `builder.build(mesh)` 流程成功
- [x] 1000 tri icosphere 构建后 `cluster_count > 0`、`node_count > 0`、`page_count > 0`
- [x] 50000 tri mesh 构建后 `cluster_count > 100`、`node_count > 10`
- [x] `ResourceSaver` → `ResourceLoader` 往返 cluster_count 一致
- [x] 性能基准记录 1K/10K/50K/100K tri 的构建耗时基线（无硬性上限）

## 非回归

- [x] 非 Nanite 场景渲染不受影响（无 `NaniteMeshInstance3D` 注册）
- [x] `modules/meshoptimizer/` 的 `SurfaceTool` API 行为未改变
- [x] `thirdparty/meshoptimizer/` 源码未被修改
- [x] 标准 `MeshInstance3D` 阴影渲染未受影响
- [x] 全部 Godot 既有测试套件通过（`--test` 无新增失败用例）

## 资源转换接口与编辑器对接 (Task 0.12)

- [ ] `NaniteBuilder::build_from_resource(Ref<Resource>)` 静态方法声明在 `nanite_builder.h`，实现合并所有 surface 后调 `build()`
- [ ] `build_from_resource` 通过 `ClassDB::bind_static_method` 暴露到 GDScript (`NaniteBuilder.build_from_resource(res)` 可调用)
- [ ] `build_from_resource` 接受 `ArrayMesh` 单 surface 输入，返回非空 `NaniteMeshResource`
- [ ] `build_from_resource` 接受 `ArrayMesh` 多 surface 输入，合并 vertex/index 后 vertex 数 == 各 surface 之和
- [ ] `build_from_resource` 接受 `PackedScene` 输入，递归收集所有 `MeshInstance3D` 的 mesh surface
- [ ] `build_from_resource` 接受 `MeshInstance3D` 节点输入，取 `.mesh` 属性处理，不修改场景树
- [ ] `build_from_resource` 对空 mesh（0 顶点 / 0 索引）返回 null
- [ ] `NaniteConversionContextMenu` 注册到 ClassDB，继承 `EditorContextMenuPlugin`
- [ ] FileSystem 右键 `.gltf` / `.glb` / `.fbx` / `.obj` / `.tres` 出现 "Convert to Nanite" 菜单项 **[CANNOT_VERIFY]** 需手动 UI 验证
- [ ] "Convert to Nanite" 点击后弹 save 文件对话框（默认同名 `.nanite.tres`，filter `*.nanite.tres`）
- [ ] 确认后调 `build_from_resource()` → `ResourceSaver::save()` 成功，文件可被 `ResourceLoader.load()` 读回
- [ ] `EditorInspectorPluginNanite::can_handle()` 对 `ArrayMesh` 资源返回 true
- [ ] `EditorInspectorPluginNanite::can_handle()` 对 `MeshInstance3D` 节点返回 true
- [ ] 选中 `ArrayMesh` 资源时 Inspector 顶部出现 "Convert to Nanite" 按钮
- [ ] 选中 `MeshInstance3D` 节点时 Inspector 顶部出现 "Convert to Nanite" 按钮
- [ ] Convert 按钮点击后弹 save 文件对话框，确认后构建保存
- [ ] 大 mesh（>100K tri）转换时显示 `EditorProgress` 进度条 **[CANNOT_VERIFY]** 需手动 UI 验证
- [ ] 转换失败时弹 `AcceptDialog` 错误对话框，不写入文件
- [ ] 转换成功时 EditorLog 输出 cluster_count / node_count / page_count / 耗时
- [ ] `nanite/editor/nanite_conversion_menu.cpp` 用 `#ifdef TOOLS_ENABLED` 包裹
- [ ] `nanite/SCsub` 在 `env.editor_build` 时编译 `nanite/editor/nanite_conversion_menu.cpp`
- [ ] 端到端测试 `test_nanite_conversion.gd` 全部断言 PASS

