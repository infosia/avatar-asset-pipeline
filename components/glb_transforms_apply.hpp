#pragma once

#include <DSPatch.h>
#include "pipelines.hpp"
#include <iostream>

namespace DSPatch {

class glb_transforms_apply final : public Component {

public:
    glb_transforms_apply(AvatarBuild::circuit_state* state)
        : Component()
        , state(state)

    {
        SetInputCount_(1);
        SetOutputCount_(1);
    }

    virtual ~glb_transforms_apply()
    {
    }

protected:
    virtual void Process_(SignalBus const& inputs, SignalBus& outputs) override
    {
        // just return immediately when there's critical error in previous component
        if (state->discarded) {
            return;
        }
        AVATAR_COMPONENT_LOG("[INFO] glb_transforms_apply");

        const auto data_ptr = inputs.GetValue<cgltf_data*>(0);
        if (data_ptr) {
            cgltf_data* data = *data_ptr;
            gltf_apply_transforms(data);
            gltf_update_inverse_bind_matrices(data);
            outputs.SetValue(0, data);
        } else {
            state->discarded = true;
        }
    }
    AvatarBuild::circuit_state* state;
};

} // namespace DSPatch