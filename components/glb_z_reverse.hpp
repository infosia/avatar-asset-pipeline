#pragma once

#include "DSPatch.h"
#include "pipelines.hpp"
#include <iostream>

namespace DSPatch {

class glb_z_reverse final : public Component {

public:
    glb_z_reverse(AvatarBuild::circuit_state* state)
        : Component()
        , state(state)
    {
        SetInputCount_(1);
        SetOutputCount_(1);
    }

    virtual ~glb_z_reverse()
    {
    }

protected:
    virtual void Process_(SignalBus const& inputs, SignalBus& outputs) override
    {
        // just return immediately when there's critical error in previous component
        if (state->discarded) {
            return;
        }
        AVATAR_COMPONENT_LOG("[INFO] glb_z_reverse");

        const auto data_ptr = inputs.GetValue<cgltf_data*>(0);

        if (data_ptr) {
            cgltf_data* data = *data_ptr;
            gltf_reverse_z(data);
            gltf_update_inverse_bind_matrices(data);
            outputs.SetValue(0, data);
        } else {
            state->discarded = true;
        }
    }

    AvatarBuild::circuit_state* state;
};

} // namespace DSPatch