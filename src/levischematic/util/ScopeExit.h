#pragma once
#include <utility>
namespace levischematic::util {

template <class F>
class ScopeExit {
public:
    
    explicit ScopeExit(F action) : mAction(std::move(action)) {}
    
    ~ScopeExit() { mAction(); }
    
    ScopeExit(ScopeExit const&) = delete;
    
    ScopeExit& operator=(ScopeExit const&) = delete;

private:
    F mAction;
};
} 
