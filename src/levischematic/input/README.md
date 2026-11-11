# P0 输入与工具模块

当前 SDK 为 LeviLamina 26.60.0 客户端版，MCP 数据库为 Minecraft 1.26.60.5。复用 SDK 的 KeyRegistry、KeyHandle、KeyInputEvent、MouseInputEvent，不另加原版输入映射 Hook。

## 默认操作

| 操作 | 输入 |
| --- | --- |
| 主菜单、设置、放置列表 | M 松开、M+C、M+P |
| 当前放置配置、基础选区编辑器 | 小键盘减号、小键盘乘号 |
| 工具开关 | M+T，或主菜单按钮 |
| 切换选区/放置工具 | 持木棍 Ctrl+滚轮，或主菜单按钮 |
| 切换 Corners/Expand | Ctrl+M，选区工具有效 |
| 选点 | Corners 左/右键设置两角点；潜行取命中面相邻格 |
| 扩展选区 | Expand 右键重置为单格，左键扩展到包含命中格 |
| 选择角点/盒子/放置 | 持木棍中键，未命中清除选择 |
| 微调选中元素 | 持木棍 Alt+滚轮，沿最近六轴视线方向逐格移动 |
| 点击移动放置 | 持木棍左/右键，默认移到命中面的相邻格，潜行移到命中格 |

P1 增加 Ctrl+Alt+S 保存选区、M+V 校验器、M+PageUp/PageDown 切换分层模式。PageUp/PageDown 按当前三轴分层模式工作；范围模式默认移动离玩家较近的一端，可在分层页指定端点或同时移动两端。“设为玩家所在层”动作默认未绑定，可在按键页设置。F7 不再默认绑定；旧 `ui-keybindings.json` 中显式 F7 改绑保留。按键页按住组合后松开保存，支持鼠标、滚轮、清除、恢复默认，Esc 取消。绑定至组合键的普通快捷键关闭 SDK 单键映射；M+C 等组合后松开 M 不再打开主菜单。

## 复用入口

- `Actions.h` 是唯一动作目录；新增动作同时定义默认键位、触发阶段、工具条件与 SDK 回调策略。
- `BindingRules.h` 提供修饰键归一化、绑定校验和冲突比较；`ChordState.h` 接受值输入，返回动作与消费结果，不持有游戏对象。
- `InputHandler` 监听 SDK 事件并同步决定是否 cancel；事件中只排入动作与会话代次，客户端 tick 再执行。不要在新模块重复注册相同动作回调。
- `app/ToolService` 封装模式、点击、选择和移动。菜单与其他模块通过会话中的 `tools()` 访问；放置写入继续经 PlacementService / PlacementStore，输入入口负责统一投影刷新。
- `selection/ToolGeometry.h` 提供六轴、射线盒和溢出安全坐标增量；`SelectionOperations.h` 提供状态值上的设点、Expand 与原子移动。业务层调用后维护 selection revision。

负键码 -1/-2/-3 表示 SDK 鼠标按钮 ID 的相反数；WheelUp/WheelDown 是模块内滚轮标识。旧 Minecraft 鼠标映射通过 SDK `_rawKeyToKey` 转换。多个 keyCodes 在 SDK 中表示独立绑定，不作为组合键数组提交给原版映射。

场景、文本框和失焦会禁止工具动作；消费过的按下与松开配对。已经透传给原版的修饰键，其松开必须继续透传，即使该次松开触发了自定义 release 组合。切维度与退出世界清除队列，队列代次也会拒绝旧会话动作。监听器配对注册、注销。

## 核验与验证

本批 `src-test/run-tests.ps1` 的九个目标通过，最终 `xmake build LeviSchematic` 完整 DLL 编译链接通过；代码差异检查通过。新增 ToolInputTests 使用上述生产算法，覆盖组合释放顺序、重复、失焦、滚轮优先级、绑定分流、F7 覆写、六轴、负坐标、跨区块、选区操作与溢出回滚。UI/HUD/PlacementStore 目标另覆盖内容路由、HUD 工具信息和清除放置选择的 revision。

SDK 与 MCP 核对了 Actor::getViewVector、BlockSource::clip 和 VanillaBlockStateTransformUtils::transformBlock。方块方向状态中多处先 `_mirror` 后 `_rotate`，项目假世界和校验器复用同一 API；既有变换算法覆盖全部旋转镜像组合，保留现有支点。

2026-11-04 已按仓库 `AUTOMATED_TESTING.md` 使用 BeDD 0.3.1，在独占的 Minecraft 1.26.60.05 / LeviLamina 26.60.0 克隆实例中加载新 DLL 实测。M 与组合释放顺序、小键盘入口、工具开关及滚轮消费、Corners/Expand、潜行取格、中键选择/清除、六轴选区微移和放置点击移动已取得游戏证据。射线查询最初因空 predicate 抛出 `std::bad_function_call`，补入有效回调后完整 DLL 与九组测试重跑通过，点击路径复测正常。

实机验收仍有失败：按键页录制 F7 / Ctrl+K 后松开仍停留在录制状态，不能据算法测试认定录制和持久化通过；加载任务状态命令另返回语法错误。旋转镜像测试中出现原点反馈不稳定，完整方向显示验收尚未通过。详细操作、截图、状态 JSON、失败与未覆盖项见 `artifacts/e2e/20261104-173132-p0/REPORT.md`，不得将本次结果表述为全部自动化测试通过。D03 世界坐标覆写问题留待 P3。

详细 SDK/MCP 取证与实机清单记录在工作区 `docs/P0操作基础与变换核验_2026-11-04.md`；该目录被当前 `.gitignore` 忽略。这里保留随源码交付的操作、模块和验证说明。

## P2 生存辅助入口

M+L 打开材料页，按住 I 显示方块信息，中键投影取物，M+中键开关取物。工具中键选择优先于取物；取物未启用或没有投影目标时保留原版中键。Easy Place 默认关闭，启用后使用键执行，可改绑；释放事件跟随对应按键，不独立改绑。放置限制和 Easy Place 的切换动作默认未绑定。

输入仅路由到会话的 `SurvivalService`；材料统计、投影查询、物品换算和背包事务有独立复用入口。辅助放置不跨 tick 借用玩家/物品引用，失焦、菜单、维度变化和资源失效会取消活动操作。
