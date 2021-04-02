/* avatar-build is distributed under MIT license:
 *
 * Copyright (c) 2021 Kota Iguchi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
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
                AVATAR_PIPELINE_LOG("[ERROR] failed to load bone mappings");
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
        pipeline_processor::wire_components();

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
            return;
        }

        outputs.SetValue(0, true); // discarded

        const auto input_size = options->input_override.size();
        const auto output_size = options->output_override.size();

        if (input_size > 0 && input_size == output_size) {
            size_t count = 0;
            for (size_t i = 0; i < options->input_override.size(); ++i) {
                if (ProcessGltf(options->input_override[i], options->output_override[i])) {
                    count++;
                }
            }
            outputs.SetValue(0, count == 0);        
        } else {
            outputs.SetValue(0, !ProcessGltf(options->input, options->output));        
        }
    }

    bool ProcessGltf(std::string input, std::string output)
    {
        AVATAR_PIPELINE_LOG("[INFO] gltf_pipeline start");
        AVATAR_PIPELINE_LOG("[INFO] reading " << input);

        cgltf_data* data = nullptr;
        cgltf_result result = cgltf_parse_file(&options->gltf_options, input.c_str(), &data);

        if (result != cgltf_result_success) {
            AVATAR_PIPELINE_LOG("[ERROR] failed to parse file " << input);
            return false;
        }

        result = cgltf_load_buffers(&options->gltf_options, data, input.c_str());

        if (result != cgltf_result_success) {
            AVATAR_PIPELINE_LOG("[ERROR] failed to load buffers from " << input);
            return false;
        }

        glb_loader->set_data(data);

        bool discarded = false;

        try {
            circuit->Tick(DSPatch::Component::TickMode::Series);
            discarded = tick_result->is_discarded();
        } catch (json::exception e) {
            AVATAR_PIPELINE_LOG("[ERROR] failed to parse JSON: " << e.what());                
            discarded = true;
        } catch (std::exception e) {
            AVATAR_PIPELINE_LOG("[ERROR] faild to process pipeline " << e.what());                
            discarded = true;
        }

        if (!discarded) {
            result = cgltf_validate(data);

            if (result == cgltf_result_success) {
                AVATAR_PIPELINE_LOG("[INFO] writing " << output);
                discarded = !gltf_write_file(&options->gltf_options, data, output);
                if (discarded) {
                    AVATAR_PIPELINE_LOG("[ERROR] faild to write output " << output);                
                }
            } else {
                discarded = true;
                AVATAR_PIPELINE_LOG("[ERROR] Invalid glTF data: " << result);
            }
        }

        if (options->debug) {
            gltf_write_json(&options->gltf_options, data, output + ".json");
        }

        cgltf_free(data);
        glb_loader->set_data(nullptr);
        data = nullptr;

        if (!discarded) {
            AVATAR_PIPELINE_LOG("[INFO] gltf_pipeline finished without errors");

            // redirect gltf pipeline output to next input.
            options->input = options->output;
            options->output = output;
        }

        return !discarded;
    }

    std::shared_ptr<glb_load> glb_loader;
};

} // namespace Avatar