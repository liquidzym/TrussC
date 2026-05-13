#pragma once
// =============================================================================
// tcxForce.h — Abstract force interface
// =============================================================================

namespace tcx {

struct Force {
    virtual ~Force() = default;
    virtual void turnOn()  = 0;
    virtual void turnOff() = 0;
    virtual bool isOn() const  = 0;
    virtual bool isOff() const = 0;
    virtual void apply() = 0;
};

} // namespace tcx
