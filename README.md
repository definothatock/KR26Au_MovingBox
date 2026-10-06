# 摘要——这是什么

**关卡编辑系统（Level Editor System，LES）**是一个谜题制作系统，支持：在引擎中配置实体，以及在游戏中编辑谜题关卡。

它允许用户：

1. 使用同一套基模配置实体和关卡；
2. 进入游戏内编辑模式，以构建或解答游戏中的谜题；
3. 使用可自定义的目标 Actor 完成关卡。

系统围绕 `ALES_SessionManager` 构建，它充当**会话级协调器**，而各个组件负责具体职责。

https://github.com/user-attachments/assets/c959d6fd-c655-4c27-bc06-44819dd392b5


### 高层架构

系统职责划分：

```text
ALES_SessionManager
│ └── 路由请求并监控会话
│
├── ULES_EditorAdaptorComponent
│   └── 接管编辑模式输入
│
├── ULES_PlacementComponent
│   └── 处理放置和编辑操作
│
├── ULES_PhysicsFreezeComponent
│   └── 在编辑期间停止物理模拟
│
└── ULES_ObjectiveComponent
    └── 监控目标 Actor 和目标区域
```

资源和配置：

```text
ULES_LevelEditorConfig                 
└── 用于标准化设置的关卡配置数据

ULES_PlaceableDefinition
└── 可在关卡中重复使用的实体数据

ALES_PlaceableBase                  
└── 可放置实体的基类

ALES_Goal     
└── 通过检查目标和自身来标记关卡成功

ALES_HUD
└── 原型/调试 HUD
```

## 编辑期间的玩家控制

| 输入 | 操作 |
| --- | --- |
| 鼠标左键（LMB） | 放置预览对象，或选择已经放置的 LES 对象 |
| 鼠标右键（RMB） | 取消预览或取消移动 |
| 鼠标滚轮 | 升高/降低放置平面 |
| Tab | 循环切换可用对象类型 |
| R | 将预览对象旋转 90° |
| M | 移动已选中的已放置对象 |
| Delete | 删除已选中的已放置对象 |
| Enter | 完成编辑并开始游戏 |
| Y | 重启当前关卡 |
| U | 在完成后加载下一关 |

<br><br><br>
**以下分别是设计师指南和程序员文档。**
<br><br><br>

---

# 设计师指南

## 用途

LES 允许你预先配置标准化内容，并在开始游戏前构建关卡的一部分。

## 配置

对于一个 LES 关卡，需要配置：

1. 一个 LES Session Manager Actor；
2. 一个 LES Level Config 数据资产。

对于关卡游戏内容，需要配置：

1. 从 `ALES_PlaceableBase` 派生的 Blueprint 类；
2. 用于描述这些 Blueprint 类的 Placeable Definition 数据资产；
3. 一个目标 Actor；
4. 一个 `ALES_Goal` Actor。

## 设置检查清单

### 1. 添加且仅添加一个 Session Manager

在关卡中放置一个 `ALES_SessionManager`。

重要事项：

- 关卡中必须**包含一个** Session Manager。
- 将其旋转保持为 `0, 0, 0`。
- 将其缩放保持为 `1, 1, 1`。
- 它的位置是所需可编辑区域的**底部中心点**。

### 2. 创建 LES Level Config 资产

创建一个 `ULES_LevelEditorConfig`，并将其分配给 Session Manager 的 **Level Config** 字段。

| 设置 | 含义 |
| --- | --- |
| `AreaHalfExtent` | 可编辑区域的大小。 |
| `GridSize` | 运行时吸附尺寸。 |
| `FloorClearance` | 放置平面上方的放置高度偏移。 |
| `AvailableEntities` | 运行时允许放置的实体。 |
| `NextLevel` | 成功完成后进入的关卡。 |
| `bDrawPlacement` | 绘制预览放置边界调试框。 |
| `bDrawArea` | 绘制可编辑区域调试框。 |

### 3. 创建可放置实体的数据

创建可在多个关卡中重复使用的实体。

#### A. 可放置 Actor

通过从 `ALES_PlaceableBase` 派生 Blueprint 来创建可放置 Actor。

基类已经包含：

```text
SceneRoot
├── VisualMesh
└── LESPlacementBounds
```

##### `VisualMesh`

`VisualMesh` 是游戏中可见的网格，也是用于放置碰撞检查的权威来源。

配置要求：

- 分配一个具有合适碰撞几何体的有效静态网格。

##### `LESPlacementBounds`

`LESPlacementBounds` 用于调试显示，以及在确认放置前提供早期警告。它不是最终的碰撞判定依据。

配置要求：

- 使其大致表示对象所占据的空间。

#### B. Placeable Definition

为每个可重复使用的对象类别创建一个 `ULES_PlaceableDefinition` Data Asset。

| 设置 | 含义 |
| --- | --- |
| `DisplayName` | 在 LES HUD 中显示的名称。 |
| `ActorClass` | 一个从 `ALES_PlaceableBase` 派生的 Blueprint 或 C++ 类。 |

这使你可以从 Session Manager 中，在 Level Config 内进一步配置这些对象。

## 激活游戏行为

可放置对象在编辑期间处于非激活状态。当玩家完成编辑后，LES 会调用：

```text
SetGameplayActive(true)
```

对于 Blueprint 可放置对象，请使用：

```text
On LES Gameplay Active Changed
```

来开始仅限游戏阶段的行为。

如果这些行为需要等待游戏开始，则**不要**在 Construction Script 或 Begin Play 中启动它们。

如果可放置对象需要在编辑结束后模拟物理：

1. 在可放置对象 Blueprint 上启用 **Simulate Visual Mesh During Gameplay**；或者
2. 在 `OnLESGameplayActiveChanged` 中管理其物理状态。

## 设置目标和目标区域

在 Session Manager 上配置：

| 字段 | 含义 |
| --- | --- |
| `TargetActor` | 玩家必须带到目标区域的对象。 |
| `GoalActor` | 定义成功区域的 `ALES_Goal` Actor。 |

当**目标 Actor 的根位置**在所需持续时间内保持位于目标体积内时，目标判定完成。系统只检查目标 Actor 的根点，不要求目标对象的整个网格都位于目标区域内。

## 设计师故障排查

### “对象无法放置。”

检查以下内容：

- 实体是否位于 LES 区域内。
- 实体是否还有剩余数量。
- `VisualMesh` 是否启用了碰撞。
- 实体是否未处于碰撞状态。
- 网格是否具有有效的碰撞几何体。
- 实体是否与目标对象、目标标记、世界几何体或另一个已放置对象发生重叠。

### “我能看到重叠，但 LES 仍然接受了放置。”

可能原因：

- 网格使用了过于宽松或过于简单的碰撞。
- 网格没有合适的简单碰撞。
- 碰撞响应被设置为 Ignore 或 Overlap。
- 两个碰撞组件没有互相 Block。

改进网格碰撞或碰撞配置文件。

### “对象看起来可以放置，但 LES 拒绝了它。”

可能原因：

- 碰撞形状大于渲染网格。
- 网格碰撞延伸到了编辑区域之外。
- 另一个不可见碰撞组件阻挡了它。
- 对象旋转后与其他对象发生碰撞。

使用碰撞可视化和 LES 调试框进行调查。

### “我的可放置对象在游戏开始前就开始移动了。”

将启动行为从 Begin Play 或 Construction Script 中移出。

请使用：

```text
On LES Gameplay Active Changed
```

并仅在 `bNowGameplayActive` 为 true 时开始移动、计时器或物理模拟。

---

# 程序员文档

## 架构意图

LES 是一个基于门面模式的独立本地运行时编辑框架。目前明确不支持多人游戏。

`ALES_SessionManager` 是系统的组合根。它拥有高层状态机，并将具体工作委托给附加的 Actor 组件。

```text
SessionManager = 编排和阶段权限
Components     = 子系统实现
Actors/assets  = 世界内容和配置
```

Session Manager 应保持为协调器。不要将详细的放置、输入、物理或目标逻辑移动到其中。

## 主要控制流程

### 启动

```text
SessionManager.BeginPlay()
  ├── 初始化 PlacementEditor
  ├── 绑定 ObjectiveMonitor.OnObjectiveCompleted
  ├── 验证设置
  └── 进入 Waiting 阶段
```

### 进入编辑模式

```text
SessionManager.Tick()
  └── TryEnterEditing()
       ├── 定位本地 PlayerController 和 Pawn
       ├── 初始化适配器输入
       ├── 冻结现有的模拟物理
       ├── 应用编辑摄像机/输入模式
       ├── PlacementEditor.BeginEditing()
       ├── 将阶段更改为 Editing
       └── 选择第一个可放置类型
```

### 光标预览更新

```text
EditorAdaptor.Tick()
  └── UpdateCursorPreview()
       ├── 将光标反投影为世界射线
       ├── 计算射线与当前放置 Z 平面的交点
       └── SessionManager.UpdatePlaceablePreview()
            └── PlacementComponent.UpdatePreviewFromPlane()
                 ├── BuildCandidateTransform()
                 ├── 移动预览 Actor
                 ├── 执行提示性边界验证
                 └── 更新预览状态
```

### 确认放置

```text
玩家按下鼠标左键
  └── EditorAdaptor.Input_ConfirmOrSelect()
       └── SessionManager.Request_ConfirmPlacement()
            └── PlacementComponent.ConfirmPlacement()
                 ├── 确认当前预览存在
                 ├── 检查新放置对象的可用数量
                 ├── 执行权威碰撞验证
                 ├── 恢复预览对象的碰撞状态
                 ├── 对于移动操作：保留新的变换
                 └── 对于新 Actor：
                      ├── 添加 FLES_PlacedRecord
                      ├── 绑定 OnDestroyed
                      └── 生成下一个预览对象
```

### 完成编辑

```text
SessionManager.Request_FinishEditing()
  ├── 验证玩家、目标对象和目标区域
  ├── 重新验证所有已放置 Actor
  ├── PlacementEditor.EndEditing()
  ├── EditorAdaptor.Request_LeaveEditing()
  ├── PhysicsFreeze.RestoreWorldPhysics()
  ├── 将阶段更改为 Gameplay
  ├── PlacementEditor.ActivatePlacedActors(true)
  └── ObjectiveMonitor.StartMonitoring()
```

## 核心规则和契约

### 会话级状态

```cpp
ELES_Phase Phase;
FString FlowStatusText;
```

`Phase` 是判断当前是否允许执行编辑操作或游戏操作的权威状态。公共请求方法应在转发给子系统之前检查阶段。

示例：

```cpp
Request_ConfirmPlacement()
Request_FinishEditing()
```

### 放置契约

1. 只有包含在 `PlacedRecords` 中的 Actor 才可以编辑。
2. 新放置预览对象是一个已经生成但尚未确认的 `ALES_PlaceableBase`。
3. 移动对象时，将已放置 Actor 重新用作预览 Actor。
4. 在确认或取消之前，必须恢复预览对象的碰撞状态。

所有目录中的可放置对象都应从以下类派生：

```cpp
ALES_PlaceableBase
```

有效的 `ALES_PlaceableBase` 必须提供：

```text
SceneRoot
VisualMesh
LESPlacementBounds
```

### `VisualMesh`

- 必须具有有效的 `UStaticMesh`。
- 必须针对世界碰撞通道和可放置对象碰撞通道设置明确的响应。

### `LESPlacementBounds`

- 提供提示性的重叠反馈。
- 用于预览调试渲染。
- 用于显示由设计师提供的占用范围。

当前实现要求它相对于 Actor 根节点居中且不带旋转。该限制由 `GetLESPlacementData()` 强制执行。

## 激活契约

`ILES_PlaceableInterface` 提供：

```cpp
void SetGameplayActive(bool bActive);
```

基类实现会：

1. 保证调用具有幂等性；
2. 跟踪激活状态；
3. 启用/禁用可选的 `VisualMesh` 物理模拟；
4. 调用 Blueprint 扩展点：

```cpp
OnLESGameplayActiveChanged(bool bNowGameplayActive)
```

除非有充分理由，否则不要在 Blueprint 子类中重写 `SetGameplayActive`；应使用 `OnLESGameplayActiveChanged` 实现 Blueprint 行为。这样可以保留基类的激活语义。

## 放置验证细节

### 候选变换：`BuildCandidateTransform()`

- 根据 Session Manager 原点对 X/Y 进行吸附。
- 使用当前放置平面作为 Z。
- 根据 `PreviewYaw` 进行旋转。
- 使用经过网格局部变换和预览旋转变换后的静态网格边界，计算可见网格底部。
- 应用 `FloorClearance`。

该变换假设：

- Session 旋转为单位旋转。
- Session 缩放为一。
- 可放置 Actor 的缩放实际上为一。
- 网格吸附仅用于 X/Y。
- Z 轴由 `PlacementPlaneZ` 控制。

### 提示性验证：`ValidatePreviewBounds()`

- 检查 `LESPlacementBounds` 是否位于配置的可编辑体积内。
- 使用盒体形状进行重叠查询。
- 当检测到相关阻挡几何体时报告警告。

该结果决定预览反馈，而不是最终放置权限。

### 权威验证：`ValidateMeshPlacement()`

- 使用 `VisualMesh` 调用 `ComponentOverlapMulti()`。
- 它是放置验证的事实来源。
- 忽略 SessionManager Actor。
- 忽略候选 Actor 自身。

## 所有权和生命周期

### 受管理 Actor

只有以下数组中的记录：

```cpp
TArray<FLES_PlacedRecord> PlacedRecords;
```

才会被视为由 Session 管理、并且可编辑的对象。

这一点通过以下函数强制执行：

```cpp
bool IsManagedActor(const AActor* Actor) const;
```

### 销毁处理

已放置的 Actor 会绑定：

```cpp
Preview->OnDestroyed.AddDynamic(
    this,
    &ULES_PlacementComponent::HandlePlacedActorDestroyed);
```

销毁清理会移除无效记录，并在必要时清除选择状态。如果其他系统销毁了已放置的 Actor，则由于剩余数量是根据有效放置记录计算的，该对象的可用数量会再次增加。

## 物理冻结行为

`ULES_PhysicsFreezeComponent` 会捕获世界中当前正在模拟的每个 `UPrimitiveComponent`：

```cpp
组件指针
线性速度
角速度
```

然后禁用物理模拟。

恢复时，它会：

1. 重新启用物理模拟；
2. 恢复线性速度；
3. 恢复角速度；
4. 唤醒刚体。

这是原型实现，影响范围过大。对于更大型的项目，应引入过滤机制或选择加入机制。

## 目标行为

`ULES_ObjectiveComponent` 监控：

```cpp
TargetActor
GoalActor
HoldTime
bMonitoring
```

每帧执行以下操作：

1. 检查目标对象和目标区域是否有效；
2. 测试 `Goal->ContainsWorldPoint(Target->GetActorLocation())`；
3. 如果目标点离开目标区域，则重置保持时间；
4. 当保持时间达到 `RequiredHoldSeconds` 时完成目标。

当前每帧检查目标 Actor 的根位置。它不检测重叠、不检测整个对象是否被包含在目标区域内，也不检测扫掠移动。

---

## 已知不足

### 放置事件有限

当前放置事件被捆绑在一起，仅用于触发声音。

### 物理冻结范围过大

`FreezeComponent` 当前会冻结所有对象，没有过滤机制。

### 内置按键绑定

UI 和编辑模式的按键绑定是硬编码的，而不是使用 IAM 和 IA，就像玩家控制所使用的方式一样。

### 添加可配置的放置策略

当前碰撞策略是围绕互相 Block 的响应硬编码的。

### 不支持保存/加载

现有的 `FLES_PlacedRecord` 是临时数据，并且只存储 Actor 指针。
