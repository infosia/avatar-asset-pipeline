/* distributed under MIT license:
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
#include "json.hpp"
#include <string>
#include <unordered_map>
#include <vector>

#pragma warning(push)
#pragma warning(disable : 4458) // declaration of 'source' hides class member
#include "verbalexpressions.hpp"
#pragma warning(pop)

using json = nlohmann::json;

static bool gltf_apply_pose(std::string name, AvatarBuild::bone_mappings* mappings)
{
    std::size_t node_count = 0;
    const auto pose_found = mappings->poses.find(name);
    if (pose_found != mappings->poses.end()) {
        const auto pose = pose_found->second;
        for (const auto bone : pose.bones) {
            const auto bone_found = mappings->name_to_node.find(bone.name);
            if (bone_found != mappings->name_to_node.end()) {
                const auto node = bone_found->second;
                node->rotation[0] = bone.rotation[0];
                node->rotation[1] = bone.rotation[1];
                node->rotation[2] = bone.rotation[2];
                node->rotation[3] = bone.rotation[3];
                node_count++;
            }
        }
    } else {
        return false;
    }
    return node_count > 0;
}

static std::unordered_map<std::string, cgltf_node*> gltf_parse_bones_to_node(json input, cgltf_data* data)
{
    std::unordered_map<std::string, cgltf_node*> name_to_node;

    // search config
    bool pattern_match = true;
    bool with_any_case = true;
    auto config = input["config"];
    if (config.is_object()) {
        pattern_match = json_get_bool(config, "pattern_match");
        with_any_case = json_get_bool(config, "with_any_case");
    }

    std::unordered_map<std::string, cgltf_node*> nodes;
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const auto node = &data->nodes[i];
        if (node->name != nullptr) {
            nodes.emplace(node->name, node);
        }
    }

    auto bones_object = input["bones"].items();
    for (auto bone_object : bones_object) {
        const auto key = bone_object.key();
        const auto bone_name = bone_object.value().get<std::string>();

        bool found = false;
        std::string actual_name;
        if (pattern_match) {
            auto expr = verex::verex().search_one_line().anything().find(bone_name).anything().with_any_case(with_any_case);
            for (const auto node : nodes) {
                const auto node_name = node.first;
                if (bone_name == node_name || expr.test(node_name)) {
                    name_to_node.emplace(key, node.second);
                    actual_name = node_name;
                    found = true;
                    break;
                }
            }
        } else {
            for (const auto node : nodes) {
                const auto node_name = node.first;
                if (bone_name == node_name) {
                    name_to_node.emplace(key, node.second);
                    actual_name = node_name;
                    found = true;
                    break;
                }
            }
        }
        if (found && !actual_name.empty()) {
            nodes.erase(actual_name);
        }
    }
    return name_to_node;
}

static std::unordered_map<std::string, AvatarBuild::pose> gltf_parse_bone_poses(json poses_obj)
{
    std::unordered_map<std::string, AvatarBuild::pose> poses;

    auto items = poses_obj.items();
    for (auto pose_obj : items) {
        const auto pose_name = pose_obj.key();
        auto bones = pose_obj.value().items();

        AvatarBuild::pose pose = {};
        pose.name = pose_name;

        for (auto bone_obj : bones) {
            const auto bone_name = bone_obj.key();
            auto props = bone_obj.value();

            AvatarBuild::bone bone = {};
            bone.name = bone_name;

            auto rotation = props["rotation"];

            if (rotation.is_array()) {
                bone.rotation[0] = rotation[0].get<float>();
                bone.rotation[1] = rotation[1].get<float>();
                bone.rotation[2] = rotation[2].get<float>();
                bone.rotation[3] = rotation[3].get<float>();
            }
            pose.bones.push_back(bone);
        }
        poses.emplace(pose.name, pose);
    }

    return poses;
}

static bool gltf_parse_bone_mappings(cgltf_data* data, AvatarBuild::bone_mappings* mappings, AvatarBuild::cmd_options* options)
{
    (void)data, mappings;

    json j;
    if (!json_parse(options->bone_config, &j)) {
        return false;
    }

    try {
        mappings->poses = gltf_parse_bone_poses(j["poses"]);
        mappings->name_to_node = gltf_parse_bones_to_node(j, data);
    } catch (json::exception&) {
        return false;
    }

    return true;
}
