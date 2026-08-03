# Iteration: Cluster 聚合算法改进 — 拓扑连通性保障

> 关联 Spec: [spec.md](./spec.md) | 关联 Task: 阶段 0.5.1 `build_hierarchy`

---

## 1. 问题定位

### 1.1 现象

在高 LOD 层级（如 LOD 5），`meshopt_partitionClusters` 产生的 partition 内多个 cluster **并非拓扑连通**——同一 partition 内的 cluster 之间不共享边或顶点，整个 partition 在空间上是断裂的。这导致：

- **简化质量下降**：`meshopt_simplifyWithAttributes` 对非连通网格做 border-vertex locking 时，锁定条件（顶点被 >=2 个 cluster 引用）在断裂处失效，locked vertex 集合为空，LockBorder 约束退化为无约束，简化时可能产生裂缝。
- **编辑器预览异常**：partition border 显示为多条不连通的轮廓线，视觉效果差。

### 1.2 实测数据

测试资源：`mitsuba2.nanite.tres`（1562 cluster, 5028 node）

```
[nanite-preview] display_id=5 lod_mode=1 force_lod=5 selected_cluster=1502
[nanite-sibling] lod=5 selected=1502 target_partition=6 siblings=4
[nanite-partition-border] lod=5 clusters=44 partitions=10 border_edges=83
```

LOD 5 共 44 个 cluster，被划分为 10 个 partition。选中 cluster 1502 时，partition 6 包含 5 个 cluster，但部分 cluster 在空间上不连通。

### 1.3 根因：`meshopt_partitionClusters` 的 `mergeSpatial` 后处理

查阅 `thirdparty/meshoptimizer/partition.cpp`，`meshopt_partitionClusters` 的核心流程：

```
Phase 1: 拓扑驱动聚合（基于 cluster 邻接图）
  - 构建邻接图（共享顶点 = 边权重）
  - 贪心堆合并：pickGroupToMerge() 评分 = 共享顶点数 × 归一化系数
  - 当 vertex_positions 传入时，评分乘以 (1 + 0.4 × boundsScore)
  → 此阶段保证拓扑连通性

Phase 2: 空间后处理 mergeSpatial()  ← 问题所在
  - 仅当 vertex_positions != NULL 时触发
  - 构建空间 KD-tree（基于 cluster 包围球中心）
  - 递归二分，在叶节点调用 mergeLeaf()
  - mergeLeaf() 纯按 boundsScore（空间近邻）合并，不检查拓扑连通性
  → 此阶段破坏拓扑连通性！
```

**关键代码**（`partition.cpp:596-606`）：

```cpp
// 如果提供了顶点位置，做最终空间合并
if (vertex_positions) {
    mergeSpatial(groups, merge_order, merge_offset,
        target_partition_size, max_partition_size, /* leaf_size= */ 8, 0);
}
```

`mergeSpatial` 使用 KD-tree 空间二分 + `mergeLeaf` 纯空间最近邻合并，完全绕过 Phase 1 构建的邻接图。在 cluster 数量多时 Phase 1 已产生足够 partition，影响有限；但 cluster 数量少时（如 LOD 5 仅 44 个），`mergeSpatial` 会将空间上近但拓扑不连通的小 partition 强行合并，产生断裂。

### 1.4 结论

编辑器预览中的 `build_partition_border_wire` 和 `build_partition_sibling_mesh` 本身不产生 partition 划分——它们只是读取已构建好的 cluster 数据，再用 `meshopt_partitionClusters` 做运行时可视化。根因在 `nanite_builder.cpp` 的 `build_hierarchy()` 和编辑器预览中调用 `meshopt_partitionClusters` 时都传入了顶点位置参数。

---

## 2. 最终方案：拓扑模式（`vertex_positions = nullptr`）

### 2.1 方案选择

| 方案 | 描述 | 改动量 | 拓扑连通 | 空间紧凑 |
|------|------|--------|----------|----------|
| A: 拓扑模式 | `vertex_positions = nullptr` | 3 行 | ✓ | 近似 SAH |
| B: 自实现 SAH | 独立贪心聚合 | ~300 行 | ✓ | 精确 SAH |

**选择方案 A**，理由：

1. **meshopt 的拓扑模式本身已近似 SAH**：`pickGroupToMerge` 用共享顶点数作为合并代价，共享顶点越多 = 接触面越大 = 合并后包围盒增量越小，等价于 SAH 近似。
2. **维护成本低**：不引入独立代码，完全依赖 meshoptimizer 库。
3. **"空间长链" 不传播**：partition 内的简化网格经 `meshopt_buildMeshletsFlex` re-cluster 后，每个 parent cluster 都是空间紧凑的，长链被截断。
4. **BVH 和 LOD 误差不受影响**：BVH 基于 re-cluster 后的 parent cluster 包围盒，与 partition 空间范围无关；LOD 误差由 `target_error` 参数控制，与 partition 分组无关。

### 2.2 改动点

三处 `meshopt_partitionClusters` 调用，均将 `vertex_positions` 参数改为 `nullptr`：

| 文件 | 函数 | 行号 |
|------|------|------|
| `nanite/core/nanite_builder.cpp` | `build_hierarchy()` | ~876 |
| `nanite/editor/nanite_mesh_editor.cpp` | `build_partition_border_wire` | ~456 |
| `nanite/editor/nanite_mesh_editor.cpp` | `build_partition_sibling_mesh` | ~912 |

### 2.3 具体修改

#### 2.3.1 `nanite_builder.cpp` — `build_hierarchy()`

```cpp
// 修改前：
size_t partition_count = meshopt_partitionClusters(
    partition_ids.ptr(),
    cluster_indices.ptr(), cluster_indices.size(),
    cluster_index_counts.ptr(), cluster_count,
    m_verts_pos.ptr(),      // 顶点位置（触发 mergeSpatial）
    vertex_count,
    vertex_stride,
    partition_size);

// 修改后：
size_t partition_count = meshopt_partitionClusters(
    partition_ids.ptr(),
    cluster_indices.ptr(), cluster_indices.size(),
    cluster_index_counts.ptr(), cluster_count,
    nullptr,                // 纯拓扑邻接图分区
    0,                      // vertex_count（NULL 时被忽略）
    0,                      // vertex_stride（NULL 时被忽略）
    partition_size);
```

#### 2.3.2 `nanite_mesh_editor.cpp` — `build_partition_border_wire`

```cpp
// 修改前：
size_t partition_count = meshopt_partitionClusters(
    partition_ids.ptr(),
    cluster_indices.ptr(), cluster_indices.size(),
    cluster_index_counts.ptr(), cl_cluster_count,
    vp_base,                // 顶点位置
    total_vertex_count,
    kVertexStride,
    4);

// 修改后：
size_t partition_count = meshopt_partitionClusters(
    partition_ids.ptr(),
    cluster_indices.ptr(), cluster_indices.size(),
    cluster_index_counts.ptr(), cl_cluster_count,
    nullptr,                // 纯拓扑邻接图分区
    0,
    0,
    4);
```

同时可移除不再使用的局部变量 `vp_base`、`total_vertex_count` 声明（如果仅用于此调用）。

#### 2.3.3 `nanite_mesh_editor.cpp` — `build_partition_sibling_mesh`

```cpp
// 修改前：
size_t partition_count = meshopt_partitionClusters(
    partition_ids.ptr(),
    cluster_indices.ptr(), cluster_indices.size(),
    cluster_index_counts.ptr(), cl_cluster_count,
    vp_base,                // 顶点位置
    total_vertex_count,
    kVertexStride,
    4);

// 修改后：
size_t partition_count = meshopt_partitionClusters(
    partition_ids.ptr(),
    cluster_indices.ptr(), cluster_indices.size(),
    cluster_index_counts.ptr(), cl_cluster_count,
    nullptr,                // 纯拓扑邻接图分区
    0,
    0,
    4);
```

`vp_base` 在 `build_partition_sibling_mesh` 中仍用于构建 sibling mesh 的顶点数据（遍历三角形时读顶点位置），因此不能移除 `vp_base` 声明，仅改调用参数。

### 2.4 边界情况处理

#### 2.4.1 单 cluster partition

当某个 cluster 不与任何其他 cluster 共享顶点时，拓扑模式下它将成为单 cluster partition。`build_hierarchy()` 已有处理：

```cpp
// 单 cluster partition：直接 promote 到 parent LOD
if (p_clusters.size() <= 1) {
    for (uint32_t ci : p_clusters) {
        uint32_t cloned_node = clone_cluster_for_lod(orig_cluster, parent_lod);
        next_level_nodes.push_back(cloned_node);
    }
    continue;
}
```

无需额外修改。

#### 2.4.2 邻接图不连通

如果整体 mesh 的 cluster 邻接图本身就不连通（例如 mesh 有多个独立组件），拓扑模式会自然地将每个连通分量划分为独立的 partition(s)。这是正确的行为——不应将不连通的组件合并，否则 border-vertex locking 会失效。

#### 2.4.3 弱连通（共享顶点极少）

两个 cluster 可能仅共享 1 个顶点（如仅在某角点接触）。在拓扑模式下，这仍被视为连通，但 `pickGroupToMerge` 的评分（基于共享顶点数）会很低，它们被合并的优先级很低，通常会被自然分到不同 partition。如果因为 cluster 数量少而被强制合并，共享 1 个顶点仍然足以让 border-vertex locking 标记该顶点为 locked，保证简化质量。

### 2.5 编辑器预览幂等性

编辑器预览中的 `build_partition_border_wire` 和 `build_partition_sibling_mesh` 与 `build_hierarchy()` 使用相同的 `meshopt_partitionClusters(nullptr)` 调用，因此预览显示的 partition 划分与构建时一致。但需注意：

- 预览依赖当前 LOD 层的 cluster 数据（`clusters_data` + `meshlet_vertices_data` + `meshlet_triangles_data`），这些数据来自已构建的 `.nanite.tres` 资源
- 旧资源（修改前构建的）的 cluster 数据不变，预览会用新策略重新划分，可能显示与构建时不同的 partition 边界
- 新构建的资源（修改后构建的）预览与构建完全一致

**建议**：修改后重新构建所有 `.nanite.tres` 资源，确保数据一致性。

---

## 3. 影响范围

| 模块 | 影响 | 改动量 |
|------|------|--------|
| `nanite/core/nanite_builder.cpp` | `build_hierarchy()` L876: `vertex_positions` → `nullptr` | 1 行 |
| `nanite/editor/nanite_mesh_editor.cpp` | `build_partition_border_wire` L456: `vertex_positions` → `nullptr` | 1 行 |
| `nanite/editor/nanite_mesh_editor.cpp` | `build_partition_sibling_mesh` L912: `vertex_positions` → `nullptr` | 1 行 |
| 已构建的 `.nanite.tres` 资源 | 需重新构建以应用新 partition 策略 | — |

---

## 4. 验证方法

1. **构建测试**：用 `mitsuba2` 等测试 mesh 重新构建，检查构建日志中每层 partition 数量和 cluster 分布
2. **编辑器预览**：在 Cluster Solid + Partition Border 模式下，选中不同 cluster，观察 partition border 是否形成连续的闭合轮廓线
3. **简化验证**：检查高 LOD 层级 parent cluster 的 `error` 值，确认简化质量未退化
4. **回归测试**：运行现有单元测试，确认构建流程无 regression

---

## 5. 参考资料

- meshoptimizer `partition.cpp`: `meshopt_partitionClusters` 实现
- meshoptimizer `partition.cpp:326-366`: `pickGroupToMerge` 合并评分逻辑
- meshoptimizer `partition.cpp:440-480`: `mergeSpatial` 空间后处理
- meshoptimizer `partition.cpp:596-606`: `mergeSpatial` 触发条件