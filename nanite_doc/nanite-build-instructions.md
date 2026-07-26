# Nanite 构建说明

> 基于 Godot 4.7.1 | Stage 1 GDExtension 桥接 (NANITE_BRIDGE_GDEXT)

本文档给出在 Windows 上启用 `NANITE_BRIDGE_GDEXT` 编译宏、生成 Godot 编辑器
+ VS 解决方案的完整命令，并解释每个参数的作用。

---

## 目录

1. [快速开始](#1-快速开始)
2. [参数详解](#2-参数详解)
3. [生成 VS 解决方案](#3-生成-vs-解决方案)
4. [常用变体](#4-常用变体)
5. [故障排查](#5-故障排查)

---

## 1. 快速开始

在仓库根目录 `d:\Code\04_Engine\godot` 下执行：

```powershell
scons platform=windows target=editor nanite_bridge=gdext accesskit=no angle=no -j8
```

完成后会在 `bin\` 目录生成：

- `godot.windows.editor.x86_64.exe` — 编辑器（带控制台）
- `godot.windows.editor.x86_64.console.exe` — 控制台版本

首次完整编译大约需要数十分钟；增量编译只需几秒到几十秒。

---

## 2. 参数详解

| 参数 | 取值 | 作用 |
|------|------|------|
| `platform` | `windows` | 目标平台（Windows） |
| `target` | `editor` | 编译目标：`editor`（带工具链）/ `template_debug`（带调试符号的运行时）/ `template_release`（发布运行时） |
| `nanite_bridge` | `gdext` | **关键开关**：启用 GDExtension 桥接，定义 `NANITE_BRIDGE_GDEXT` 宏，编译 `nanite/bridge/` 下的 `NaniteGDExtBridge`（`CompositorEffect` 子类） |
| `accesskit` | `no` | 关闭 AccessKit（屏幕阅读器支持）依赖，避免 MSBuild 编译时找不到相关库 |
| `angle` | `no` | 关闭 ANGLE 渲染驱动依赖，避免 MSBuild 编译时找不到相关库 |
| `-j8` | 数字 | 并行编译线程数，建议设置为 CPU 物理核心数 |

### `nanite_bridge` 合法值

| 值 | 编译的桥接 | 定义的宏 | Stage |
|----|-----------|---------|-------|
| `gdext` | `nanite/bridge/` | `NANITE_BRIDGE_GDEXT` | Stage 1（已实现） |
| `module` | `modules/nanite_bridge_module/` | `NANITE_BRIDGE_MODULE` | Stage 2（未实现） |
| `deep` | `nanite_bridge_deep/` | `NANITE_BRIDGE_DEEP` | Stage 3（未实现） |
| `all` | 上述全部 | 上述全部 | 调试用 |

> 默认值为 `gdext`（[SConstruct:472](file:///d:/Code/04_Engine/godot/SConstruct#L472)），所以理论上可省略
> `nanite_bridge=gdext`，但显式写出有助于消除歧义。

---

## 3. 生成 VS 解决方案

加入 `vsproj=yes` 即可在编译同时生成 `godot.sln` 及对应 `.vcxproj`：

```powershell
scons platform=windows target=editor nanite_bridge=gdext accesskit=no angle=no vsproj=yes -j8
```

生成的 `godot.sln` 在仓库根目录，可用 Visual Studio 2022 (14.3) 打开。

### IntelliSense 注意事项

nanite 模块内部 include 全部使用带 `nanite/` 前缀的绝对路径（如
`#include "nanite/core/nanite_server.h"`），从仓库根目录解析，**不依赖**
per-module `CPPPATH`。这样 VS IntelliSense 能正确解析所有 nanite 内部头文件，
不会因引擎自身有顶层 `core/` 目录而被遮蔽。

如果发现 sln 里某些 nanite 头文件仍然标红，请：
1. 确认使用的是最新生成的 `godot.sln`（看时间戳）
2. 在 VS 里执行 `Project → Rescan Solution`
3. 删除 `godot.sln`、`.vs/`、`*.vcxproj*` 后重新运行上面命令

---

## 4. 常用变体

### 4.1 仅生成 sln 不编译（快速验证配置）

```powershell
scons platform=windows target=editor nanite_bridge=gdext accesskit=no angle=no vsproj=yes -j1 --config=none
```

### 4.2 调试构建（带断言和运行时检查）

```powershell
scons platform=windows target=editor nanite_bridge=gdext accesskit=no angle=no dev_build=yes -j8
```

### 4.3 仅编译 nanite 静态库（增量开发时更快）

```powershell
scons platform=windows target=editor nanite_bridge=gdext accesskit=no angle=no -j8 bin\obj\nanite\nanite.windows.editor.x86_64.lib
```

### 4.4 清理 nanite 编译产物后重新编译

```powershell
Remove-Item -Recurse -Force bin\obj\nanite
scons platform=windows target=editor nanite_bridge=gdext accesskit=no angle=no -j8
```

### 4.5 运行 doctest 单元测试

```powershell
.\bin\godot.windows.editor.x86_64.console.exe --test --test-suite="*Nanite*"
```

---

## 5. 故障排查

### 5.1 `error C1083: 无法打开包含文件 "nanite/core/xxx.h"`

**原因**：从仓库根目录之外运行 scons，或 `nanite_bridge` 未设置。

**解决**：确认在 `d:\Code\04_Engine\godot` 下运行，并显式传 `nanite_bridge=gdext`。

### 5.2 `fatal error C1083: 无法打开包含文件 "core/config/project_settings.h"`

**原因**：缺少引擎内部 include 路径。

**解决**：确认 `nanite/SCsub` 中有 `env_nanite.Append(CPPDEFINES=["GODOT_MODULE"])`，
这会让 nanite 代码能像引擎模块一样访问内部头文件。

### 5.3 链接错误 `LNK2019: 无法解析的外部符号 initialize_nanite_module`

**原因**：`nanite_bridge` 未启用，导致 `register_types.cpp` 未被编译进静态库。

**解决**：显式传 `nanite_bridge=gdext`。

### 5.4 MSBuild 报 accesskit / angle 相关错误

**原因**：Stage 1 编译时这两个依赖尚未准备好。

**解决**：始终加 `accesskit=no angle=no`（参见
[project_memory](file:///c:/Users/alonecat06/.trae-cn/memory/projects/-d-Code-04-Engine-godot/project_memory.md)
中的 "MSBuild 编译约束"）。

### 5.5 GLSL shader 头文件缺失

**原因**：`nanite/shaders/*.glsl.gen.h` 由 SCons 的 `GLSL_HEADER` builder
自动生成，首次编译时需要先跑完 SConscript 配置阶段。

**解决**：先完整跑一次 `scons ... -j8` 让 builder 生成头文件，再尝试
增量编译。如果 `.glsl.gen.h` 已存在但被改动过导致 diff，可以手动删除
`nanite/shaders/*.glsl.gen.h` 让 scons 重新生成。

---

## 6. 相关文件参考

- [SConstruct](file:///d:/Code/04_Engine/godot/SConstruct) — 顶层构建脚本，第 472 行解析 `nanite_bridge`
- [nanite/SCsub](file:///d:/Code/04_Engine/godot/nanite/SCsub) — nanite 模块构建脚本
- [nanite/config.py](file:///d:/Code/04_Engine/godot/nanite/config.py) — 模块元数据（`is_enabled()` 返回 `True` 才会参与编译）
- [nanite-overall-design.md](file:///d:/Code/04_Engine/godot/nanite_doc/nanite-overall-design.md) — 总体设计文档
- [nanite-implementation-tasks.md](file:///d:/Code/04_Engine/godot/nanite_doc/nanite-implementation-tasks.md) — 五阶段任务规划
- [spec/stage1-gdext-bridge/spec.md](file:///d:/Code/04_Engine/godot/nanite_doc/spec/stage1-gdext-bridge/spec.md) — Stage 1 详细 spec
