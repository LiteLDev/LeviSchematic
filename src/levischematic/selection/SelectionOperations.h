#pragma once
#include "SelectionState.h"

namespace levischematic::selection::tool {

inline bool click(SelectionState& state, BlockPos pos, bool secondary) {
    if (state.selectedElement == Element::None) return false;
    if (state.cornerMode == CornerMode::Corners) {
        if (secondary) state.corner2 = pos;
        else state.corner1 = pos;
    } else if (secondary || !state.corner1 || !state.corner2) {
        state.corner1 = state.corner2 = pos;
    } else {
        Position a{state.corner1->x, state.corner1->y, state.corner1->z};
        Position b{state.corner2->x, state.corner2->y, state.corner2->z};
        expand(a, b, {pos.x, pos.y, pos.z});
        state.corner1 = BlockPos{a[0], a[1], a[2]};
        state.corner2 = BlockPos{b[0], b[1], b[2]};
    }
    return true;
}
inline bool nudge(SelectionState& state, BlockPos delta) {
    auto a = state.corner1, b = state.corner2;
    bool first  = state.selectedElement == Element::Corner1 || state.selectedElement == Element::Box;
    bool second = state.selectedElement == Element::Corner2 || state.selectedElement == Element::Box;
    if ((!first || !a) && (!second || !b)) return false;
    auto move = [&](std::optional<BlockPos>& p) {
        if (!p) return true;
        auto next = offset({p->x, p->y, p->z}, {delta.x, delta.y, delta.z});
        if (!next) return false;
        p = BlockPos{(*next)[0], (*next)[1], (*next)[2]};
        return true;
    };
    if ((first && !move(a)) || (second && !move(b))) return false;
    state.corner1 = a;
    state.corner2 = b;
    return true;
}
} 
