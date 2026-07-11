# Godot Engine 3D 角色控制器深度分析

## 1. 概述

Godot 的 3D 角色控制器以 `CharacterBody3D` 为核心，提供 `move_and_slide()` 碰撞滑动逻辑，但**不包含**内置的第一人称/第三人称控制器模板。与 UE 的 `ACharacter` + `UCharacterMovementComponent` 全功能框架相比，Godot 采用**极简核心 + 用户拼装**的设计哲学，移动、动画、相机三者松耦合，需用户在 GDScript/C# 中手动串联。

## 2. 检索过程

1. `scene/3d/physics/character_body_3d.h` → CharacterBody3D 完整类定义，MotionMode 枚举，CollisionState 联合体
2. `scene/3d/physics/character_body_3d.cpp` → `move_and_slide()` / `_move_and_slide_grounded()` / `_move_and_slide_floating()` 完整实现
3. `scene/3d/physics/physics_body_3d.h` → PhysicsBody3D，`move_and_collide()` 底层接口
4. `scene/3d/physics/collision_object_3d.h` → CollisionObject3D，ShapeData 结构
5. `scene/3d/camera_3d.h` → Camera3D，投影类型、frustum、cull_mask
6. `scene/3d/physics/spring_arm_3d.h` → SpringArm3D，第三人称相机弹簧臂
7. `scene/animation/animation_tree.h` → AnimationTree + AnimationNode 体系
8. `scene/animation/animation_node_state_machine.h` → AnimationNodeStateMachine + Transition
9. 全项目搜索 "fps_controller" / "first_person" / "third_person" / "character_controller" → **无内置控制器模板**

## 3. 核心类层次

### 3.1 CharacterBody3D 继承体系

```mermaid
classDiagram
    class Node {
        +add_child(node)
        +get_parent()
        +_process(delta) virtual
        +_physics_process(delta) virtual
        +_input(event) virtual
    }

    class Node3D {
        +transform : Transform3D
        +position : Vector3
        +rotation : Vector3
        +scale : Vector3
        +set_global_transform(xform)
        +look_at(target, up)
        +rotate_y(angle)
    }

    class CollisionObject3D {
        +collision_layer : uint32_t
        +collision_mask : uint32_t
        +collision_priority : float
        +input_event signal
        +mouse_entered signal
        -rid : RID
        -shapes : RBMap~uint32_t, ShapeData~
    }

    class PhysicsBody3D {
        +move_and_collide(params, result) bool
        +test_move(from, motion) bool
        +get_gravity() Vector3
        +set_axis_lock(axis, lock)
        +add_collision_exception_with(node)
        -motion_cache : Ref~KinematicCollision3D~
    }

    class CharacterBody3D {
        +velocity : Vector3
        +up_direction : Vector3
        +motion_mode : MotionMode
        +floor_max_angle : float
        +floor_snap_length : float
        +max_slides : int
        +safe_margin : float
        +move_and_slide() bool
        +apply_floor_snap()
        +is_on_floor() bool
        +is_on_wall() bool
        +is_on_ceiling() bool
        +get_floor_normal() Vector3
        +get_wall_normal() Vector3
        +get_real_velocity() Vector3
        +get_platform_velocity() Vector3
        +get_last_motion() Vector3
        +get_slide_collision_count() int
        -collision_state : CollisionState
        -platform_rid : RID
        -platform_velocity : Vector3
        -floor_normal : Vector3
        -wall_normal : Vector3
        -ceiling_normal : Vector3
        -motion_results : Vector~MotionResult~
    }

    Node <|-- Node3D
    Node3D <|-- CollisionObject3D
    CollisionObject3D <|-- PhysicsBody3D
    PhysicsBody3D <|-- CharacterBody3D
```

### 3.2 MotionMode 枚举与 CollisionState

```mermaid
classDiagram
    class MotionMode {
        <<enumeration>>
        MOTION_MODE_GROUNDED
        MOTION_MODE_FLOATING
    }

    class PlatformOnLeave {
        <<enumeration>>
        PLATFORM_ON_LEAVE_ADD_VELOCITY
        PLATFORM_ON_LEAVE_ADD_UPWARD_VELOCITY
        PLATFORM_ON_LEAVE_DO_NOTHING
    }

    class CollisionState {
        <<bitfield union>>
        state : uint32_t
        floor : bool
        wall : bool
        ceiling : bool
    }

    CharacterBody3D --> MotionMode
    CharacterBody3D --> PlatformOnLeave
    CharacterBody3D --> CollisionState
```

### 3.3 Camera3D 体系

```mermaid
classDiagram
    class Node3D {
        +transform : Transform3D
    }

    class Camera3D {
        +projection : ProjectionType
        +fov : float
        +near : float
        +far : float
        +cull_mask : uint32_t
        +environment : Ref~Environment~
        +attributes : Ref~CameraAttributes~
        +compositor : Ref~Compositor~
        +make_current()
        +clear_current()
        +is_current() bool
        +project_ray_origin(pos) Vector3
        +project_ray_normal(pos) Vector3
        +project_position(point, depth) Vector3
        +unproject_position(pos) Point2
        +is_position_behind(pos) bool
        +get_frustum() Vector~Plane~
        +set_perspective(fov, near, far)
        +set_orthogonal(size, near, far)
    }

    class ProjectionType {
        <<enumeration>>
        PROJECTION_PERSPECTIVE
        PROJECTION_ORTHOGONAL
        PROJECTION_FRUSTUM
    }

    class SpringArm3D {
        +spring_length : float
        +current_spring_length : float
        +shape : Ref~Shape3D~
        +mask : uint32_t
        +margin : float
        +add_excluded_object(rid)
        +remove_excluded_object(rid)
        +get_hit_length() float
        -process_spring()
    }

    Node3D <|-- Camera3D
    Node3D <|-- SpringArm3D
    SpringArm3D --> Camera3D : parent of
```

### 3.4 动画系统集成

```mermaid
classDiagram
    class AnimationMixer {
        <<abstract>>
        +root_node : NodePath
        +callback_mode_process : ProcessMode
    }

    class AnimationTree {
        +root_animation_node : Ref~AnimationRootNode~
        +animation_player : NodePath
        +advance_expression_base_node : NodePath
        +set_animation_player(path)
        +set_root_animation_node(node)
        +is_state_invalid() bool
        -properties : AHashMap~StringName, Pair~
    }

    class AnimationNode {
        <<abstract Resource>>
        +inputs : LocalVector~Input~
        +filter : AHashMap~NodePath, bool~
        +filter_enabled : bool
        +set_parameter(name, value)
        +get_parameter(name) Variant
        +process(playback_info) NodeTimeInfo virtual
        +blend_animation(name, info)
        +blend_node(node, subpath, info) NodeTimeInfo
        +blend_input(input, info) NodeTimeInfo
    }

    class AnimationRootNode {
        <<abstract>>
    }

    class AnimationNodeStateMachine {
        +state_machine_type : StateMachineType
        +add_node(name, node)
        +remove_node(name)
        +add_transition(from, to, transition)
        +set_start_node(name)
        +get_start_node() StringName
    }

    class AnimationNodeStateMachineTransition {
        +switch_mode : SwitchMode
        +advance_mode : AdvanceMode
        +advance_condition : StringName
        +advance_expression : String
        +xfade_time : float
        +xfade_curve : Ref~Curve~
        +reset : bool
        +priority : int
    }

    class AnimationNodeBlendSpace2D {
        +blend_position : Vector2
        +min_space : Vector2
        +max_space : Vector2
        +add_blend_point(node, pos)
        +add_triangle(point1, point2, point3)
    }

    class AnimationNodeBlendTree {
        +add_node(name, node)
        +connect_node(name, input, source)
    }

    AnimationMixer <|-- AnimationTree
    AnimationNode <|-- AnimationRootNode
    AnimationRootNode <|-- AnimationNodeStateMachine
    AnimationRootNode <|-- AnimationNodeBlendTree
    AnimationNode <|-- AnimationNodeBlendSpace2D
    AnimationNodeStateMachine --> AnimationNodeStateMachineTransition : contains
    AnimationTree --> AnimationRootNode : holds
```

## 4. move_and_slide 详解

### 4.1 Grounded 模式流程

```mermaid
flowchart TD
    A["move_and_slide()"] --> B["读取 delta, 锁定轴"]
    B --> C["计算 platform_velocity"]
    C --> D{motion_mode?}
    D -->|GROUNDED| E["_move_and_slide_grounded"]
    D -->|FLOATING| F["_move_and_slide_floating"]

    E --> G["计算 motion = velocity * delta"]
    G --> H["初始化 sliding_enabled, can_apply_constant_speed"]
    H --> I["循环 iteration 0..max_slides"]
    I --> J["move_and_collide"]
    J --> K{碰撞?}
    K -->|否| L{floor_constant_speed + on_floor_if_snapped?}
    L -->|是| M["回退并应用恒速滑动"]
    L -->|否| N["跳出循环"]
    K -->|是| O["_set_collision_direction"]
    O --> P{碰撞类型?}

    P -->|floor + stop_on_slope| Q["回退, velocity=0, break"]
    P -->|ceiling + platform_push| R["应用 ceiling_velocity"]
    P -->|wall + floor_block_on_wall| S["取消前进运动, 仅保留侧向"]
    P -->|default sliding| T["motion = remainder.slide"]

    Q --> U["_snap_on_floor"]
    R --> U
    S --> V{还有剩余运动?}
    T --> V
    V -->|是| I
    V -->|否| U

    U --> W["real_velocity = position_delta / delta"]
    W --> X{离开平台?}
    X -->|是| Y["根据 platform_on_leave 添加速度"]
    X -->|否| Z[返回]
```

### 4.2 Floor Snap 机制

```mermaid
flowchart TD
    A["_snap_on_floor"] --> B{已在地面?}
    B -->|是| C[跳过]
    B -->|否| D{之前在地面?}
    D -->|否| C
    D -->|是| E{速度朝上?}
    E -->|是| C
    E -->|否| F["apply_floor_snap"]

    F --> G["向下投射: motion = -up * floor_snap_length"]
    G --> H["move_and_collide test_only"]
    H --> I{命中地面?}
    I -->|是| J["将 body 沿 up 方向移回贴合地面"]
    I -->|否| K[不修改位置]
```

### 4.3 Floating 模式

```
_move_and_slide_floating(delta):
    简化版滑动，不区分 floor/wall/ceiling
    适用于飞行/游泳/零重力场景
    循环 max_slides 次:
        move_and_collide(motion)
        if 碰撞: motion = remainder.slide(wall_normal)
        if 无碰撞或 motion ≈ 0: break
```

## 5. 第一人称控制器

### 5.1 典型节点结构

```
CharacterBody3D (FPSController)
├── CollisionShape3D (CapsuleShape3D)
├── Node3D (Head) ← 相机挂载点
│   └── Camera3D
└── MeshInstance3D (可选: 第一人称手臂)
```

### 5.2 FPS 控制器流程

```mermaid
sequenceDiagram
    participant Input as Input
    participant Script as GDScript
    participant Body as CharacterBody3D
    participant Head as Node3D Head

    Input->>Script: get_vector move
    Script->>Script: 计算方向 dir

    Input->>Script: InputEventMouseMotion
    Script->>Head: rotate_y and rotate_x
    Note over Head: 旋转 Head 子节点而非 Body

    Script->>Body: velocity = dir * speed
    Script->>Body: velocity.y -= gravity * delta
    Script->>Body: move_and_slide
    Body->>Body: 碰撞滑动处理
    Body-->>Script: 返回碰撞状态

    Script->>Body: if on_floor and jump then velocity.y = jump_velocity
```

### 5.3 FPS 关键代码模式

```gdscript
extends CharacterBody3D

@export var speed := 5.0
@export var jump_velocity := 4.5
@export var mouse_sensitivity := 0.002
@export var gravity_multiplier := 1.0

@onready var head: Node3D = $Head
@onready var camera: Camera3D = $Head/Camera3D

var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")

func _ready() -> void:
    Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseMotion:
        rotate_y(-event.relative.x * mouse_sensitivity)
        head.rotate_x(-event.relative.y * mouse_sensitivity)
        head.rotation.x = clamp(head.rotation.x, deg_to_rad(-89), deg_to_rad(89))

func _physics_process(delta: float) -> void:
    var input_dir := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
    var direction := (transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()

    if not is_on_floor():
        velocity.y -= gravity * gravity_multiplier * delta

    if is_on_floor() and Input.is_action_just_pressed("jump"):
        velocity.y = jump_velocity

    if direction:
        velocity.x = direction.x * speed
        velocity.z = direction.z * speed
    else:
        velocity.x = move_toward(velocity.x, 0, speed)
        velocity.z = move_toward(velocity.z, 0, speed)

    move_and_slide()
```

## 6. 第三人称控制器

### 6.1 典型节点结构

```
CharacterBody3D (TPSController)
├── CollisionShape3D (CapsuleShape3D)
├── Node3D (ModelPivot) ← 角色模型 + 动画
│   └── Skeleton3D + MeshInstance3D
├── SpringArm3D ← 相机弹簧臂
│   └── Camera3D
└── AnimationTree
    └── (引用) AnimationPlayer
```

### 6.2 TPS 控制器流程

```mermaid
sequenceDiagram
    participant Input as Input
    participant Script as GDScript
    participant Body as CharacterBody3D
    participant Spring as SpringArm3D
    participant AnimTree as AnimationTree

    Input->>Script: get_vector move
    Script->>Spring: get_global_transform basis
    Note over Script: 计算相机空间方向
    Script->>Script: direction = cam_basis * input_dir

    Input->>Script: InputEventMouseMotion
    Script->>Spring: rotate_y and rotate_x

    Script->>Body: velocity = direction * speed
    Script->>Body: move_and_slide

    Script->>Body: if direction.length > 0.1 then ModelPivot.look_at
    Script->>AnimTree: set blend_position = velocity.xz
    Script->>AnimTree: set conditions grounded and moving
```

### 6.3 SpringArm3D 工作机制

```mermaid
flowchart TD
    A["SpringArm3D physics_process"] --> B["process_spring"]
    B --> C["沿 -Z 轴投射 spring_length 距离"]
    C --> D{命中障碍物?}
    D -->|是| E["current_spring_length = 碰撞距离 - margin"]
    D -->|否| F["current_spring_length = spring_length"]
    E --> G["移动子 Camera3D 到碰撞点"]
    F --> G
    G --> H["Camera3D.global_position = SpringArm3D.global_position - Z * current_spring_length"]
```

### 6.4 TPS 关键代码模式

```gdscript
extends CharacterBody3D

@export var speed := 5.0
@export var jump_velocity := 4.5
@export var mouse_sensitivity := 0.002

@onready var spring_arm: SpringArm3D = $SpringArm3D
@onready var model: Node3D = $ModelPivot
@onready var anim_tree: AnimationTree = $AnimationTree

var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")

func _ready() -> void:
    Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseMotion:
        spring_arm.rotate_y(-event.relative.x * mouse_sensitivity)
        spring_arm.rotation.x = clamp(spring_arm.rotation.x - event.relative.y * mouse_sensitivity, deg_to_rad(-60), deg_to_rad(30))

func _physics_process(delta: float) -> void:
    var input_dir := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
    var cam_basis := spring_arm.global_transform.basis
    var direction := (cam_basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()
    direction.y = 0
    direction = direction.normalized()

    if not is_on_floor():
        velocity.y -= gravity * delta

    if is_on_floor() and Input.is_action_just_pressed("jump"):
        velocity.y = jump_velocity

    if direction:
        velocity.x = direction.x * speed
        velocity.z = direction.z * speed
        model.look_at(global_position + direction)
    else:
        velocity.x = move_toward(velocity.x, 0, speed)
        velocity.z = move_toward(velocity.z, 0, speed)

    # 动画驱动
    var velocity_xz := Vector2(velocity.x, velocity.z).length()
    anim_tree.set("parameters/MoveBlend/blend_position", Vector2(input_dir.x, -input_dir.y))
    anim_tree.set("parameters/conditions/grounded", is_on_floor())
    anim_tree.set("parameters/conditions/moving", velocity_xz > 0.5)

    move_and_slide()
```

## 7. 动画与移动的集成

### 7.1 AnimationTree 参数驱动流程

```mermaid
flowchart TD
    subgraph Script["_physics_process"]
        A["计算 velocity"] --> B["move_and_slide"]
        B --> C["获取 is_on_floor / is_on_wall"]
        C --> D["计算 speed"]
        D --> E["计算 input_direction"]
    end

    subgraph AnimParams["AnimationTree 参数设置"]
        F["MoveBlend blend_position = Vector2 input"]
        G["conditions grounded = is_on_floor"]
        H["conditions moving = speed > threshold"]
        I["conditions jumping = not on_floor and vel.y > 0"]
        J["conditions falling = not on_floor and vel.y < 0"]
    end

    Script --> AnimParams

    subgraph StateMachine["AnimationNodeStateMachine"]
        K["Idle"] -->|"grounded and moving"| L["MoveBlend"]
        L -->|"not moving"| K
        K -->|"jumping"| M["JumpStart"]
        M -->|"airborne"| N["Falling"]
        N -->|"grounded"| K
        L -->|"not grounded"| N
    end

    AnimParams --> StateMachine

    subgraph MoveBlend["AnimationNodeBlendSpace2D"]
        O["0,-1 Forward"]
        P["1,0 Right"]
        Q["0,1 Backward"]
        R["-1,0 Left"]
    end

    StateMachine --> MoveBlend
```

### 7.2 Root Motion 集成

```mermaid
flowchart TD
    A["AnimationPlayer 播放动画"] --> B{"动画含 Root Motion 轨道?"}
    B -->|是| C["AnimationTree 提取 root_motion"]
    C --> D["root_motion_position : Vector3<br>root_motion_rotation : Quaternion<br>root_motion_scale : Vector3"]
    D --> E["_physics_process 中读取"]
    E --> F["velocity = anim_tree.get_root_motion_position / delta"]
    F --> G["move_and_slide"]

    B -->|否| H["velocity 由输入计算"]
    H --> G

    style C fill:#fff3e0
    style F fill:#e8f5e9
```

**Root Motion API**：
```gdscript
# AnimationTree 中启用 Root Motion
func _physics_process(delta):
    var root_motion_pos := anim_tree.get_root_motion_position()
    var root_motion_rot := anim_tree.get_root_motion_rotation()

    velocity = root_motion_pos / delta
    if root_motion_rot:
        global_transform.basis = global_transform.basis * Basis(root_motion_rot)

    velocity.y -= gravity * delta
    move_and_slide()
```

## 8. 与 Unreal Engine 对比

### 8.1 架构对比

```mermaid
flowchart LR
    subgraph Godot["Godot 角色控制器"]
        G1["CharacterBody3D<br>仅移动+碰撞滑动"]
        G2["Camera3D + SpringArm3D<br>用户手动组装"]
        G3["AnimationTree<br>松耦合, 需手动驱动参数"]
        G4["GDScript 手写<br>FPS/TPS 控制逻辑"]
        G1 --> G4
        G4 --> G2
        G4 --> G3
    end

    subgraph UE["UE 角色控制器"]
        U1["ACharacter<br>完整角色框架"]
        U2["UCharacterMovementComponent<br>6 种移动模式+网络复制"]
        U3["USpringArm + UCameraComponent<br>内置组件"]
        U4["AnimBP + StateMachine<br>深度集成, 自动驱动"]
        U5["APlayerController<br>输入到角色解耦"]
        U1 --> U2
        U1 --> U4
        U5 --> U1
        U3 --> U1
    end

    Godot -.->|差距| UE
```

### 8.2 详细功能对比

| 功能 | Godot | UE | 差距 |
|------|-------|-----|------|
| **移动基础** | CharacterBody3D.move_and_slide | UCharacterMovementComponent | Godot 仅碰撞滑动，UE 有完整移动框架 |
| **移动模式** | 2 种 Grounded/Floating | 6 种 Walk/Fall/Fly/Swim/Custom/NavWalk | 🔴 严重缺失 |
| **地面检测** | floor_max_angle + floor_snap_length | WalkableFloorAngle + floor sweep | 🟡 功能相似 |
| **斜坡处理** | floor_stop_on_slope, floor_constant_speed | 根据角度自动调整 | 🟡 功能相似 |
| **台阶步进** | 无 | StepUp / StepDown 自动处理 | 🔴 缺失 |
| **平台跟随** | platform_velocity + platform_on_leave | BasedMovement + BasedRotation | 🟡 功能相似 |
| **墙壁滑动** | wall_min_slide_angle, floor_block_on_wall | 自动处理 | 🟡 功能相似 |
| **网络复制** | 无内置 | Server-authoritative 复制 | 🔴 严重缺失 |
| **NavMesh 集成** | 无内置移动集成 | NavWalk 模式 | 🔴 缺失 |
| **Root Motion** | AnimationTree.get_root_motion_position | RootMotionFromEverything | 🟡 功能相似 |
| **动画驱动移动** | 手动脚本驱动 | AnimBP 自动驱动 CharacterMovement | 🟡 松耦合 vs 深耦合 |
| **相机系统** | Camera3D + SpringArm3D 手动组装 | SpringArm + Camera 内置组件 | 🟡 功能相似但需更多手动工作 |
| **第一人称** | 无内置模板，需手写 | FPS Template 项目模板 | 🔴 缺失模板 |
| **第三人称** | 无内置模板，需手写 | TPS Template 项目模板 | 🔴 缺失模板 |
| **IK 集成** | 无内置 | AnimBP 内置 FootIK/HandIK | 🔴 缺失 |
| **Montage 系统** | 无 | AnimMontage 动作动画系统 | 🔴 缺失 |
| **瞄准偏移** | 手动修改 AnimationTree blend_position | AimOffset 节点 | 🟡 可模拟但无专用工具 |
| **角色物理交互** | 无内置 | PushForce/RVO Avoidance | 🔴 缺失 |
| **镜头抖动** | 无内置 | CameraShake 系统 | 🔴 缺失 |
| **死亡/重生** | 无内置 | Possess/Unpossess 机制 | 🔴 缺失 |
| **观战模式** | 无内置 | SpectatorPawn | 🔴 缺失 |

### 8.3 UE UCharacterMovementComponent 核心功能

```mermaid
classDiagram
    class ACharacter {
        +CapsuleComponent : UCapsuleCollisionProfile
        +CharacterMovementComponent : UCharacterMovementComponent
        +Mesh : USkeletalMeshComponent
        +Jump() virtual
        +Crouch() virtual
        +UnCrouch() virtual
        +CanJump() bool virtual
        +CanCrouch() bool virtual
        +LaunchCharacter(velocity, xy_override, z_override)
    }

    class UCharacterMovementComponent {
        +MovementMode : EMovementMode
        +CustomMovementMode : uint8
        +Velocity : FVector
        +Acceleration : FVector
        +MaxWalkSpeed : float
        +MaxFlySpeed : float
        +MaxSwimSpeed : float
        +JumpZVelocity : float
        +GravityScale : float
        +WalkableFloorAngle : float
        +StepHeight : float
        +bOrientRotationToMovement : bool
        +RotationRate : FRotator
        +bIsCrouched : bool
        +SetMovementMode(mode, custom_mode)
        +AddMovementInput(direction, scale)
        +PerformMovement(delta)
        +ReplicateMoveToServer(delta, acceleration)
        +SmoothClientPosition(delta)
    }

    class EMovementMode {
        <<enumeration>>
        MOVE_None
        MOVE_Walking
        MOVE_NavWalking
        MOVE_Falling
        MOVE_Swimming
        MOVE_Flying
        MOVE_Custom
    }

    ACharacter --> UCharacterMovementComponent : contains
    UCharacterMovementComponent --> EMovementMode
```

### 8.4 UE 动画-移动深度集成

```mermaid
flowchart TD
    subgraph UEIntegration["UE 动画-移动集成"]
        A["UCharacterMovementComponent.Velocity"] --> B["AnimBP Update Animation"]
        B --> C["计算 Speed, Direction, bIsInAir, bIsCrouching"]
        C --> D["驱动 StateMachine 状态转换"]
        D --> E["Idle to MoveBlendSpace to Jump to Land"]
        E --> F["BlendSpace 根据 Speed/Direction 混合动画"]
        F --> G{"Root Motion?"}
        G -->|是| H["Extract Root Motion then Apply to CharacterMovement"]
        G -->|否| I["普通动画播放"]
        H --> I

        J["AnimMontage 播放"] --> K["Ability System 触发"]
        K --> L["Montage 期间可锁定移动/旋转"]
        L --> M["Montage Root Motion to CharacterMovement"]
    end

    subgraph GodotIntegration["Godot 动画-移动集成"]
        N["GDScript _physics_process"] --> O["手动计算 velocity"]
        O --> P["move_and_slide"]
        P --> Q["手动设置 AnimationTree 参数"]
        Q --> R["AnimationTree StateMachine 转换"]
        R --> S["BlendSpace2D 混合"]
        S --> T{"Root Motion?"}
        T -->|是| U["get_root_motion_position then 手动应用"]
        T -->|否| V["普通播放"]
    end

    style UEIntegration fill:#e3f2fd
    style GodotIntegration fill:#fff3e0
```

## 9. 缺失功能详细分析

### 9.1 移动模式状态机（严重缺失）

```
Godot 仅提供 2 种 MotionMode (Grounded / Floating)
UE 提供 6 种内置移动模式 + 自定义模式

缺失的移动模式：
├── MOVE_Swimming    — 水中游泳（浮力、水阻、水面检测）
├── MOVE_Flying      — 自由飞行（需完整飞行物理）
├── MOVE_NavWalking  — NavMesh 导航行走（AI 角色）
├── MOVE_Climbing    — 攀爬（梯子、墙壁）
├── MOVE_Crouching   — 蹲伏（碰撞体高度变化）
└── MOVE_Sliding     — 滑铲（碰撞体降低 + 速度维持）

当前 Godot 需要用户在 _physics_process 中
手动切换不同移动逻辑（if/elif 链）
```

### 9.2 台阶步进（严重缺失）

```
UE 自动处理：
  StepHeight = 45.0f
  → 碰撞到低障碍物时自动"迈步"上去
  → StepDown 自动贴合地面
  → StepUp / StepDown 由 CharacterMovementComponent 内部处理

Godot 无此功能：
  → 碰撞到台阶会停住
  → 需要用户手动实现：
     1. 检测碰撞点高度 < step_height
     2. 向上移动 step_height
     3. 尝试 move_and_collide
     4. 失败则回退
  → 实现复杂且容易出 bug
```

### 9.3 网络复制（严重缺失）

```
UE 的网络移动架构：
  ACharacter
  ├── Server: PerformMovement() → 保存移动状态
  ├── Client: ReplicateMoveToServer() → 发送移动输入
  ├── Server: ServerMoveResult() → 确认/纠正
  └── Client: ClientAdjustPosition() → 平滑修正

Godot 无内置网络移动复制：
  → 需要手动实现：
     1. RPC 发送位置/速度
     2. 客户端预测
     3. 服务器纠正
     4. 插值平滑
  → multiplayer_synchronizer 仅做简单同步
  → 无预测-回滚框架
```

### 9.4 动画 Montage 系统（严重缺失）

```
UE AnimMontage：
  ├── 分段播放（Section）
  ├── 根运动提取
  ├── 通知（AnimNotify）触发游戏逻辑
  ├── 混合出入控制
  ├── 蒙太奇期间锁定移动
  └── 多层 Montage 混合

Godot 无 Montage：
  → AnimationPlayer.play() + queue() 是最接近的功能
  → 无法在动画中间触发逻辑（需 AnimationPlayer.call_method_track）
  → 无法在动画期间锁定移动（需手动 flag）
  → 无分段播放（需多个 Animation + StateMachine 模拟）
```

### 9.5 IK 系统（严重缺失）

```
UE 内置 IK：
  ├── TwoBoneIK（手脚骨骼对齐地面/目标）
  ├── FABRIK（全身 IK 链）
  ├── CCD IK（头部注视等）
  └── FootPlacement（自动脚部贴合地形）

Godot 无内置 IK：
  → Skeleton3D 有 bone.pose 而无 IK solver
  → 需 GDExtension 自实现或用 look_at + 数学计算
  → 社区有 IK 插件但不成熟
```

### 9.6 相机系统增强（中等缺失）

```
缺失功能：
├── CameraShake — 镜头抖动（爆炸、受伤、冲刺）
├── CameraLensEffect — 镜头效果（景深、运动模糊）
├── CameraImpulse — 冲量响应（后坐力、击中反馈）
├── CameraModeTransition — 第一/第三人称切换过渡
├── ViewmodelCamera — 第一人称武器/手臂渲染分层
└── CameraCollisionPadding — 相机碰撞时更智能的偏移
```

### 9.7 缺失功能严重度总览

| 功能 | 严重度 | 实现难度 | 影响范围 |
|------|--------|---------|---------|
| **移动模式状态机** | 🔴 严重 | 高 | 所有类型角色 |
| **台阶步进** | 🔴 严重 | 中 | 地面角色移动体验 |
| **网络复制** | 🔴 严重 | 极高 | 多人游戏 |
| **Montage 系统** | 🔴 严重 | 高 | 动作游戏 |
| **IK 系统** | 🔴 严重 | 高 | 角色动画质量 |
| **内置控制器模板** | 🟡 中等 | 低 | 新手入门 |
| **相机增强** | 🟡 中等 | 中 | 视觉体验 |
| **角色物理交互** | 🟡 中等 | 中 | 物理丰富度 |
| **镜头抖动** | 🟢 轻微 | 低 | 视觉反馈 |
| **观战模式** | 🟢 轻微 | 低 | 多人游戏 |

## 10. Godot 角色控制器成熟度评级

```
Godot 角色控制器成熟度：★★☆☆☆ (2/5)

对比：
- UE ACharacter + CMC： ★★★★★ (5/5)
- Unity CharacterController：★★★☆☆ (3/5)
- Unity Kinematic Character Controller (资产)：★★★★☆ (4/5)
- Godot CharacterBody3D：  ★★☆☆☆ (2/5)
```

### 10.1 Godot 的强项

1. **move_and_slide() 简洁有效**：Grounded/Floating 两种模式覆盖基本需求
2. **碰撞滑动算法成熟**：max_slides、slope 处理、wall slide 均经过社区打磨
3. **SpringArm3D 轻量**：弹簧臂相机方案简单直接
4. **AnimationTree 灵活**：状态机 + BlendSpace + Root Motion 支持良好
5. **GDScript 快速原型**：100 行即可实现基础 FPS/TPS 控制器
6. **平台跟随**：platform_velocity 和 platform_on_leave 功能完整

### 10.2 Godot 的弱点

1. **无移动模式状态机**：Walking/Falling/Swimming/Flying/Climbing 需手动 if/elif
2. **无台阶步进**：碰到低矮台阶会卡住
3. **无网络复制**：多人游戏移动同步需完全自建
4. **无 Montage**：动作动画播放缺乏专业工具
5. **无 IK 系统**：脚部贴合地形等高级动画无法实现
6. **无内置模板**：新手需从零手写 FPS/TPS 控制器
7. **松耦合**：移动-动画-相机需大量胶水代码串联

## 11. 关键源码索引

| 类别 | 路径 |
|------|------|
| CharacterBody3D | `scene/3d/physics/character_body_3d.h` / `character_body_3d.cpp` |
| PhysicsBody3D | `scene/3d/physics/physics_body_3d.h` |
| CollisionObject3D | `scene/3d/physics/collision_object_3d.h` |
| KinematicCollision3D | `scene/3d/physics/kinematic_collision_3d.h` |
| Camera3D | `scene/3d/camera_3d.h` / `camera_3d.cpp` |
| SpringArm3D | `scene/3d/physics/spring_arm_3d.h` / `spring_arm_3d.cpp` |
| RayCast3D | `scene/3d/physics/ray_cast_3d.h` |
| ShapeCast3D | `scene/3d/physics/shape_cast_3d.h` |
| AnimationTree | `scene/animation/animation_tree.h` / `animation_tree.cpp` |
| AnimationPlayer | `scene/animation/animation_player.h` |
| AnimationNodeStateMachine | `scene/animation/animation_node_state_machine.h` |
| AnimationNodeBlendSpace2D | `scene/animation/animation_blend_space_2d.h` |
| AnimationNodeBlendSpace1D | `scene/animation/animation_blend_space_1d.h` |
| AnimationNodeBlendTree | `scene/animation/animation_blend_tree.h` |
| Node3D | `scene/3d/node_3d.h` |
| PhysicsServer3D | `servers/physics_3d/physics_server_3d.h` |
