#pragma once

#include "DSPatch.h"
#include <string>
#include <iostream>
#include <vector>

#include "pipelines.hpp"

namespace AvatarBuild {

// A component that just pass cgltf_data to signal bus
class glb_load final : public DSPatch::Component {
public:
    glb_load(cmd_options* options)
        : DSPatch::Component()
        , data(nullptr)
        , options(options)
    {
        SetInputCount_(0);
        SetOutputCount_(4);
    }
    void set_data(cgltf_data* _data)
    {
        data = _data;

        if (data != nullptr) {
            if (!gltf_parse_bone_mappings(data, &bone_mappings, options)) {
                AVATAR_COMPONENT_LOG("[ERROR] failed to load bone mappings");
            }
        }
    }
protected:
    virtual void Process_(DSPatch::SignalBus const&, DSPatch::SignalBus& outputs) override
    {
        if (data == nullptr) {
            return;
        }
        outputs.SetValue(0, false);    // <bool>  discarded
        outputs.SetValue(1, data);     // cgltf_data*
        outputs.SetValue(2, &bone_mappings);  // bone_mappings*
        outputs.SetValue(3, nullptr);  // data3
    }
    cgltf_data* data;
    cmd_options* options;
    bone_mappings bone_mappings;
};

class gltf_pipeline final : public pipeline_processor {

public:
    gltf_pipeline(std::string name, cmd_options* options)
        : pipeline_processor(name, options)
    {
        SetInputCount_(1);
        SetOutputCount_(1);
    }

    virtual ~gltf_pipeline()
    {
    }

    virtual void wire_components() override
    {
        glb_loader = std::make_shared<glb_load>(options);
        circuit->AddComponent(glb_loader);

        const auto front = components.front();
        circuit->ConnectOutToIn(glb_loader, 0, front, 0);
        circuit->ConnectOutToIn(glb_loader, 1, front, 1);
        circuit->ConnectOutToIn(glb_loader, 2, front, 2);
        circuit->ConnectOutToIn(glb_loader, 3, front, 3);
    }

protected:
    virtual void Process_(DSPatch::SignalBus const& inputs, DSPatch::SignalBus& outputs) override
    {
        // just return immediately when there's critical error in previous component
        const auto inputs0 = inputs.GetValue<bool>(0);
        if (inputs0 && *inputs0) {
            Reset();
            return;
        }

        AVATAR_PIPELINE_LOG("[INFO] gltf_pipeline start");

        const std::string input = options->input;

        cgltf_data* data = nullptr;
        cgltf_options gltf_options = {};
        cgltf_result result = cgltf_parse_file(&gltf_options, input.c_str(), &data);

        if (result != cgltf_result_success) {
            AVATAR_PIPELINE_LOG("[ERROR] failed to parse file " << input);
            return;
        }

        result = cgltf_load_buffers(&gltf_options, data, input.c_str());

        if (result != cgltf_result_success) {
            AVATAR_PIPELINE_LOG("[ERROR] failed to load buffers from " << input);
            return;
        }

        glb_loader->set_data(data);

        circuit->Tick(DSPatch::Component::TickMode::Series);
       
        auto discarded = tick_result->is_discarded();
        outputs.SetValue(0, discarded);

        if (!tick_result->is_discarded()) {
            result = cgltf_validate(data);

            if (result == cgltf_result_success) {
                discarded = !gltf_write_file(data, options->output);
                if (discarded) {
                    AVATAR_PIPELINE_LOG("[ERROR] faild to write output " << options->output);                
                }
            } else {
                discarded = true;
                AVATAR_PIPELINE_LOG("[WARN] Invalid glTF data: " << result);
            }
        }

        cgltf_free(data);
        glb_loader->set_data(nullptr);
        data = nullptr;

        if (!discarded) {
            AVATAR_PIPELINE_LOG("[INFO] gltf_pipeline finished without errors");
        }
    }

    std::shared_ptr<glb_load> glb_loader;
};

} // namespace Avatar