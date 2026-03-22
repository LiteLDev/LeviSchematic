#pragma once
// ============================================================
// SelectionHook.h
// 选区相关 Hook 声明（Phase 4）
//
// Hook 1: LevelRendererCamera::renderStructureWireframes
//   → 渲染选区线框（包围盒 + 角点高亮）
//
// Hook 2: GameMode::_startDestroyBlock
//   → 左键设置选区角点1（手持木棍时）
//
// Hook 3: GameMode::_sendUseItemOnEvents
//   → 右键设置选区角点2（手持木棍时）
//
// 这些 Hook 通过 LL_AUTO_TYPE_INSTANCE_HOOK 自动注册，
// 包含此头文件的 .cpp 即可激活。
// ============================================================

namespace levishematic::hook {

// Hook 在 SelectionHook.cpp 中通过 LL_AUTO_TYPE_INSTANCE_HOOK 宏自动注册。
// 此头文件仅作为文档和模块入口声明。

} // namespace levishematic::hook
