# Godot Engine 引擎循环与线程模型深度分析

## 1. 概述

Godot 引擎采用**主线程串行 + 服务器可选多线程**的架构。核心帧循环在主线程上串行执行，但物理服务器和渲染服务器可配置为独立线程运行，通过 `CommandQueueMT` 命令队列与主线程同步。动画、脚本、输入均在主线程上执行。

## 2. 检索过程

1. `main/main.cpp:4835-5058` → `Main::iteration()` 完整帧循环实现
2. `core/os/main_loop.h` → `MainLoop` 抽象接口（5 个虚方法）
3. `scene/main/scene_tree.h:85-350` → `SceneTree` 继承 `MainLoop`，ProcessGroup 多线程处理
4. `scene/main/scene_tree.cpp:628-1300` → `physics_process()`/`process()`/`_process_group()`/`_process_groups_thread()`
5. `servers/physics_3d/physics_server_3d_wrap_mt.h` → `PhysicsServer3DWrapMT`，`create_thread` 标志
6. `servers/physics_3d/physics_server_3d_wrap_mt.cpp` → 线程创建、`_thread_loop()`、`sync()`/`step()`
7. `servers/rendering/rendering_server_default.h:77-82` → `CommandQueueMT`、`create_thread`、`server_thread`
8. `servers/rendering/rendering_server_default.cpp:269-467` → `init()`/`sync()`/`draw()` 多线程逻辑
9. `servers/server_wrap_mt_common.h` → `FUNC*` 宏，命令队列 push/flush/sync 模式
10. `core/object/worker_thread_pool.h` → `WorkerThreadPool` 线程池
11. `core/templates/command_queue_mt.h` → `CommandQueueMT` 命令队列实现

## 3. 核心类层次

### 3.1 MainLoop 接口与 SceneTree

```mermaid
classDiagram
    class Object {
        +notification(what)
        +call(method, args)
    }

    class MainLoop {
        <<abstract>>
        +initialize() virtual
        +iteration_prepare() virtual
        +physics_process(time) bool virtual
        +iteration_end() virtual
        +process(time) bool virtual
        +finalize() virtual
    }

    class SceneTree {
        +physics_process(time) bool override
        +process(time) bool override
        +initialize() override
        +flush_transform_notifications()
        -_process(physics) void
        -_process_group(group, physics) void
        -_process_groups_thread(index, physics) void
        -process_groups : LocalVector~ProcessGroup~
        -default_process_group : ProcessGroup
        -node_threading_disabled : bool
        -physics_process_time : double
        -process_time : double
        -current_frame : int64_t
        -root : Window
        -_quit : bool
    }

    Object <|-- MainLoop
    MainLoop <|-- SceneTree
```

### 3.2 ServerWrapMT 线程包装体系

```mermaid
classDiagram
    class PhysicsServer3D {
        <<abstract>>
        +sync() virtual
        +flush_queries() virtual
        +end_sync() virtual
        +step(delta) virtual
        +init() virtual
        +finish() virtual
    }

    class PhysicsServer3DWrapMT {
        +sync() override
        +flush_queries() override
        +end_sync() override
        +step(delta) override
        +init() override
        +finish() override
        -physics_server_3d : PhysicsServer3D
        -command_queue : CommandQueueMT
        -server_thread : ThreadID
        -server_task_id : WorkerThreadPool TaskID
        -create_thread : bool
        -exit : bool
        -doing_sync : SafeFlag
        -_thread_loop()
        -_thread_sync()
        -_thread_exit()
    }

    class RenderingServerDefault {
        +sync() override
        +draw(present, frame_step) override
        +init() override
        +finish() override
        -command_queue : CommandQueueMT
        -server_thread : ThreadID
        -server_task_id : WorkerThreadPool TaskID
        -create_thread : bool
        -exit : bool
        -_thread_loop()
        -_draw(present, frame_step)
    }

    class CommandQueueMT {
        +push(target, method, args...)
        +push_and_ret(target, method, ret, args...)
        +push_and_sync(target, method, args...)
        +sync()
        +flush_all()
        +flush_if_pending()
        +set_pump_task_id(tid)
    }

    class WorkerThreadPool {
        +add_task(callable, high_prio, desc, long_task) TaskID
        +add_group_task(callable, count, tasks_per_thread, high_prio, desc) GroupID
        +wait_for_task_completion(tid)
        +wait_for_group_task_completion(gid)
        +yield()
    }

    PhysicsServer3D <|-- PhysicsServer3DWrapMT
    PhysicsServer3DWrapMT --> CommandQueueMT : uses
    PhysicsServer3DWrapMT --> WorkerThreadPool : creates task
    RenderingServerDefault --> CommandQueueMT : uses
    RenderingServerDefault --> WorkerThreadPool : creates task
```

### 3.3 ProcessGroup 多线程处理

```mermaid
classDiagram
    class ProcessGroup {
        +call_queue : CallQueue
        +nodes : Vector~Node~
        +physics_nodes : Vector~Node~
        +node_order_dirty : bool
        +physics_node_order_dirty : bool
        +removed : bool
        +owner : Node
        +last_pass : uint64_t
    }

    class SceneTree {
        +process_groups : LocalVector~ProcessGroup~
        +default_process_group : ProcessGroup
        +process_groups_dirty : bool
        +node_threading_disabled : bool
        -_process_group(group, physics)
        -_process_groups_thread(index, physics)
        -_process(physics)
    }

    SceneTree --> ProcessGroup : manages
```

## 4. 完整帧循环详解

### 4.1 Main::iteration() 流程

```mermaid
flowchart TD
    A["Main::iteration()"] --> B["计算 MainFrameTime<br>main_timer_sync.advance()"]
    B --> C["XRServer._process()"]
    C --> D["=== 物理步进循环 ==="]

    D --> E["循环 N 次 (advance.physics_steps)"]
    E --> F["Input.flush_buffered_events()"]
    F --> G["MainLoop.iteration_prepare()"]
    G --> H["PhysicsServer3D.sync() + flush_queries()"]
    H --> I["PhysicsServer2D.sync() + flush_queries()"]
    I --> J["MainLoop.physics_process(step)"]

    J --> K["NavigationServer2D/3D.physics_process()"]
    K --> L["message_queue.flush()"]
    L --> M["PhysicsServer3D.end_sync() + step()"]
    M --> N["PhysicsServer2D.end_sync() + step()"]
    N --> O["message_queue.flush()"]
    O --> P["MainLoop.iteration_end()"]

    P --> Q{还有物理步?}
    Q -->|是| E
    Q -->|否| R["=== 帧处理 ==="]

    R --> S["Input.flush_buffered_events()"]
    S --> T["MainLoop.process(step)"]
    T --> U["message_queue.flush()"]
    U --> V["NavigationServer2D/3D.process()"]
    V --> W["RenderingServer.sync()"]

    W --> X{需要绘制?}
    X -->|是| Y["RenderingServer.draw()"]
    X -->|否| Z["跳过绘制"]

    Y --> AA["GDExtensionManager.frame()"]
    Z --> AA
    AA --> BB["ScriptServer.frame() - 各语言帧回调"]
    BB --> CC["AudioServer.update()"]
    CC --> DD["EngineDebugger.iteration()"]
    DD --> EE["帧结束"]
```

### 4.2 帧时序图

```mermaid
sequenceDiagram
    participant Main as MainThread
    participant Input as Input
    participant Tree as SceneTree
    participant Phys3D as Physics3D
    participant Phys2D as Physics2D
    participant Nav as Navigation
    participant RS as Rendering
    participant Audio as Audio
    participant Script as Script

    Note over Main: frame start

    Main->>Input: flush events
    Input-->>Main: ok

    loop physics step
        Main->>Tree: iterationPrepare
        Main->>Phys3D: syncAndFlush
        Main->>Phys2D: syncAndFlush
        Main->>Tree: physicsProcess
        Main->>Nav: physicsProcess
        Main->>Phys3D: endSyncAndStep
        Main->>Phys2D: endSyncAndStep
        Note over Main: msgQueueFlush
        Main->>Tree: iterationEnd
    end

    Main->>Input: flushEvents
    Main->>Tree: process
    Note over Main: msgQueueFlush
    Main->>Nav: process
    Main->>RS: sync
    Main->>RS: draw
    Main->>Script: frame
    Main->>Audio: update

    Note over Main: frame end
```

## 5. 多线程模型详解

### 5.1 线程架构总览

```mermaid
flowchart TD
    subgraph MainThread["主线程 (Main Thread)"]
        M1["Main::iteration()"]
        M2["SceneTree._process()"]
        M3["SceneTree.physics_process()"]
        M4["AnimationTree 更新"]
        M5["GDScript / C# 脚本"]
        M6["Input 事件处理"]
        M7["NavigationServer"]
        M8["AudioServer.update()"]
        M9["ScriptServer.frame()"]
    end

    subgraph PhysicsThread["物理线程 (可选)"]
        P1["PhysicsServer3D.step()"]
        P2["PhysicsServer2D.step()"]
        P3["碰撞检测"]
        P4["刚体模拟"]
    end

    subgraph RenderThread["渲染线程 (可选)"]
        R1["RenderingServer._draw()"]
        R2["RenderingDevice 提交"]
        R3["GPU 命令编码"]
        R4["Present"]
    end

    subgraph WorkerPool["WorkerThreadPool"]
        W1["工作线程 1"]
        W2["工作线程 2"]
        W3["工作线程 N"]
        W4["ProcessGroup 并行处理"]
    end

    M1 --> M2
    M1 --> M3
    M3 -->|"sync + flush_queries"| P1
    P1 -->|"end_sync + step (异步)"| P2
    M2 -->|"sync"| R1
    M2 -->|"draw (异步)"| R1
    M3 -->|"ProcessGroup 并行"| W4
    W1 --> W4
    W2 --> W4
    W3 --> W4

    style MainThread fill:#e3f2fd
    style PhysicsThread fill:#fff3e0
    style RenderThread fill:#e8f5e9
    style WorkerPool fill:#fce4ec
```

### 5.2 CommandQueueMT 同步机制

```mermaid
flowchart TD
    subgraph MainThreadSide["主线程端"]
        A["调用 RenderingServer API"]
        B{"Thread.get_caller_id == server_thread?"}
        B -->|是| C["直接调用底层实现<br>flush_if_pending"]
        B -->|否| D["command_queue.push"]
        D --> E{需要返回值?}
        E -->|是| F["push_and_ret<br>阻塞等待返回值"]
        E -->|否| G{需要同步?}
        G -->|是| H["push_and_sync<br>阻塞等待执行完成"]
        G -->|否| I["push<br>异步投递，不等待"]
    end

    subgraph ServerThreadSide["服务器线程端"]
        J["_thread_loop"]
        J --> K["WorkerThreadPool yield"]
        K --> L["被唤醒"]
        L --> M{doing_sync?}
        M -->|否| N["command_queue.flush_all"]
        M -->|是| O["等待 end_sync"]
        N --> J
        O --> J
    end

    I -->|"命令入队"| J
    F -->|"同步等待"| J
    H -->|"同步等待"| J
```

### 5.3 物理服务器线程模型

```mermaid
flowchart TD
    A["PhysicsServer3DWrapMT 初始化"] --> B{create_thread?}
    B -->|是| C["WorkerThreadPool.add_task(_thread_loop)"]
    C --> D["物理线程开始运行"]
    D --> E["_thread_loop() 无限循环"]
    E --> F["yield() 等待唤醒"]
    F --> G{doing_sync?}
    G -->|否| H["flush_all() 处理命令队列"]
    G -->|是| I["等待 end_sync"]
    H --> E
    I --> E

    B -->|否| J["server_thread = MAIN_ID<br>所有调用直接执行"]

    subgraph SyncProtocol["同步协议 (每物理步)"]
        K["主线程: sync()"] --> L["push_and_sync(_thread_sync)"]
        L --> M["doing_sync.set()"]
        M --> N["physics_server_3d->sync()"]
        N --> O["flush_queries()"]
        O --> P["主线程: physics_process()"]
        P --> Q["主线程: end_sync()"]
        Q --> R["doing_sync.clear()"]
        R --> S["主线程: step()"]
        S --> T["push(step 命令到队列)"]
        T --> U["物理线程: 执行 step()"]
    end

    style SyncProtocol fill:#f3e5f5
```

### 5.4 渲染服务器线程模型

```mermaid
flowchart TD
    A["RenderingServerDefault 初始化"] --> B{create_thread?}
    B -->|是| C["DisplayServer.release_rendering_thread()"]
    C --> D["WorkerThreadPool.add_task(_thread_loop)"]
    D --> E["渲染线程开始运行"]

    B -->|否| F["server_thread = MAIN_ID<br>所有调用直接执行"]

    subgraph RenderSync["渲染同步协议 (每帧)"]
        G["主线程: sync()"] --> H{create_thread?}
        H -->|是| I["command_queue.sync()<br>等待所有命令执行完"]
        H -->|否| J["flush_all()"]

        K["主线程: draw()"] --> L{create_thread?}
        L -->|是| M["push(_draw 命令到队列)<br>渲染线程异步执行"]
        L -->|否| N["直接调用 _draw()"]
    end

    style RenderSync fill:#e8f5e9
```

### 5.5 create_thread 决策

```
PhysicsServer3DWrapMT 构造:
  PhysicsServer3DWrapMT(PhysicsServer3D *p_contained, bool p_create_thread)

RenderingServerDefault 构造:
  RenderingServerDefault(bool p_create_thread)

决策因素:
  ┌─────────────────────────────────────────────────────┐
  │  PhysicsServer3D:                                    │
  │  ├── 默认: create_thread = true (桌面平台)           │
  │  ├── 移动端: create_thread = false                   │
  │  └── 由 PhysicsServer3DManager 根据平台决定          │
  │                                                      │
  │  RenderingServerDefault:                             │
  │  ├── 默认: create_thread = true (桌面平台)           │
  │  ├── 移动端/Web: create_thread = false               │
  │  └── 由 DisplayServer 和项目设置决定                 │
  └─────────────────────────────────────────────────────┘
```

## 6. 各子系统线程归属

### 6.1 详细线程归属表

| 子系统 | 默认线程 | 可否独立线程 | 同步机制 |
|--------|---------|-------------|---------|
| **脚本 (_process)** | 主线程 | 否 | N/A |
| **脚本 (_physics_process)** | 主线程 | 否 | N/A |
| **输入 (Input)** | 主线程 | 否 | N/A |
| **动画 (AnimationTree)** | 主线程 | 否 | N/A |
| **物理 (PhysicsServer3D)** | 可选独立线程 | 是 | sync/end_sync/step 命令队列 |
| **物理 (PhysicsServer2D)** | 可选独立线程 | 是 | sync/end_sync/step 命令队列 |
| **渲染 (RenderingServer)** | 可选独立线程 | 是 | sync/draw 命令队列 |
| **导航 (NavigationServer)** | 主线程 | 否 | N/A |
| **音频 (AudioServer)** | 主线程调用 | 内部有音频线程 | 音频驱动内部线程 |
| **XR (XRServer)** | 主线程 | 否 | N/A |
| **MessageQueue** | 主线程 flush | 否 | flush() 在帧边界执行 |
| **ProcessGroup** | WorkerThreadPool | 可选并行 | add_group_task 并行分发 |

### 6.2 线程交互时序

```mermaid
sequenceDiagram
    participant MT as 主线程
    participant PT as 物理线程
    participant RT as 渲染线程
    participant WT as WorkerThreadPool

    Note over MT,WT: === 物理步 ===

    MT->>PT: sync() - 阻塞等待物理线程完成上一帧
    MT->>PT: flush_queries() - 刷新碰撞回调
    MT->>MT: SceneTree.physics_process() - 脚本处理
    MT->>PT: end_sync() - 释放物理线程
    MT->>PT: step(delta) - 投递物理步进命令（异步）

    Note over PT: 物理线程开始 step() 模拟
    Note over MT: 主线程不等待，继续下一物理步

    Note over MT,WT: === 帧处理 ===

    MT->>MT: SceneTree.process() - 脚本处理
    MT->>RT: sync() - 阻塞等待渲染线程完成上一帧 draw
    MT->>RT: draw() - 投递绘制命令（异步）

    Note over RT: 渲染线程开始 _draw() 渲染

    MT->>WT: ProcessGroup 并行分发（可选）
    WT-->>MT: 等待所有 group 完成

    MT->>MT: AudioServer.update()
    Note over MT: 帧结束，下一帧 sync() 会等待渲染完成
```

## 7. SceneTree 的 ProcessGroup 多线程

### 7.1 ProcessGroup 并行处理流程

```mermaid
flowchart TD
    A["SceneTree._process(physics)"] --> B{process_groups_dirty?}
    B -->|是| C["重建 process_groups 列表"]
    B -->|否| D["遍历 process_groups"]

    C --> D
    D --> E{group 有 owner 且<br>node_threading_disabled = false?}
    E -->|否| F["_process_group(group, physics)<br>主线程直接处理"]
    E -->|是| G["加入 local_process_group_cache"]

    G --> H{cache 大小 > 阈值?}
    H -->|否| F
    H -->|是| I["WorkerThreadPool.add_template_group_task<br>并行处理所有 group"]

    I --> J["每个工作线程:<br>_process_groups_thread(index, physics)"]
    J --> K["Node.current_process_thread_group = group.owner"]
    K --> L["_process_group(group, physics)"]
    L --> M["遍历 group.nodes<br>调用 node._process() 或 _physics_process()"]
    M --> N["Node.current_process_thread_group = null"]

    I --> O["wait_for_group_task_completion()"]
    O --> P["所有 group 处理完成"]
```

### 7.2 ProcessGroup 线程安全

```
ProcessGroup 并行处理的前提条件:
1. Node 设置了 process_thread_group 属性 (owner != null)
2. node_threading_disabled == false (全局开关)
3. 同一 ProcessGroup 内的节点在同一个工作线程执行
4. 跨 ProcessGroup 的节点在不同工作线程执行
5. default_process_group 始终在主线程执行
6. call_queue 用于跨 ProcessGroup 的延迟调用

限制:
- 同一 ProcessGroup 内仍是串行执行
- 不同 ProcessGroup 之间可并行
- GUI/Control 节点通常在 default_process_group（主线程）
- call_deferred() 通过 CallQueue 跨线程安全通信
```

## 8. 物理时间步进机制

### 8.1 MainTimerSync

```mermaid
flowchart TD
    A["MainTimerSync.advance(physics_step, ticks_per_sec)"] --> B["计算 elapsed = 当前时间 - 上帧时间"]
    B --> C["elapsed *= time_scale"]
    C --> D["计算 physics_steps = floor(elapsed / physics_step)"]
    D --> E["限制 physics_steps <= max_physics_steps_per_frame"]
    E --> F["计算 interpolation_fraction<br>= 余数 / physics_step"]
    F --> G["返回 MainFrameTime<br>{physics_steps, process_step, interpolation_fraction}"]

    style F fill:#fff3e0
```

### 8.2 固定时间步 vs 变量时间步

```
物理步 (固定时间步):
  ├── physics_step = 1.0 / physics_ticks_per_second (默认 60 Hz)
  ├── 每帧可能执行 0-8 次物理步
  ├── 用于: PhysicsServer.step(), SceneTree._physics_process()
  └── delta 值恒定 = physics_step

帧步 (变量时间步):
  ├── process_step = 帧间隔时间
  ├── 每帧执行 1 次
  ├── 用于: SceneTree._process(), AnimationTree 更新, RenderingServer.draw()
  └── delta 值可变 = 实际帧时间
```

## 9. MessageQueue 跨线程通信

### 9.1 MessageQueue 工作机制

```mermaid
flowchart TD
    A["任意线程: call_deferred()"] --> B["MessageQueue.push_call(object, method, args)"]
    B --> C["消息入队 (线程安全)"]

    D["主线程: message_queue.flush()"] --> E["在帧边界执行"]
    E --> F["遍历队列中所有消息"]
    F --> G["调用 object.method(args)"]
    G --> H["处理删除队列"]
    H --> I["队列清空"]

    style E fill:#e8f5e9
```

### 9.2 三种跨线程通信机制对比

| 机制 | 用途 | 线程安全 | 阻塞 | 使用场景 |
|------|------|---------|------|---------|
| **CommandQueueMT** | Server API 调用 | 是 | push_and_ret/ push_and_sync 阻塞，push 不阻塞 | 物理/渲染服务器线程通信 |
| **MessageQueue** | 通用延迟调用 | 是 | 否 | call_deferred(), 帧边界执行 |
| **CallQueue (ProcessGroup)** | 跨 ProcessGroup 通信 | 是 | 否 | ProcessGroup 内 call_deferred |

## 10. 与 UE 的线程模型对比

### 10.1 架构对比

```mermaid
flowchart LR
    subgraph Godot["Godot 线程模型"]
        G1["主线程<br>脚本+动画+输入+导航"]
        G2["物理线程 (可选)<br>PhysicsServer3D/2D.step()"]
        G3["渲染线程 (可选)<br>RenderingServer._draw()"]
        G4["WorkerThreadPool<br>ProcessGroup 并行"]

        G1 -->|"sync/end_sync/step"| G2
        G1 -->|"sync/draw"| G3
        G1 -->|"group_task"| G4
    end

    subgraph UE["UE 线程模型"]
        U1["GameThread<br>脚本+动画+逻辑"]
        U2["渲染线程 (RHI)<br>始终独立"]
        U3["物理线程 (PhysX)<br>始终独立"]
        U4["任务图 (TaskGraph)<br>并行任务系统"]
        U5["音频线程"]
        U6["动画线程 (可选)"]

        U1 -->|"EnqueueRenderCommand"| U2
        U1 -->|"物理同步"| U3
        U1 -->|"任务分发"| U4
    end

    style Godot fill:#e3f2fd
    style UE fill:#fff3e0
```

### 10.2 详细对比

| 维度 | Godot | UE |
|------|-------|-----|
| **渲染线程** | 可选（桌面默认开启） | 始终独立运行 |
| **物理线程** | 可选（桌面默认开启） | 始终独立运行（PhysX） |
| **动画线程** | 无（主线程串行） | 可选（动画评估可卸载） |
| **脚本线程** | 无（主线程串行） | 无（GameThread 串行） |
| **并行框架** | WorkerThreadPool + ProcessGroup | TaskGraph + ParallelFor |
| **同步方式** | CommandQueueMT push/sync | EnqueueRenderCommand + FRenderCommandFence |
| **帧同步粒度** | 粗粒度（sync/draw 两步） | 细粒度（Fence + 逐资源同步） |
| **跨线程通信** | CommandQueueMT + MessageQueue | TQueue + EnqueueRenderCommand |
| **音频** | AudioServer 内部线程 | 独立音频线程 |
| **网络** | 主线程 | 主线程 + 可选 IO 线程 |
| **资源加载** | 主线程 + WorkerThreadPool | 异步加载线程池 |

## 11. Godot 线程模型的关键限制

### 11.1 动画在主线程

```
问题:
  AnimationTree 在 SceneTree._process() 中更新
  → 复杂骨骼动画占用主线程时间
  → 无法与渲染/物理并行

UE 对比:
  动画评估可在工作线程执行（通过 TaskGraph）
  动画驱动的 Root Motion 通过 FAnimTickFunction 调度

影响:
  角色密集场景（如 100+ 带 AnimationTree 的角色）
  动画更新成为主线程瓶颈
```

### 11.2 脚本无并行

```
问题:
  GDScript / C# _process() 和 _physics_process() 均在主线程
  ProcessGroup 并行仅限不同 group 之间
  同一 group 内的节点仍串行执行

UE 对比:
  UE 的 GameThread 也是串行执行脚本
  但 UE 有更完善的多线程任务系统
  可在 C++ 中创建异步任务

影响:
  脚本密集型游戏（如策略游戏、大量 AI）
  主线程成为瓶颈
```

### 11.3 渲染同步粒度粗

```
问题:
  RenderingServer.sync() 等待整个上一帧渲染完成
  没有逐资源/逐 Pass 的细粒度同步
  主线程在 sync() 处阻塞等待

UE 对比:
  UE 使用 FRenderCommandFence 细粒度同步
  可以逐资源检查是否可安全修改
  渲染线程始终运行，不等主线程

影响:
  主线程在 sync() 处可能卡顿
  特别是 GPU 密集场景
```

### 11.4 缺失的功能

| 功能 | 说明 | 严重度 |
|------|------|--------|
| **动画并行评估** | AnimationTree 无法在工作线程执行 | 🟡 中等 |
| **细粒度渲染同步** | 无 Fence/信号量机制，sync 粒度粗 | 🟡 中等 |
| **异步资源流式加载** | 无后台线程资源流式加载 | 🟡 中等 |
| **任务图系统** | 无依赖关系的任务调度图 | 🔴 严重 |
| **帧间流水线** | 无多帧流水线（主线程和渲染线程无法真正重叠） | 🟡 中等 |

## 12. 关键源码索引

| 类别 | 路径 |
|------|------|
| 主循环入口 | `main/main.cpp` (Main::iteration, L4835-L5060) |
| MainLoop 接口 | `core/os/main_loop.h` |
| SceneTree | `scene/main/scene_tree.h` / `scene_tree.cpp` |
| ProcessGroup | `scene/main/scene_tree.h` (L96-L105) |
| PhysicsServer3DWrapMT | `servers/physics_3d/physics_server_3d_wrap_mt.h` / `.cpp` |
| PhysicsServer2DWrapMT | `servers/physics_2d/physics_server_2d_wrap_mt.h` / `.cpp` |
| RenderingServerDefault | `servers/rendering/rendering_server_default.h` / `.cpp` |
| CommandQueueMT | `core/templates/command_queue_mt.h` / `.cpp` |
| ServerWrapMT 宏 | `servers/server_wrap_mt_common.h` |
| WorkerThreadPool | `core/object/worker_thread_pool.h` / `.cpp` |
| MessageQueue | `core/object/message_queue.h` / `.cpp` |
| MainTimerSync | `core/os/main_timer_sync.h` / `.cpp` |
