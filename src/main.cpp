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
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_leakcheck.h"

#include "CLI11.hpp"
#include "DSPatch.h"
#include "json.hpp"

#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#define CGLTF_VRM_v0_0
#define CGLTF_VRM_v0_0_IMPLEMENTATION
#include "cgltf_write.h"

#pragma warning(push)
#pragma warning(disable : 4458) // declaration of 'source' hides class member
#include "verbalexpressions.hpp"
#pragma warning(pop)

#include <ghc/filesystem.hpp>

namespace fs = ghc::filesystem;
using json = nlohmann::json;

#include "pipelines.hpp"
#include "gltf_func.inl"
#include "gltf_overrides_func.inl"
#include "json_func.inl"
#include "bones_func.inl"
#include "vrm0_func.inl"

#include "glb_T_pose.hpp"
#include "glb_jpeg_to_png.hpp"
#include "glb_fix_roll.hpp"
#include "glb_transforms_apply.hpp"
#include "glb_z_reverse.hpp"
#include "glb_overrides.hpp"
#include "vrm0_fix_joint_buffer.hpp"
#include "vrm0_default_extensions.hpp"
#include "vrm0_remove_extensions.hpp"
#include "gltf_pipeline.hpp"
#include "gltfpack_execute.hpp"
#include "gltfpack_pipeline.hpp"
#include "noop.hpp"

#include "fbx2gltf_execute.hpp"
#include "fbx_pipeline.hpp"

using namespace AvatarBuild;

using json = nlohmann::json;

static std::shared_ptr<DSPatch::Component> create_component(std::string name, cmd_options* options)
{
    if (name == "glb_z_reverse") {
        return std::make_shared<DSPatch::glb_z_reverse>(options);
    } else if (name == "glb_transforms_apply") {
        return std::make_shared<DSPatch::glb_transforms_apply>(options);
    } else if (name == "glb_jpeg_to_png") {
        return std::make_shared<DSPatch::glb_jpeg_to_png>(options);
    } else if (name == "glb_fix_roll") {
        return std::make_shared<DSPatch::glb_fix_roll>(options);
    } else if (name == "glb_T_pose") {
        return std::make_shared<DSPatch::glb_T_pose>(options);
    } else if (name == "glb_overrides") {
        return std::make_shared<DSPatch::glb_overrides>(options);
    } else if (name == "vrm0_fix_joint_buffer") {
        return std::make_shared<DSPatch::vrm0_fix_joint_buffer>(options);
    } else if (name == "vrm0_default_extensions") {
        return std::make_shared<DSPatch::vrm0_default_extensions>(options);
    } else if (name == "vrm0_remove_extensions") {
        return std::make_shared<DSPatch::vrm0_remove_extensions>(options);
    } else if (name == "fbx2gltf_execute") {
        return std::make_shared<DSPatch::fbx2gltf_execute>(options);
    } else if (name == "gltfpack_execute") {
        return std::make_shared<DSPatch::gltfpack_execute>(options);
    }
    return std::make_shared<DSPatch::noop>(options, name);
}

static std::shared_ptr<pipeline_processor> create_pipeline(std::string name, cmd_options* options)
{
    if (name == "gltf_pipeline") {
        return std::make_shared<AvatarBuild::gltf_pipeline>(name, options);
    } else if (name == "fbx_pipeline") {
        return std::make_shared<AvatarBuild::fbx_pipeline>(name, options);
    } else if (name == "gltfpack_pipeline") {
        return std::make_shared<AvatarBuild::gltfpack_pipeline>(name, options);
    }
    return std::make_shared<AvatarBuild::pipeline_processor>(name, options);
}

static std::shared_ptr<pipeline_processor> wire_pipeline(pipeline* p, cmd_options* options)
{
    const auto pipeline = create_pipeline(p->name, options);

    for (size_t i = 0; i < p->components.size(); ++i) {
        pipeline->add_component(create_component(p->components[i], options));
    }

    pipeline->wire_components();

    return pipeline;
}

static bool build_and_start_circuits(cmd_options* options, json config_json)
{
    try {
        const auto& pipelines_obj = config_json["pipelines"];
        if (!pipelines_obj.is_array()) {
            std::cout << "[ERROR] pipeline '" << config_json["name"] << "' is not an array" << std::endl;
            return false;
        }

        auto circuit = std::make_shared<DSPatch::Circuit>();

        std::vector<std::shared_ptr<pipeline_processor>> pipelines;
        for (const auto& obj : pipelines_obj) {
            pipeline p;
            p.name = obj["name"];
            p.components = json_get_string_items("components", obj);

            auto component = wire_pipeline(&p, options);
            circuit->AddComponent(component);

            if (!pipelines.empty()) {
                circuit->ConnectOutToIn(pipelines.back(), 0, component, 0);
            }

            pipelines.push_back(component);
        }

        // make sure to execute pipeline in TickMode::Series
        // because pipeline has state and not thread safe.
        circuit->Tick(DSPatch::Component::TickMode::Series);

        // Check if pipeline has failure
        for (const auto pipeline : pipelines) {
            if (pipeline->is_discarded())
                return false;
        }

        return true;
    } catch (json::exception& e) {
        std::cout << "[ERROR] error while parsing pipeline '" << config_json["name"] << "'" << std::endl;
        std::cout << "\t " << e.what() << std::endl;
        return false;
    }
}

static bool start_pipelines(cmd_options* options)
{
    json config_json;

    if (!json_parse(options->config, &config_json)) {
        return false;
    }

    if (options->verbose) {
        std::cout << "[INFO] Starting pipeline '" << config_json["name"] << "'" << std::endl;
        std::cout << "[INFO] " << config_json["description"] << std::endl;
    }

    return build_and_start_circuits(options, config_json);
}

int main(int argc, char** argv)
{
    CLI::App app { "avatar-build: Run avatar asset pipeline" };

    std::string config = "pipelines/config.json";
    app.add_option("-p,--pipeline", config, "Pipeline configuration file name (JSON)")->required();

    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Verbose log output");

    bool debug = false;
    app.add_flag("-d,--debug", debug, "Enable debug output");

    std::string input;
    app.add_option("-i,--input", input, "Input file name")->check(CLI::ExistingFile)->required();

    std::string output;
    app.add_option("-o,--output", output, "Output file name")->required();

    std::string input_config;
    app.add_option("-m,--input_config", input_config, "Input configuration file name (JSON)");

    std::string output_config;
    app.add_option("-n,--output_config", output_config, "Output configuration file name (JSON)");

    std::string fbx2gltf = "extern/fbx2gltf.exe";
    app.add_option("-x,--fbx2gltf", fbx2gltf, "Path to fbx2gltf executable")->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    // common mistake
    if (input == output) {
        AVATAR_PIPELINE_LOG("[ERROR] Input and Output file should not be same: " << input);
        return 1;
    }

    cgltf_options gltf_options = { };

#ifdef WIN32
    // enable multibyte file name
    gltf_options.file.read = &gltf_file_read;
#endif

    // setup memory allocation check
    if (debug) {
        gltf_options.memory.alloc = &gltf_leakcheck_malloc;
        gltf_options.memory.free = &gltf_leackcheck_free;

        pipeline_leackcheck_enabled = true;
    }

    pipeline_verbose_enabled = verbose;

    cmd_options options = { config, input, output, input_config, output_config, fbx2gltf, verbose, debug, gltf_options };

    if (!options.input_config.empty() && !json_parse(options.input_config, &options.input_config_json)) {
        AVATAR_PIPELINE_LOG("[ERROR] Unable to load " << options.input_config);
        return 1;
    }
    if (!options.output_config.empty() && !json_parse(options.output_config, &options.output_config_json)) {
        AVATAR_PIPELINE_LOG("[ERROR] Unable to load " << options.output_config);
        return 1;
    }

    int status = 0;
    if (!start_pipelines(&options)) {
        status = 1;
    }

    if (debug && stb_leakcheck_dumpmem()) {
        status = 1;
    }

    return status;
}