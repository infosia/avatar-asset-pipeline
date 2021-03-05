#pragma once

#include <DSPatch.h>
#include "pipelines.hpp"
#include <iostream>

namespace DSPatch {

class glb_transforms_apply final : public Component {

public:
    glb_transforms_apply(AvatarBuild::cmd_options* options)
        : Component()
        , options(options)

    {
        SetInputCount_(2);
        SetOutputCount_(2);
    }

    virtual ~glb_transforms_apply()
    {
    }

protected:
    virtual void Process_(SignalBus const& inputs, SignalBus& outputs) override
    {
        // just return immediately when there's critical error in previous component
        const auto discarded = inputs.GetValue<bool>(0);
        if (discarded && *discarded) {
            Reset();
            return;
        }
        AVATAR_COMPONENT_LOG("[INFO] glb_transforms_apply");

        const auto data_ptr = inputs.GetValue<cgltf_data*>(1);
        if (data_ptr) {
            cgltf_data* data = *data_ptr;
            gltf_apply_transforms(data);
            gltf_update_inverse_bind_matrices(data);
            outputs.SetValue(0, false);    // discarded
            outputs.SetValue(1, data);
        } else {
            AVATAR_COMPONENT_LOG("[ERROR] glb_transforms_apply: input.1 not found");
            outputs.SetValue(0, true);    // discarded
        }
    }
    AvatarBuild::cmd_options* options;
};

} // namespace DSPatch