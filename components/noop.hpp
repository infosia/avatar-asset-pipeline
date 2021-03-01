#pragma once

#include <iostream>
#include "DSPatch.h"
#include "pipelines.hpp"

namespace DSPatch {

class noop final : public Component {

public:
    noop(AvatarBuild::circuit_state* state, std::string name)
        : Component()
        , state(state)
        , name(name)
    {

    }

    virtual ~noop()
    {    
    
    }

protected:
    virtual void Process_(SignalBus const&, SignalBus&) override
    {
        AVATAR_COMPONENT_LOG("[WARN] No Component is found for '" << name << "'");    
    }

    AvatarBuild::circuit_state* state;
    std::string name;
};

} // namespace DSPatch