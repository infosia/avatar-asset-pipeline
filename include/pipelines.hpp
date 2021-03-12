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
#include <unordered_map>

#define AVATAR_PIPELINE_LOG(msg)  if (options->verbose) std::cout << msg << std::endl;
#define AVATAR_COMPONENT_LOG(msg) if (options->verbose) std::cout << msg << std::endl;

namespace AvatarBuild {

struct cmd_options {
    std::string config;
    std::string input;
    std::string output;
    std::string input_config;
    std::string output_config;
    std::string fbx2gltf;
    bool verbose;
    bool debug;
    cgltf_options gltf_options;
};

struct pipeline {
    std::string name;
    std::vector<std::string> components;
};

struct bone {
    std::string name;
    cgltf_float rotation[4];
};

struct pose {
    std::string name;
    std::vector<bone> bones;
};

struct bone_mappings {
    std::unordered_map<std::string, pose> poses;
    std::unordered_map<std::string, cgltf_node*> name_to_node;  // bone name, node
    std::unordered_map<std::string, cgltf_int> node_index_map; // node->name, node index
};

// A component that just get result from signal bus
class components_result final : public DSPatch::Component {
public:
    components_result()
        : DSPatch::Component()
        , discarded(false)
    {
        SetInputCount_(1);
        SetOutputCount_(0);
    }

    bool is_discarded() {
        return discarded;
    }
protected:
    virtual void Process_(DSPatch::SignalBus const& inputs, DSPatch::SignalBus&) override
    {
        const auto input0 = inputs.GetValue<bool>(0);
        if (input0 && *input0) {
            discarded = *input0;
        }
    }
    bool discarded;
};

class pipeline_processor : public DSPatch::Component {

public:
    pipeline_processor(std::string name, cmd_options* options)
        : Component()
        , name(name)
        , options(options)
        , circuit(std::make_shared<DSPatch::Circuit>())
        , tick_result(std::make_shared<components_result>())
    {
        circuit->AddComponent(tick_result);
    }

    virtual ~pipeline_processor()
    {
    }

    void add_component(std::shared_ptr<DSPatch::Component> next)
    {
        circuit->AddComponent(next);
        if (!components.empty()) {
            const auto back = components.back();
            circuit->ConnectOutToIn(back, 0, next, 0); // <bool>  discarded
            circuit->ConnectOutToIn(back, 1, next, 1); // <void*> data1
            circuit->ConnectOutToIn(back, 2, next, 2); // <void*> data2
            circuit->ConnectOutToIn(back, 3, next, 3); // <void*> data3
        }
        components.push_back(next);
    }

    virtual void wire_components()
    {
        // nothing to do
    }

protected:
    virtual void Process_(DSPatch::SignalBus const&, DSPatch::SignalBus&) override
    {
        AVATAR_PIPELINE_LOG("[WARN] No Pipeline is connected for '" << name << "'");
    }

    std::string name;
    cmd_options* options;
    std::shared_ptr<DSPatch::Circuit> circuit;
    std::shared_ptr<components_result> tick_result;
    std::vector<std::shared_ptr<DSPatch::Component>> components;
};

} // namespace Avatar