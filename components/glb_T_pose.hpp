#pragma once

#include "DSPatch.h"
#include "pipelines.hpp"
#include <iostream>

namespace DSPatch {

class glb_T_pose final : public Component {

public:
    glb_T_pose(AvatarBuild::circuit_state* state)
        : Component()
        , state(state)
    {
        SetInputCount_(1);
        SetOutputCount_(1);
    }

    virtual ~glb_T_pose()
    {
    }

protected:
    virtual void Process_(SignalBus const& inputs, SignalBus& outputs) override
    {
        // just return immediately when there's critical error in previous component
        if (state->discarded) {
            return;
        }
        AVATAR_COMPONENT_LOG("[INFO] glb_T_pose");

        const auto data_ptr = inputs.GetValue<cgltf_data*>(0);

        if (data_ptr) {
            cgltf_data* data = *data_ptr;
            // TODO rotate bones
            gltf_skinning(data);

            outputs.SetValue(0, data);
        } else {
            state->discarded = true;
        }
    }

    AvatarBuild::circuit_state* state;
};

} // namespace DSPatch