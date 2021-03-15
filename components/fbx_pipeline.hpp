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