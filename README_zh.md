# LeviShematic

[English](README.md)

## 项目介绍

LeviShematic 是一个面向 Bedrock 客户端的结构投影模组，核心围绕 `.mcstructure` 文件工作。
它支持将已保存的结构加载为世界中的半透明投影，管理多个放置实例，调整位置与变换，框选区域并重新导出为 `.mcstructure`，同时还能把投影期望方块与当前世界中的实际方块进行对照校验。

## 功能

### 结构加载与放置管理

- 通过 `/schem load` 加载 `.mcstructure` 结构文件
- 通过 `/schem files` 列出可用结构文件
- 支持同时加载多个 placement
- 支持选择、删除、禁用、隐藏、查看和清空 placement

### 放置编辑

- 支持按相对偏移移动当前 placement
- 支持直接设置新的放置原点
- 支持按 `cw90`、`ccw90`、`r180` 旋转
- 支持沿 `x` 或 `z` 轴镜像，也支持取消镜像
- 支持将变换重置为默认状态
- 支持对单个投影方块执行隐藏、替换、清除覆写

### 投影渲染与校验

- 以半透明虚影方块的形式在世界中渲染投影
- 在放置发生变化后刷新对应区块渲染
- 根据已加载 placement 生成期望方块集合
- 将投影期望方块与世界中的实际方块进行比较
- 对方块类型不匹配和属性不匹配使用不同颜色进行提示

### 选区与导出

- 使用 `pos1` 和 `pos2` 记录两个选区角点
- 在选区完整后显示线框包围盒
- 使用 `/schem save` 将选区保存为 `.mcstructure`
- 支持在游戏内切换 selection mode

## 指令使用说明

所有指令都以 `/schem` 为前缀。

### 通用指令

| 指令 | 作用 | 使用说明 |
|------|------|------|
| `/schem` | 显示内置帮助信息。 | 需要快速查看可用子命令时使用。 |
| `/schem files` | 列出结构目录中的全部 `.mcstructure` 文件。 | 不确定文件名时建议先执行这个命令。 |

### 加载指令

| 指令 | 作用 | 使用说明 |
|------|------|------|
| `/schem load <filename>` | 在玩家当前位置加载结构。 | `filename` 可以带 `.mcstructure` 后缀，也可以不带。 |
| `/schem load <filename> <x> <y> <z>` | 在指定坐标加载结构。 | 会创建一个新的 placement，并返回对应 ID。 |

示例：

```text
/schem load house
/schem load tower 100 64 -32
```

### 放置管理指令

| 指令 | 作用 | 使用说明 |
|------|------|------|
| `/schem list` | 列出当前已加载的所有 placement。 | 会显示 ID、名称、原点、变换状态、启用状态和渲染状态。 |
| `/schem select <id>` | 选择一个 placement 作为当前操作对象。 | 大多数变换和单方块编辑命令都依赖当前选中项。 |
| `/schem remove` | 删除当前选中的 placement。 | 适合先 `/schem select` 再删除。 |
| `/schem remove <id>` | 按 ID 删除指定 placement。 | 已知目标 ID 时更直接。 |
| `/schem toggle` | 切换当前 placement 的启用状态。 | 被禁用的 placement 不再按正常投影逻辑参与处理。 |
| `/schem toggle render` | 切换当前 placement 的渲染显示状态。 | 仅隐藏或显示投影，不会删除 placement。 |
| `/schem info` | 查看当前 placement 的详细信息。 | 会显示原点、变换、渲染状态、非空气方块数和包围盒。 |
| `/schem clear` | 清空全部 placement。 | 一次性移除当前全部已加载结构。 |

示例：

```text
/schem list
/schem select 2
/schem toggle render
/schem info
```

### 变换指令

| 指令 | 作用 | 使用说明 |
|------|------|------|
| `/schem move <dx> <dy> <dz>` | 按相对偏移移动当前 placement。 | 支持正负偏移值。 |
| `/schem origin <x> <y> <z>` | 将当前 placement 原点设置到指定坐标。 | 适合做绝对位置调整。 |
| `/schem rotate <cw90\|ccw90\|r180>` | 旋转当前 placement。 | 可用方向只有 `cw90`、`ccw90`、`r180`。 |
| `/schem mirror <x\|z\|none>` | 沿指定轴镜像当前 placement。 | `none` 用于取消镜像。 |
| `/schem reset` | 重置当前 placement 的旋转和镜像状态。 | 会恢复到默认变换状态。 |

示例：

```text
/schem move 5 0 -3
/schem origin 200 70 200
/schem rotate cw90
/schem mirror z
/schem reset
```

### 单方块覆写指令

| 指令 | 作用 | 使用说明 |
|------|------|------|
| `/schem block hide <x> <y> <z>` | 隐藏指定世界坐标处的投影方块。 | 适合去掉当前 placement 中个别不想显示的投影方块。 |
| `/schem block set <x> <y> <z> <block>` | 将指定位置的投影方块替换为另一个方块。 | 用于对单个投影方块进行局部修改。 |
| `/schem block clear <x> <y> <z>` | 清除之前设置的单方块覆写。 | 会恢复该位置原本的投影表现。 |

示例：

```text
/schem block hide 120 64 90
/schem block set 120 64 90 minecraft:glass
/schem block clear 120 64 90
```

### 选区指令

| 指令 | 作用 | 使用说明 |
|------|------|------|
| `/schem pos1` | 将选区角点 1 设为玩家当前位置。 | 适合快速记录当前位置。 |
| `/schem pos1 <x> <y> <z>` | 将选区角点 1 设为指定坐标。 | 适合精确框选。 |
| `/schem pos2` | 将选区角点 2 设为玩家当前位置。 | 与 `pos1` 对应，用于设置第二个角点。 |
| `/schem pos2 <x> <y> <z>` | 将选区角点 2 设为指定坐标。 | 两个角点都设置后即可形成完整选区。 |
| `/schem save <filename>` | 将当前选区保存为 `.mcstructure` 文件。 | 执行前必须先设置 `pos1` 和 `pos2`。 |
| `/schem selection clear` | 清空当前选区。 | 会移除两个角点记录。 |
| `/schem selection mode` | 开关 selection mode。 | 打开后也可以配合木棍直接在世界里点选角点。 |
| `/schem selection info` | 显示当前选区状态信息。 | 选区完整时会显示尺寸、最小角点和总体积。 |

示例：

```text
/schem pos1
/schem pos2 150 72 150
/schem selection info
/schem save room_copy
```

### Selection Mode 交互说明

源码中还支持在开启 selection mode 后，使用木棍直接在世界中设置选区角点：

- 手持木棍左键方块，设置 `pos1`
- 手持木棍右键方块，设置 `pos2`

## 说明

- 项目当前只围绕 `.mcstructure` 文件工作。
- 运行时会使用 `schematics` 目录来加载和保存结构文件。
- 很多指令依赖当前已选中的 placement。如果没有选中项，请先使用 `/schem load` 或 `/schem select <id>`。
