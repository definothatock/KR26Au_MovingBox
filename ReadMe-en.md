# Abstract - What is this

The **Level Editor System (LES)** is a puzzle making system that supports: in-engine Entity configuration, and in-game editing system for puzzle level.

It lets user:
1. Configure Entities and Levels under the same schema;
2. Enter in-game edit mode to build/solve in game puzzles;
3. Finish the Level with customisable Target Actor.

The system is structured around `ALES_SessionManager`, which acts as the **session-level coordinator**, while individual components own specific responsibilities.

### High-level architecture

System segregations:
```text
ALES_SessionManager
│ └── Route requests and monitor session
│
├── ULES_EditorAdaptorComponent
│   └── Edit Mode input takeover
│
├── ULES_PlacementComponent
│   └── Handle placement and editing actions
│
├── ULES_PhysicsFreezeComponent
│   └── Stops physics during editing
│
└── ULES_ObjectiveComponent
    └── Watches target actor and goal
```

Assets and Configurations:
```text
ULES_LevelEditorConfig                 
└── Level config Data for standardised setups

ULES_PlaceableDefinition
└── Eitity Data for repeated use in Levels

ALES_PlaceableBase                  
└── Base class for placeable Entities

ALES_Goal     
└── Flag level success via checking target and self

ALES_HUD
└── Prototype/debug HUD
```

## Player controls during editing

| Input             | Action                                                |
| ----------------- | ----------------------------------------------------- |
| Left Mouse (LMB)  | Place preview, or select an already placed LES object |
| Right Mouse (RMB) | Cancel preview or cancel a move                       |
| Mouse Wheel       | Raise/lower placement plane                           |
| Tab               | Cycle available object type                           |
| R                 | Rotate preview by 90°                                 |
| M                 | Move selected placed object                           |
| Delete            | Remove selected placed object                         |
| Enter             | Finish editing and start gameplay                     |
| Y                 | Restart current level                                 |
| U                 | Load next level after completion                      |

The Below are the Designer Guide and Programmer Documentation, receptively.

---

# Designer Guide

## Purpose
LES lets you pre-config standardised content, and build part of a level before beginning gameplay.


## Configuration

For an LES level, configure:
1. One LES Session Manager actor.
2. One LES Level Config data asset.

For level gameplay, configure:
1. Blueprint classes derived from `ALES_PlaceableBase`.
2. Placeable Definition data assets that describe the blueprint classes.
3. A target actor.
4. An `ALES_Goal` actor.


## Setup checklist

### 1. Add exactly one Session Manager
Place one `ALES_SessionManager` in the level.

Important:
- The level must contain **exactly one** session manager.
- Keep its rotation at `0, 0, 0`.
- Keep its scale at `1, 1, 1`.
- Its location is the **bottom-center point** of desired editable area.

### 2. Create an LES Level Config asset
Create a `ULES_LevelEditorConfig` and assign it to the Session Manager’s **Level Config** field.

| Setting             | Meaning                                        |
| ------------------- | ---------------------------------------------- |
| `AreaHalfExtent`    | Size of the editable area.                     |
| `GridSize`          | Runtime snap size.                             |
| `FloorClearance`    | Placement height offset above placement plane  |
| `AvailableEntities` | The Entities that may be placed during runtime |
| `NextLevel`         | Level-to-go after successful completion.       |
| `bDrawPlacement`    | Draws preview placement-bound debug boxes.     |
| `bDrawArea`         | Draws the editable-area debug box.             |

### 3. Create placeable entities' data
create reusable entities for levels. 

#### A. Placeable Actor
Create placeable actors by deriving a Blueprint from `ALES_PlaceableBase`.

The base class already contains:
```text
SceneRoot
├── VisualMesh
└── LESPlacementBounds
```

##### `VisualMesh`
`VisualMesh` is the gameplay-visible mesh and the authoritative source used for placement collision checks.
Configure:
- Assign a valid static mesh with suitable collision geometry.

##### `LESPlacementBounds`
`LESPlacementBounds` is used for debug display and early warning before placement confirmation. It is not the final collision authority.
Configure:
- Make it approximately represent the space occupied by the object.

#### B. Placeable Definition
Create a `ULES_PlaceableDefinition` Data Asset for every reusable object category.

| Setting       | Meaning                                                     |
| ------------- | ----------------------------------------------------------- |
| `DisplayName` | Name shown in the LES HUD.                                  |
| `ActorClass`  | A Blueprint or C++ class derived from `ALES_PlaceableBase`. |

This allows further configuration in the Level Config, from the SessionManager.


## Activating gameplay behavior

Placeable objects are inactive during editing. When the player finishes editing, LES calls:

```text
SetGameplayActive(true)
```

For Blueprint placeables, use:

```text
On LES Gameplay Active Changed
```

to begin gameplay-only behavior.

Do **not** start these behaviors in Construction Script or Begin Play if they should wait until gameplay starts.


If the placeable should simulate physics after editing:
1. Enable **Simulate Visual Mesh During Gameplay** on the placeable Blueprint, or
2. Manage its physics in `OnLESGameplayActiveChanged`.


## Setting the target and goal

On the Session Manager, assign:

| Field | Meaning |
|---|---|
| `TargetActor` | The object the player must bring to the goal. |
| `GoalActor` | The `ALES_Goal` actor that defines the success area. |

The objective completes when the **target actor’s root position** stays inside the goal volume for the required duration. The system checks the target root point only. It does not require the entire target mesh to fit inside the goal.


## Designer troubleshooting

### “The object cannot be placed.”
Check if:
- Entity inside the LES area.
- Entity has remaining quantity.
- `VisualMesh` collision is enabled.
- Entity is not collided.
- The mesh has valid collision geometry.
- An entity is not overlapping as both: the target, goal marker, world geometry, or another placed object.

### “I can see overlap, but LES accepts placement.”
Possible causes:
- The mesh has loose/simple collision.
- The mesh has no appropriate simple collision.
- Collision responses are set to Ignore or Overlap.
- The two collision components do not mutually block each other.

Improve the mesh collision or collision profile.

### “The object looks placeable, but LES rejects it.”
Possible causes:
- The collision shape is larger than the rendered mesh.
- The mesh collision extends outside the edit volume.
- Another invisible collision component is blocking it.
- The object has rotated into a collision.

Use collision visualization and LES debug boxes to investigate.

### “My placeable starts moving before gameplay begins.”
Move the startup behavior out of Begin Play or Construction Script.

Use:
```text
On LES Gameplay Active Changed
```
and only begin movement, timers, or physics when `bNowGameplayActive` is true.


---

# Programmer Documentation


## Architectural intent
LES is a standalone, local runtime editing framework based on a facade pattern. Multiplayer is intentionally not supported at this moment.

`ALES_SessionManager` is the composition root for the system. It owns the high-level state machine and delegates detailed work to attached actor components.

```text
SessionManager = orchestration and phase authority
Components     = subsystem implementation
Actors/assets  = world content and configuration
```

The Session Manager should remain a coordinator. Avoid moving detailed placement, input, physics, or objective logic into it.


## Main control flow

### Startup
```text
SessionManager.BeginPlay()
  ├── Initialize PlacementEditor
  ├── Bind ObjectiveMonitor.OnObjectiveCompleted
  ├── Validate setup
  └── Enter Waiting phase
```

### Entering edit mode
```text
SessionManager.Tick()
  └── TryEnterEditing()
       ├── Locate local PlayerController and pawn
       ├── Initialize adaptor input
       ├── Freeze existing simulated physics
       ├── Apply editing camera/input mode
       ├── PlacementEditor.BeginEditing()
       ├── Change phase to Editing
       └── Select first placeable type
```

### Cursor preview update
```text
EditorAdaptor.Tick()
  └── UpdateCursorPreview()
       ├── Deproject cursor to world ray
       ├── Intersect ray with active placement Z plane
       └── SessionManager.UpdatePlaceablePreview()
            └── PlacementComponent.UpdatePreviewFromPlane()
                 ├── BuildCandidateTransform()
                 ├── Move preview actor
                 ├── Run advisory bounds validation
                 └── Update preview status
```

### Confirming placement
```text
Player left-click
  └── EditorAdaptor.Input_ConfirmOrSelect()
       └── SessionManager.Request_ConfirmPlacement()
            └── PlacementComponent.ConfirmPlacement()
                 ├── Confirm active preview exists
                 ├── Check available quantity for new placements
                 ├── Run authoritative collision validation
                 ├── Restore preview collision state
                 ├── For move: keep new transform
                 └── For new actor:
                      ├── Add FLES_PlacedRecord
                      ├── Bind OnDestroyed
                      └── Spawn next preview
```

### Finishing editing
```text
SessionManager.Request_FinishEditing()
  ├── Validate player, target, and goal
  ├── Revalidate all placed actors
  ├── PlacementEditor.EndEditing()
  ├── EditorAdaptor.Request_LeaveEditing()
  ├── PhysicsFreeze.RestoreWorldPhysics()
  ├── Change phase to Gameplay
  ├── PlacementEditor.ActivatePlacedActors(true)
  └── ObjectiveMonitor.StartMonitoring()
```


## Core Rules and Contracts

### Session-level state
```cpp
ELES_Phase Phase;
FString FlowStatusText;
```
`Phase` is the authority for whether editing or gameplay operations are permitted. Public request methods should check phase before forwarding to a subsystem.

Examples:
```cpp
Request_ConfirmPlacement()
Request_FinishEditing()
```


### Placement contract

1. Only actors contained in `PlacedRecords` are editable.
2. A new placement preview is a spawned but unconfirmed `ALES_PlaceableBase`.
3. Moving an object reuses a placed actor as the preview actor.
4. Preview collision state must be restored before confirming or cancelling.


All catalog placeables should derive from:
```cpp
ALES_PlaceableBase
```

A valid `ALES_PlaceableBase` must provide:
```text
SceneRoot
VisualMesh
LESPlacementBounds
```

### `VisualMesh`
- Must have a valid `UStaticMesh`.
- Must have intentional responses against world/placeable collision channels.

### `LESPlacementBounds`
- Advisory overlap feedback.
- Preview debug rendering.
- Designer-provided footprint visualization.

The current implementation requires it to be centered and unrotated relative to its actor root. That restriction is enforced by `GetLESPlacementData()`.


## Activation contract

`ILES_PlaceableInterface` provides:
```cpp
void SetGameplayActive(bool bActive);
```

The base implementation:
1. Is idempotent.
2. Tracks activation state.
3. Enables/disables optional `VisualMesh` physics.
4. Calls the Blueprint extension point:

```cpp
OnLESGameplayActiveChanged(bool bNowGameplayActive)
```

Do not override `SetGameplayActive` in Blueprint children unless there is a compelling reason; Use `OnLESGameplayActiveChanged` for Blueprint behavior. This preserves base activation semantics.


## Placement validation details

### Candidate transform: `BuildCandidateTransform()`

- Snaps X/Y relative to Session Manager origin.
- Uses current placement plane for Z.
- Rotates according to `PreviewYaw`.
- Computes visual mesh bottom from the static mesh bounds transformed by the mesh’s local transform and preview rotation.
- Applies `FloorClearance`.

The transform assumes:
- Session rotation is identity.
- Session scale is one.
- Placeable actor scale is effectively one.
- Grid snapping is intended only for X/Y.
- Z is controlled through `PlacementPlaneZ`.

### Advisory validation: `ValidatePreviewBounds()`

- Checks `LESPlacementBounds` against the configured editable volume.
- Uses overlap query with a box shape.
- Reports a warning when it detects relevant blocking geometry.

This result determines preview feedback, not final placement authority.

### Authoritative validation: `ValidateMeshPlacement()`

- Uses `ComponentOverlapMulti()` with `VisualMesh`.
- Placement validation source of truth.
- Ignores the SessionManager actor.
- Ignores the candidate actor itself.


## Ownership and lifecycle

### Managed actors

Only records in:
```cpp
TArray<FLES_PlacedRecord> PlacedRecords;
```
are considered session-managed/editable.

This is enforced through:
```cpp
bool IsManagedActor(const AActor* Actor) const;
```

### Destruction handling

Placed actors bind:
```cpp
Preview->OnDestroyed.AddDynamic(
    this,
    &ULES_PlacementComponent::HandlePlacedActorDestroyed);
```

Destruction cleanup removes invalid records and clears selection when necessary. If another system destroys placed actors, their quantity becomes available again because remaining quantity is calculated from valid placement records.


## Physics freeze behavior

`ULES_PhysicsFreezeComponent` captures every currently simulating `UPrimitiveComponent` in the world:
```cpp
Component pointer
Linear velocity
Angular velocity
```
It then disables simulation.

On restoration it:
1. Re-enables simulation.
2. Restores linear velocity.
3. Restores angular velocity.
4. Wakes rigid bodies.

Prototypical; overextended affect. For larger projects, introduce filtering or an opt-in mechanism.


## Objective behavior

`ULES_ObjectiveComponent` monitors:
```cpp
TargetActor
GoalActor
HoldTime
bMonitoring
```

Every tick:
1. It checks validity of target and goal.
2. It tests `Goal->ContainsWorldPoint(Target->GetActorLocation())`.
3. It resets hold time if the target point leaves the goal.
4. It completes when hold time reaches `RequiredHoldSeconds`.

Prototypical; Tick() should be avoidable via delegates or overlap events. The objective uses the target actor’s root location only. It does not test overlap, containment of the full object, or swept movement.

---

## Known Shortcomings

### Limited placement events
Placement events currently are bundled for sound triggering only.

### Overextending physic freezing
FreezeComponent Currently freezing everything with no filter.

### Embedded keybindings
UI and Editor Mode keybindings are hard coded, instead of using IAM and IA like player controls.

### Add configurable placement policy
The current collision policy is hard coded around mutual blocking responses. 

### No save/load
The existing `FLES_PlacedRecord` is transient and stores only actor pointers.
