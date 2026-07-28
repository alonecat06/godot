# Nanite Mesh 编辑器生成流程分析报告

> 基于 `/workspace/nanite/` 现有代码库（2026-07-29 状态）的源码级分析。
> 覆盖从用户触发到 NaniteMeshResource 保存到磁盘的完整调用链。

---

## 1. 概览：两条触发路径

编辑器中有两条独立路径可以触发 Nanite Mesh 生成：

| 路径 | 触发方式 | 入口 | 输入类型 |
|------|---------|------|---------|
| **Inspector Convert** | 选中 ArrayMesh / MeshInstance3D，点击 Inspector 中的 "Convert to Nanite..." 按钮 | `EditorInspectorPluginNanite::parse_begin()` | `ArrayMesh` / `MeshInstance3D` |
| **FileSystem Context Menu** | 在 FileSystem dock 中右键 `.gltf/.glb/.fbx/.obj/.tres` 文件，选择 "Convert to Nanite..." | `NaniteConversionContextMenu::get_options()` | 文件路径 → `Resource` |

两条路径最终都汇聚到 `NaniteBuilder::build_from_resource()` → `NaniteBuilder::build()`。

```mermaid
flowchart LR
    subgraph Path1["路径1: Inspector 按钮"]
        A1["用户选中 ArrayMesh<br/>或 MeshInstance3D"] --> A2["EditorInspectorPluginNanite<br/>::parse_begin()"]
        A2 --> A3["显示 Convert to Nanite... 按钮"]
        A3 --> A4["_on_convert_pressed()"]
    end

    subgraph Path2["路径2: FileSystem 右键菜单"]
        B1["用户右键 .gltf/.glb/.fbx<br/>/.obj/.tres 文件"] --> B2["NaniteConversionContextMenu<br/>::get_options()"]
        B2 --> B3["显示 Convert to Nanite... 菜单项"]
        B3 --> B4["_on_convert_callback()"]
    end

    A4 --> Merge["NaniteBuilder::build_from_resource()"]
    B4 --> Merge
    Merge --> Build["NaniteBuilder::build()"]
    Build --> Save["ResourceSaver::save()<br/>→ .nanite.tres"]
```

---

## 2. 涉及的类与职责

### 2.1 类层次总览

```mermaid
classDiagram
    class NaniteEditorPlugin {
        +get_plugin_name() String
        +handles(Object*) bool
        +edit(Object*) void
        +make_visible(bool) void
        -NaniteMeshResourceEditorWindow *viewer_window
    }
    class EditorPlugin {
        <<Godot built-in>>
        +add_inspector_plugin(Ref~EditorInspectorPlugin~)
        +add_context_menu_plugin(slot, Ref~EditorContextMenuPlugin~)
    }
    class EditorInspectorPluginNanite {
        +can_handle(Object*) bool
        +parse_begin(Object*) void
        -_on_convert_pressed(Object*)
        -_on_convert_save_confirmed(String)
        -EditorFileDialog *convert_save_dialog
        -ObjectID pending_object_id
        -Ref~Resource~ pending_resource
    }
    class EditorInspectorPlugin {
        <<Godot built-in>>
        +can_handle(Object*) bool
        +parse_begin(Object*) void
        +add_custom_control(Control*)
    }
    class NaniteConversionContextMenu {
        +get_options(Vector~String~) void
        -_on_convert_callback(Variant)
        -_on_save_confirmed(String)
        -String pending_path
        -EditorFileDialog *save_dialog
        -Ref~Resource~ pending_resource
    }
    class EditorContextMenuPlugin {
        <<Godot built-in>>
        +get_options(Vector~String~) void
    }
    class NaniteBuilder {
        +build(Ref~ArrayMesh~) Ref~NaniteMeshResource~
        +build_from_resource(Ref~Resource~) Ref~NaniteMeshResource~
        -preprocess_mesh(vertices, indices, normals, uvs) bool
        -collect_materials(Ref~ArrayMesh~) bool
        -build_leaf_clusters() bool
        -build_hierarchy() bool
        -build_bvh() bool
        -build_shadow_mesh() bool
        -finalize_resource() Ref~NaniteMeshResource~
        -BuilderConfig m_cfg
        -LocalVector~NaniteCluster~ m_clusters
        -LocalVector~NaniteClusterNode~ m_nodes
        -LocalVector~meshopt_Meshlet~ m_meshlets
    }
    class BuilderConfig {
        +uint32_t max_vertices = 64
        +uint32_t max_triangles = 128
        +uint32_t partition_size = 4
        +float cone_weight = 0.5
        +float simplification_ratio = 0.5
        +int shadow_lod_depth = 3
        +is_valid() bool
    }
    class NaniteMeshResource {
        +PackedByteArray vertex_data
        +PackedByteArray clusters_data
        +PackedByteArray nodes_data
        +PackedByteArray page_table_data
        +Ref~ArrayMesh~ shadow_mesh
        +get_cluster_count() int
        +get_node_count() int
        +get_page_count() int
    }
    class NaniteMeshResourceEditorWindow {
        +edit(Ref~NaniteMeshResource~)
        -NaniteMeshEditor *editor
    }
    class NaniteMeshEditor {
        +edit(Ref~NaniteMeshResource~)
        -_rebuild_preview()
        -_on_display_mode_selected(int)
        -_on_lod_mode_selected(int)
        -SubViewport *viewport
        -MeshInstance3D *solid_instance
        -MeshInstance3D *wire_instance
        -OptionButton *display_mode_btn
        -OptionButton *lod_mode_btn
        -SpinBox *force_lod_spinner
        -Label *stats_label
    }
    class NaniteResourcePreviewGenerator {
        +handles(String) bool
        +generate(Ref~Resource~, Size2, Dictionary) Ref~Texture2D~
    }
    class PagePacker {
        +pack(clusters, config) PageTable
        -sort_by_lod_then_locality(...)
        -emit_page(clusters_subset) PageEntry
    }
    class meshoptimizer {
        <<C library, thirdparty>>
        +meshopt_buildMeshletsFlex()
        +meshopt_computeMeshletBounds()
        +meshopt_partitionClusters()
        +meshopt_simplifyWithAttributes()
        +meshopt_optimizeMeshletLevel()
        +meshopt_encodeMeshlet()
        +meshopt_simplifyScale()
    }

    EditorPlugin <|-- NaniteEditorPlugin
    EditorInspectorPlugin <|-- EditorInspectorPluginNanite
    EditorContextMenuPlugin <|-- NaniteConversionContextMenu
    NaniteEditorPlugin --> EditorInspectorPluginNanite : registers
    NaniteEditorPlugin --> NaniteConversionContextMenu : registers
    NaniteEditorPlugin --> NaniteResourcePreviewGenerator : registers
    NaniteEditorPlugin --> NaniteMeshResourceEditorWindow : creates
    NaniteMeshResourceEditorWindow --> NaniteMeshEditor : hosts
    EditorInspectorPluginNanite --> NaniteBuilder : calls
    NaniteConversionContextMenu --> NaniteBuilder : calls
    NaniteBuilder --> BuilderConfig : reads
    NaniteBuilder --> PagePacker : delegates
    NaniteBuilder --> meshoptimizer : calls
    NaniteBuilder --> NaniteMeshResource : produces
    NaniteMeshEditor --> NaniteMeshResource : reads
    NaniteResourcePreviewGenerator --> NaniteMeshResource : reads
```

### 2.2 各类职责详解

| 类 | 文件 | 职责 |
|----|------|------|
| `NaniteEditorPlugin` | `nanite/editor/nanite_editor_plugin.h/cpp` | **编辑器插件入口**。注册 `EditorInspectorPluginNanite`、`NaniteConversionContextMenu`、`NaniteResourcePreviewGenerator`；处理 `.nanite.tres` 双击打开预览窗口 |
| `EditorInspectorPluginNanite` | 同上 | **Inspector 面板增强**。当用户选中 `ArrayMesh` 或 `MeshInstance3D` 时，在 Inspector 顶部添加 "Convert to Nanite..." 按钮 |
| `NaniteConversionContextMenu` | `nanite/editor/nanite_conversion_menu.h/cpp` | **FileSystem 右键菜单**。为 `.gltf/.glb/.fbx/.obj/.tres` 文件添加右键菜单项 |
| `NaniteBuilder` | `nanite/core/nanite_builder.h/cpp` | **离线构建核心**。驱动完整的 Nanite 构建管线：预处理 → 叶子聚类 → 层次化简化 → BVH 装配 → Shadow Mesh → 序列化 |
| `BuilderConfig` | `nanite/core/builder_config.h/cpp` | **构建参数**。可配置的构建参数：cluster 大小、简化比例、shadow LOD 深度等 |
| `NaniteMeshResource` | `nanite/core/nanite_resource.h/cpp` | **资源载体**。序列化后的 Nanite 数据：顶点池、cluster 元数据、BVH 节点、Page Table、Shadow Mesh |
| `NaniteMeshEditor` | `nanite/editor/nanite_mesh_editor.h/cpp` | **3D 预览控件**。继承 `SubViewportContainer`，在 Inspector 中渲染 3D 预览，支持 Display Mode / LOD Mode 切换 |
| `NaniteMeshResourceEditorWindow` | `nanite/editor/nanite_resource_editor_window.h/cpp` | **独立预览窗口**。双击 `.nanite.tres` 时弹出的独立窗口，内嵌 `NaniteMeshEditor` |
| `NaniteResourcePreviewGenerator` | `nanite/editor/nanite_resource_preview_gen.h/cpp` | **缩略图生成器**。为 FileSystem dock 生成 `.nanite.tres` 的缩略图 |
| `PagePacker` | `nanite/core/page_packer.h/cpp` | **Page 划分**。按 LOD + 空间局部性排序，将 cluster 划分到固定大小的 Page |
| `meshoptimizer` | `thirdparty/meshoptimizer/` | **第三方库**。提供 meshlet 构建、聚类、简化、编码等全部底层算法 |

---

## 3. 完整调用链

### 3.1 路径 1：Inspector Convert 按钮

```mermaid
sequenceDiagram
    participant User as 用户
    participant Inspector as Godot Inspector
    participant EPIN as EditorInspectorPluginNanite
    participant Builder as NaniteBuilder
    participant MeshOpt as meshoptimizer
    participant Packer as PagePacker
    participant Saver as ResourceSaver

    Note over User,Saver: 步骤1: 注册阶段 (引擎启动时)
    User->>Inspector: 启动编辑器
    Inspector->>EPIN: register_types.cpp → MODULE_INITIALIZATION_LEVEL_EDITOR
    Note over EPIN: NaniteEditorPlugin 构造时注册<br/>add_inspector_plugin(EditorInspectorPluginNanite)

    Note over User,Saver: 步骤2: 触发阶段
    User->>Inspector: 选中 ArrayMesh 资源
    Inspector->>EPIN: can_handle(p_object) → true (ArrayMesh)
    Inspector->>EPIN: parse_begin(p_object)
    EPIN->>EPIN: 创建 VBoxContainer + Button("Convert to Nanite...")
    EPIN->>Inspector: add_custom_control(vb)

    Note over User,Saver: 步骤3: 点击按钮
    User->>EPIN: 点击 "Convert to Nanite..."
    EPIN->>EPIN: _on_convert_pressed(p_object)
    EPIN->>EPIN: 提取 input_res (ArrayMesh)
    EPIN->>EPIN: 创建 EditorFileDialog 保存对话框
    EPIN->>User: 弹出保存对话框

    Note over User,Saver: 步骤4: 确认保存
    User->>EPIN: 选择路径 → 确认
    EPIN->>EPIN: _on_convert_save_confirmed(path)
    EPIN->>Builder: NaniteBuilder::build_from_resource(pending_resource)

    Note over Builder: build_from_resource() 内部:
    Builder->>Builder: 解析资源类型 (ArrayMesh/PackedScene/MeshInstance3D)
    Builder->>Builder: 多 surface 合并为单 ArrayMesh
    Builder->>Builder: build(merged_mesh)

    Note over Builder,MeshOpt: build() 核心管线:
    Builder->>Builder: 1. surface_get_arrays(0) → vertices/indices/normals/uvs
    Builder->>Builder: 2. preprocess_mesh(vertices, indices, normals, uvs)
    Builder->>MeshOpt: meshopt_generateVertexRemap()
    MeshOpt-->>Builder: remap table
    Builder->>MeshOpt: meshopt_remapVertexBuffer()
    Builder->>MeshOpt: meshopt_remapIndexBuffer()
    Builder->>MeshOpt: meshopt_optimizeVertexCache()
    Builder->>MeshOpt: meshopt_optimizeVertexFetchRemap()
    Builder->>Builder: 3. collect_materials(p_mesh)
    Builder->>Builder: 4. build_leaf_clusters()
    Builder->>MeshOpt: meshopt_buildMeshletsFlex(max_vertices=64, max_triangles=128, cone_weight=0.5)
    MeshOpt-->>Builder: meshlet 数组
    loop 每个 meshlet
        Builder->>MeshOpt: meshopt_optimizeMeshletLevel(level=3)
        Builder->>MeshOpt: meshopt_computeMeshletBounds()
    end
    Builder->>Builder: 5. build_hierarchy()
    loop 自底向上 (cluster_count > 1)
        Builder->>MeshOpt: meshopt_partitionClusters(target=4)
        loop 每个 partition (4 clusters)
            Builder->>Builder: 合并 4 簇的 index + vertex 子集
            Builder->>Builder: 计算 vertex_lock (锁组边界)
            Builder->>MeshOpt: meshopt_simplifyWithAttributes(target=50%, LockBorder)
            Builder->>MeshOpt: meshopt_buildMeshletsFlex(简化为 2 簇)
            Builder->>Builder: parent.error = max(child.errors)
            Builder->>Builder: parent.bounds = union(child.bounds)
        end
    end
    Builder->>Builder: 6. build_bvh()
    Builder->>Builder: linearize_bvh_recursive() → 线性节点数组
    Builder->>Builder: 7. build_shadow_mesh()
    Builder->>Builder: 从 BVH 第 shadow_lod_depth 层取 cluster
    Builder->>Builder: 合并为粗 LOD ArrayMesh
    Builder->>Builder: 8. finalize_resource()
    Builder->>Packer: PagePacker::pack(clusters, config)
    Packer-->>Builder: PageTable
    Builder->>Builder: 序列化: vertex_data, clusters_data, nodes_data, page_table_data
    Builder-->>EPIN: Ref<NaniteMeshResource>

    EPIN->>Saver: ResourceSaver::save(nanite_res, path)
    Saver-->>EPIN: OK
    EPIN->>Inspector: EditorFileSystem::scan() 刷新文件列表
    EPIN->>Inspector: EditorLog 输出构建统计
```

### 3.2 路径 2：FileSystem 右键菜单

```mermaid
sequenceDiagram
    participant User as 用户
    participant FS as FileSystem Dock
    participant Menu as NaniteConversionContextMenu
    participant Builder as NaniteBuilder
    participant Saver as ResourceSaver

    Note over User,Saver: 步骤1: 注册阶段
    Note over Menu: NaniteEditorPlugin 构造时注册<br/>add_context_menu_plugin(CONTEXT_SLOT_FILESYSTEM, menu)

    Note over User,Saver: 步骤2: 右键触发
    User->>FS: 右键 .gltf/.glb/.fbx/.obj/.tres 文件
    FS->>Menu: get_options(paths)
    Menu->>Menu: 检查文件扩展名是否匹配
    Menu->>FS: 添加 "Convert to Nanite..." 菜单项

    Note over User,Saver: 步骤3: 点击菜单项
    User->>Menu: 点击 "Convert to Nanite..."
    Menu->>Menu: _on_convert_callback(arg)
    Menu->>Menu: 加载文件: ResourceLoader::load(path)
    Menu->>Menu: 创建 EditorFileDialog 保存对话框

    Note over User,Saver: 步骤4: 确认保存
    User->>Menu: 选择路径 → 确认
    Menu->>Menu: _on_save_confirmed(path)
    Menu->>Builder: NaniteBuilder::build_from_resource(pending_resource)
    Note over Builder: 后续流程与路径1完全相同
    Builder-->>Menu: Ref<NaniteMeshResource>
    Menu->>Saver: ResourceSaver::save(nanite_res, path)
    Menu->>FS: EditorFileSystem::scan() 刷新文件列表
```

### 3.3 NaniteBuilder::build() 内部管线

```mermaid
flowchart TD
    Start(["Input: Ref<ArrayMesh>"]) --> Validate{验证输入}
    Validate -- "null / 无效 config / 无 surface" --> Fail([返回 null])
    Validate -- "有效" --> Extract["surface_get_arrays(0)<br/>→ vertices, indices, normals, uvs"]

    Extract --> Preprocess["preprocess_mesh()"]
    Preprocess --> VR["meshopt_generateVertexRemap<br/>→ 去重"]
    VR --> Remap["meshopt_remapVertexBuffer<br/>+ meshopt_remapIndexBuffer"]
    Remap --> Cache["meshopt_optimizeVertexCache<br/>→ 顶点缓存优化"]
    Cache --> Fetch["meshopt_optimizeVertexFetchRemap<br/>→ 顶点预取局部性"]

    Fetch --> Materials["collect_materials()<br/>提取 surface 0 的 albedo color"]

    Materials --> Leaf["build_leaf_clusters()"]
    Leaf --> L1["meshopt_buildMeshletsFlex<br/>max_vertices=64, max_triangles=128<br/>cone_weight=0.5, split_factor=0.5"]
    L1 --> L2["meshopt_optimizeMeshletLevel<br/>level=3 (每个 meshlet 内部重排)"]
    L2 --> L3["meshopt_computeMeshletBounds<br/>→ bounds + cone_axis + cone_cutoff"]
    L3 --> L0["L0 叶子: error=0, group_id=0"]

    L0 --> Hierarchy["build_hierarchy()"]
    Hierarchy --> Loop{cluster_count > 1?}
    Loop -- "是" --> Part["meshopt_partitionClusters<br/>target_partition_size=4"]
    Part --> Merge["合并 4 簇的 index + vertex 子集"]
    Merge --> Lock["计算 vertex_lock<br/>锁住跨组共享边界"]
    Lock --> Simplify["meshopt_simplifyWithAttributes<br/>target=50%, LockBorder+Regularize"]
    Simplify --> Recluster["meshopt_buildMeshletsFlex<br/>简化结果再切 2 簇"]
    Recluster --> Parent["parent.error = max(child.errors)<br/>parent.bounds = union(child.bounds)<br/>parent.group_id += 1"]
    Parent --> Loop
    Loop -- "否" --> BVH

    BVH["build_bvh()"] --> Linearize["linearize_bvh_recursive()<br/>→ 线性 NaniteClusterNode 数组"]
    Linearize --> Chain["build_binary_chain()<br/>→ 多子节点链转为二叉树"]

    Chain --> Shadow["build_shadow_mesh()"]
    Shadow --> ShadowExtract["从 BVH 第 shadow_lod_depth 层<br/>取 cluster，合并为 ArrayMesh"]
    ShadowExtract --> ShadowMesh["粗 LOD Shadow Mesh<br/>供 mesh_set_shadow_mesh 使用"]

    ShadowMesh --> Finalize["finalize_resource()"]
    Finalize --> Pack["PagePacker::pack()<br/>按 LOD+空间局部性排序<br/>按 page_size_bytes 切页"]
    Pack --> Serialize["序列化: vertex_data(32B stride)<br/>+ clusters_data + nodes_data<br/>+ page_table_data + shadow_mesh"]
    Serialize --> Result(["Output: Ref<NaniteMeshResource>"])

    Preprocess -- "失败" --> Fail
    Materials -- "失败" --> Fail
    Leaf -- "失败" --> Fail
    Hierarchy -- "失败" --> Fail
    BVH -- "失败" --> Fail
    Shadow -- "失败" --> Fail
```

---

## 4. 使用的算法

### 4.1 顶点预处理 (preprocess_mesh)

| 步骤 | 算法 | meshoptimizer API | 作用 |
|------|------|-------------------|------|
| 1 | 顶点去重 | `meshopt_generateVertexRemap()` | 合并位置/法线/UV 完全相同的顶点，减少冗余 |
| 2 | 顶点/索引重映射 | `meshopt_remapVertexBuffer()` + `meshopt_remapIndexBuffer()` | 应用 remap table，重新排列 buffer |
| 3 | 顶点缓存优化 | `meshopt_optimizeVertexCache()` | 重排三角形顺序，使顶点在 GPU 顶点缓存中的命中率最大化（Tom Forsyth 算法） |
| 4 | 顶点预取优化 | `meshopt_optimizeVertexFetchRemap()` | 重排顶点 buffer，使顶点在内存中的访问局部性最大化 |

### 4.2 叶子层聚类 (build_leaf_clusters)

| 步骤 | 算法 | 关键参数 | 作用 |
|------|------|---------|------|
| 1 | Meshlet 构建 | `meshopt_buildMeshletsFlex()` | `max_vertices=64`, `max_triangles=128`, `cone_weight=0.5` | 将三角形按空间局部性分组为 meshlet，带法线锥权重 |
| 2 | Meshlet 内部优化 | `meshopt_optimizeMeshletLevel()` | `level=3` | 在 meshlet 内部重排三角形，改善顶点缓存局部性 |
| 3 | 包围盒计算 | `meshopt_computeMeshletBounds()` | — | 计算每个 meshlet 的 AABB + 法线锥 (cone_axis, cone_cutoff) |

### 4.3 层次化简化 (build_hierarchy)

这是一个**自底向上**的迭代过程：

```
当前层 cluster 数量 = N
while N > 1:
    ┌─ 1. 分组: meshopt_partitionClusters(clusters, target_partition_size=4)
    │      将 N 个 cluster 按空间邻近性分为若干组，每组约 4 个
    │
    ├─ 2. 合并: 对每组 (partition):
    │      合并 4 个 cluster 的三角形 index + vertex 数据
    │
    ├─ 3. 锁定边界: 计算 vertex_lock
    │      标记跨分组共享边的顶点，防止简化时产生裂缝
    │
    ├─ 4. QEM 简化: meshopt_simplifyWithAttributes(target=50%, options=LockBorder+Regularize)
    │      使用二次误差度量 (Quadric Error Metrics) 简化到约 50% 三角形数
    │      LockBorder: 不移动锁定边界的顶点
    │      Regularize: 正则化网格，防止退化三角形
    │
    ├─ 5. 重聚类: meshopt_buildMeshletsFlex()
    │      将简化后的三角形重新切分为 2 个 cluster
    │
    └─ 6. 计算父节点属性:
           parent.error = max(child1.error, child2.error, ..., 简化误差)
           parent.bounds = AABB::union(child1.bounds, child2.bounds, ...)
           parent.group_id = 上一层的 group_id + 1
```

**关键算法——QEM 简化**：
- `meshopt_simplifyWithAttributes` 基于 Garland-Heckbert 的 Quadric Error Metrics 算法
- 每条边赋一个误差矩阵（quadric），表示合并该边对模型外观的影响
- 贪心地选择误差最小的边进行折叠，直到达到目标三角形数
- `LockBorder` 选项确保跨 group 的共享边界顶点不被移动，防止相邻 group 之间出现裂缝
- `Regularize` 选项在简化过程中添加正则化项，防止产生过于细长的退化三角形

### 4.4 BVH 装配 (build_bvh)

| 步骤 | 方法 | 说明 |
|------|------|------|
| 1 | `linearize_bvh_recursive()` | 递归遍历 hierarchy tree，将每个 MERGE 节点及其子节点展平为线性 `NaniteClusterNode` 数组 |
| 2 | `build_binary_chain()` | 将 MERGE 节点的多个源子节点链转为二叉树（二分叉），每个节点最多两个子节点 |

### 4.5 粗 LOD Shadow Mesh (build_shadow_mesh)

从 BVH 树的第 `config.shadow_lod_depth` 层（默认 3）提取所有 cluster 的三角形，合并为一个 `ArrayMesh`。这个 mesh 三角形数远少于原始 mesh，用于 GDExtension 阶段通过 `mesh_set_shadow_mesh()` 设置阴影代理。

### 4.6 Page 划分 (PagePacker)

| 步骤 | 说明 |
|------|------|
| 1 | 按 LOD 层级 + 空间局部性排序所有 cluster |
| 2 | 按 `config.page_size_bytes`（默认 64KB）切分为 Page |
| 3 | 每个 cluster 分配 `page_id`，输出 `PageTable` |

### 4.7 序列化 (finalize_resource)

将以下数据打包到 `NaniteMeshResource`：

| 数据 | 格式 | 说明 |
|------|------|------|
| `vertex_data` | `PackedByteArray` | 原始 stride-32 顶点数据 (pos.xyz + normal.xyz + uv.xy = 8 floats) |
| `clusters_data` | `PackedByteArray` | 所有 `NaniteCluster` 的序列化二进制 |
| `nodes_data` | `PackedByteArray` | 所有 `NaniteClusterNode` 的序列化二进制 |
| `page_table_data` | `PackedByteArray` | Page Table 的序列化二进制 |
| `shadow_mesh` | `Ref<ArrayMesh>` | 粗 LOD 阴影 mesh |

---

## 5. 模块初始化流程

```mermaid
sequenceDiagram
    participant Main as main.cpp
    participant Modules as Module Initialization
    participant Reg as register_types.cpp
    participant Editor as EditorPlugins

    Note over Main,Editor: 阶段1: SERVERS level
    Main->>Modules: initialize_modules(SERVERS)
    Modules->>Reg: initialize_nanite_module(SERVERS)
    Reg->>Reg: ClassDB::register_class<NaniteDebug>()
    Reg->>Reg: ClassDB::register_class<NanitePageCache>()
    Reg->>Reg: ClassDB::register_class<NaniteServer>()
    Reg->>Reg: NaniteServer *ns = memnew(NaniteServer)
    Reg->>Reg: ns->init()

    Note over Main,Editor: 阶段2: SCENE level (RenderingDevice 已就绪)
    Main->>Modules: initialize_modules(SCENE)
    Modules->>Reg: initialize_nanite_module(SCENE)
    Reg->>Reg: ClassDB::register_class<BuilderConfig>()
    Reg->>Reg: ClassDB::register_class<NaniteBuilder>()
    Reg->>Reg: ClassDB::register_class<NaniteMeshResource>()
    Reg->>Reg: ClassDB::register_class<NaniteMeshInstance3D>()
    Reg->>Reg: ns->init_engine_post() (GPU pipeline 初始化)

    Note over Main,Editor: 阶段3: EDITOR level
    Main->>Modules: initialize_modules(EDITOR)
    Modules->>Reg: initialize_nanite_module(EDITOR)
    Reg->>Reg: ClassDB::register_class<NaniteMeshEditor>()
    Reg->>Reg: ClassDB::register_class<NaniteMeshResourceEditorWindow>()
    Reg->>Reg: ClassDB::register_class<EditorInspectorPluginNanite>()
    Reg->>Reg: ClassDB::register_class<NaniteResourcePreviewGenerator>()
    Reg->>Reg: ClassDB::register_class<NaniteEditorPlugin>()
    Reg->>Reg: ClassDB::register_class<NaniteConversionContextMenu>()
    Reg->>Editor: EditorPlugins::add_by_type<NaniteEditorPlugin>()
    Editor->>Editor: NaniteEditorPlugin 构造:
    Editor->>Editor:   add_inspector_plugin(EditorInspectorPluginNanite)
    Editor->>Editor:   add_preview_generator(NaniteResourcePreviewGenerator)
    Editor->>Editor:   add_context_menu_plugin(FILESYSTEM, NaniteConversionContextMenu)
```

---

## 6. 预览与调试可视化

### 6.1 NaniteMeshEditor 预览控件

`NaniteMeshEditor` 继承 `SubViewportContainer`，在 Inspector 中提供 3D 预览。

**结构**：
```
NaniteMeshEditor (SubViewportContainer)
├── SubViewport (独立 World3D)
│   ├── Camera3D (透视相机)
│   ├── DirectionalLight3D × 2 (双光源)
│   └── Node3D (rotation_node, 鼠标拖拽旋转)
│       ├── MeshInstance3D (solid_instance, 实体渲染)
│       └── MeshInstance3D (wire_instance, 线框叠加)
├── HBoxContainer (UI 工具栏)
│   ├── OptionButton (display_mode_btn)
│   ├── OptionButton (lod_mode_btn)
│   └── SpinBox (force_lod_spinner)
└── Label (stats_label, 构建统计)
```

**Display Mode 选项**：
| 模式 | 值 | 说明 |
|------|-----|------|
| Normal | 0 | 标准材质渲染 |
| Normal+Wireframe | 1 | 标准 + 白色线框叠加 |
| Cluster Solid | 2 | 每个 Cluster 随机颜色 |
| Cluster Solid+Wireframe | 3 | 随机颜色 + 线框 |
| Wireframe Only | 4 | 纯线框 |

**LOD Mode 选项**：
| 模式 | 说明 |
|------|------|
| Nanite Auto | 使用 Nanite GPU 管线的自动 LOD 选择（Stage 1 才可用） |
| Force LOD Level | 强制渲染指定 LOD 层级的 cluster（通过 `SpinBox` 选择） |

**注意**：Stage 0 的预览渲染**不依赖 Nanite GPU 管线**（CompositorEffect / Cull Shader / Rasterize Shader）。它从 `NaniteMeshResource` 的编码 blob 在 CPU 侧解码，构建标准 `ArrayMesh`，由 Godot 的 `MeshInstance3D` 通过常规 forward 渲染管线渲染。这意味着即使没有编译任何 Nanite bridge，预览也能正常工作。

### 6.2 双击打开独立预览窗口

当用户在 FileSystem dock 中双击 `.nanite.tres` 文件时：

```
EditorNode → 查找 handles() 返回 true 的 EditorPlugin
    → NaniteEditorPlugin::handles(NaniteMeshResource*) → true
    → NaniteEditorPlugin::edit(p_object)
        → 创建/复用 NaniteMeshResourceEditorWindow
        → viewer_window->edit(resource)
            → 内部 NaniteMeshEditor::edit(resource)
```

### 6.3 缩略图生成

`NaniteResourcePreviewGenerator` 在 FileSystem dock 中为 `.nanite.tres` 文件生成缩略图：

```
EditorResourcePreview::queue_resource_preview()
    → NaniteResourcePreviewGenerator::handles("NaniteMeshResource") → true
    → NaniteResourcePreviewGenerator::generate(resource, size, metadata)
        → 创建离屏 SubViewport + Camera3D + MeshInstance3D
        → 渲染一帧 → 截取 Texture2D 缩略图
```

---

## 7. 关键数据流

```
┌─────────────────────────────────────────────────────────────────────┐
│                        编辑器 Nanite 生成数据流                       │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  输入源                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐      │
│  │ ArrayMesh    │  │ PackedScene  │  │ MeshInstance3D node  │      │
│  │ (直接选中)   │  │ (.gltf/.glb) │  │ (场景中选中)          │      │
│  └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘      │
│         │                 │                      │                   │
│         └─────────────────┼──────────────────────┘                   │
│                           ▼                                          │
│              ┌────────────────────────┐                             │
│              │ build_from_resource()  │  ← 统一入口                   │
│              │ 多 surface 合并        │                              │
│              └───────────┬────────────┘                             │
│                          ▼                                           │
│              ┌────────────────────────┐                             │
│              │ build()                │  ← 核心管线                   │
│              │ preprocess_mesh()      │    去重 + 缓存优化            │
│              │ collect_materials()    │    提取材质                    │
│              │ build_leaf_clusters()  │    叶子 meshlet 聚类           │
│              │ build_hierarchy()      │    自底向上简化                │
│              │ build_bvh()            │    线性 BVH 装配               │
│              │ build_shadow_mesh()    │    粗 LOD Shadow Mesh         │
│              │ finalize_resource()    │    Page 划分 + 序列化         │
│              └───────────┬────────────┘                             │
│                          ▼                                           │
│              ┌────────────────────────┐                             │
│              │ NaniteMeshResource     │  ← 最终产物                   │
│              │  .vertex_data          │    顶点池 (32B stride)        │
│              │  .clusters_data        │    Cluster 元数据             │
│              │  .nodes_data           │    BVH 节点                   │
│              │  .page_table_data      │    Page Table                │
│              │  .shadow_mesh          │    粗 LOD 阴影                │
│              └───────────┬────────────┘                             │
│                          ▼                                           │
│              ┌────────────────────────┐                             │
│              │ ResourceSaver::save()  │  ← 保存到磁盘                 │
│              │ → .nanite.tres         │    文本序列化                  │
│              └────────────────────────┘                             │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 8. 总结

| 维度 | 描述 |
|------|------|
| **触发方式** | Inspector 按钮 + FileSystem 右键菜单，双路径 |
| **核心算法** | meshoptimizer 全家桶：顶点去重/缓存优化、meshlet 构建、QEM 简化、聚类 |
| **构建管线** | 预处理 → 叶子聚类 → 层次化简化(自底向上) → BVH 装配 → Shadow Mesh → Page 划分 → 序列化 |
| **预览方式** | CPU 侧解码 → 标准 ArrayMesh → Godot 常规 forward 渲染（不依赖 GPU Nanite 管线） |
| **保存格式** | `.nanite.tres`（Godot 文本资源格式，内嵌二进制 blob） |
| **模块注册** | SERVERS → SCENE → EDITOR 三级初始化，编辑器插件在 EDITOR 级注册 |