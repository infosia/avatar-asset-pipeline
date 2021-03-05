#pragma once

#include <DSPatch.h>
#include "pipelines.hpp"
#include <iostream>

#include <reproc++/run.hpp>

static inline int run_fbx2gltf(AvatarBuild::cmd_options* options)
{
    std::string output = options->output + ".fbx.glb";

    std::vector<std::string> arguments;

    arguments.push_back(options->fbx2gltf);
    arguments.push_back("--binary");
    arguments.push_back("--input");
    arguments.push_back(options->input);
    arguments.push_back("--output");
    arguments.push_back(output);

    reproc::arguments reproc_args(arguments);
    reproc::options reproc_options = {};

    int status = -1;
    std::error_code ec;

    std::tie(status, ec) = reproc::run(reproc_args, reproc_options);

    if (status != 0) {
        std::cout << "[ERROR] Failed to execute " << options->fbx2gltf << std::endl;    
        std::cout << "Please check if " << options->input << " is valid FBX file" << std::endl;    
        std::cout << "Make sure to specify path to fbx2gltf executable with --fbx2gltf option" << std::endl;    
    }

    if (ec) {
        std::cout << "[ERROR] " << ec.message() << std::endl;
    }

    return ec ? ec.value() : status;
}


namespace DSPatch {

class fbx2gltf_execute final : public Component {

public:
    fbx2gltf_execute(AvatarBuild::cmd_options* options)
        : Component()
        , options(options)
    {
        SetInputCount_(1);
        SetOutputCount_(1);
    }

    virtual ~fbx2gltf_execute()
    {
    }

protected:
    virtual void Process_(SignalBus const& inputs, SignalBus& outputs) override
    {
        // just return immediately when there's critical error in previous component
        const auto discarded = inputs.GetValue<bool>(0);
        if (discarded && *discarded) {
            Reset();
            return;
        }
        AVATAR_COMPONENT_LOG("[INFO] fbx2gltf_execute");

        outputs.SetValue(0, run_fbx2gltf(options) != 0);
    }
    AvatarBuild::cmd_options* options;
};

} // namespace DSPatch