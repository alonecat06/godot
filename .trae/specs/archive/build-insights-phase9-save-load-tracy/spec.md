# 保存/加载 .tracy Spec (Phase 9)

## Why
Phase 7/8 已实现 InsightsTracyBridge 的 save_tracy_file() 和 load_tracy_file() 基本功能，但保存时自动生成路径（`res://` 下），用户无法选择保存位置；也没有"保存"按钮和文件保存对话框；更重要的是，缺少对 .tracy 文件完整性的验证——保存后重新加载的数据应与原始数据一致，且外部 Tracy 工具能直接打开。

## What Changes
- 在 InsightsDock 工具栏添加 "Save" 按钮，点击弹出 FileDialog 让用户选择保存路径
- 添加保存文件对话框（.tracy 过滤器）
- 实现 round-trip 验证：save_tracy_file → load_tracy_file → 对比 zone_count/frame_count 一致
- 修改 Stop 流程：停止录制后弹出保存对话框（而非自动保存到 res://）
- 添加 InsightsTracyBridge::get_file_info() 辅助方法，返回 .tracy 文件元数据（版本、帧数、线程数等）

## Impact
- Affected code: `modules/insights/editor/insights_dock.h/.cpp`, `modules/insights/insights_tracy_bridge.h/.cpp`
- 无破坏性变更，现有功能不受影响

## ADDED Requirements

### Requirement: Save 按钮和保存对话框
系统 SHALL 提供 "Save" 按钮，允许用户将当前录制的 Tracy 数据保存为 .tracy 文件到指定路径。

#### Scenario: 点击 Save 按钮
- **WHEN** 用户点击 Save 按钮且 TracyBridge 有数据
- **THEN** 弹出 FileDialog，默认过滤 *.tracy
- **WHEN** 用户选择路径后
- **THEN** 调用 save_tracy_file() 保存文件，成功后 print_line 确认

#### Scenario: 无数据时 Save 禁用
- **WHEN** TracyBridge 无数据
- **THEN** Save 按钮禁用

### Requirement: Stop 后提示保存
系统 SHALL 在停止录制后提示用户保存 .tracy 文件。

#### Scenario: 停止录制
- **WHEN** 用户点击 Stop 且 TracyBridge 有录制数据
- **THEN** 自动弹出保存对话框，默认文件名含时间戳
- **WHEN** 用户取消保存
- **THEN** 数据仍然在内存中（TracyBridge 仍持有 Worker），用户可稍后通过 Save 按钮保存

### Requirement: Round-trip 验证
系统 SHALL 确保保存的 .tracy 文件能被 load_tracy_file 正确加载，数据一致。

#### Scenario: Save → Load 验证
- **WHEN** 保存 .tracy 文件后重新加载
- **THEN** load_tracy_file 返回 OK
- **THEN** 重新加载后的 zone_count 和 frame_count 与保存前一致

### Requirement: 文件元数据查询
系统 SHALL 提供 get_file_info() 方法返回 .tracy 文件元数据。

#### Scenario: 查询元数据
- **WHEN** TracyBridge 持有数据
- **THEN** get_file_info() 返回 Dictionary 包含 frame_count, zone_count, thread_count, gpu_context_count

## MODIFIED Requirements

### Requirement: InsightsDock Stop 流程
Phase 8 的 Stop 流程自动保存到 res:// 路径。Phase 9 修改为弹出保存对话框让用户选择路径，取消时数据仍保留在内存中。
