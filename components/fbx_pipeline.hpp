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

    }

    virtual ~fbx_pipeline()
    {

    }

protected:
    virtual void Process_(DSPatch::SignalBus const&, DSPatch::SignalBus&) override
    {
        AVATAR_PIPELINE_LOG("[INFO] fbx_pipeline start");

        circuit->Tick(DSPatch::Component::TickMode::Series);
       
        if (!state.discarded) {
            AVATAR_PIPELINE_LOG("[INFO] fbx_pipeline finished without errors");
        }
    }

};

} // namespace Avatar