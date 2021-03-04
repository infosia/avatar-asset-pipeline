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
    virtual void Process_(SignalBus const& inputs, SignalBus& outputs) override
    {
        // just return immediately when there's critical error in previous component
        if (state->discarded) {
            return;
        }
        AVATAR_COMPONENT_LOG("[INFO] vrm0_default_extensions");

        const auto data_ptr = inputs.GetValue<cgltf_data*>(0);
        if (data_ptr) {
            cgltf_data* data = *data_ptr;
            (void)data;
            outputs.SetValue(0, data);
        } else {
            state->discarded = true;
        }
    }
    AvatarBuild::circuit_state* state;
};

} // namespace DSPatch