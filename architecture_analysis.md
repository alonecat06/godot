# Godot Engine 代码架构分析

## 1. 项目概述

本项目为 **Godot Engine** 的 C++ 源码，是一款开源的跨平台游戏引擎。引擎采用模块化架构设计，核心由以下几大子系统构成：

| 子系统 | 目录 | 职责 |
|--------|------|------|
| Core | `core/` | 基础类型系统、对象模型、IO、数学库、线程等 |
| Scene | `scene/` | 场景节点树、2D/3D节点、GUI、动画、资源 |
| Servers | `servers/` | 渲染、物理、音频、导航等后台服务 |
| Modules | `modules/` | 可选功能模块（GDScript、Mono、物理引擎等） |
| Editor | `editor/` | 编辑器界面与工具 |
| Drivers | `drivers/` | 平台相关的底层驱动 |
| Main | `main/` | 引擎入口与主循环 |
| Platform | `platform/` | 平台抽象层 |

---

## 2. 顶层目录结构

```
workspace/
├── core/           # 核心基础库
├── scene/          # 场景系统
├── servers/        # 后台服务器
├── modules/        # 可选模块
├── editor/         # 编辑器
├── drivers/        # 驱动程序
├── main/           # 入口与主循环
├── platform/       # 平台抽象
├── tests/          # 测试框架
├── thirdparty/     # 第三方库
├── doc/            # 文档生成
├── SConstruct      # SCons 构建入口
└── methods.py      # 构建辅助脚本
```

---

## 3. Core 模块架构

`core/` 是引擎的最底层基础库，所有其他模块都依赖它。其内部结构如下：

```
core/
├── object/         # 对象系统（Object, ClassDB, MethodBind, Signal）
├── variant/        # 动态类型系统（Variant, Callable, Array, Dictionary）
├── math/           # 数学库（Vector, Matrix, AABB, BVH, 几何算法）
├── io/             # IO系统（文件访问、资源加载/保存、网络、压缩）
├── os/             # 操作系统抽象（OS, Thread, Memory, MainLoop）
├── string/         # 字符串系统（String, StringName, NodePath, 翻译）
├── config/         # 配置系统（Engine, ProjectSettings）
├── input/          # 输入系统（Input, InputEvent, InputMap）
├── crypto/         # 加密系统（AES, Hashing, Crypto）
├── debugger/       # 调试系统（远程调试、性能分析）
├── extension/      # GDExtension 系统（C/C++ 扩展接口）
├── error/          # 错误处理（错误列表、宏）
├── templates/      # 容器模板（HashMap, HashSet, Vector, List, RID）
├── profiling/      # 性能分析工具
└── register_core_types.cpp  # 核心类型注册入口
```

### 3.1 对象系统核心类层次

```
Object                    # 所有引擎对象的基类
├── RefCounted            # 引用计数对象
│   ├── Resource          # 资源基类（可序列化、可加载）
│   │   ├── Texture
│   │   ├── Mesh
│   │   ├── Material
│   │   ├── Shader
│   │   └── ...
│   ├── Image
│   ├── InputEvent
│   └── ...
└── Node (在 scene/ 中)   # 场景节点基类
    ├── Node2D
    ├── Node3D
    ├── Control
    └── ...
```

### 3.2 ClassDB — 运行时类型注册系统

`ClassDB`（`core/object/class_db.h`）是引擎的类型注册中心，负责：

- **类注册**：通过 `GDREGISTER_CLASS(ClassName)` 宏将类注册到系统
- **继承查询**：`get_parent_class()`、`is_parent_class()`
- **类列表获取**：`get_class_list()`、`get_inheriters_from_class()`
- **实例化**：`instantiate()`、`can_instantiate()`
- **方法绑定**：`bind_method()`、`get_method()`、`get_method_list()`
- **属性查询**：`get_property_list()`、`set_property()`、`get_property()`
- **信号管理**：`add_signal()`、`get_signal_list()`

### 3.3 Variant — 动态类型系统

`Variant`（`core/variant/variant.h`）是引擎的通用数据类型，支持：

- 基础类型：int, float, bool, String
- 数学类型：Vector2, Vector3, Color, Rect2, Transform2D, Transform3D
- 容器类型：Array, Dictionary
- 引擎类型：Object*, RID, Callable, Signal
- 运算符重载和类型转换

### 3.4 核心绑定（core_bind）

`core_bind.h/cpp` 将核心功能暴露给脚本系统，包括：

- `_ResourceLoader`：资源加载接口
- `_ResourceSaver`：资源保存接口
- `_OS`：操作系统接口
- `_Engine`：引擎状态接口
- `_ClassDB`：类数据库脚本接口
- `_JSON`：JSON 解析接口

---

## 4. Scene 模块架构

`scene/` 包含所有场景节点和资源定义：

```
scene/
├── main/           # 核心场景类（Node, SceneTree, Viewport, Window）
├── 2d/             # 2D 节点
│   ├── physics/    #   2D 物理节点（RigidBody2D, Area2D, CollisionShape2D...）
│   └── navigation/ #   2D 导航节点
├── 3d/             # 3D 节点
│   ├── physics/    #   3D 物理节点（RigidBody3D, Area3D, CollisionShape3D...）
│   ├── navigation/ #   3D 导航节点
│   └── xr/         #   XR 节点
├── gui/            # GUI 控件（Control, Button, Label, Tree, RichTextLabel...）
├── animation/      # 动画系统（AnimationPlayer, AnimationTree, Tween）
├── audio/          # 音频播放节点
├── resources/      # 场景资源定义
│   ├── 2d/         #   2D 资源（Shape2D, TileSet, NavigationPolygon...）
│   └── 3d/         #   3D 资源（Shape3D, Mesh, Sky, World3D...）
├── theme/          # 主题系统（ThemeDB, 默认主题, 图标）
├── debugger/       # 场景调试器
└── register_scene_types.cpp  # 场景类型注册入口
```

### 4.1 场景节点继承体系

```
Node                          # 场景节点基类（树结构、生命周期回调）
├── CanvasItem                # 可绘制节点基类
│   ├── Node2D                # 2D 空间节点
│   │   ├── Sprite2D, AnimatedSprite2D
│   │   ├── Camera2D, Light2D
│   │   ├── CollisionObject2D → Area2D / PhysicsBody2D
│   │   └── ...
│   └── Control               # GUI 控件基类
│       ├── Button, Label, LineEdit
│       ├── Container, Panel, Tree
│       └── ...
├── Node3D                    # 3D 空间节点
│   ├── VisualInstance3D → MeshInstance3D, GPUParticles3D
│   ├── Camera3D, Light3D
│   ├── CollisionObject3D → Area3D / PhysicsBody3D
│   └── ...
├── AnimationPlayer, AnimationTree
├── AudioStreamPlayer
├── Timer
└── Viewport → Window
```

### 4.2 SceneTree — 场景树管理

`SceneTree`（`scene/main/scene_tree.h`）继承自 `MainLoop`，是场景的核心管理器：

- 管理场景节点树的根节点
- 驱动节点生命周期（`_enter_tree`, `_ready`, `_process`, `_physics_process`）
- 管理节点组（Group）和组内查询
- 管理 Tween 动画
- 多人网络 API 接入

---

## 5. Servers 模块架构

`servers/` 实现引擎的后台服务，采用**接口-实现分离**模式：

```
servers/
├── rendering/              # 渲染服务器
│   ├── rendering_server.h          # 渲染接口定义
│   ├── rendering_server_default    # 默认实现（调度器）
│   ├── renderer_rd/                # Vulkan/Metal 渲染器实现
│   │   ├── forward_clustered/      #   前向聚类渲染管线
│   │   ├── forward_mobile/         #   移动端前向渲染管线
│   │   ├── storage_rd/             #   渲染数据存储
│   │   ├── effects/                #   后处理效果
│   │   ├── environment/            #   环境渲染（GI, 雾, 天空）
│   │   └── shaders/                #   GLSL 着色器
│   ├── dummy/                      # 空渲染器（无头模式）
│   └── storage/                    # 渲染数据存储接口
├── physics_2d/             # 2D 物理服务器接口
├── physics_3d/             # 3D 物理服务器接口
├── navigation_2d/          # 2D 导航服务器接口
├── navigation_3d/          # 3D 导航服务器接口
├── audio/                  # 音频服务器
│   ├── audio_server.h              # 音频接口
│   └── effects/                    # 音频效果（混响、合唱、延迟...）
├── display/                # 显示服务器（窗口管理、输入）
├── camera/                 # 摄像头服务器
├── text/                   # 文本渲染服务器
├── xr/                     # XR 服务器
├── movie_writer/           # 视频录制
├── debugger/               # 服务器调试
└── register_server_types.cpp  # 服务器类型注册入口
```

### 5.1 服务器架构模式

每个服务器遵循统一模式：

```
Server接口 (抽象类)           # 定义 API（如 PhysicsServer3D）
├── ServerDummy (空实现)      # 无操作实现，用于无头模式
├── ServerExtension (扩展点)  # 允许 GDExtension 实现自定义后端
└── ServerWrapMT (线程安全包装) # 多线程安全包装器
```

实际实现位于 `modules/` 中：
- `modules/godot_physics_2d/` → Godot 自带 2D 物理引擎
- `modules/godot_physics_3d/` → Godot 自带 3D 物理引擎
- `modules/jolt_physics/` → Jolt 3D 物理引擎

---

## 6. Modules 模块系统

`modules/` 包含可选功能模块，每个模块遵循统一结构：

```
modules/<module_name>/
├── config.py              # 模块配置（启用/禁用）
├── register_types.cpp     # 模块类型注册
├── register_types.h       # 注册函数声明
├── SCsub                  # SCons 构建脚本
└── <module_sources>       # 模块源码
```

### 6.1 关键模块一览

| 模块 | 功能 |
|------|------|
| `gdscript/` | GDScript 脚本语言（词法分析、解析、编译、VM） |
| `mono/` | C# 脚本语言支持 |
| `godot_physics_2d/` | 2D 物理引擎实现 |
| `godot_physics_3d/` | 3D 物理引擎实现 |
| `jolt_physics/` | Jolt 3D 物理引擎 |
| `gltf/` | glTF 导入/导出 |
| `fbx/` | FBX 导入 |
| `enet/` | ENet 网络多人 |
| `websocket/` | WebSocket 网络通信 |
| `webrtc/` | WebRTC 通信 |
| `multiplayer/` | 多人游戏同步框架 |
| `navigation_2d/` | 2D 导航实现 |
| `navigation_3d/` | 3D 导航实现 |
| `freetype/` | FreeType 字体渲染 |
| `text_server_adv/` | 高级文本服务器 |
| `msdfgen/` | MSDF 字体生成 |
| `mbedtls/` | TLS/SSL 加密 |
| `regex/` | 正则表达式 |
| `svg/` | SVG 图像加载 |
| `webp/` | WebP 图像编解码 |
| `dds/` | DDS 纹理加载 |
| `etcpak/` | ETC 纹理压缩 |
| `basis_universal/` | Basis 纹理压缩 |
| `openxr/` | OpenXR VR 支持 |
| `lightmapper_rd/` | GPU 光照贴图 |
| `noise/` | 噪声生成 |
| `gridmap/` | 网格地图 |
| `csg/` | CSG 几何体 |
| `zip/` | ZIP 压缩/解压 |
| `jsonrpc/` | JSON-RPC 协议 |
| `interactive_music/` | 交互式音乐系统 |
| `ogg/`, `vorbis/`, `mp3/`, `theora/` | 音视频编解码 |
| `camera/` | 摄像头访问 |
| `mobile_vr/` | 移动端 VR |
| `upnp/` | UPnP 网络穿透 |
| `objectdb_profiler/` | 对象数据库性能分析 |

---

## 7. Editor 模块架构

```
editor/
├── editor_node.h/cpp        # 编辑器主节点（核心入口）
├── editor_interface.h/cpp    # 编辑器脚本接口
├── editor_data.h/cpp         # 编辑器数据管理
├── editor_log.h/cpp          # 编辑器日志
├── editor_main_screen.h/cpp  # 编辑器主屏幕
├── editor_undo_redo_manager  # 撤销/重做管理
├── register_editor_types.cpp # 编辑器类型注册
└── ...
```

编辑器通过 `EditorNode` 类整合所有编辑功能，`EditorInterface` 提供脚本访问接口，`EditorPlugin` 支持插件扩展。

---

## 8. 引擎初始化与主循环

### 8.1 初始化序列

引擎入口位于 `main/main.cpp`，初始化按以下顺序进行：

```
1. OS 初始化（平台抽象层）
2. Core 初始化
   ├── 注册核心类型（register_core_types）
   │   ├── Object, RefCounted, Resource
   │   ├── Variant 系统
   │   ├── ClassDB 类型系统
   │   └── IO, String, Math 等子系统
   └── 初始化核心单例
3. Drivers 初始化（register_driver_types）
   ├── 音频驱动
   ├── 显示驱动
   └── 输入驱动
4. Servers 初始化（register_server_types）
   ├── RenderingServer
   ├── PhysicsServer2D / PhysicsServer3D
   ├── AudioServer
   ├── NavigationServer2D / NavigationServer3D
   ├── DisplayServer
   └── TextServer, XRServer, CameraServer
5. Modules 初始化（register_module_types）
   ├── 各模块按 config.py 配置选择性编译
   └── 每个模块通过 register_types.cpp 注册
6. Scene 初始化（register_scene_types）
   ├── Node, SceneTree
   ├── 2D/3D 节点
   ├── GUI 控件
   ├── 动画系统
   └── 场景资源
7. Editor 初始化（register_editor_types，仅编辑器模式）
8. 创建 MainLoop / SceneTree
9. 加载主场景或启动编辑器
```

### 8.2 主循环生命周期

```
MainLoop (抽象基类)
└── SceneTree (具体实现)
    ├── initialize()         # 初始化场景
    ├── physics_process()    # 物理帧回调（固定步长）
    ├── process()            # 逻辑帧回调（可变步长）
    └── finalize()           # 清理退出
```

每帧执行流程：

```
┌─────────────────────────────────────────┐
│              Frame Start                │
├─────────────────────────────────────────┤
│  1. 处理输入事件（Input）                │
│  2. 物理步进（physics_process）          │
│     ├── Node._physics_process()         │
│     └── PhysicsServer.step()            │
│  3. 逻辑更新（process）                  │
│     └── Node._process()                 │
│  4. 场景树同步                           │
│  5. 渲染（RenderingServer）              │
│  6. 音频更新（AudioServer）              │
│  7. 显示刷新（DisplayServer）            │
└─────────────────────────────────────────┘
```

---

## 9. 检索与查询机制

引擎中存在多种检索/查询机制，覆盖类型查找、资源加载、节点搜索等场景。

### 9.1 类检索（ClassDB）

**流程**：类名 → ClassDB 查询 → 类信息/实例

```
ClassDB::get_class_list()              # 获取所有已注册类
ClassDB::get_inheriters_from_class()   # 获取某类的所有子类
ClassDB::get_parent_class()            # 获取父类
ClassDB::can_instantiate(class_name)   # 检查是否可实例化
ClassDB::instantiate(class_name)       # 创建实例
ClassDB::get_method_list()             # 获取方法列表
ClassDB::get_method(class, method)     # 获取方法绑定（MethodBind*）
ClassDB::get_property_list()           # 获取属性列表
```

**内部数据结构**：
- 全局哈希表 `classes` 存储所有 `ClassInfo` 结构
- 每个 `ClassInfo` 包含：方法表、属性表、信号表、常量表、父类指针
- 方法查找沿继承链向上递归

### 9.2 资源检索与加载（ResourceLoader）

**流程**：资源路径 → ResourceLoader → 格式匹配 → 加载器 → Resource 实例

```
ResourceLoader::load(path)
    │
    ├── 1. 检查缓存（resource_cache）
    │       └── 命中 → 返回缓存的 Resource
    │
    ├── 2. 遍历已注册的 ResourceFormatLoader
    │       ├── recognize_path() → 按扩展名匹配
    │       └── exists() → 确认文件存在
    │
    ├── 3. 调用匹配的 loader.load()
    │       ├── ResourceFormatLoaderText (.tscn, .tres)
    │       ├── ResourceFormatLoaderBinary (.scn, .res)
    │       └── 各模块自定义加载器（.png, .ogg, .glb...）
    │
    ├── 4. 依赖递归加载
    │       └── 资源内部引用的其他资源
    │
    └── 5. 写入缓存并返回
```

**关键类**：
- `ResourceLoader`：全局加载管理器
- `ResourceFormatLoader`：格式加载器接口（策略模式）
- `Resource`：资源基类，包含 `path` 和 `resource_uid`
- `ResourceUID`：资源唯一标识系统

### 9.3 场景节点检索（SceneTree / Node）

**流程**：节点路径/条件 → Node 方法 → 目标节点

```
# 路径检索
Node::get_node(path)               # 按路径获取节点（"Parent/Child"）
Node::get_node_or_null(path)       # 安全版本
Node::find_child(pattern)          # 按模式查找子节点
Node::find_children(pattern)       # 查找所有匹配子节点

# 组检索
SceneTree::get_nodes_in_group()    # 获取组内所有节点
SceneTree::get_first_node_in_group() # 获取组内第一个节点
Node::get_tree()->get_nodes_in_group("enemies")

# 类型检索
Node::find_parent(type)            # 向上查找指定类型父节点
Object::is_class("RigidBody3D")    # 类型检查
```

**内部数据结构**：
- 场景树：父子关系的树结构，每个 Node 维护 `children` 向量
- 组系统：`SceneTree` 内部维护 `group_map`（HashMap<StringName, Group>）

### 9.4 方法与属性查找

**方法查找流程**：

```
Object::call(method_name, args)
    │
    ├── 1. 查找 MethodBind
    │       ClassDB::get_method(class_name, method_name)
    │       └── 沿继承链向上递归查找
    │
    ├── 2. 脚本方法
    │       ScriptInstance::call()
    │
    └── 3. Variant 调用
            Variant::call()
```

**属性查找流程**：

```
Object::get(property_name)
    │
    ├── 1. 核心属性（_get_property_list 注册的）
    ├── 2. 脚本属性
    └── 3. _get 回调（虚属性）
```

### 9.5 信号连接与分发

**信号连接流程**：

```
Object::connect(signal, callable)
    │
    ├── 1. 获取/创建 SignalData
    │       signal_map[signal_name]
    │
    └── 2. 添加 Connection 到 SignalData
            connections.push_back({callable, flags})
```

**信号发射流程**：

```
Object::emit_signal(signal_name, args)
    │
    ├── 1. 查找 signal_map
    │
    ├── 2. 遍历所有 Connection
    │       └── callable.call(args)
    │
    └── 3. 支持延迟调用（CONNECT_DEFERRED）
            └── MessageQueue::push_call()
```

### 9.6 Variant 动态调度

`Variant` 是引擎动态类型系统的核心，支持运行时类型推断和多态调用：

- **类型识别**：`get_type()` 返回 `Variant::Type` 枚举
- **动态调用**：`Variant::call()`、`Variant::get()`、`Variant::set()`
- **运算符调度**：`Variant::evaluate()` 根据类型分派运算
- **类型转换**：`Variant::construct()` 支持类型间转换

---

## 10. 构建系统

引擎使用 **SCons** 作为构建系统，入口文件为 `SConstruct`：

- `SConstruct`：主构建入口
- `methods.py`：构建辅助函数
- `SCsub`：每个子目录的构建配置
- `config.py`：模块配置（启用/禁用）
- `platform_methods.py`：平台相关构建逻辑

构建流程：`scons platform=<platform> target=<editor/template_debug/template_release>`

---

## 11. 架构设计模式总结

### 11.1 分层架构

```
┌──────────────────────────────────┐
│         Editor (编辑器)          │
├──────────────────────────────────┤
│         Scene (场景层)           │
│  Node, Control, Node2D, Node3D   │
├──────────────────────────────────┤
│        Servers (服务层)          │
│  Rendering, Physics, Audio, Nav  │
├──────────────────────────────────┤
│         Core (核心层)            │
│  Object, Variant, ClassDB, IO    │
├──────────────────────────────────┤
│      Drivers (驱动层)            │
│  GPU, Audio, Input drivers       │
├──────────────────────────────────┤
│       Platform (平台层)          │
│  OS abstraction, windowing       │
└──────────────────────────────────┘
```

### 11.2 关键设计模式

| 模式 | 应用场景 |
|------|----------|
| **单例模式** | Engine, ResourceLoader, ResourceSaver, Input, ClassDB |
| **策略模式** | ResourceFormatLoader（不同资源格式的加载策略） |
| **观察者模式** | Signal 系统（信号连接与分发） |
| **工厂模式** | ClassDB::instantiate()（按类名创建对象） |
| **接口-实现分离** | Server 接口 vs 具体实现（PhysicsServer3D vs GodotPhysics3D） |
| **注册表模式** | register_*_types() 系列函数统一注册类型 |
| **组合模式** | Node 树结构（场景节点树） |
| **命令模式** | MessageQueue（延迟调用） |
| **装饰器模式** | ServerWrapMT（线程安全包装） |
| **模块化** | Modules 系统（可选编译、统一注册接口） |

### 11.3 类型注册机制

所有引擎类型通过统一的注册机制在启动时注册：

```cpp
// 注册宏
GDREGISTER_CLASS(ClassName)           // 注册普通类
GDREGISTER_ABSTRACT_CLASS(ClassName)  // 注册抽象类
GDREGISTER_VIRTUAL_CLASS(ClassName)   // 注册虚类

// 注册入口
register_core_types()     // 核心类型
register_server_types()   // 服务器类型
register_scene_types()    // 场景类型
register_driver_types()   // 驱动类型
register_module_types()   // 模块类型
register_editor_types()   // 编辑器类型
```

---

## 12. 数据流总览

```
用户输入
  │
  ▼
DisplayServer (事件捕获)
  │
  ▼
Input (输入抽象)
  │
  ▼
SceneTree._input() → Node._input() / _unhandled_input()
  │
  ▼
Node._process() / _physics_process()
  │
  ├──→ PhysicsServer (物理模拟)
  ├──→ ScriptInstance (脚本逻辑)
  └──→ RenderingServer (渲染提交)
        │
        ▼
      GPU 渲染
        │
        ▼
      DisplayServer (画面输出)
```
