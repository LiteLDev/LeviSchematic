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
- 方块实体模型与普通方块共用投影合成；`projection.alpha` 为背景权重（0 不透明，1 完全透明），使用 `/schem config reload` 应用。
- 在放置发生变化后刷新对应区块渲染
- 支持使用全局 Y 范围过滤控制投影显示层
- 根据已加载 placement 生成期望方块集合
- 将投影期望方块与世界中的实际方块进行比较
- 对方块类型不匹配和属性不匹配分别显示 Hull
- 主手持木棍时显示原生字体信息板：哑光绿色文字、半透明黑色背景，包含模式、当前选中投影文件名、选区角点或投影原点、尺寸及变换
- 设置页的“信息叠加 → HUD 布局”或 `/schem ui hud` 可分别调整投影信息、目标方块、材料和校验 HUD。左键按住拖动，大小支持 50%–200%（滑条或数字输入），退出自动保存；可单项或全部恢复默认。
- 未启用或无数据的 HUD 在编辑器中显示示例；正常游戏仍遵循各自显示条件。`hud.panels` 按上述顺序保存安全区域内的归一化位置和 `scalePercent`。旧 `hud.position` 保留为未自定义面板的左右侧默认布局，旧配置自动补齐四个面板。
- 中键取物与辅助放置的全部操作反馈仅在 `diagnostics=true` 时显示。
- 菜单、聊天界面或隐藏 HUD 时不显示信息板；长文件名自动省略

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
| `/schem load <filename> <x> <y> <z>` | 在指定坐标加载结构。 | 立即返回排队任务编号；前台完成后反馈 placement ID。 |

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
| `/schem clear` | 清空全部 placement。 | 同时取消未完成加载，防止旧任务在清空后重新创建 placement。 |

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

### 视图层范围指令

| 指令 | 作用 | 使用说明 |
|------|------|------|
| `/schem view y min <value>` | 设置最小可见 Y 值并启用过滤。 | 使用世界坐标 Y。低于该值的投影层会被隐藏。 |
| `/schem view y max <value>` | 设置最大可见 Y 值并启用过滤。 | 使用世界坐标 Y。高于该值的投影层会被隐藏。 |
| `/schem view y range <min> <max>` | 设置完整可见 Y 范围并启用过滤。 | 可见范围是闭区间：`min <= y <= max`。 |
| `/schem view y enable` | 重新启用当前保存的 Y 范围过滤。 | 会继续使用之前保存的 `min` 和 `max`。 |
| `/schem view y disable` | 关闭 Y 范围过滤。 | 不会清除当前保存的范围值。 |
| `/schem view y info` | 查看当前 Y 范围过滤状态。 | 会显示过滤是否启用，以及当前的 `min` 和 `max`。 |

示例：

```text
/schem view y range 64 96
/schem view y min 80
/schem view y max 120
/schem view y info
/schem view y disable
```

### 选区指令

| 指令 | 作用 | 使用说明 |
|------|------|------|
| `/schem pos1` | 将选区角点 1 设为玩家当前位置。 | 适合快速记录当前位置。 |
| `/schem pos1 <x> <y> <z>` | 将选区角点 1 设为指定坐标。 | 适合精确框选。 |
| `/schem pos2` | 将选区角点 2 设为玩家当前位置。 | 与 `pos1` 对应，用于设置第二个角点。 |
| `/schem pos2 <x> <y> <z>` | 将选区角点 2 设为指定坐标。 | 两个角点都设置后即可形成完整选区。 |
| `/schem save <filename>` | 排队渐进采集选区并写入新的 `.mcstructure`。 | 必须设置两角点；已有文件需在保存页面确认覆盖。 |
| `/schem save-status` / `/schem save-cancel` | 查询进度或取消任务。 | 缺失区块明确报错；采集跨越多个 tick。 |
| `/schem ui <save\|layers\|verifier\|session>` | 打开保存、分层、校验或会话页面。 | 主菜单也有对应入口。 |
| `/schem verify <start\|layers\|stop\|reset\|info>` | 控制选中放置的独立校验任务。 | `layers` 校验渲染范围；隐藏投影不停止任务。 |
| `/schem layer mode <all\|single\|range\|below\|above>` | 设置分层模式。 | 另有 `layer axis <x\|y\|z>`、`layer boundary <整数>` 和 `layer info`。 |
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

### 预览渲染器

| 指令 | 作用 | 说明 |
|------|------|------|
| `/schem renderer stats` | 查看预览缓存、几何与方块实体合成统计。 | 包含各 placement 的区块、图层提交、累计区块提交覆盖、构建失败状态、方块实体分发、Highlighted 记录、材质变体次数及跳过原因；提交次数不表示屏幕可见数量。`diagnostics=true` 时另写入模组数据目录 `diagnostics/renderer-stats.json`，含预期及已提交区块的局部起点。 |
| `/schem renderer reset` | 释放预览运行时资源。 | 保留 placement、变换和覆写，下个有效渲染帧重建。 |

切换维度时释放预览缓存并保留 placement；退出世界时清空 placement 和选区，重进后开始新会话。保留实际方块实体显示链；实体遵循校验和 Y 范围过滤，CPU 网格任务独占假世界期间暂不分发实体绘制。

## 说明

- 项目当前只围绕 `.mcstructure` 文件工作。
- 运行时会使用 `schematics` 目录来加载和保存结构文件；没有可用的本地世界路径时，结构文件操作保持不可用，等待后续取得世界路径。
- Y 范围过滤是当前投影视图的全局设置，不是单个 placement 的独立设置。
- 很多指令依赖当前已选中的 placement。如果没有选中项，请先使用 `/schem load` 或 `/schem select <id>`。

## 渐进加载与动态配置

`/schem load` 立即返回排队任务编号。一个后台线程负责文件读取和 NBT 数据解析，默认最多四个未完成请求；客户端 tick 分批解析调色板并准备资产。完成消息给出 placement 编号，随后校验、假世界和网格逐步准备。任务完成不等于全部区块已显示。取消已经发布的任务不会删除 placement，删除请使用 `remove`。

| 指令 | 作用 |
|---|---|
| `/schem load status <taskId>` | 查询阶段、已处理方块索引数量以及完成/错误详情。 |
| `/schem load cancel <taskId>` | 取消当前会话尚未完成的任务。 |
| `/schem config reload` | 校验 `config.json`，下一客户端 tick 应用。 |
| `/schem config save` | 保存最近提交或已应用的有效配置。 |

`config.json` 位于 LeviLamina 提供的模组配置目录。`app::ConfigStore` 提供 `snapshot()`、`submit(value)`、`reload()`、`save()`；外部线程提交值对象，消费者只拿快照。整份配置校验失败时保留上一次有效配置。投影纹理改变只重置纹理，Hull 样式改变只重置 Hull 资源，预算用于后续处理。

`visibility.renderDistanceBlocks` 默认 **128 格**，普通投影、验证 Hull 和投影方块实体共用；按渲染相机到包围盒最近点的三维距离判断，而非 placement 原点距离，`0` 表示不限距离。`visibility.frustumCulling` 默认 `true`，普通投影和 Hull 使用保守的子区块视锥裁剪；方块实体只独立做距离过滤，不增加真实地形遮挡剔除。旧版 version-1 配置自动补齐新字段的默认值，不改变其他配置。修改后执行 `/schem config reload`；距离变化不销毁几何缓存，也不停止验证服务。

```powershell
xmake f -a x64 -m release -p windows --target_type=client
xmake
powershell -ExecutionPolicy Bypass -File src-test/run-tests.ps1
```

## 客户端 UI

世界内松开 **M**、按 **F7** 或执行 `/schem ui` 打开 Litematica 风格主菜单。菜单由原生场景承载，使用 C++ 绘制并处理输入；安装时复制完整 `bin/LeviSchematic`，包括 `assets/ui` 中的 Litematica、MaLiLib 图集和 JE 26.3 按钮图片。

加载结构、已加载资源、放置实例使用独立列表。配置支持分类标签、搜索与逐项重置；合法修改在客户端 tick 生效，离开配置页或关闭菜单时保存，非法输入和保存失败保留页面供修正。Esc 优先取消捕获或确认弹窗，再返回父页。HUD 布局编辑器可分别配置四个模组信息板。按键覆盖仍保存于模组配置目录的 `ui-keybindings.json`；结构格式仍为 `.mcstructure`。

界面通过 LeviLamina i18n API 跟随打开时的游戏语言，内置简体中文和英文。首次打开会在模组配置目录生成 `i18n/ui/zh_CN.json` 和 `en_US.json`；可修改文本或添加其他语言的 JSON，关闭后重新打开生效。已有翻译文件不会被覆盖，缺失条目使用内置翻译或英文回退。

界面未出现时使用 `/schem ui status` 查看 C++ 后端与生命周期状态。`/schem ui layout` 将最近完成帧导出至模组数据目录的 `diagnostics/ui-layout.json`，包含视口、缩放、控件矩形、裁剪、文字边界与资源 UV。

