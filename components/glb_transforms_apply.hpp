#pragma once

#include <DSPatch.h>
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
    virtual void Process_(SignalBus const&, SignalBus&) override
    {
        // just return immediately when there's critical error in previous component
        if (state->discarded) {
            return;
        }
        AVATAR_COMPONENT_LOG("[INFO] glb_transforms_apply");

        cgltf_data* data = static_cast<cgltf_data*>(state->data);

        gltf_apply_transforms(data);
        gltf_update_inverse_bind_matrices(data);
    }
    AvatarBuild::circuit_state* state;
};

} // namespace DSPatch