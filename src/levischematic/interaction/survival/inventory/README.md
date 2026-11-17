# 生存辅助公共模块

BlockItems 提供取物物品、施工需求及不可解析原因；MaterialRules 为纯值规则。InventoryAccess 提供背包计数、客户端选槽、平衡事务交换和创造获取，调用方必须处于有效会话及客户端 tick。

坐标、变换与覆写统一使用 schematic/placement/query/ProjectionQuery；纯逆变换位于 schematic/placement/model/ProjectionAlgorithms，体素射线位于 shared/geometry/VoxelRay。菜单和输入通过 interaction/survival/service 的 MaterialService、SurvivalService 调用本模块。

当前构建 SDK 为 LeviLamina 26.60.4；以下历史取证使用 26.60.0 SDK，MCP 核验 Minecraft 1.26.60.5。原版 LocalPlayer::pickBlock 读取真实世界，FillingContainer::swapSlots 只改本地，Player::setSelectedSlot 仅服务端生效，不能直接当作投影取物、联网交换或客户端选槽。

创造事务按 MCP 核验的 `_createServerSideAction` 语义使用公共 `InventoryAction`：槽 0 丢弃旧物品，槽 1 创建新物品；该内部 helper 未导出，不直接调用。放置限制在 SDK `GameMode::$buildBlock` Hook 中先计算原版落点，再调用共享服务检查，早于原版事务提交；SDK 权限事件单独取消不足以阻止客户端提交，仍保留作后续校验。非潜行的箱子、门等交互保留原版行为。

材料当前统计主层；副层结构会提示。花盆/炼药锅内容与缺失床颜色 NBT 报告未解析。Easy Place 无法预测准确状态时跳过。算法、DLL、实机和远程服务器验证分别报告。完整记录见 docs/P2生存辅助实现与核验_2026-11-05.md。
