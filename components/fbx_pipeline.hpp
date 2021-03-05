#pragma once

#include "DSPatch.h"
#include "pipelines.hpp"
#include <string>
#include <iostream>
#include <vector>

namespace AvatarBuild {

class fbx_pipeline final : public pipeline_processor {

public:
    fbx_pipeline(std::string name, cmd_options* options)
        : pipeline_processor(name, options)
    {
        SetInputCount_(1);  // <bool> discarded
        SetOutputCount_(1); // <bool> discarded
    }

    virtual ~fbx_pipeline()
    {

    }

protected:
    virtual void Process_(DSPatch::SignalBus const& inputs, DSPatch::SignalBus& outputs) override
    {
        // just return immediately when there's critical error in previous component
        const auto discarded = inputs.GetValue<bool>(0);
        if (discarded && *discarded) {
            Reset();
            return;
        }

        AVATAR_PIPELINE_LOG("[INFO] fbx_pipeline start");

        circuit->Tick(DSPatch::Component::TickMode::Series);
       
        outputs.SetValue(0, tick_result->is_discarded());

        if (!tick_result->is_discarded()) {
            AVATAR_PIPELINE_LOG("[INFO] fbx_pipeline finished without errors");
            // redirect fbx pipeline output to gltf pipeline input. assuming gltf_pipeline is executed next
           options->input = options->output + ".fbx.glb";
        }
    }
};

} // namespace Avatar