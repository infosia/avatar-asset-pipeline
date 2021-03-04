#pragma once

#include "DSPatch.h"
#include <string>
#include <iostream>
#include <vector>

#include "json.hpp"

#define AVATAR_PIPELINE_LOG(msg)  if (state.options->verbose) std::cout << msg << std::endl;
#define AVATAR_COMPONENT_LOG(msg) if (state->options->verbose) std::cout << msg << std::endl;

namespace AvatarBuild {

struct cmd_options {
    std::string config;
    std::string input;
    std::string output;
    std::string bone_config;
    std::string fbx2gltf;
    bool verbose;
};

struct pipeline {
    std::string name;
    std::vector<std::string> components;
};

struct circuit_state {
    cmd_options* options;
    bool discarded;
};

class pipeline_processor : public DSPatch::Component {

public:
    pipeline_processor(std::string name, cmd_options* options)
        : Component()
        , name(name)
        , state { options, false }
        , circuit(std::make_shared<DSPatch::Circuit>())
    {

    }

    virtual ~pipeline_processor()
    {
    }

    circuit_state* get_state()
    {
        return &state;
    }

    void add_component(std::shared_ptr<DSPatch::Component> next)
    {
        circuit->AddComponent(next);
        if (!components.empty()) {
            circuit->ConnectOutToIn(components.back(), 0, next, 0);        
        }
        components.push_back(next);
    }

protected:
    virtual void Process_(DSPatch::SignalBus const&, DSPatch::SignalBus&) override
    {
        AVATAR_PIPELINE_LOG("[WARN] No Pipeline is connected for '" << name << "'");
    }

    std::shared_ptr<DSPatch::Circuit> circuit;

    std::string name;
    circuit_state state;
    std::vector<std::shared_ptr<DSPatch::Component>> components;
};

} // namespace Avatar