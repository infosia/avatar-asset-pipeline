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

#include "gltfpackapi.h"
#include "pipelines.hpp"
#include <DSPatch.h>
#include <iostream>

namespace DSPatch {

class gltfpack_execute final : public Component {

public:
    gltfpack_execute(AvatarBuild::cmd_options* options)
        : Component()
        , options(options)
    {
        SetInputCount_(1);
        SetOutputCount_(1);
    }

    virtual ~gltfpack_execute()
    {
    }

protected:
    json select_value(std::string name, json item, json items_defaults)
    {
        return item[name].is_null() ? items_defaults[name] : item[name];
    }

    Settings defaults(json item, json items_defaults)
    {
        Settings settings = {};
        settings.quantize = false;
        settings.pos_bits = 14;
        settings.tex_bits = 12;
        settings.nrm_bits = 8;
        settings.col_bits = 8;
        settings.trn_bits = 16;
        settings.rot_bits = 12;
        settings.scl_bits = 16;
        settings.anim_freq = 30;
        settings.simplify_threshold = 1.f;
        settings.simplify_aggressive = false;
        settings.texture_quality = 8;
        settings.texture_scale = 1.f;
        settings.verbose = false;

        settings.keep_extras = true;
        settings.keep_materials = false;
        settings.keep_nodes = false;

        settings.use_uint8_joints = false;
        settings.use_uint8_weights = false;

        auto simplity_threshold = select_value("simplity_threshold", item, items_defaults);
        auto simplify_aggressive = select_value("simplity_threshold", item, items_defaults);
        auto quantize = select_value("quantize", item, items_defaults);
        auto verbose = select_value("verbose", item, items_defaults);
        auto keep_extras = select_value("keep_extras", item, items_defaults);
        auto keep_materials = select_value("keep_materials", item, items_defaults);
        auto keep_nodes = select_value("keep_nodes", item, items_defaults);
        auto use_uint8_joints = select_value("use_uint8_joints", item, items_defaults);
        auto use_uint8_weights = select_value("use_uint8_weights", item, items_defaults);

        if (simplity_threshold.is_number())
            settings.simplify_threshold = simplity_threshold.get<float>();

        if (simplify_aggressive.is_boolean())
            settings.simplify_aggressive = simplify_aggressive.get<bool>();

        if (quantize.is_boolean())
            settings.quantize = quantize.get<bool>();

        if (verbose.is_boolean())
            settings.verbose = verbose.get<bool>();

        if (keep_extras.is_boolean())
            settings.keep_extras = keep_extras.get<bool>();

        if (keep_materials.is_boolean())
            settings.keep_materials = keep_materials.get<bool>();

        if (keep_nodes.is_boolean())
            settings.keep_nodes = keep_nodes.get<bool>();

        if (use_uint8_joints.is_boolean())
            settings.use_uint8_joints = use_uint8_joints.get<bool>();

        if (use_uint8_weights.is_boolean())
            settings.use_uint8_weights = use_uint8_weights.get<bool>();

        return settings;
    }

    virtual void Process_(SignalBus const& inputs, SignalBus& outputs) override
    {
        // just return immediately when there's critical error in previous component
        const auto discarded = inputs.GetValue<bool>(0);
        if (discarded && *discarded) {
            return;
        }
        AVATAR_PIPELINE_LOG("[INFO] gltfpack_execute");

        size_t count = 0;
        const auto gltfpack_obj = options->output_config_json["gltfpack"];
        if (gltfpack_obj.is_object()) {
            const auto items = gltfpack_obj["LOD"];
            const auto items_defaults = gltfpack_obj["defaults"];
            if (items.is_array()) {
                const auto size = items.size();
                for (size_t i = 0; i < size; ++i) {
                    const auto item = items[i];
                    const auto name_LOD = item["name"].is_string() ? item["name"].get<std::string>() : "LOD" + std::to_string(i);
                    const auto output_LOD = path_without_extension(options->output).u8string() + "." + name_LOD + fs::path(options->output).extension().u8string();

                    // use options->output for source assuming gltf_pipeline is executed before gltfpack
                    if (gltfpack(options->output.c_str(), output_LOD.c_str(), nullptr, defaults(item, items_defaults)) != 0) {
                        AVATAR_PIPELINE_LOG("[ERROR] failed to execute gltfpack for " << name_LOD << ". Skipping.");
                    } else {
                        ++count;
                    }
                }
            }
        }

        outputs.SetValue(0, count > 0); // discarded
    }
    AvatarBuild::cmd_options* options;
};

} // namespace DSPatch