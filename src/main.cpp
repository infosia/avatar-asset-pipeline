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
#include "json.hpp"
#include <DSPatch.h>

#include <glb_z_reverse.hpp>

using json = nlohmann::json;

struct cmd_options {
    std::string config;
    bool verbose;
};

struct pipeline {
    std::string name;
    std::vector<std::string> pre;
    std::vector<std::string> components;
    std::vector<std::string> post;
    std::vector<std::string> finalizers;
};

static inline std::vector<std::string> get_items(std::string name, json obj)
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

static inline void wire_component(pipeline p, std::shared_ptr<DSPatch::Circuit> circuit)
{


}

static inline std::shared_ptr<DSPatch::Circuit> build_circuit(cmd_options options, json config_obj)
{
    auto circuit = std::make_shared<DSPatch::Circuit>();

    const auto pipelines_obj = config_obj["pipelines"];
    if (!pipelines_obj.is_array()) {
        std::cout << "[ERROR] pipeline '" << config_obj["name"] << "' is not an array" << std::endl;
        return circuit;
    }

    for (const auto& obj : pipelines_obj) {
        pipeline p;
        p.name = obj["name"];
        p.pre  = get_items("pre-conditions", obj);
        p.post = get_items("post-conditions", obj);
        p.components = get_items("components", obj);
        p.finalizers = get_items("finalizers", obj);

        wire_component(p, circuit);
    }

    return circuit;
}

static inline bool start_pipelines(cmd_options options)
{
    std::ifstream f(options.config, std::ios::in);
    if (f.fail()) {
        std::cout << "[ERROR] failed to parse config file " << options.config << std::endl;
        return false;
    }

    json config;

    try {
        f >> config;

        if (options.verbose) {
            std::cout << "[INFO] Starting pipeline '" << config["name"] << "'" << std::endl;
            std::cout << "=== " << config["description"] << " ===" << std::endl;
        }

    } catch (json::parse_error& e) {
        std::cout << "[ERROR] failed to parse config file " << options.config << std::endl;
        std::cout << "\t" << e.what() << std::endl;
        return false;
    }

    const auto circuit = build_circuit(options, config);

    circuit->StartAutoTick(DSPatch::Component::TickMode::Series);

    return true;
}

int main(int argc, char** argv)
{
    CLI::App app { "avatar-build: Run avatar asset pipeline" };

    std::string config = "pipelines/config.json";
    app.add_option("-c,--config", config, "Pipeline configuration file name (JSON)");

    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Verbose log output");

    CLI11_PARSE(app, argc, argv);

    cmd_options options = { config, verbose };

    if (!start_pipelines(options)) {
        return 1;
    }

    return 0;
}