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
#include <string>
#include <unordered_map>
#include <vector>

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

                glm::quat a = glm::make_quat(node->rotation);
                glm::quat b = glm::make_quat(bone.rotation);

                // apply given rotation to the bone
                glm::quat r = glm::normalize(a * b);

                node->rotation[0] = r.x;
                node->rotation[1] = r.y;
                node->rotation[2] = r.z;
                node->rotation[3] = r.w;

                node_count++;
            }
        }
    } else {
        return false;
    }
    return node_count > 0;
}

static bool gltf_fix_roll(std::string name, AvatarBuild::bone_mappings* mappings)
{
    std::size_t node_count = 0;
    const auto pose_found = mappings->poses.find(name);
    if (pose_found != mappings->poses.end()) {
        const auto pose = pose_found->second;
        for (const auto bone : pose.bones) {
            const auto bone_found = mappings->name_to_node.find(bone.name);
            if (bone_found != mappings->name_to_node.end()) {
                const auto node = bone_found->second;

                glm::quat a = glm::make_quat(node->rotation);
                glm::quat b = glm::make_quat(bone.rotation);
                glm::quat b_inversed = glm::inverse(b);

                // apply given rotation to the bone
                glm::quat r = glm::normalize(a * b);

                node->rotation[0] = r.x;
                node->rotation[1] = r.y;
                node->rotation[2] = r.z;
                node->rotation[3] = r.w;

                for (cgltf_size i = 0; i < node->children_count; ++i) {
                    const auto child = node->children[i];
                    glm::quat ca = glm::make_quat(child->rotation);
                    glm::quat cr = glm::normalize(ca * b_inversed);

                    child->rotation[0] = cr.x;
                    child->rotation[1] = cr.y;
                    child->rotation[2] = cr.z;
                    child->rotation[3] = cr.w;
                }

                node_count++;
            }
        }
    } else {
        return false;
    }
    return node_count > 0;
}

static bool gltf_bone_symmetry_naming_test(const std::string& name_to_test, const std::string bone_key, const std::string& bone_name, const bool with_any_case)
{
    static auto right_test = verex::verex().search_one_line().start_of_line().then("Right").anything();
    static auto left_test = verex::verex().search_one_line().start_of_line().then("Left").anything();

    auto re_same = verex::verex().search_one_line().anything().then(bone_name).with_any_case(with_any_case);

    auto re_r1 = verex::verex().search_one_line().anything().then("right").anything().then(bone_name).with_any_case(with_any_case);
    auto re_l1 = verex::verex().search_one_line().anything().then("left").anything().then(bone_name).with_any_case(with_any_case);
    auto re_r2 = verex::verex().search_one_line().anything().then(bone_name).anything().then("right").with_any_case(with_any_case);
    auto re_l2 = verex::verex().search_one_line().anything().then(bone_name).anything().then("left").with_any_case(with_any_case);

    auto re_r3 = verex::verex().search_one_line().anything().any_of("r_|r\\.|r\\s+").then(bone_name).with_any_case(with_any_case);
    auto re_l3 = verex::verex().search_one_line().anything().any_of("l_|l\\.|l\\s+").then(bone_name).with_any_case(with_any_case);
    auto re_r4 = verex::verex().search_one_line().anything().then(bone_name).any_of("_r|\\.r|\\s+r").with_any_case(with_any_case);
    auto re_l4 = verex::verex().search_one_line().anything().then(bone_name).any_of("_l|\\.l|\\s+l").with_any_case(with_any_case);

    if (right_test.test(bone_key)) {
        return (right_test.test(bone_name) && re_same.test(name_to_test)) || re_r1.test(name_to_test) || re_r2.test(name_to_test) || re_r3.test(name_to_test) || re_r4.test(name_to_test);
    }

    if (left_test.test(bone_key)) {
        return (left_test.test(bone_name) && re_same.test(name_to_test)) || re_l1.test(name_to_test) || re_l2.test(name_to_test) || re_l3.test(name_to_test) || re_l4.test(name_to_test);
    }

    return true;
}

static void gltf_parse_bones_to_node(json input, cgltf_data* data, AvatarBuild::bone_mappings* mappings)
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
            mappings->node_index_map.emplace(node->name, (cgltf_int)i);
        }
    }

    auto bones_object = input["bones"].items();
    for (auto bone_object : bones_object) {
        const auto bone_key = bone_object.key();
        const auto bone_name = bone_object.value().get<std::string>();

        const auto found_iter = nodes.find(bone_name);

        if (found_iter != nodes.end()) {
            name_to_node.emplace(bone_key, nodes[bone_name]);
            nodes.erase(bone_name);
        } else if (pattern_match) {
            std::string name_to_erase;
            auto expr = verex::verex().search_one_line().anything().then(bone_name).with_any_case(with_any_case);
            for (const auto node : nodes) {
                const auto node_name = node.first;
                if (bone_name == node_name || expr.test(node_name)) {
                    if (gltf_bone_symmetry_naming_test(node_name, bone_key, bone_name, with_any_case)) {
                        name_to_node.emplace(bone_key, node.second);
                        name_to_erase = node_name;
                        break;
                    }
                }
            }
            if (!name_to_erase.empty())
                nodes.erase(name_to_erase);
        }
    }
    mappings->name_to_node = name_to_node;
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
    try {
        mappings->poses = gltf_parse_bone_poses(options->input_config_json["poses"]);
        gltf_parse_bones_to_node(options->input_config_json, data, mappings);
    } catch (json::exception&) {
        return false;
    }

    return true;
}
