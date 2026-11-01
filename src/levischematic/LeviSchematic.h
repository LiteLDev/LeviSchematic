#pragma once

#include "ll/api/mod/NativeMod.h"

namespace levischematic {


class LeviSchematic {

public:
    static LeviSchematic& getInstance();


    LeviSchematic() : mSelf(*ll::mod::NativeMod::current()) {}


    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }


    bool load();


    bool enable();


    bool disable();


private:
    ll::mod::NativeMod& mSelf;
};

} 
