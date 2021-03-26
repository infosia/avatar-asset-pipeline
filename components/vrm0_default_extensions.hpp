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

#include <DSPatch.h>
#include <iostream>

namespace DSPatch {

/// <summary>
/// vrm0_default_extensions is a component that fills in default properties in order to satisfy VRM 0.0 spec.
/// Use --output_config [file] option to specify output configuration file
/// </summary>
class vrm0_default_extensions final : public Component {

public:
    vrm0_default_extensions(AvatarBuild::cmd_options* options)
        : Component()
        , options(options)
    {
        SetInputCount_(3);
        SetOutputCount_(3);
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
            return;
        }

        AVATAR_PIPELINE_LOG("[INFO] vrm0_default_extensions");

        const auto data_ptr = inputs.GetValue<cgltf_data*>(1);
        const auto bones_ptr = inputs.GetValue<AvatarBuild::bone_mappings*>(2);

        if (data_ptr && bones_ptr) {
            cgltf_data* data = *data_ptr;
            AvatarBuild::bone_mappings* mappings = *bones_ptr;

            json vrm0_config = options->output_config_json;
            if (vrm0_config["defaults"].is_object()) {
                auto vrm0_defaults = vrm0_config["defaults"];

                vrm0_update_bones(mappings, data);
                vrm0_update_meta(vrm0_defaults["meta"], &data->vrm_v0_0);
                vrm0_ensure_defaults(vrm0_defaults, data);

                const auto validate_result = vrm0_validate(data);
                if (validate_result != cgltf_result_success) {
                    AVATAR_PIPELINE_LOG("[ERROR] Invalid VRM data: " << validate_result);
                    outputs.SetValue(0, true);    // discarded
                } else {
                    outputs.SetValue(0, false);    // discarded
                }
            } else {
                AVATAR_PIPELINE_LOG("[ERROR] vrm0_default_extensions: failed to get 'defaults' property in " << options->output_config);
                outputs.SetValue(0, true);    // discarded
            }

            outputs.SetValue(1, data);
            outputs.SetValue(2, *bones_ptr);
        } else {
            AVATAR_PIPELINE_LOG("[ERROR] vrm0_default_extensions: inputs not found");
            outputs.SetValue(0, true);    // discarded
        }
    }
    AvatarBuild::cmd_options* options;
};

} // namespace DSPatch