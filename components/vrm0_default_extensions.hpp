#pragma once

#include <DSPatch.h>
#include <iostream>

namespace DSPatch {

class vrm0_default_extensions final : public Component {

public:
    vrm0_default_extensions(AvatarBuild::cmd_options* options)
        : Component()
        , options(options)
    {
        SetInputCount_(2);
        SetOutputCount_(2);
    }

    virtual ~vrm0_default_extensions()
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
        AVATAR_COMPONENT_LOG("[INFO] vrm0_default_extensions");

        const auto data_ptr = inputs.GetValue<cgltf_data*>(1);
        if (data_ptr) {
            cgltf_data* data = *data_ptr;
            outputs.SetValue(0, false);    // discarded
            outputs.SetValue(1, data);
        } else {
            AVATAR_COMPONENT_LOG("[ERROR] vrm0_default_extensions: input.1 not found");
            outputs.SetValue(0, true);    // discarded
        }
    }
    AvatarBuild::cmd_options* options;
};

} // namespace DSPatch