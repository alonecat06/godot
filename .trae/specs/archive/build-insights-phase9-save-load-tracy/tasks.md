# Tasks

- [x] Task 1: 添加 InsightsTracyBridge::get_file_info() 方法
  - [x] 1.1: 在 insights_tracy_bridge.h 中声明 `Dictionary get_file_info() const`（条件编译）
  - [x] 1.2: 在 insights_tracy_bridge.cpp 中实现：返回 Dictionary 包含 frame_count, zone_count, thread_count, gpu_context_count
  - [x] 1.3: 绑定 GDScript 方法

- [x] Task 2: 在 InsightsDock 添加 Save 按钮和保存对话框
  - [x] 2.1: 在 insights_dock.h 中添加 Button *btn_save 和 FileDialog *save_dialog 成员
  - [x] 2.2: 在 insights_dock.cpp 构造函数中创建 Save 按钮（条件编译）和 .tracy 保存对话框
  - [x] 2.3: 实现 _on_save_pressed()：检查 TracyBridge 是否有数据，有则弹出保存对话框
  - [x] 2.4: 实现 _on_save_file_selected()：调用 save_tracy_file，成功后 print_line 确认
  - [x] 2.5: 在 _update_button_states 中同步 Save 按钮状态（有数据时启用）

- [x] Task 3: 修改 Stop 流程 — 弹出保存对话框
  - [x] 3.1: 修改 _on_stop_pressed()：停止录制后弹出保存对话框（而非自动保存到 res://）
  - [x] 3.2: 取消保存时数据保留在内存中，Save 按钮仍可用

- [x] Task 4: 编译验证和基础测试
  - [x] 4.1: 非 Tracy 构建编译通过
  - [x] 4.2: Tracy 构建编译通过
  - [x] 4.3: 启动引擎，验证 Insights 面板正常显示

- [x] Task 5: 单元测试
  - [x] 5.1: 创建 `tests/test_insights_phase9.h`
  - [x] 5.2: 测试 get_file_info 在无 Worker 时返回空 Dictionary
  - [x] 5.3: 测试 save_tracy_file 无数据时返回 ERR_UNAVAILABLE
  - [x] 5.4: 测试 get_file_info 返回正确格式的 Dictionary（包含 frame_count, zone_count 等键）
  - [x] 5.5: 测试条件编译隔离（非 TRACY_SERVER_ENABLED 时测试文件为空）

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 depends on Task 2
- Task 4 depends on Task 2, Task 3
- Task 5 depends on Task 1, Task 4
