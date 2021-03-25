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
    Settings defaults()
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
        settings.simplify_threshold = 0.7f;
        settings.texture_quality = 8;
        settings.texture_scale = 1.f;

        //settings.keep_extras = true;
        //settings.keep_materials = true;
        //settings.keep_nodes = true;

        settings.use_uint8_joints = false;
        settings.use_uint8_weights = false;

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

        outputs.SetValue(0, gltfpack(options->input.c_str(), options->output.c_str(), nullptr, defaults()) != 0);
    }
    AvatarBuild::cmd_options* options;
};

} // namespace DSPatch