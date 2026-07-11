# Godot Engine 渲染架构深度分析

## 1. 渲染系统总览

Godot 的渲染系统采用**分层抽象**架构，从上到下分为：

```
┌─────────────────────────────────────────────────┐
│  Scene 层 (Node3D, MeshInstance3D, Light3D...)  │  用户可见的场景节点
├─────────────────────────────────────────────────┤
│  RenderingServer (统一 API 接口)                 │  RID 资源操作入口
├─────────────────────────────────────────────────┤
│  RenderingServerDefault (调度层)                 │  RID 分发与线程管理
├─────────────────────────────────────────────────┤
│  RendererCompositorRD (渲染器组合器)             │  协调场景/画布渲染
├─────────────────────────────────────────────────┤
│  RendererSceneRenderRD / RendererCanvasRenderRD │  3D/2D 渲染器
├─────────────────────────────────────────────────┤
│  ForwardClustered / ForwardMobile (渲染管线)     │  具体渲染算法
├─────────────────────────────────────────────────┤
│  Storage (Texture/Material/Mesh/Light)          │  GPU 资源存储
├─────────────────────────────────────────────────┤
│  RenderingDevice (GPU API 抽象)                  │  Vulkan/Metal/D3D12
├─────────────────────────────────────────────────┤
│  RenderingDeviceDriver (驱动层)                  │  平台特定实现
├─────────────────────────────────────────────────┤
│  RenderingContextDriver (上下文层)               │  窗口/设备管理
└─────────────────────────────────────────────────┘
```

---

## 2. 目录结构

```
servers/rendering/
├── rendering_server.h / .cpp              # 渲染服务器公共接口
├── rendering_server_default.h / .cpp      # 默认实现（调度层）
├── rendering_server_globals.h             # 全局渲染单例指针
├── rendering_method.h                     # 渲染方法抽象接口
├── rendering_server_constants.h           # 渲染常量定义
│
├── renderer_compositor.h / .cpp           # 组合器抽象接口
├── renderer_scene_render.h / .cpp         # 场景渲染抽象接口
├── renderer_canvas_render.h / .cpp        # 画布渲染抽象接口
├── renderer_viewport.h / .cpp            # 视口管理
├── renderer_scene_cull.h / .cpp          # 场景裁剪
├── renderer_scene_occlusion_cull.h / .cpp # 遮挡剔除
├── renderer_canvas_cull.h / .cpp         # 2D 裁剪
├── renderer_geometry_instance.h / .cpp    # 几何实例
│
├── rendering_device.h / .cpp             # GPU API 抽象层
├── rendering_device_driver.h / .cpp      # 驱动层接口
├── rendering_device_binds.h / .cpp       # RD 辅助绑定
├── rendering_device_commons.h / .cpp     # RD 公共定义
├── rendering_device_graph.h / .cpp       # RD 命令图
├── rendering_context_driver.h / .cpp     # 上下文驱动
│
├── shader_compiler.h / .cpp              # 着色器编译器
├── shader_language.h / .cpp              # 着色器语言解析
├── shader_preprocessor.h / .cpp          # 着色器预处理器
├── shader_types.h / .cpp                 # 着色器类型注册
├── shader_include_db.h / .cpp            # 着色器包含管理
├── shader_warnings.h / .cpp              # 着色器警告
│
├── instance_uniforms.h / .cpp            # 实例 Uniform 管理
├── multi_uma_buffer.h                    # 多 UMA 缓冲区
│
├── storage/                              # 存储接口层（抽象）
│   ├── texture_storage.h
│   ├── material_storage.h
│   ├── mesh_storage.h
│   ├── light_storage.h
│   ├── render_data.h / .cpp
│   ├── render_scene_buffers.h / .cpp
│   ├── render_scene_data.h / .cpp
│   ├── camera_attributes_storage.h / .cpp
│   ├── environment_storage.h / .cpp
│   ├── compositor_storage.h / .cpp
│   ├── utilities.h / .cpp
│   └── variant_converters.h
│
├── renderer_rd/                          # RD 渲染器实现
│   ├── renderer_compositor_rd.h / .cpp   # 组合器实现
│   ├── renderer_scene_render_rd.h / .cpp # 3D 场景渲染器
│   ├── renderer_canvas_render_rd.h / .cpp# 2D 画布渲染器
│   ├── shader_rd.h / .cpp               # RD 着色器封装
│   ├── pipeline_cache_rd.h / .cpp       # 管线缓存
│   ├── framebuffer_cache_rd.h / .cpp    # 帧缓冲缓存
│   ├── uniform_set_cache_rd.h / .cpp    # Uniform 集缓存
│   ├── cluster_builder_rd.h / .cpp      # 聚类构建器
│   ├── pipeline_hash_map_rd.h           # 管线哈希映射
│   ├── pipeline_deferred_rd.h           # 延迟管线定义
│   │
│   ├── forward_clustered/               # 前向聚类渲染管线
│   │   ├── render_forward_clustered.h / .cpp
│   │   └── scene_shader_forward_clustered.h / .cpp
│   │
│   ├── forward_mobile/                  # 移动端前向渲染管线
│   │   ├── render_forward_mobile.h / .cpp
│   │   └── scene_shader_forward_mobile.h / .cpp
│   │
│   ├── storage_rd/                      # RD 存储实现
│   │   ├── texture_storage.h / .cpp
│   │   ├── material_storage.h / .cpp
│   │   ├── mesh_storage.h / .cpp
│   │   ├── light_storage.h / .cpp
│   │   ├── particles_storage.h / .cpp
│   │   ├── render_buffer_custom_data_rd.h
│   │   ├── render_data_rd.h
│   │   ├── render_scene_buffers_rd.h / .cpp
│   │   ├── render_scene_data_rd.h / .cpp
│   │   ├── forward_id_storage.h
│   │   └── utilities.h / .cpp
│   │
│   ├── effects/                         # 后处理效果
│   │   ├── bokeh_dof.h / .cpp           # 景深
│   │   ├── ss_effects.h / .cpp          # 屏幕空间效果 (SSAO/SSIL)
│   │   ├── fsr.h / .cpp                 # FSR 1.0 超分
│   │   ├── fsr2.h / .cpp                # FSR 2.0 超分
│   │   ├── smaa.h / .cpp                # SMAA 抗锯齿
│   │   ├── taa.h / .cpp                 # TAA 时间抗锯齿
│   │   ├── tone_mapper.h / .cpp         # 色调映射
│   │   ├── luminance.h / .cpp           # 自动曝光
│   │   ├── vrs.h / .cpp                 # 可变速率着色
│   │   ├── motion_vectors_store.h / .cpp# 运动矢量
│   │   ├── copy_effects.h / .cpp        # 拷贝效果
│   │   ├── resolve.h / .cpp             # MSAA 解析
│   │   ├── roughness_limiter.h / .cpp   # 粗糙度限制
│   │   ├── debug_effects.h / .cpp       # 调试效果
│   │   └── sort_effects.h / .cpp        # 排序效果
│   │
│   ├── environment/                     # 环境系统
│   │   ├── sky.h / .cpp                 # 天空渲染
│   │   ├── fog.h / .cpp                 # 体积雾
│   │   └── gi.h / .cpp                  # 全局光照 (SDFGI/VoxelGI)
│   │
│   └── shaders/                         # GLSL 着色器源码
│       ├── canvas.glsl                  # 2D 画布着色器
│       ├── scene_forward_clustered.glsl # 前向聚类场景着色器
│       ├── scene_forward_mobile.glsl    # 移动端场景着色器
│       ├── effects/                     # 后处理着色器
│       ├── environment/                 # 环境着色器
│       └── ...
│
└── dummy/                               # 空渲染器（无头模式）
    ├── rasterizer_dummy.h
    ├── rasterizer_canvas_dummy.h
    ├── rasterizer_scene_dummy.h
    └── storage/                         # 空存储实现
```

---

## 3. RID 资源标识系统

### 3.1 RID 核心机制

RID（Resource ID）是渲染系统中所有资源的**不透明句柄**，定义在 `core/templates/rid.h`：

```cpp
class RID {
    uint64_t id = 0;
public:
    bool is_valid() const;
    bool is_null() const;
    uint32_t get_local_index() const;
    bool operator==(const RID &p_rid) const;
    // ...
};
```

### 3.2 RID_Owner — 资源所有权管理

`RID_Owner<T>`（`core/templates/rid_owner.h`）是 RID 系统的核心，负责：

- **资源注册**：`make_rid(T*)` → 创建 RID 并关联资源
- **资源查找**：`get_or_null(RID)` → 从 RID 检索资源指针
- **资源释放**：`free(RID)` → 释放 RID 和资源
- **线程安全**：可选的互斥锁保护
- **类型校验**：确保 RID 指向正确类型的资源

```
RID_Owner<Texture>
    ├── 内部: HashMap<uint32_t, Texture*> + 自由列表
    ├── make_rid(texture_ptr) → RID
    ├── get_or_null(rid) → Texture*
    └── free(rid) → 释放资源
```

### 3.3 RID 在渲染系统中的流转

```
场景层调用                          渲染服务器内部
──────────                          ────────────
RS::texture_2d_create(image)
    │
    ▼
RenderingServerDefault
    │ → RSG::texture_storage->texture_2d_create(image)
    │       │
    │       ▼
    │   TextureStorage (内部 RID_Owner<Texture>)
    │       ├── new Texture → RID_Owner.make_rid() → RID
    │       └── 返回 RID 给调用者
    │
    ▼
返回 RID（不透明句柄）

后续操作：
RS::texture_2d_update(rid, image)
    │ → RSG::texture_storage->texture_2d_update(rid, image)
    │       │
    │       ▼
    │   TextureStorage
    │       └── RID_Owner.get_or_null(rid) → Texture* → 更新GPU资源
```

---

## 4. RenderingServer 接口层

### 4.1 API 分类

`RenderingServer`（`rendering_server.h`）提供以下资源类别的操作：

| API 类别 | 主要方法 | 对应存储 |
|----------|----------|----------|
| **纹理** | `texture_2d_create`, `texture_2d_update`, `texture_2d_get` | TextureStorage |
| **着色器** | `shader_create`, `shader_set_code`, `shader_get_param_list` | MaterialStorage |
| **材质** | `material_create`, `material_set_shader`, `material_set_param` | MaterialStorage |
| **网格** | `mesh_create`, `mesh_add_surface`, `mesh_surface_set_material` | MeshStorage |
| **多网格** | `multimesh_create`, `multimesh_set_mesh`, `multimesh_instance_set_transform` | MeshStorage |
| **骨架** | `skeleton_create`, `skeleton_bone_set_transform` | MeshStorage |
| **光源** | `directional_light_create`, `omni_light_create`, `spot_light_create` | LightStorage |
| **反射探针** | `reflection_probe_create`, `reflection_probe_set_size` | LightStorage |
| **贴花** | `decal_create`, `decal_set_texture` | TextureStorage |
| **体素GI** | `voxel_gi_create`, `voxel_gi_set_data` | RendererSceneRenderRD |
| **光照贴图** | `lightmap_create`, `lightmap_set_textures` | RendererSceneRenderRD |
| **粒子** | `particles_create`, `particles_set_emitting` | ParticlesStorage |
| **粒子碰撞** | `particles_collision_create` | ParticlesStorage |
| **雾体积** | `fog_volume_create`, `fog_volume_set_shape` | RendererSceneRenderRD |
| **摄像机** | `camera_create`, `camera_set_perspective` | RenderingMethod |
| **视口** | `viewport_create`, `viewport_set_size`, `viewport_attach_camera` | RendererViewport |
| **天空** | `sky_create`, `sky_set_radiance_size` | Environment |
| **环境** | `environment_create`, `environment_set_bg` | EnvironmentStorage |
| **实例** | `instance_create`, `instance_set_base`, `instance_set_transform` | RendererSceneCull |
| **画布** | `canvas_create`, `canvas_item_add_*` | RendererCanvasCull |
| **画布灯光** | `canvas_light_create`, `canvas_light_set_color` | RendererCanvasCull |
| **全局Uniform** | `global_shader_uniforms_add`, `global_shader_uniforms_set` | MaterialStorage |

### 4.2 RenderingServerDefault — 调度层

`RenderingServerDefault`（`rendering_server_default.h/.cpp`）是 `RenderingServer` 的默认实现，职责：

1. **RID 分发**：将 API 调用路由到对应的存储/渲染子系统
2. **线程管理**：支持多线程渲染（通过命令队列）
3. **帧控制**：`draw()`, `sync()`, `tick()`

```cpp
// 分发模式示例
RID RenderingServerDefault::texture_2d_create(const Ref<Image> &p_image) {
    return RSG::texture_storage->texture_2d_create(p_image);
    //     ^^^^^^^^^^^^^^^^^^^^^^^^
    //     全局单例指针，指向具体存储实现
}

void RenderingServerDefault::material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) {
    RSG::material_storage->material_set_param(p_material, p_param, p_value);
}
```

### 4.3 全局渲染单例

`rendering_server_globals.h` 定义全局访问点：

```cpp
namespace RSG {
    extern RendererCompositor *compositor;
    extern RendererCanvasRender *canvas_render;
    extern RendererSceneRender *scene_render;
    extern RendererStorage *storage;
    extern TextureStorage *texture_storage;
    extern MaterialStorage *material_storage;
    extern MeshStorage *mesh_storage;
    extern LightStorage *light_storage;
    extern Utilities *utilities;
}
```

这些指针在 `RenderingServerDefault` 初始化时设置，指向 `RendererCompositorRD` 创建的具体实现。

---

## 5. RenderingMethod — 渲染方法接口

`RenderingMethod`（`rendering_method.h`）定义了场景渲染的核心抽象接口：

```cpp
class RenderingMethod {
public:
    virtual RID camera_create() = 0;
    virtual void camera_set_perspective(RID, float, float, float) = 0;
    virtual RID occluder_create() = 0;
    virtual RID scenario_create() = 0;
    virtual RID instance_create() = 0;
    virtual void instance_set_base(RID, RID) = 0;
    virtual void instance_set_transform(RID, const Transform3D &) = 0;
    virtual void render_scene(const RenderData *) = 0;
    virtual void set_time(double p_time, double p_step) = 0;
    // ...
};
```

**实现链**：

```
RenderingMethod (接口)
└── RendererSceneCull (裁剪 + 实例管理)
    └── RendererSceneRenderRD (RD 场景渲染器)
        ├── RenderForwardClustered (前向聚类管线)
        └── RenderForwardMobile (移动端管线)
```

---

## 6. RendererCompositorRD — 渲染器组合器

`RendererCompositorRD`（`renderer_rd/renderer_compositor_rd.h`）是整个渲染系统的**编排中心**：

### 6.1 初始化流程

```
RendererCompositorRD::RendererCompositorRD()
    │
    ├── 创建缓存系统
    │   ├── UniformSetCacheRD (Uniform 集缓存)
    │   ├── FramebufferCacheRD (帧缓冲缓存)
    │   └── PipelineCacheRD (管线缓存)
    │
    ├── 创建存储子系统
    │   ├── TextureStorageRD
    │   ├── MaterialStorageRD
    │   ├── MeshStorageRD
    │   ├── LightStorageRD
    │   └── ParticlesStorageRD
    │
    ├── 创建渲染器
    │   ├── RendererCanvasRenderRD (2D 画布渲染器)
    │   └── RendererSceneRenderRD (3D 场景渲染器)
    │
    └── 设置全局单例 (RSG::*)
```

### 6.2 渲染循环

```
begin_frame()
    │
    ├── 更新时间 (set_time)
    ├── 更新全局 Uniform
    └── 各存储子系统 begin_frame()

render_viewport(viewport)
    │
    ├── 3D 渲染
    │   └── scene_render->render_scene(render_data)
    │       ├── 场景裁剪 (cull)
    │       ├── 渲染阴影贴图
    │       ├── 渲染 GI (SDFGI/VoxelGI)
    │       ├── 渲染天空
    │       ├── 渲染不透明物体
    │       ├── 渲染透明物体
    │       └── 后处理效果链
    │
    └── 2D 渲染
        └── canvas_render->render_canvas(canvas_item)
            ├── 渲染画布项目
            └── 渲染画布灯光

end_frame()
    │
    ├── 提交 GPU 命令
    ├── 释放临时资源
    └── 各缓存系统清理
```

---

## 7. 场景渲染管线

### 7.1 渲染管线选择

Godot 提供两种 3D 渲染管线：

| 管线 | 目录 | 特点 | 适用平台 |
|------|------|------|----------|
| **Forward Clustered** | `forward_clustered/` | 聚类光照、完整特性、高质量 | 桌面端 |
| **Forward Mobile** | `forward_mobile/` | 简化光照、低开销、兼容性好 | 移动端/Web |

### 7.2 Forward Clustered 管线

`RenderForwardClustered`（`forward_clustered/render_forward_clustered.h`）的核心流程：

```
render_scene(render_data)
    │
    ├── 1. 准备渲染数据
    │   ├── 构建 Cluster (3D 空间划分)
    │   │   └── ClusterBuilderRD: 将光源/反射探针/贴花分配到体素网格
    │   ├── 收集几何实例
    │   └── 排序渲染列表
    │
    ├── 2. 渲染阴影
    │   ├── 方向光阴影 (级联)
    │   ├── 点光源阴影 (omni)
    │   └── 聚光灯阴影 (spot)
    │
    ├── 3. 渲染 GI
    │   ├── SDFGI (有符号距离场全局光照)
    │   └── VoxelGI (体素全局光照)
    │
    ├── 4. 渲染天空
    │   └── SkyRD: 物理天空/全景天空
    │
    ├── 5. 不透明通道
    │   ├── 深度预通道 (depth pre-pass)
    │   ├── 不透明几何体渲染
    │   └── 使用 Cluster 数据进行光照计算
    │
    ├── 6. 透明通道
    │   └── 透明几何体渲染 (前向渲染 + 深度测试)
    │
    └── 7. 后处理链
        ├── 运动矢量存储
        ├── TAA 时间抗锯齿
        ├── 屏幕空间效果 (SSAO/SSIL)
        ├── 自动曝光 (Luminance)
        ├── 景深 (Bokeh DoF)
        ├── 体积雾
        ├── 色调映射 (Tone Mapper)
        ├── SMAA / FXAA 抗锯齿
        ├── FSR / FSR2 超分辨率
        ├── 可变速率着色 (VRS)
        └── 拷贝到输出
```

### 7.3 Forward Mobile 管线

`RenderForwardMobile`（`forward_mobile/render_forward_mobile.h`）的简化流程：

```
render_scene(render_data)
    │
    ├── 1. 准备渲染数据 (简化版)
    │   └── 无 Cluster，使用最大光源数限制
    │
    ├── 2. 渲染阴影 (简化)
    │
    ├── 3. 不透明通道
    │   └── 逐物体前向渲染，光源数受限
    │
    ├── 4. 透明通道
    │
    └── 5. 简化后处理
        ├── 色调映射
        └── 拷贝到输出
```

**关键差异**：
- Forward Clustered 使用 **Cluster** 技术，将屏幕空间划分为 3D 体素网格，每个体素记录影响它的光源列表，支持大量动态光源
- Forward Mobile 使用**传统前向渲染**，每个物体最多受固定数量光源影响，性能开销更低

---

## 8. 存储系统架构

### 8.1 存储接口层

`storage/` 目录定义了存储的**抽象接口**，`renderer_rd/storage_rd/` 提供 RD 实现：

```
RendererStorage (抽象基类)
├── TextureStorage          # 纹理资源管理
├── MaterialStorage         # 材质/着色器管理
├── MeshStorage             # 网格/骨架/多网格管理
├── LightStorage            # 光源/反射探针管理
├── ParticlesStorage        # 粒子系统管理
├── CameraAttributesStorage # 相机属性管理
├── EnvironmentStorage      # 环境设置管理
├── CompositorStorage       # 合成效果管理
└── Utilities               # 工具函数
```

### 8.2 TextureStorage

```
TextureStorageRD
├── RID_Owner<Texture> textures           # 所有纹理资源
├── RID_Owner<CanvasTexture> canvas_textures  # 2D 画布纹理
├── RID_Owner<Texture2DInfo> texture_2d_info  # 2D 纹理信息
│
├── texture_2d_create(image) → RID
├── texture_2d_update(rid, image)
├── texture_2d_get(rid) → Image
├── texture_rd_create(rd_texture) → RID   # 从 RD 纹理创建
├── texture_proxy_create(base) → RID      # 代理纹理
│
└── GPU 资源管理
    ├── Texture RD_ID 映射
    ├── 纹理压缩/解压
    └── 纹理流式加载
```

### 8.3 MaterialStorage

```
MaterialStorageRD
├── RID_Owner<Shader> shaders             # 着色器资源
├── RID_Owner<Material> materials         # 材质资源
│
├── shader_create() → RID
├── shader_set_code(rid, code)            # 编译着色器
│   └── ShaderPreprocessor → ShaderCompiler → SPIR-V
├── material_create() → RID
├── material_set_shader(rid, shader_rid)
├── material_set_param(rid, name, value)
│
├── 全局 Uniform 管理
│   ├── global_shader_uniforms_add()
│   └── global_shader_uniforms_set()
│
└── Uniform 缓冲区管理
    ├── 材质 Uniform 集
    └── 实例 Uniform 集
```

### 8.4 MeshStorage

```
MeshStorageRD
├── RID_Owner<Mesh> meshes                # 网格资源
├── RID_Owner<MultiMesh> multimeshes      # 多网格资源
├── RID_Owner<Skeleton> skeletons         # 骨架资源
│
├── mesh_create() → RID
├── mesh_add_surface(rid, data)           # 添加网格表面
│   ├── 顶点数据上传到 GPU
│   ├── 索引缓冲区创建
│   └── BLAS 构建（光线追踪）
├── multimesh_create() → RID
├── multimesh_set_mesh(rid, mesh_rid)
├── skeleton_create() → RID
├── skeleton_bone_set_transform(rid, bone, transform)
│
└── GPU 缓冲区管理
    ├── 顶点缓冲区
    ├── 索引缓冲区
    └── SSBO (多网格/骨架数据)
```

### 8.5 LightStorage

```
LightStorageRD
├── RID_Owner<Light> lights               # 光源资源
├── RID_Owner<ReflectionProbe> reflection_probes
│
├── directional_light_create() → RID
├── omni_light_create() → RID
├── spot_light_create() → RID
├── light_set_color(rid, color)
├── light_set_param(rid, param, value)
├── light_set_shadow(rid, enabled)
│
└── 光源数据组织
    ├── 方向光列表
    ├── 空间索引 (BVH/八叉树)
    └── 阴影配置
```

---

## 9. 裁剪系统

### 9.1 场景裁剪流程

```
RendererSceneCull (场景裁剪管理器)
│
├── 实例管理
│   ├── RID_Owner<Instance> instances
│   ├── instance_create() → RID
│   ├── instance_set_base() → 关联网格/光源/粒子等
│   └── instance_set_transform() → 设置世界变换
│
├── 场景管理
│   ├── RID_Owner<Scenario> scenarios
│   ├── scenario_create() → RID
│   └── 场景内实例的空间组织
│
└── 裁剪流程
    ├── 1. 视锥裁剪 (Frustum Culling)
    │   └── 检测实例包围盒与视锥体的相交
    │
    ├── 2. 遮挡裁剪 (Occlusion Culling)
    │   └── RendererSceneOcclusionCull
    │       ├── BVH 遮挡器
    │       └── 软件光栅化遮挡检测
    │
    ├── 3. 可见性通知
    │   └── 实例进入/离开视锥时触发回调
    │
    └── 4. 生成渲染列表
        └── 按材质/深度排序的几何实例列表
```

### 9.2 画布裁剪

```
RendererCanvasCull (2D 画布裁剪)
│
├── RID_Owner<Canvas> canvases            # 画布
├── RID_Owner<CanvasItem> canvas_items    # 画布项目
├── RID_Owner<CanvasLight> canvas_lights  # 画布灯光
│
├── canvas_item_add_rect(rid, rect, color)
├── canvas_item_add_texture_rect(rid, rect, texture)
├── canvas_item_add_polygon(rid, points, colors)
│
└── 2D 裁剪
    ├── 屏幕空间包围盒检测
    ├── 画布层级裁剪
    └── 生成 2D 渲染指令
```

---

## 10. 渲染设备抽象层

### 10.1 三层驱动架构

```
RenderingContextDriver          # 上下文管理
├── 窗口表面创建
├── 设备枚举与选择
├── Vulkan: VulkanContextDriver
├── Metal: MetalContextDriver
└── D3D12: D3D12ContextDriver

RenderingDeviceDriver           # 设备操作
├── 纹理创建/销毁
├── 缓冲区创建/销毁
├── 着色器编译
├── 管线创建
├── 命令录制/提交
├── 同步原语 (Fence, Semaphore)
└── 平台特定实现

RenderingDevice                 # 高层抽象
├── 缓存管理 (纹理/管线/帧缓冲)
├── 资源生命周期
├── 命令图 (RenderingDeviceGraph)
├── 统一接口 (跨平台)
└── 调试工具
```

### 10.2 RenderingDevice 核心 API

```cpp
class RenderingDevice {
    // 纹理
    RID texture_create(const TextureFormat &);
    RID texture_create_shared(const TextureView &, RID);
    void texture_update(RID, layer, image_data);
    Vector<uint8_t> texture_get_data(RID, layer);

    // 帧缓冲
    RID framebuffer_create(attachments);
    RID framebuffer_create_multipass(attachments, passes);

    // 着色器
    RID shader_create_from_spirv(spirv_data);
    RID shader_create_uniform_set(shader, textures);

    // 管线
    RID render_pipeline_create(shader, framebuffer_format, vertex_format, ...);
    RID compute_pipeline_create(shader);

    // 绘制
    void draw_list_begin(framebuffer, draw_mode);
    void draw_list_bind_render_pipeline(draw_list, pipeline);
    void draw_list_bind_uniform_set(draw_list, uniform_set, set_index);
    void draw_list_draw(draw_list, indexed, instance_count);
    void draw_list_end();

    // 计算
    void compute_list_begin();
    void compute_list_bind_compute_pipeline(compute_list, pipeline);
    void compute_list_dispatch(compute_list, x, y, z);
    void compute_list_end();

    // 同步
    void submit();
    void sync();
};
```

---

## 11. 着色器编译管线

### 11.1 编译流程

```
用户着色器代码 (Godot Shader Language)
    │
    ▼
ShaderPreprocessor               # 预处理
├── #include 展开
├── 宏定义替换
├── 条件编译 (#if/#ifdef)
└── 生成预处理后的源码
    │
    ▼
ShaderLanguage                   # 解析
├── 词法分析 (Tokenizer)
├── 语法分析 (Parser) → AST
├── 语义检查
└── 生成中间表示
    │
    ▼
ShaderCompiler                   # 编译
├── 转换为 GLSL/Vulkan GLSL
├── 注入引擎内置 Uniform
├── 生成渲染管线特定代码
│   ├── Forward Clustered 变体
│   └── Forward Mobile 变体
└── 编译为 SPIR-V
    │
    ▼
RenderingDevice                  # 加载
├── shader_create_from_spirv()
└── 创建 GPU 着色器对象
```

### 11.2 ShaderRD — 着色器封装

`ShaderRD`（`renderer_rd/shader_rd.h`）封装了多版本着色器管理：

```cpp
class ShaderRD {
    // 同一着色器代码生成多个变体
    // - 3D 场景着色器 (vertex + fragment + compute)
    // - 2D 画布着色器
    // - 不同功能组合 (阴影/深度/颜色)
    void initialize(shader_code);
    RID version_get_shader(version, variant);
    // ...
};
```

---

## 12. GPU 资源缓存系统

### 12.1 三级缓存

```
PipelineCacheRD                  # 渲染管线缓存
├── HashMap<PipelineKey, RID> pipelines
├── 避免重复创建管线对象
└── Key = {shader, format, blend, topology, ...}

FramebufferCacheRD               # 帧缓冲缓存
├── HashMap<FbKey, RID> framebuffers
├── 避免重复创建帧缓冲
└── Key = {texture_ids, size, format}

UniformSetCacheRD                # Uniform 集缓存
├── HashMap<UniformKey, RID> uniform_sets
├── 避免重复创建描述符集
└── Key = {shader, set_index, textures/buffers}
```

### 12.2 缓存查找流程

```
渲染时需要 Pipeline:
    │
    ├── 1. 计算 PipelineKey (shader + vertex_format + blend + ...)
    ├── 2. 查找 PipelineCacheRD
    │       ├── 命中 → 返回缓存的 RID
    │       └── 未命中 → 创建新 Pipeline → 缓存 → 返回 RID
    │
    ├── 3. 绑定 Pipeline 到 DrawList
    └── 4. 绑定 UniformSet (同样通过缓存查找)
```

---

## 13. 后处理效果管线

### 13.1 效果执行顺序

```
3D 场景渲染完成
    │
    ▼
┌─────────────────────────────────────┐
│  1. 运动矢量存储 (MotionVectorsStore) │  为 TAA/FXR 提供运动信息
├─────────────────────────────────────┤
│  2. TAA 时间抗锯齿 (TAA)             │  累积多帧采样
├─────────────────────────────────────┤
│  3. 屏幕空间效果 (SSEffects)          │
│     ├── SSAO (屏幕空间环境光遮蔽)     │
│     └── SSIL (屏幕空间间接光照)       │
├─────────────────────────────────────┤
│  4. 自动曝光 (Luminance)             │  计算场景平均亮度
├─────────────────────────────────────┤
│  5. 景深 (BokehDoF)                 │  模拟镜头景深效果
├─────────────────────────────────────┤
│  6. 体积雾 (Fog)                    │  大气散射效果
├─────────────────────────────────────┤
│  7. 色调映射 (ToneMapper)           │  HDR → LDR 转换
├─────────────────────────────────────┤
│  8. SMAA 抗锯齿                     │  子像素形态学抗锯齿
├─────────────────────────────────────┤
│  9. FSR/FSR2 超分辨率               │  AMD 超分辨率技术
├─────────────────────────────────────┤
│  10. 可变速率着色 (VRS)              │  按区域降低着色率
├─────────────────────────────────────┤
│  11. 拷贝到输出 (CopyEffects)        │  最终输出到视口
└─────────────────────────────────────┘
```

### 13.2 RenderSceneBuffersRD — 渲染缓冲区管理

```
RenderSceneBuffersRD
├── 颜色缓冲区 (Color)
│   ├── 内部格式 (RGBA16F / RGBA32F)
│   ├── MSAA 版本
│   └── 多层 (立体渲染)
├── 深度缓冲区 (Depth)
│   ├── 深度格式 (D32F / D24S8)
│   └── MSAA 版本
├── 法线缓冲区 (Normal)
├── 运动矢量缓冲区 (Motion Vectors)
├── 粗糙度缓冲区 (Roughness)
├── 体积雾缓冲区 (Volumetric Fog)
├── SDFGI 缓冲区
└── 自定义数据缓冲区 (Custom Data)
```

---

## 14. 环境系统

### 14.1 天空渲染 (SkyRD)

```
SkyRD
├── 物理天空 (Physical Sky)
│   ├── 大气散射模型
│   ├── 太阳/月亮位置
│   └── 瑞利/米氏散射
├── 全景天空 (Panorama Sky)
│   └── HDR 环境贴图
└── 辐射度图 (Radiance Map)
    ├── 预过滤卷积
    ├── 多级 Mipmap
    └── 用于环境反射和 IBL
```

### 14.2 全局光照 (GIRD)

```
GI (全局光照系统)
├── SDFGI (有符号距离场全局光照)
│   ├── 场景体素化
│   ├── 有符号距离场构建
│   ├── 光照传播
│   ├── 多级级联
│   └── 实时间接光照
├── VoxelGI (体素全局光照)
│   ├── 烘焙体素数据
│   ├── 三线性插值
│   └── 间接光照计算
└── 光照贴图 (Lightmap)
    ├── 烘焙光照贴图
    ├── 动态物体 SH 探针
    └── 双光照贴图 (实时+烘焙)
```

### 14.3 体积雾 (FogRD)

```
FogRD
├── 体积雾密度计算
├── 光照散射
├── 阴影接收
├── 时间重投影 (TAA)
└── 与 SDFGI 集成
```

---

## 15. 完整渲染帧流程

```
┌──────────────────────────────────────────────────────────────┐
│                     RenderingServer::draw()                   │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  for each Viewport:                                          │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ 1. RendererSceneCull::render_scene()                   │  │
│  │    ├── 收集可见实例 (视锥/遮挡裁剪)                     │  │
│  │    ├── 排序渲染列表                                    │  │
│  │    └── 生成 RenderData                                │  │
│  ├────────────────────────────────────────────────────────┤  │
│  │ 2. RendererSceneRenderRD::render_scene()               │  │
│  │    ├── 准备渲染缓冲区 (RenderSceneBuffersRD)           │  │
│  │    ├── 渲染阴影贴图                                    │  │
│  │    │   ├── 方向光级联阴影                              │  │
│  │    │   └── 点光/聚光阴影                               │  │
│  │    ├── 渲染 GI                                        │  │
│  │    │   ├── SDFGI 更新与渲染                            │  │
│  │    │   └── VoxelGI 采样                                │  │
│  │    ├── 渲染天空                                        │  │
│  │    │   └── 辐射度图 + 背景天空                         │  │
│  │    ├── ForwardClustered/ForwardMobile 渲染              │  │
│  │    │   ├── 深度预通道                                   │  │
│  │    │   ├── 不透明物体渲染                               │  │
│  │    │   ├── 透明物体渲染                                 │  │
│  │    │   └── Cluster 光照计算                             │  │
│  │    └── 后处理链                                        │  │
│  │        ├── 运动矢量 → TAA                              │  │
│  │        ├── SSAO/SSIL                                   │  │
│  │        ├── 自动曝光                                     │  │
│  │        ├── 景深                                         │  │
│  │        ├── 体积雾                                       │  │
│  │        ├── 色调映射                                     │  │
│  │        ├── SMAA                                        │  │
│  │        └── FSR 超分                                     │  │
│  ├────────────────────────────────────────────────────────┤  │
│  │ 3. RendererCanvasRenderRD::render_canvas()             │  │
│  │    ├── 2D 画布项目渲染                                 │  │
│  │    ├── 画布灯光与阴影                                  │  │
│  │    └── 2D 后处理                                       │  │
│  └────────────────────────────────────────────────────────┘  │
│                                                              │
│  Compositor::end_frame()                                     │
│  ├── 提交 GPU 命令                                          │
│  └── 释放临时资源                                            │
└──────────────────────────────────────────────────────────────┘
```

---

## 16. 渲染资源检索流程总结

### 16.1 纹理检索

```
场景节点: Sprite2D.texture = load("res://icon.png")
    │
    ▼
ResourceLoader::load("res://icon.png")
    │ → Image 加载
    │
    ▼
RenderingServer::texture_2d_create(image)
    │ → TextureStorageRD::texture_2d_create()
    │     ├── new Texture → GPU 纹理创建
    │     └── RID_Owner.make_rid() → RID
    │
    ▼
返回 RID → Sprite2D 内部存储 RID
    │
    ▼
渲染时: CanvasRenderRD 通过 RID 查找纹理 → 绑定到着色器
```

### 16.2 材质检索

```
场景节点: MeshInstance3D.set_surface_override_material(0, mat)
    │
    ▼
RenderingServer::instance_set_surface_material(instance_rid, 0, material_rid)
    │ → RendererSceneCull 更新实例材质引用
    │
    ▼
渲染时:
    ├── MaterialStorageRD::material_get_uniform_set(material_rid, ...)
    │     ├── 查找材质的 Uniform 集
    │     └── 如果参数变更 → 重新创建 Uniform 集
    │
    └── 绑定到渲染管线
```

### 16.3 光源检索

```
场景节点: Light3D 加入场景树
    │
    ▼
RenderingServer::instance_create() → instance_rid
RenderingServer::instance_set_base(instance_rid, light_rid)
    │
    ▼
RendererSceneCull: 将实例加入 Scenario
    │
    ▼
裁剪时:
    ├── 视锥内光源 → 加入可见光源列表
    │
    ▼
渲染时:
    ├── LightStorageRD: 通过 RID 查找光源数据
    ├── ClusterBuilderRD: 将光源分配到 Cluster
    └── ForwardClustered: 从 Cluster 读取光照信息
```

### 16.4 网格检索

```
场景节点: MeshInstance3D 加入场景树
    │
    ▼
RenderingServer::instance_create() → instance_rid
RenderingServer::instance_set_base(instance_rid, mesh_rid)
    │
    ▼
RendererSceneCull: 将实例加入 Scenario
    │
    ▼
裁剪时:
    ├── 视锥裁剪: 检测包围盒与视锥相交
    ├── 遮挡裁剪: 检测是否被遮挡
    │
    ▼
渲染时:
    ├── MeshStorageRD: 通过 RID 查找网格顶点/索引数据
    ├── MaterialStorageRD: 通过材质 RID 查找着色器/Uniform
    ├── 创建 GeometryInstanceRD
    └── 加入渲染列表 → 按材质排序 → 绘制
```

---

## 17. 设计模式总结

| 模式 | 应用 |
|------|------|
| **外观模式** | RenderingServer 统一 API，隐藏内部复杂性 |
| **桥接模式** | RenderingMethod 接口分离抽象与实现 |
| **策略模式** | ForwardClustered vs ForwardMobile 可切换 |
| **组合器模式** | RendererCompositorRD 协调多个子系统 |
| **享元模式** | PipelineCacheRD/FramebufferCacheRD/UniformSetCacheRD 缓存共享 |
| **代理模式** | RID 作为资源的不透明代理/句柄 |
| **命令模式** | RenderingDeviceGraph 命令图延迟执行 |
| **观察者模式** | 可见性通知器 (VisibilityNotifier) |
| **空对象模式** | Dummy 渲染器 (无头模式) |
| **工厂方法** | 各 Storage 的 `*_create()` 方法 |
| **模板方法** | ShaderRD 多版本着色器生成 |
