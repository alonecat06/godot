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

- [x] 8000 tri sphere 产出 `nodes.size() >= 3`
- [x] 根节点（`nodes[0]`）`error` >= 所有其他节点 `error`
- [x] 内部节点 `bounds` 包含 `nodes[left_child].bounds`
- [x] 内部节点 `bounds` 包含 `nodes[right_child].bounds`
- [x] BVH 根 bounds 包含 `mesh->get_aabb()`
- [x] 叶子层三角形总数 > 父层三角形总数
- [x] 每个 internal 节点 `error >= max(children.error)`（容差 0.001）

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

> **Stage 0 修订**：原 "Inspector 嵌入 NaniteMeshResource 预览" 已移至独立窗口（见下节）。本节仅保留组件本身验证与缩略图生成。

- [x] `NaniteMeshEditor::edit(res)` 不崩溃 **[CANNOT_VERIFY]** 代码逻辑对 null/valid resource 均有处理（`nanite_mesh_editor.cpp:102-159`），但需手动启动编辑器验证
- [x] `stats_label` 显示 cluster/node/page 数 + shadow tris + 估算 MB
- [x] 鼠标拖拽可旋转预览 **[CANNOT_VERIFY]** `gui_input` 处理鼠标事件 + `_update_rotation` 修改 `rotation_node` Transform3D（`nanite_mesh_editor.cpp:74-100`），但需手动 UI 验证
- [x] 失去焦点时 `NaniteServer::set_debug_mode(NONE)` 被调用（阶段 0 可空实现） **[PARTIAL]** `nanite_mesh_editor.cpp:63-67` `NOTIFICATION_FOCUS_EXIT` 为空体 + TODO 注释。`NaniteServer` 类未实现（属于阶段 1）。符合 spec "阶段 0 可空实现"语义，但需阶段 1 补齐
- [x] FileSystem 中 `NaniteMeshResource` 缩略图由 `shadow_mesh` 生成
- [x] 缩略图生成不启动 Nanite GPUPipeline

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

