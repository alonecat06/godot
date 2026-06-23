# Tasks

- [x] Task 1: 创建 modules/insights/ 模块骨架与 SCons 构建注册
  - [x] 1.1 创建 `modules/insights/` 目录结构：`insights_core/`、`channels/`、`gpu/`、`editor/`、`tools/`（原 `core/` 重命名为 `insights_core/` 以避免与引擎 `core/` 目录冲突）
  - [x] 1.2 创建 `modules/insights/register_types.h/cpp`，注册 `InsightsManager`、`InsightsChannel`、`InsightsDatabase`、`InsightsCapture`（abstract）、`NativeCapture` 到 ClassDB
  - [x] 1.3 创建 `modules/insights/SCsub`，配置源文件列表和 include 路径
  - [x] 1.4 创建 `modules/insights/config.py`，定义 `module_insights_enabled` 编译选项（默认启用）
  - [x] 1.5 SCons 自动发现机制已正确检测 insights 模块（无需手动修改 `modules/SCsub`）
  - [x] 1.6 验证：`scons module_insights_enabled=yes` 编译通过 ✅

- [x] Task 2: 创建 core/profiling/insights.h 宏扩展
  - [x] 2.1 创建 `core/profiling/insights.h`，定义 `GodotProfileZoneC`、`GodotProfileZoneH`、`GodotProfileFiber`、`GodotProfilePlot`、`GodotProfileMessage`、`GodotProfileResourceLoad`、`GodotProfileGpuStage` 宏 + 8 个颜色常量
  - [x] 2.2 在 `GODOT_USE_TRACY` 分支下，将宏映射到 Tracy API（ZoneNamedN + ZoneColor / TracyPlot / TracyMessage / TracyGpuZone 等）
  - [x] 2.3 在无 Tracy 分支下，所有宏为 no-op stub，颜色常量为 0
  - [x] 2.4 `core/profiling/SCsub` 无需修改（insights.h 为纯头文件，无需编译）
  - [x] 2.5 验证：编译通过，宏在无 Tracy 时为 no-op ✅

- [x] Task 3: 实现 InsightsChannel 基类与 ChannelCategory 枚举
  - [x] 3.1 创建 `modules/insights/channels/insights_channel.h/cpp`，定义 `InsightsChannel::ChannelCategory` 枚举（类内枚举，CPU/GPU/MEMORY/SCRIPT/LOADING/NETWORK/LOG/CUSTOM）
  - [x] 3.2 实现 `InsightsChannel` 基类：`name`、`color`、`category`、`is_enabled` 属性 + `on_event()` / `serialize()` 虚方法
  - [x] 3.3 实现命名规范校验：`validate_zone_name()` — 校验 `godot:<channel>/<sub>/<op>` 格式
  - [x] 3.4 实现 `get_category_from_zone()` — 从 zone 名前缀自动推断 ChannelCategory
  - [x] 3.5 实现 ChannelCategory → Color 映射表（CPU=#7AC0E5, GPU=#C0E57A, Memory=#E57AC0, Script=#478CBF, Loading=#E5C77A, Network=#E5E57A, Log=#C0C0C0）
  - [x] 3.6 验证：编译通过 ✅（单元测试待后续补充）

- [x] Task 4: 实现 InsightsManager 单例
  - [x] 4.1 创建 `modules/insights/insights_core/insights_manager.h/cpp`，定义 `InsightsManager` 单例（继承 Object）
  - [x] 4.2 实现 `start_capture(path)` / `stop_capture()` / `is_recording()` 生命周期方法
  - [x] 4.3 实现 `register_channel()` / `get_channel()` / `get_channel_count()` Channel 管理方法
  - [x] 4.4 实现 `tick(delta)` 每帧回调
  - [x] 4.5 在 `main/main.cpp` 中添加 `InsightsManager::tick()` 调用（`MODULE_INSIGHTS_ENABLED` 条件编译）
  - [x] 4.6 实现安全停止：引擎关闭时若正在录制则自动 stop_capture
  - [x] 4.7 验证：编译通过 ✅（单元测试待后续补充）

- [x] Task 5: 实现 InsightsDatabase 基础框架
  - [x] 5.1 创建 `modules/insights/insights_core/insights_database.h/cpp`
  - [x] 5.2 实现 `open(path)` / `close()` — 内存数据库打开/关闭（Phase 1 使用内存+二进制序列化替代 SQLite）
  - [x] 5.3 实现 `create_tables()` — API 兼容（内存模式为 no-op）
  - [x] 5.4 实现 `insert_zone()` / `insert_frame_marker()` / `insert_allocation()` 基本插入方法
  - [x] 5.5 实现 `query_zone(name, start_ns, end_ns)` 按 name + 时间范围查询（含 HashMap 索引）
  - [x] 5.6 验证：编译通过 ✅（单元测试待后续补充）

- [x] Task 6: 实现 InsightsCapture 抽象与 NativeCapture
  - [x] 6.1 创建 `modules/insights/insights_core/insights_capture.h/cpp`，定义 `InsightsCapture` 抽象基类（GDREGISTER_ABSTRACT_CLASS）
  - [x] 6.2 创建 `modules/insights/insights_core/native_capture.h/cpp`
  - [x] 6.3 实现双缓冲事件队列（基于 `LocalVector<TraceEvent>` + Mutex，生产者-消费者模式）
  - [x] 6.4 定义 TraceEvent 结构体（type 枚举 + union 承载不同事件类型数据）
  - [x] 6.5 实现消费者线程：从事件队列读取事件并批量写入 InsightsDatabase
  - [x] 6.6 实现 `NativeCapture::start()` — 启动消费者线程
  - [x] 6.7 实现 `NativeCapture::stop()` — 设置停止标志 + 等待消费者线程完成 + flush
  - [x] 6.8 验证：编译通过 ✅（单元测试待后续补充）

# Task Dependencies
- [Task 2] depends on [Task 1] — insights.h 需要 modules/insights 编译环境
- [Task 3] depends on [Task 1] — InsightsChannel 在 modules/insights 中
- [Task 4] depends on [Task 3, Task 6] — InsightsManager 依赖 Channel 体系和 Capture
- [Task 5] depends on [Task 1] — InsightsDatabase 在 modules/insights 中
- [Task 6] depends on [Task 5] — NativeCapture 写入 InsightsDatabase
- [Task 1] 无依赖，可先行
