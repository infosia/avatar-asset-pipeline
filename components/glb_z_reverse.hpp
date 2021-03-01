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
    virtual void Process_(SignalBus const&, SignalBus&) override
    {
        // just return immediately when there's critical error in previous component
        if (state->discarded) {
            return;
        }
        AVATAR_COMPONENT_LOG("[INFO] glb_z_reverse");

        cgltf_data* data = static_cast<cgltf_data*>(state->data);
        gltf_reverse_z(data);
        gltf_update_inverse_bind_matrices(data);
    }

    AvatarBuild::circuit_state* state;
};

} // namespace DSPatch