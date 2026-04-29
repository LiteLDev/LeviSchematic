// #include "levischematic/LeviSchematic.h"

// #include "mc/world/level/LevelListener.h"


// namespace levischematic::verifier_block {

// class VerifierBlockTest : public LevelListener {
// public:
//     virtual void onBlockChanged(
//         ::BlockSource&                 source,
//         ::BlockPos const&              pos,
//         uint                           layer,
//         ::Block const&                 block,
//         ::Block const&                 oldBlock,
//         int                            updateFlags,
//         ::ActorBlockSyncMessage const* syncMsg,
//         ::BlockChangedEventTarget      eventTarget,
//         ::Actor*                       blockChangeSource
//     ){
//         // 这里写入验证方块的逻辑
//         // 首先判断变化的方块是否在投影的范围内
//         // 然后再判断方块名对不对，然后是方块方向、状态之类的
//         // 如果是容器类，再对比容器内物品对不对，物品对比名字，数量即可
//         // 验证均通过就取消渲染当前位置的方块投影
//         // 如果是方块名对，但是方向、状态、容器的内容物不对，就给当前方块渲染加上谈黄色
//         // 如果是方块名都不对，当前方块渲染就加上红色
//     }
// };


// } // namespace levischematic::verifier_block
