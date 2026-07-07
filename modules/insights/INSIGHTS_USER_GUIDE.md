# Godot Insights 功能说明与使用指南

## 概述

Godot Insights 是一个对标 Unreal Insights 的内置性能录制分析工具，以 Godot 引擎模块方式实现。它提供 CPU / GPU / 内存 / 脚本 / 网络 / 资源加载等多通道录制，以及编辑器内嵌的可视化分析界面。

---

## 1. 编译与启用

### 1.1 编译参数

Insights 作为可选模块，编译时需显式启用：

```bash
# 编译带 Insights 的编辑器（开发版）
scons platform=windows module_insights_enabled=yes dev_build=yes

# 编译带 Insights 的编辑器（正式版）
scons platform=windows module_insights_enabled=yes
```

关键参数：
- `module_insights_enabled=yes` — 必须传入，否则模块不会被编译
- `dev_build=yes` — 开发版本，包含调试信息

### 1.2 Tracy 后端（可选）

如需启用 Tracy 实时连接后端，额外添加：

```bash
scons platform=windows module_insights_enabled=yes profiler=tracy
```

---

## 2. 编辑器面板

### 2.1 打开 Insights 面板

编译完成后启动编辑器，Insights 面板位于底部面板区域。可通过以下方式访问：

- **底部面板标签**：点击底部面板中的 "Insights" 标签
- **菜单栏**：`编辑器 → 编辑器面板 → Insights`

### 2.2 面板操作

| 操作 | 方法 |
|------|------|
| **关闭面板** | 点击面板标签上的关闭按钮 |
| **重新打开** | 菜单 `编辑器 → 编辑器面板 → Insights` |
| **浮动面板** | 点击工具栏上的 "Float" 按钮，面板将脱离主窗口成为独立浮动窗口 |

### 2.3 面板布局

```
┌──────────────────────────────────────────────────────────────────┐
│  [▶ Start] [⏹ Stop] [📂 Open] [⚖ Compare] [🗑 Clear] [Float]   │
├──────────────────────────────────────────────────────────────────┤
│  [CPU] [GPU] [Memory] [Loading] [Network] [Log] [Compare]       │
├──────────────────────────────────────────────────────────────────┤
│  [▶ Play] [⏸ Pause] [Speed: 1x] [Time: 0:00.000 / 0:10.000]   │
├──────────────────────────────────────────────────────────────────┤
│  时间轴区域 (Canvas 自绘)                                        │
│  CPU:  ████ ████ ████                                           │
│  GPU:       ████ ████                                           │
│  Load:        ████                                              │
│  Net:  ████ ████                                                │
├──────────────────────────────────────────────────────────────────┤
│  详情区域（随 Channel Tab 切换）                                  │
│  - Flamegraph 火焰图 / Memory 瀑布图 / Loading 依赖树 / ...     │
└──────────────────────────────────────────────────────────────────┘
```

---

## 3. 录制工作流

### 3.1 开始/停止录制

1. 打开 Insights 底部面板
2. 点击工具栏 **▶ Start** 按钮开始录制
3. 在编辑器中运行项目（F5）或进行需要分析的操作
4. 点击 **⏹ Stop** 按钮停止录制
5. 录制数据自动保存为 `.gitracy` 文件

### 3.2 打开已有录制

1. 点击工具栏 **📂 Open** 按钮
2. 选择 `.gitracy` 或 `.tracy` 文件
3. 数据加载后在面板中显示

### 3.3 清除当前数据

点击 **🗑 Clear** 按钮清除当前面板中的所有录制数据。

### 3.4 远程录制（Live Profiling）

Insights 支持连接到正在运行的游戏进程进行实时性能分析：

```gdscript
# 通过 InsightsManager 连接远程实例
var mgr = InsightsManager.get_singleton()
mgr.start_live_capture("localhost", 8086)

# 检查是否处于 live 模式
if mgr.is_live_mode():
    var live_db = mgr.get_live_database()
    # 实时查询数据

# 停止 live 捕获
mgr.stop_live_capture()
```

---

## 4. Channel 体系

Insights 使用 Channel 体系对性能数据进行分类。每个 Channel 代表一个子系统，拥有独立的颜色标识和数据类型。

### 4.1 内置 Channel

| Channel | 颜色 | 说明 | Zone 命名规范 |
|---------|------|------|---------------|
| **CPU** | `#7AC0E5` | CPU Zone 追踪，包含主线程、工作线程、物理、渲染等 | `godot:cpu/<sub>/<op>` |
| **GPU** | `#C0E57A` | GPU 时间戳查询，支持 Vulkan/D3D12/Metal 后端 | `godot:gpu/command/<op>` |
| **Memory** | `#E57AC0` | 内存分配/释放追踪，泄漏检测 | `godot:mem/<op>` |
| **Script** | `#478CBF` | GDScript/C# 脚本函数调用追踪 | `godot:script/<lang>/<op>` |
| **Loading** | `#E5C77A` | 资源加载追踪，依赖关系图 | `godot:loading/<op>` |
| **Network** | `#E5E57A` | 多人网络 RPC/包流量 | `godot:network/<op>` |
| **Log** | `#C0C0C0` | 日志消息（错误/警告/信息）与 Zone 关联 | `godot:log/<op>` |
| **Custom** | 用户定义 | 用户自定义 Channel | 自定义 |

### 4.2 CPU Channel — 锁竞争检测

CPU Channel 支持锁竞争追踪，可识别多线程环境中的锁瓶颈：

```gdscript
var cpu_channel = InsightsManager.get_singleton().get_channel("cpu")
cpu_channel.enable_contention_tracking()

# 获取竞争事件
var events = cpu_channel.get_contention_events()
for event in events:
    print("锁: ", event.lock_name)
    print("等待线程: ", event.waiter_thread)
    print("等待时间: ", event.wait_time_ns, " ns")
```

竞争事件包含以下信息：
- `lock_name` — 锁的名称
- `owner_thread` — 持有锁的线程
- `waiter_thread` — 等待锁的线程
- `wait_time_ns` — 等待时间（纳秒）
- `acquire_ns` — 获得锁的时间
- `release_ns` — 释放锁的时间

### 4.3 Custom Channel — 自定义 Zone

用户和 GDExtension 可通过 Custom Channel API 插入自定义的性能追踪 Zone：

```gdscript
# 注册自定义 Channel
var mgr = InsightsManager.get_singleton()
var custom = CustomChannel.new()
custom.set_name("my_game_ai")
custom.set_color(Color(0.8, 0.2, 0.8))
mgr.register_channel(custom)

# 开始/结束自定义 Zone
custom.begin_zone("ai/pathfinding", "ai_controller.cpp", 42)
# ... 执行寻路逻辑 ...
custom.end_zone()

# 查询已记录的 Zone
var zones = custom.get_zones()
for zone in zones:
    print("Zone: ", zone.name, " Channel: ", zone.channel)
```

---

## 5. 可视化视图

### 5.1 Timeline 时间轴

时间轴视图显示所有 Channel 的 Zone 在时间轴上的分布：

- **水平轴**：时间（纳秒精度）
- **垂直轴**：按 Channel 和线程分行显示
- **交互**：
  - 鼠标滚轮缩放
  - 拖拽平移
  - 点击 Zone 查看详情
  - 帧标尺显示帧边界

### 5.2 Flamegraph 火焰图

火焰图以调用栈形式展示 Zone 的层级关系：

- **宽度**：表示 Zone 持续时间
- **高度**：表示调用深度
- **颜色**：按 Channel 着色
- **交互**：
  - 搜索过滤
  - 点击缩放
  - 悬停显示详情

### 5.3 Memory Panel 内存面板

内存面板显示内存分配的生命周期：

- 分配/释放时间线
- 峰值内存统计
- 泄漏检测（未释放的分配）
- 按大小过滤

### 5.4 Loading Panel 加载面板

加载面板以依赖树形式展示资源加载过程：

- 资源加载依赖关系图
- 加载耗时排序
- 瓶颈检测（最慢加载）
- 内存大小排序

### 5.5 Network Panel 网络面板

网络面板显示多人游戏中的网络活动：

- RPC 调用追踪
- 数据包流量
- 带宽指标

### 5.6 Compare Panel 对比面板

对比面板支持两个 Trace 文件的差异分析：

1. 点击 **⚖ Compare** 按钮
2. 选择 baseline（基准）和 current（当前）`.gitracy` 文件
3. 系统自动计算差异：
   - **Regressions**（回归）：变慢的 Zone
   - **Improvements**（改善）：变快的 Zone
   - **New Zones**：新增的 Zone
   - **Removed Zones**：移除的 Zone
4. 红/绿高亮显示差异

### 5.7 Contention Panel 竞争面板

竞争面板专门显示锁竞争事件，包含两个 Tree 控件：
- 按锁名称分组的竞争事件列表
- 按等待时间排序的热点锁

---

## 6. Trace 文件格式

### 6.1 .gitracy 格式

Godot Insights 的原生录制格式为 `.gitracy`，采用双层设计：

- **元数据层**（TOML）：记录项目名、引擎版本、捕获时间、Channel 配置等
- **数据层**（二进制序列化）：Zone、分配、帧标记、资源加载等事件数据

### 6.2 .tracy 格式兼容

Insights 支持与 Tracy 原生 `.tracy` 格式的互转。

#### 命令行转换

```bash
# .gitracy → .tracy
godot --insights-convert input.gitracy output.tracy

# .tracy → .gitracy
godot --insights-convert input.tracy output.gitracy
```

#### TracyConverter API

```gdscript
var converter = TracyConverter.new()
var err = converter.gitracy_to_tracy("res://recording.gitracy", "res://recording.tracy")
if err == OK:
    print("转换成功")
```

---

## 7. 脚本集成

### 7.1 GDScript @profiler_zone 装饰器

在 GDScript 中使用 `@profiler_zone` 装饰器自动追踪函数执行时间：

```gdscript
extends Node

# 使用函数名作为 Zone 名称
@profiler_zone
func _ready():
    # 此函数执行时自动记录到 Script Channel
    pass

# 使用自定义名称
@profiler_zone("heavy_computation")
func expensive_calculation():
    var sum = 0
    for i in range(10000):
        sum += i
    return sum
```

当 Insights 录制进行时，被装饰的函数会自动产生 Zone 记录。

### 7.2 C# 插桩

通过 MonoProfilerBridge 自动追踪 C# 方法调用（需启用 Mono 模块）：

```csharp
// C# 方法调用在录制期间会自动通过 MonoProfilerBridge 记录
// 支持异步/协程方法的挂起和恢复追踪
public partial class Player : Node3D
{
    public override void _PhysicsProcess(double delta)
    {
        // 此方法调用会被自动记录到 Script Channel
        MoveAndSlide();
    }
}
```

### 7.3 C++ 插桩宏

引擎和模块开发者可使用以下宏进行性能插桩：

```cpp
#include "core/profiling/insights.h"

// 通道化 Zone
GodotProfileZoneC("cpu", "godot:physics/3d/step");

// 层级化 Zone（自动用 / 分隔）
GodotProfileZoneH("physics", "3d", "step");

// 异步任务追踪
GodotProfileFiber("async_task");

// 资源加载追踪
GodotProfileResourceLoad("res://texture.png");

// GPU 阶段
GodotProfileGpuStage("render_forward");

// 计数器
GodotProfilePlot("fps", 60.0);

// 自定义消息
GodotProfileMessage("Checkpoint reached");
```

当 `GODOT_USE_TRACY` 未定义时，所有宏为零开销的空操作。

---

## 8. 工具与导出

### 8.1 Web 导出器

将 Trace 数据导出为独立的 HTML 报告，可在浏览器中查看：

```gdscript
var exporter = WebExporter.new()
var err = exporter.export_to_html("res://recording.gitracy", "res://report.html")
if err == OK:
    print("导出成功，用浏览器打开 report.html 查看")
```

HTML 报告包含：
- 内嵌 Timeline 可视化
- 内嵌 Flamegraph 可视化
- 无需安装任何软件即可查看

### 8.2 CLI 命令行工具

Insights 提供命令行工具，适合 CI/CD 集成：

```bash
# 比较两个 trace，检测性能回归（10% 阈值）
godot --insights-compare baseline.gitracy pr.gitracy --threshold 0.1

# 导出 trace 为 HTML
godot --insights-export-html recording.gitracy output.html

# 格式转换
godot --insights-convert recording.gitracy recording.tracy
```

CI 集成示例：

```yaml
# GitHub Actions 示例
- name: Performance Regression Check
  run: |
    godot --insights-compare baseline.gitracy current.gitracy --threshold 0.1
    # 如果回归超过 10%，退出码为 1，CI 将失败
```

### 8.3 启动带 Insights 的游戏

通过 InsightsLauncher 工具启动带 Tracy 连接的游戏进程：

```gdscript
var launcher = InsightsLauncher.new()
var err = launcher.launch_with_insights("res://my_project", 8086)
if err == OK:
    print("游戏已启动，Tracy 端口: 8086")
    # InsightsManager 自动连接到游戏进程
```

---

## 9. AI 辅助分析

Insights 集成了 AI 分析器，可自动识别性能瓶颈并给出优化建议：

```gdscript
var analyzer = AIAnalyzer.new()

# 构建分析提示词
var prompt = analyzer.build_analysis_prompt(database)

# 解析 AI 响应（需配合外部 LLM 服务）
var result = analyzer.parse_response(llm_response)

print("瓶颈: ", result.bottleneck)
print("严重程度: ", result.severity)  # LOW / MEDIUM / HIGH / CRITICAL
for suggestion in result.suggestions:
    print("建议: ", suggestion)
```

---

## 10. 完整使用流程示例

### 场景：分析项目帧率下降

1. **启动编辑器**（带 Insights 编译）
2. **打开 Insights 面板**：底部面板 → "Insights"
3. **开始录制**：点击 ▶ Start
4. **运行项目**：F5 运行，复现帧率下降场景
5. **停止录制**：点击 ⏹ Stop
6. **查看 Timeline**：切到 CPU 标签页，在时间轴上找到帧时间异常的帧
7. **分析 Flamegraph**：点击 Flamegraph 标签，查看该帧的调用栈，找到耗时最长的 Zone
8. **检查 Memory**：切到 Memory 标签，确认是否有内存泄漏导致 GC 压力
9. **导出报告**：使用 Web Exporter 导出 HTML 报告分享给团队
10. **对比验证**：优化代码后重新录制，使用 Compare Panel 对比优化效果

---

## 11. 项目设置

以下项目设置可在 `Project Settings → Insights` 中配置：

| 设置 | 默认值 | 说明 |
|------|--------|------|
| `insights/cpu_enabled` | `true` | 启用 CPU Channel |
| `insights/gpu_enabled` | `true` | 启用 GPU Channel |
| `insights/memory_enabled` | `true` | 启用内存追踪 |
| `insights/script_enabled` | `true` | 启用脚本追踪 |
| `insights/loading_enabled` | `true` | 启用资源加载追踪 |
| `insights/network_enabled` | `false` | 启用网络追踪 |
| `insights/log_enabled` | `true` | 启用日志捕获 |
| `insights/log_severity_threshold` | `WARNING` | 日志最低捕获级别 |
| `insights/tracy_port` | `8086` | Tracy 连接端口 |
| `insights/max_capture_size_mb` | `1024` | 录制文件大小上限（MB） |
| `insights/frame_sample_rate` | `1` | 帧采样率（1 = 每帧采样） |

---

## 12. 性能影响

| 场景 | 开销 |
|------|------|
| 未启用 `module_insights_enabled` | 零开销（模块不编译） |
| 模块启用但未录制 | 极低开销（宏为空操作） |
| Native 录制中 | < 5% CPU 开销 |
| Tracy 后端录制 | < 5% CPU 开销 + 网络带宽 |
| GPU 时间戳 | 每帧额外 ~0.1ms（取决于查询数量） |

---

## 13. 常见问题

**Q: 编辑器中没有看到 Insights 面板？**

A: 确保编译时添加了 `module_insights_enabled=yes` 参数。如果之前编译过不带 Insights 的版本，需要清理后重新编译。

**Q: 面板关闭后如何重新打开？**

A: 通过菜单 `编辑器 → 编辑器面板 → Insights` 重新打开。

**Q: 如何让面板浮动？**

A: 点击工具栏上的 "Float" 按钮。

**Q: 录制文件太大？**

A: 可以通过项目设置降低 `frame_sample_rate`（如设为 5 表示每 5 帧采样一次），或减小 `max_capture_size_mb`。

**Q: 如何在 CI 中自动检测性能回归？**

A: 使用 CLI 工具：`godot --insights-compare baseline.gitracy current.gitracy --threshold 0.1`，回归超过阈值时返回非零退出码。
