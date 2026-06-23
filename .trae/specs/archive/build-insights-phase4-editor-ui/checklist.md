# Checklist — Phase 4：编辑器 UI

## InsightsComparator Diff 算法

- [x] InsightsComparator 类已创建
- [x] InsightsDiff 结构体包含 regressions, improvements, new_zones, removed_zones
- [x] ZoneDiff 结构体包含 name, time_increase_ns, percentage_change
- [x] compute_diff() 按 zone name 匹配并计算差异
- [x] highlight_regressions() 按阈值标记回归
- [x] generate_summary() 生成文本摘要

## InsightsReplay 回放引擎

- [x] InsightsReplay 类已创建
- [x] play() / pause() / seek() / set_speed() 方法已实现
- [x] get_zone_at(time_ns) 查询指定时间的活跃 zone
- [x] tick(delta) 推进 current_time_ns

## InsightsDatabase 查询扩展

- [x] query_zones_by_depth(depth) 按嵌套深度查询 zone
- [x] query_allocations_by_size(min_size) 按大小过滤分配记录
- [x] query_resource_loads() 查询所有资源加载记录
- [x] query_resource_dependencies(path) 查询资源依赖关系
- [x] get_peak_memory() 计算峰值内存
- [x] get_leaked_allocations() 获取未释放分配列表

## InsightsEditorPlugin 入口

- [x] InsightsEditorPlugin 类已创建，继承 EditorPlugin
- [x] _enter_tree() 注册主屏幕和底部面板
- [x] _exit_tree() 清理注册
- [x] _has_main_screen() 返回 true
- [x] _get_plugin_name() 返回 "Insights"
- [x] 菜单项已添加：Start Capture、Open Trace、Compare Traces
- [x] _start_capture() / _open_trace() / _compare_traces() 回调已实现

## InsightsDock 底部面板

- [x] InsightsDock 类已创建，继承 VBoxContainer
- [x] 工具栏按钮已创建（Start/Stop/Open/Compare/Clear/Settings）
- [x] Channel 标签页已创建（CPU/GPU/Memory/Loading/Network/Log/Compare）
- [x] 回放控制栏已创建（Play/Pause/Speed/Time）
- [x] 信号回调已连接

## InsightsTimeline 时间轴

- [x] InsightsTimeline 类已创建，继承 Control
- [x] _draw() 自绘时间轴（帧标尺 + 多 Track）
- [x] _gui_input() 处理缩放/平移/点击
- [x] set_database() 从数据库加载 zone 数据
- [x] get_total_duration_ns() 返回总时长
- [x] get_visible_range() 返回可见时间范围
- [x] get_selected_zone() 返回选中 zone
- [x] zoom_to_zone() 缩放到指定 zone

## InsightsFlamegraph 火焰图

- [x] InsightsFlamegraph 类已创建，继承 Control
- [x] FlameNode 结构体已定义
- [x] set_database() 从数据库构建层级树
- [x] get_root_nodes() 返回根节点列表
- [x] get_children(id) 返回子节点列表
- [x] set_search_query() 搜索过滤功能
- [x] get_filtered_nodes() 返回过滤后的节点
- [x] _draw() 渲染火焰图层级

## InsightsMemoryPanel 内存瀑布

- [x] InsightsMemoryPanel 类已创建，继承 Control
- [x] AllocEntry / LeakEntry 结构体已定义
- [x] set_database() 从数据库加载分配数据
- [x] get_peak_memory() 返回峰值内存
- [x] get_leaked_allocations() 返回泄漏列表
- [x] set_size_filter() 大小过滤功能
- [x] get_filtered_allocations() 返回过滤后的分配列表
- [x] _draw() 渲染内存瀑布图

## InsightsLoadingPanel 资源依赖图

- [x] InsightsLoadingPanel 类已创建，继承 Control
- [x] LoadNode 结构体已定义
- [x] set_database() 从数据库构建依赖树
- [x] get_root_loads() 返回根节点列表
- [x] get_dependencies(id) 返回依赖子节点列表
- [x] get_bottleneck() 返回最慢的加载节点

## InsightsNetworkPanel 网络流量

- [x] InsightsNetworkPanel 类已创建，继承 Control
- [x] set_database() 从数据库加载网络事件
- [x] _draw() 渲染网络流量时间线

## InsightsComparePanel 对比面板

- [x] InsightsComparePanel 类已创建，继承 Control
- [x] set_baseline() / set_current() 设置对比数据源
- [x] compute_diff() 调用 InsightsComparator 计算差异
- [x] _draw() 左右对比渲染，红绿高亮差异

## 注册与编译

- [x] register_types.cpp 已注册所有新类
- [x] config.py 的 doc_classes 列表已添加新类名
- [x] SCsub 已添加 editor/*.cpp 编译
- [x] `scons module_insights_enabled=yes platform=windows dev_build=yes` 编译通过

## 单元测试

- [x] 测试文件 `modules/insights/tests/test_insights_phase4.h` 已创建
- [x] EditorPlugin 注册测试通过 — _get_plugin_name() 返回 "Insights"
- [x] EditorPlugin 注册测试通过 — _has_main_screen() 返回 true
- [x] EditorPlugin 注册测试通过 — InsightsDock 可创建且为有效控件
- [x] Timeline 渲染测试通过 — get_total_duration_ns() 正确
- [x] Timeline 渲染测试通过 — get_visible_range() 返回有效范围
- [x] Timeline 渲染测试通过 — zone 选中功能正常
- [x] Flamegraph 层级测试通过 — get_root_nodes() 返回正确根节点
- [x] Flamegraph 层级测试通过 — get_children() 返回正确子节点
- [x] Flamegraph 层级测试通过 — set_search_query() 过滤功能正常
- [x] Memory Panel 测试通过 — get_peak_memory() 计算正确
- [x] Memory Panel 测试通过 — get_leaked_allocations() 返回正确泄漏列表
- [x] Memory Panel 测试通过 — set_size_filter() 过滤功能正常
- [x] Loading Panel 测试通过 — get_root_loads() 返回正确根节点
- [x] Loading Panel 测试通过 — get_dependencies() 返回正确依赖子节点
- [x] Loading Panel 测试通过 — get_bottleneck() 返回最慢加载节点
- [x] Compare Panel 测试通过 — compute_diff() regressions 列表正确
- [x] Compare Panel 测试通过 — compute_diff() new_zones 列表正确
