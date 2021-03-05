#pragma once

#include <iostream>
#include "DSPatch.h"
#include "pipelines.hpp"

namespace DSPatch {

class noop final : public Component {

public:
    noop(AvatarBuild::cmd_options* options, std::string name)
        : Component()
        , options(options)
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

    AvatarBuild::cmd_options* options;
    std::string name;
};

} // namespace DSPatch