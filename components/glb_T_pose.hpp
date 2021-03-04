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
    virtual void Process_(SignalBus const&, SignalBus&) override
    {
        // just return immediately when there's critical error in previous component
        if (state->discarded) {
            return;
        }
        AVATAR_COMPONENT_LOG("[INFO] glb_T_pose");

        cgltf_data* data = static_cast<cgltf_data*>(state->data);

        // TODO rotate bones

        gltf_skinning(data);
    }

    AvatarBuild::circuit_state* state;
};

} // namespace DSPatch