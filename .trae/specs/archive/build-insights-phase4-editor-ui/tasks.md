# Tasks — Phase 4：编辑器 UI

## 1. InsightsComparator Diff 算法

- [x] Task 1.1: 实现 InsightsComparator 类（`modules/insights/insights_core/insights_comparator.h/cpp`）
  - [x] 定义 InsightsDiff 结构体（regressions, improvements, new_zones, removed_zones, total_cpu_time_delta, peak_memory_delta）
  - [x] 定义 ZoneDiff 结构体（name, time_increase_ns, percentage_change）
  - [x] 实现 compute_diff(db_a, db_b) 方法 — 按 zone name 匹配，计算差异
  - [x] 实现 highlight_regressions() 方法 — 按阈值标记回归
  - [x] 实现 generate_summary() 方法 — 生成文本摘要
  - [x] GDREGISTER_CLASS 注册

## 2. InsightsReplay 回放引擎

- [x] Task 2.1: 实现 InsightsReplay 类（`modules/insights/insights_core/insights_replay.h/cpp`）
  - [x] 定义成员：database, current_time_ns, is_playing, speed_scalar
  - [x] 实现 play() / pause() / seek(time_ns) / set_speed(scalar) 方法
  - [x] 实现 get_zone_at(time_ns) 方法 — 查询指定时间的活跃 zone
  - [x] 实现 tick(delta) 方法 — 推进 current_time_ns
  - [x] GDREGISTER_CLASS 注册

## 3. InsightsDatabase 查询扩展

- [x] Task 3.1: 扩展 InsightsDatabase 查询方法
  - [x] 实现 query_zones_by_depth(depth) — 按嵌套深度查询 zone
  - [x] 实现 query_allocations_by_size(min_size) — 按大小过滤分配记录
  - [x] 实现 query_resource_loads() — 查询所有资源加载记录
  - [x] 实现 query_resource_dependencies(path) — 查询资源依赖关系
  - [x] 实现 get_peak_memory() — 计算峰值内存
  - [x] 实现 get_leaked_allocations() — 获取未释放分配列表

## 4. InsightsEditorPlugin 入口

- [x] Task 4.1: 实现 InsightsEditorPlugin（`modules/insights/editor/insights_editor_plugin.h/cpp`）
  - [x] 继承 EditorPlugin
  - [x] 实现 _enter_tree() — 注册主屏幕和底部面板
  - [x] 实现 _exit_tree() — 清理注册
  - [x] 实现 _has_main_screen() → true
  - [x] 实现 _get_plugin_name() → "Insights"
  - [x] 实现 _make_visible(bool) — 控制主屏幕显隐
  - [x] 添加菜单项：Start Capture、Open Trace、Compare Traces
  - [x] 实现 _start_capture() / _open_trace() / _compare_traces() 回调
  - [x] GDREGISTER_CLASS 注册

## 5. InsightsDock 底部面板

- [x] Task 5.1: 实现 InsightsDock（`modules/insights/editor/insights_dock.h/cpp`）
  - [x] 继承 VBoxContainer
  - [x] 创建工具栏 HBoxContainer（Start/Stop/Open/Compare/Clear/Settings 按钮）
  - [x] 创建 Channel 标签页 TabContainer（CPU/GPU/Memory/Loading/Network/Log/Compare）
  - [x] 创建回放控制栏 HBoxContainer（Play/Pause/Speed/Time 显示）
  - [x] 实现 _on_start_capture() / _on_stop_capture() 信号回调
  - [x] 实现 _on_open_trace() — 文件对话框选择 .gitracy
  - [x] 实现 _on_compare_traces() — 选择两个 .gitracy 文件
  - [x] GDREGISTER_CLASS 注册

## 6. InsightsTimeline 时间轴

- [x] Task 6.1: 实现 InsightsTimeline（`modules/insights/editor/insights_timeline.h/cpp`）
  - [x] 继承 Control，重写 _draw() 自绘时间轴
  - [x] 定义成员：current_scale, scroll_x, frame_markers, cpu_zones, gpu_zones, load_zones, net_zones
  - [x] 定义成员：hovered_zone, selected_zone
  - [x] 实现 set_database(db) — 从数据库加载 zone 数据
  - [x] 实现 _draw() — 渲染帧标尺、CPU Track、GPU Track、Loading Track、Network Track
  - [x] 实现 _gui_input() — 处理鼠标滚轮缩放、拖拽平移、点击选中
  - [x] 实现 get_total_duration_ns() — 返回总时长
  - [x] 实现 get_visible_range() — 返回当前可见时间范围
  - [x] 实现 get_selected_zone() — 返回选中的 zone
  - [x] 实现 zoom_to_zone(zone) — 缩放到指定 zone
  - [x] GDREGISTER_CLASS 注册

## 7. InsightsFlamegraph 火焰图

- [x] Task 7.1: 实现 InsightsFlamegraph（`modules/insights/editor/insights_flamegraph.h/cpp`）
  - [x] 继承 Control，重写 _draw() 自绘火焰图
  - [x] 定义 FlameNode 结构体（id, name, start_ns, end_ns, depth, children）
  - [x] 定义成员：root_zones, stack_depth, color_by_channel, search_query
  - [x] 实现 set_database(db) — 从数据库构建层级树
  - [x] 实现 get_root_nodes() — 返回根节点列表
  - [x] 实现 get_children(id) — 返回子节点列表
  - [x] 实现 set_search_query(query) — 设置搜索过滤
  - [x] 实现 get_filtered_nodes() — 返回过滤后的节点
  - [x] 实现 _draw() — 渲染火焰图层级
  - [x] 实现 _gui_input() — 处理点击选中
  - [x] GDREGISTER_CLASS 注册

## 8. InsightsMemoryPanel 内存瀑布

- [x] Task 8.1: 实现 InsightsMemoryPanel（`modules/insights/editor/insights_memory_panel.h/cpp`）
  - [x] 继承 Control，重写 _draw() 自绘内存瀑布图
  - [x] 定义 AllocEntry 结构体（ptr, size, alloc_site, start_ns, end_ns）
  - [x] 定义 LeakEntry 结构体（ptr, size, alloc_site）
  - [x] 定义成员：allocations, leaked_objects, size_filter, peak_memory
  - [x] 实现 set_database(db) — 从数据库加载分配数据
  - [x] 实现 get_peak_memory() — 返回峰值内存
  - [x] 实现 get_leaked_allocations() — 返回泄漏列表
  - [x] 实现 set_size_filter(min_size) — 设置大小过滤
  - [x] 实现 get_filtered_allocations() — 返回过滤后的分配列表
  - [x] 实现 _draw() — 渲染内存瀑布图
  - [x] GDREGISTER_CLASS 注册

## 9. InsightsLoadingPanel 资源依赖图

- [x] Task 9.1: 实现 InsightsLoadingPanel（`modules/insights/editor/insights_loading_panel.h/cpp`）
  - [x] 继承 Control，使用 Tree 控件渲染依赖树
  - [x] 定义 LoadNode 结构体（id, path, start_ns, end_ns, memory_size, parent_path）
  - [x] 实现 set_database(db) — 从数据库构建依赖树
  - [x] 实现 get_root_loads() — 返回根节点列表
  - [x] 实现 get_dependencies(id) — 返回依赖子节点列表
  - [x] 实现 get_bottleneck() — 返回最慢的加载节点
  - [x] 实现 _on_node_selected() — 选中节点回调
  - [x] GDREGISTER_CLASS 注册

## 10. InsightsNetworkPanel 网络流量

- [x] Task 10.1: 实现 InsightsNetworkPanel（`modules/insights/editor/insights_network_panel.h/cpp`）
  - [x] 继承 Control，重写 _draw() 自绘网络流量图
  - [x] 实现 set_database(db) — 从数据库加载网络事件
  - [x] 实现 _draw() — 渲染 RPC/包流量时间线
  - [x] GDREGISTER_CLASS 注册

## 11. InsightsComparePanel 对比面板

- [x] Task 11.1: 实现 InsightsComparePanel（`modules/insights/editor/insights_compare_panel.h/cpp`）
  - [x] 继承 Control，使用 Tree 控件渲染对比结果
  - [x] 定义成员：diff_data, baseline_trace, current_trace
  - [x] 实现 set_baseline(db) / set_current(db) — 设置对比数据源
  - [x] 实现 compute_diff() — 调用 InsightsComparator 计算差异
  - [x] 实现 _draw() — 左右对比渲染，红绿高亮差异
  - [x] GDREGISTER_CLASS 注册

## 12. 注册与编译

- [x] Task 12.1: 更新 `modules/insights/register_types.cpp`
  - [x] 添加所有新类的 include
  - [x] 添加 GDREGISTER_CLASS 注册

- [x] Task 12.2: 更新 `modules/insights/config.py`
  - [x] 在 doc_classes 列表中添加新类名

- [x] Task 12.3: 更新 `modules/insights/SCsub`
  - [x] 添加 `env_insights.add_source_files(module_obj, "editor/*.cpp")` 编译编辑器 UI

- [x] Task 12.4: 编译验证
  - [x] 运行 `scons module_insights_enabled=yes platform=windows dev_build=yes` 确保编译通过

## 13. 单元测试

> 对应设计文档 `godot_insights_design.md` Phase 4 单元测试部分，使用 doctest 框架的 `TEST_CASE` 宏。
> 测试文件路径：`modules/insights/tests/test_insights_phase4.h`

- [x] Task 13.1: 创建 EditorPlugin 注册与生命周期测试
  - [x] 创建测试文件 `modules/insights/tests/test_insights_phase4.h`
  - [x] 测试 InsightsEditorPlugin 可实例化
  - [x] 测试 _get_plugin_name() 返回 "Insights"
  - [x] 测试 _has_main_screen() 返回 true
  - [x] 测试 InsightsDock 可创建且为有效控件

- [x] Task 13.2: 创建 Timeline 渲染测试
  - [x] 测试 InsightsTimeline 设置 database 后 get_total_duration_ns() 正确
  - [x] 测试 InsightsTimeline 的 get_visible_range() 返回有效范围
  - [x] 测试 InsightsTimeline 设置 zone 数据后可查询选中 zone

- [x] Task 13.3: 创建 Flamegraph 层级构建测试
  - [x] 测试 InsightsFlamegraph 设置嵌套 zone 数据后 get_root_nodes() 正确
  - [x] 测试 get_children() 返回正确的子节点
  - [x] 测试 set_search_query() 过滤功能

- [x] Task 13.4: 创建 Memory Panel 分配生命周期测试
  - [x] 测试 InsightsMemoryPanel 设置分配数据后 get_peak_memory() 正确
  - [x] 测试 get_leaked_allocations() 返回正确的泄漏列表
  - [x] 测试 set_size_filter() 过滤功能

- [x] Task 13.5: 创建 Loading Panel 依赖树测试
  - [x] 测试 InsightsLoadingPanel 设置加载依赖数据后 get_root_loads() 正确
  - [x] 测试 get_dependencies() 返回正确的依赖子节点
  - [x] 测试 get_bottleneck() 返回最慢的加载节点

- [x] Task 13.6: 创建 Compare Panel Diff 测试
  - [x] 测试 InsightsComparePanel 设置 baseline 和 current 后 compute_diff() 正确
  - [x] 测试 regressions 列表包含正确的回归 zone
  - [x] 测试 new_zones 列表包含正确的新增 zone

# Task Dependencies

- Task 1.1 (Comparator) is independent
- Task 2.1 (Replay) is independent
- Task 3.1 (Database query) is independent
- Task 4.1 (EditorPlugin) depends on Task 5.1 (Dock)
- Task 5.1 (Dock) depends on Task 6.1-11.1 (各面板控件)
- Task 6.1-11.1 are independent of each other（可并行）
- Task 12.x depends on all other tasks
- Task 13.x depends on Task 12.x（测试在编译通过后执行）
