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

#include "CLI11.hpp"
#include "DSPatch.h"
#include "json.hpp"

#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#define CGLTF_VRM_v0_0
#define CGLTF_VRM_v0_0_IMPLEMENTATION
#include "cgltf_write.h"

#include "gltf_func.inl"

#include "pipelines.hpp"
#include "gltf_pipeline.hpp"


#include "noop.hpp"
#include "glb_z_reverse.hpp"
#include "glb_transforms_apply.hpp"
#include "vrm0_default_extensions.hpp"

using namespace AvatarBuild;

using json = nlohmann::json;

static std::vector<std::string> get_items(std::string name, json obj)
{
    const auto items_obj = obj[name];
    std::vector<std::string> items;
    if (items_obj.is_array()) {
        for (const auto& item : items_obj) {
            items.push_back(item.get<std::string>());
        }
    }
    return items;
}

static std::shared_ptr<DSPatch::Component> create_component(std::string name, circuit_state* state)
{
    if (name == "glb_z_reverse") {
        return std::make_shared<DSPatch::glb_z_reverse>(state);
    } else if (name == "glb_transforms_apply") {
        return std::make_shared<DSPatch::glb_transforms_apply>(state);
    } else if (name == "vrm0_default_extensions") {
        return std::make_shared<DSPatch::vrm0_default_extensions>(state);
    }
    return std::make_shared<DSPatch::noop>(state, name);
}

static std::shared_ptr<pipeline_processor> create_pipeline(std::string name, cmd_options* options)
{
    if (name == "gltf_pipeline") {
        return std::make_shared<AvatarBuild::gltf_pipeline>(name, options);
    }
    return std::make_shared<AvatarBuild::pipeline_processor>(name, options);
}

static std::shared_ptr<DSPatch::Component> wire_pipeline(pipeline* p, cmd_options* options)
{
    const auto pipeline = create_pipeline(p->name, options);
    const auto state = pipeline->get_state();

    for (size_t i = 0; i < p->components.size(); ++i) {
        pipeline->add_component(create_component(p->components[i], state));
    }

    return pipeline;
}

static bool build_and_start_circuits(cmd_options* options, json config_obj)
{
    const auto pipelines_obj = config_obj["pipelines"];
    if (!pipelines_obj.is_array()) {
        std::cout << "[ERROR] pipeline '" << config_obj["name"] << "' is not an array" << std::endl;
        return false;
    }

    auto circuit = std::make_shared<DSPatch::Circuit>();
    auto pipeline_count = pipelines_obj.size();

    std::shared_ptr<DSPatch::Component> previous;
    for (size_t i = 0; i < pipeline_count; ++i) {
        const auto& obj = pipelines_obj[i];

        pipeline p;
        p.name = obj["name"];
        p.components = get_items("components", obj);

        auto component = wire_pipeline(&p, options);
        circuit->AddComponent(component);

        if (i > 0) {
            circuit->ConnectOutToIn(previous, 0, component, 0);
        }

        previous = component;
    }

    circuit->Tick(DSPatch::Component::TickMode::Series);

    return true;
}

static bool start_pipelines(cmd_options* options)
{
    std::ifstream f(options->config, std::ios::in);
    if (f.fail()) {
        std::cout << "[ERROR] failed to parse config file " << options->config << std::endl;
        return false;
    }

    json config;

    try {
        f >> config;

        if (options->verbose) {
            std::cout << "[INFO] Starting pipeline '" << config["name"] << "'" << std::endl;
            std::cout << "[INFO] " << config["description"] << std::endl;
        }

    } catch (json::parse_error& e) {
        std::cout << "[ERROR] failed to parse config file " << options->config << std::endl;
        std::cout << "\t" << e.what() << std::endl;
        return false;
    }

    return build_and_start_circuits(options, config);
}

int main(int argc, char** argv)
{
    CLI::App app { "avatar-build: Run avatar asset pipeline" };

    std::string config = "pipelines/config.json";
    app.add_option("-c,--config", config, "Pipeline configuration file name (JSON)");

    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Verbose log output");

    std::string input = "input.glb";
    app.add_option("-i,--input", input, "Input file name")->check(CLI::ExistingFile);

    std::string output = "output.glb";
    app.add_option("-o,--output", output, "Output file name");

    CLI11_PARSE(app, argc, argv);

    // common mistake
    if (input == output) {
        std::cout << "[ERROR] Input and Output file should not be same: " << input << std::endl;
        return 1;
    }

    cmd_options options = { config, input, output, verbose };

    if (!start_pipelines(&options)) {
        return 1;
    }

    return 0;
}