#pragma once

#include <DSPatch.h>
#include <iostream>

namespace DSPatch {

class vrm0_default_extensions final : public Component {

public:
    vrm0_default_extensions(AvatarBuild::circuit_state* state)
        : Component()
        , state(state)
    {
        SetInputCount_(1);
        SetOutputCount_(1);
    }

    virtual ~vrm0_default_extensions()
    {
    }

protected:
    virtual void Process_(SignalBus const&, SignalBus&) override
    {
        // just return immediately when there's critical error in previous component
        if (state->discarded) {
            return;
        }
        AVATAR_COMPONENT_LOG("[INFO] vrm0_default_extensions");

        cgltf_data* data = static_cast<cgltf_data*>(state->data);
        (void)data;
    }
    AvatarBuild::circuit_state* state;
};

} // namespace DSPatch