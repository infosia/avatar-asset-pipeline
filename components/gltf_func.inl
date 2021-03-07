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
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_SILENT_WARNINGS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

static void gltf_f3_min(cgltf_float* a, cgltf_float* b, cgltf_float* out)
{
    out[0] = a[0] < b[0] ? a[0] : b[0];
    out[1] = a[1] < b[1] ? a[1] : b[1];
    out[2] = a[2] < b[2] ? a[2] : b[2];
}

static void gltf_f3_max(cgltf_float* a, cgltf_float* b, cgltf_float* out)
{
    out[0] = a[0] > b[0] ? a[0] : b[0];
    out[1] = a[1] > b[1] ? a[1] : b[1];
    out[2] = a[2] > b[2] ? a[2] : b[2];
}

static void gltf_reverse_z_accessor(cgltf_accessor* accessor)
{
    uint8_t* buffer_data = (uint8_t*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;

    accessor->max[0] = -FLT_MAX;
    accessor->max[2] = -FLT_MAX;
    accessor->min[0] = FLT_MAX;
    accessor->min[2] = FLT_MAX;

    for (cgltf_size i = 0; i < accessor->count; ++i) {
        cgltf_float* element = (cgltf_float*)(buffer_data + (accessor->stride * i));
        element[0] = -element[0];
        element[2] = -element[2];

        gltf_f3_max(element, accessor->max, accessor->max);
        gltf_f3_min(element, accessor->min, accessor->min);
    }
}

static void gltf_apply_transform_accessor(cgltf_node* node, cgltf_accessor* accessor)
{
    uint8_t* buffer_data = (uint8_t*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;

    accessor->max[0] = -FLT_MAX;
    accessor->max[1] = -FLT_MAX;
    accessor->max[2] = -FLT_MAX;
    accessor->min[0] = FLT_MAX;
    accessor->min[1] = FLT_MAX;
    accessor->min[2] = FLT_MAX;

    for (cgltf_size i = 0; i < accessor->count; ++i) {
        cgltf_float* element = (cgltf_float*)(buffer_data + (accessor->stride * i));

        if (node->has_scale) {
            element[0] *= node->scale[0];
            element[1] *= node->scale[1];
            element[2] *= node->scale[2];
        }

        if (node->has_translation) {
            element[0] += node->translation[0];
            element[1] += node->translation[1];
            element[2] += node->translation[2];
        }

        if (node->has_rotation) {
            const glm::vec3 pos = glm::make_vec3(element);
            const glm::quat rot = glm::make_quat(node->rotation);

            glm::vec3 newpos = rot * pos;

            element[0] = newpos.x;
            element[1] = newpos.y;
            element[2] = newpos.z;
        }

        gltf_f3_max(element, accessor->max, accessor->max);
        gltf_f3_min(element, accessor->min, accessor->min);
    }
}

static void gltf_reverse_z(cgltf_data* data)
{
    std::set<cgltf_accessor*> accessor_coord_done;
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const auto node = &data->nodes[i];
        const auto mesh = node->mesh;

        if (mesh == nullptr)
            continue;

        for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
            const auto primitive = &mesh->primitives[j];

            for (cgltf_size k = 0; k < primitive->attributes_count; ++k) {
                const auto attr = &primitive->attributes[k];
                const auto accessor = attr->data;

                if (accessor_coord_done.count(accessor) > 0) {
                    continue;
                }
                if (attr->type == cgltf_attribute_type_position) {
                    gltf_reverse_z_accessor(accessor);
                } else if (attr->type == cgltf_attribute_type_normal) {
                    gltf_reverse_z_accessor(accessor);
                }
                accessor_coord_done.emplace(accessor);
            }

            for (cgltf_size k = 0; k < primitive->targets_count; ++k) {
                const auto target = &primitive->targets[k];
                for (cgltf_size a = 0; a < target->attributes_count; ++a) {
                    const auto attr = &target->attributes[a];
                    const auto accessor = attr->data;
                    if (accessor_coord_done.count(accessor) > 0) {
                        continue;
                    }
                    if (attr->type == cgltf_attribute_type_position) {
                        gltf_reverse_z_accessor(accessor);
                    } else if (attr->type == cgltf_attribute_type_normal) {
                        gltf_reverse_z_accessor(accessor);
                    }
                    accessor_coord_done.emplace(accessor);
                }
            }
        }
    }

    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const auto node = &data->nodes[i];
        if (node->has_translation) {
            node->translation[0] = -node->translation[0];
            node->translation[2] = -node->translation[2];
        }
    }
}

static uint32_t gltf_get_buffer_size(cgltf_data* data)
{
    uint32_t size = 0;
    for (cgltf_size i = 0; i < data->buffers_count; ++i) {
        cgltf_buffer* buffer = &data->buffers[i];
        cgltf_size aligned_size = (buffer->size + 3) & ~3;

        size += (uint32_t)aligned_size + 8;
    }
    return size;
}

static std::string gltf_get_json(cgltf_data* data)
{
    cgltf_options options = {};
    auto size = cgltf_write(&options, NULL, 0, data);
    auto buffer = (char*)calloc(size, sizeof(char*));
    cgltf_write(&options, buffer, size, data);

    // compress JSON
    auto j = nlohmann::json::parse(buffer, nullptr, false, true);

    free(buffer);

    if (j.is_object()) {
        auto dump = j.dump();
        cgltf_size dump_size = dump.size();
        cgltf_size aligned_size = (dump_size + 3) & ~3;
        cgltf_size align = aligned_size - dump_size;
        for (cgltf_size i = 0; i < align; ++i) {
            dump.append(" ");
        }
        return dump;
    }

    return "";
}

static bool gltf_write_file(cgltf_data* data, std::string output)
{
    std::ofstream fout(output, std::ios::trunc | std::ios::binary);
    if (fout.fail()) {
        return false;
    }

    const auto in_json = gltf_get_json(data);
    const auto in_json_cstr = in_json.c_str();
    const auto in_json_size = (uint32_t)strlen(in_json_cstr);
    uint32_t total_size = GlbHeaderSize + GlbChunkHeaderSize + in_json_size + gltf_get_buffer_size(data);

    fout.write(reinterpret_cast<const char*>(&GlbMagic), 4);
    fout.write(reinterpret_cast<const char*>(&GlbVersion), 4);
    fout.write(reinterpret_cast<const char*>(&total_size), 4);

    fout.write(reinterpret_cast<const char*>(&in_json_size), 4);
    fout.write(reinterpret_cast<const char*>(&GlbMagicJsonChunk), 4);

    fout.write(in_json_cstr, in_json_size);

    for (cgltf_size i = 0; i < data->buffers_count; ++i) {
        cgltf_buffer* buffer = &data->buffers[i];
        cgltf_size aligned_size = (buffer->size + 3) & ~3;
        cgltf_size align = aligned_size - buffer->size;

        fout.write(reinterpret_cast<const char*>(&aligned_size), 4);
        fout.write(reinterpret_cast<const char*>(&GlbMagicBinChunk), 4);
        fout.write(reinterpret_cast<const char*>(buffer->data), buffer->size);

        for (uint32_t j = 0; j < align; ++j) {
            fout.write(0, 1);
        }
    }

    fout.close();

    return true;
}

static glm::mat4 gltf_get_node_transform(const cgltf_node* node)
{
    auto matrix = glm::mat4(1.0f);
    auto translation = glm::make_vec3(node->translation);
    auto scale = glm::make_vec3(node->scale);
    auto rotation = glm::make_quat(node->rotation);

    return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
}

static glm::mat4 gltf_get_global_node_transform(const cgltf_node* node)
{
    auto m = gltf_get_node_transform(node);
    cgltf_node* parent = node->parent;
    while (parent != nullptr) {
        m = gltf_get_node_transform(parent) * m;
        parent = parent->parent;
    }

    return m;
}

// Check if given node is a joint
static bool gltf_is_joint_node(const cgltf_node* node)
{
    // node that has mesh is not a joint
    if (node->mesh != nullptr)
        return false;

    // node that has mesh child is not a joint
    for (cgltf_size i = 0; i < node->children_count; i++) {
        if (node->children[i]->mesh != nullptr)
            return false;
    }

    return true;
}

// Get global transform of joint, that does not include non-joint transform
static glm::mat4 gltf_get_global_joint_transform(const cgltf_node* node)
{
    auto m = gltf_get_node_transform(node);
    cgltf_node* parent = node->parent;
    while (parent != nullptr) {
        if (parent->mesh)
            break;
        if (!gltf_is_joint_node(parent))
            break;
        m = gltf_get_node_transform(parent) * m;
        parent = parent->parent;
    }
    return m;
}

static void gltf_apply_transform_meshes(cgltf_data* data)
{
    std::set<cgltf_accessor*> accessor_coord_done;
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const auto node = &data->nodes[i];
        const auto mesh = node->mesh;

        if (mesh == nullptr)
            continue;

        for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
            const auto primitive = &mesh->primitives[j];

            for (cgltf_size k = 0; k < primitive->attributes_count; ++k) {
                const auto attr = &primitive->attributes[k];
                const auto accessor = attr->data;

                if (accessor_coord_done.count(accessor) > 0) {
                    continue;
                }
                if (attr->type == cgltf_attribute_type_position) {
                    gltf_apply_transform_accessor(node, accessor);
                } else if (attr->type == cgltf_attribute_type_normal) {
                    gltf_apply_transform_accessor(node, accessor);
                }
                accessor_coord_done.emplace(accessor);
            }

            for (cgltf_size k = 0; k < primitive->targets_count; ++k) {
                const auto target = &primitive->targets[k];
                for (cgltf_size a = 0; a < target->attributes_count; ++a) {
                    const auto attr = &target->attributes[a];
                    const auto accessor = attr->data;
                    if (accessor_coord_done.count(accessor) > 0) {
                        continue;
                    }
                    if (attr->type == cgltf_attribute_type_position) {
                        gltf_apply_transform_accessor(node, accessor);
                    } else if (attr->type == cgltf_attribute_type_normal) {
                        gltf_apply_transform_accessor(node, accessor);
                    }
                    accessor_coord_done.emplace(accessor);
                }
            }
        }
    }
}

static void gltf_apply_transform(cgltf_node* node, const glm::mat4 parent_matrix)
{
    const glm::mat4 identity = glm::mat4(1.0f);

    const glm::vec3 pos = glm::make_vec3(node->translation);
    const glm::vec3 scale = glm::make_vec3(node->scale);
    const glm::quat rot = glm::make_quat(node->rotation);

    glm::mat4 self = glm::translate(glm::mat4(1.0f), pos) * glm::mat4(rot) * glm::scale(glm::mat4(1.0f), scale) * identity;
    glm::mat4 bind_matrix = parent_matrix * self;

    glm::quat rot_parent;
    glm::vec3 scale_parent, pos_parent, skew_unused;
    glm::vec4 perspective_unused;
    glm::decompose(parent_matrix, scale_parent, rot_parent, pos_parent, skew_unused, perspective_unused);

    glm::vec3 newpos = rot_parent * pos;

    node->translation[0] = newpos.x * scale_parent.x;
    node->translation[1] = newpos.y * scale_parent.y;
    node->translation[2] = newpos.z * scale_parent.z;
    node->rotation[0] = 0;
    node->rotation[1] = 0;
    node->rotation[2] = 0;
    node->rotation[3] = 1;
    node->scale[0] = 1;
    node->scale[1] = 1;
    node->scale[2] = 1;

    for (cgltf_size i = 0; i < node->children_count; i++) {
        gltf_apply_transform(node->children[i], bind_matrix);
    }
}

static inline void gltf_from_mat4_to_floats(const glm::mat4& from, cgltf_float* to)
{
    to[0] = from[0][0];
    to[1] = from[0][1];
    to[2] = from[0][2];
    to[3] = from[0][3];

    to[4] = from[1][0];
    to[5] = from[1][1];
    to[6] = from[1][2];
    to[7] = from[1][3];

    to[8] = from[2][0];
    to[9] = from[2][1];
    to[10] = from[2][2];
    to[11] = from[2][3];

    to[12] = from[3][0];
    to[13] = from[3][1];
    to[14] = from[3][2];
    to[15] = from[3][3];
}

static bool gltf_apply_weight(cgltf_node* skin_node, cgltf_float* positions, cgltf_uint* joints, cgltf_float* weights, cgltf_float* normals, bool joint_transform)
{
    glm::mat4 skinMat = glm::mat4(0.f);

    const cgltf_skin* skin = skin_node->skin;

    const cgltf_accessor* accessor = skin_node->skin->inverse_bind_matrices;
    uint8_t* ibm_data = (uint8_t*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;

    glm::mat4 globalTransform = gltf_get_global_node_transform(skin_node);
    glm::mat4 globalInverseTransform = glm::inverse(globalTransform);

    for (cgltf_size i = 0; i < 4; ++i) {
        const cgltf_uint joint_index = joints[i];
        cgltf_float weight = weights[i];

        if (weight <= 0)
            continue;

        if (skin->joints_count <= joint_index)
            continue;

        const cgltf_float* ibm_mat = (cgltf_float*)(ibm_data + accessor->stride * joint_index);

        const auto joint = skin->joints[joint_index];

        glm::mat4 globalTransformOfJointNode = gltf_get_global_node_transform(joint);
        glm::mat4 inverseBindMatrixForJoint = joint_transform ? glm::inverse(gltf_get_global_joint_transform(joint)) : glm::make_mat4(ibm_mat);
        glm::mat4 jointMatrix = globalTransformOfJointNode * inverseBindMatrixForJoint;
        jointMatrix = globalInverseTransform * jointMatrix;

        skinMat += jointMatrix * weight;
    }

    glm::vec4 inPos = glm::vec4(positions[0], positions[1], positions[2], 1.f);
    glm::vec4 locPos = globalTransform * skinMat * inPos;

    positions[0] = locPos.x;
    positions[1] = locPos.y;
    positions[2] = locPos.z;

    if (normals != nullptr) {
        glm::vec3 inNormal = glm::make_vec3(normals);
        glm::vec3 outNormal = glm::normalize(glm::transpose(glm::inverse(glm::mat3(globalTransform * skinMat))) * inNormal);

        normals[0] = outNormal.x;
        normals[1] = outNormal.y;
        normals[2] = outNormal.z;
    }

    return true;
}

static bool gltf_apply_weights(cgltf_node* skin_node, cgltf_accessor* positions, cgltf_accessor* joints, cgltf_accessor* weights, cgltf_accessor* normals, bool joint_transform)
{
    cgltf_uint* joints_data = (cgltf_uint*)calloc(joints->count * 4, sizeof(cgltf_uint));
    for (cgltf_size i = 0; i < joints->count; ++i) {
        cgltf_accessor_read_uint(joints, i, joints_data + (i * 4), 4);
    }

    cgltf_size unpack_count = weights->count * 4;
    cgltf_float* weights_data = (cgltf_float*)calloc(unpack_count, sizeof(cgltf_float));
    cgltf_accessor_unpack_floats(weights, weights_data, unpack_count);

    uint8_t* positions_data = (uint8_t*)positions->buffer_view->buffer->data + positions->buffer_view->offset + positions->offset;
    uint8_t* normals_data = nullptr;

    if (normals != nullptr) {
        normals_data = (uint8_t*)normals->buffer_view->buffer->data + normals->buffer_view->offset + normals->offset;
    }

    positions->max[0] = -FLT_MAX;
    positions->max[1] = -FLT_MAX;
    positions->max[2] = -FLT_MAX;
    positions->min[0] = FLT_MAX;
    positions->min[1] = FLT_MAX;
    positions->min[2] = FLT_MAX;

    for (cgltf_size i = 0; i < positions->count; ++i) {
        cgltf_float* position = (cgltf_float*)(positions_data + (positions->stride * i));
        cgltf_float* normal = normals_data != nullptr ? (cgltf_float*)(normals_data + (normals->stride * i)) : nullptr;

        gltf_apply_weight(skin_node, position, joints_data + (i * 4), weights_data + (i * 4), normal, joint_transform);

        gltf_f3_max(position, positions->max, positions->max);
        gltf_f3_min(position, positions->min, positions->min);
    }

    return true;
}

static bool gltf_skinning(cgltf_data* data, bool joint_transform)
{
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const auto node = &data->nodes[i];
        const auto mesh = node->mesh;
        if (mesh == nullptr)
            continue;
        for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
            const auto primitive = &mesh->primitives[j];
            cgltf_accessor* acc_POSITION = nullptr;
            cgltf_accessor* acc_JOINTS = nullptr;
            cgltf_accessor* acc_WEIGHTS = nullptr;
            cgltf_accessor* acc_NORMAL = nullptr;
            for (cgltf_size k = 0; k < primitive->attributes_count; ++k) {
                const auto attr = &primitive->attributes[k];
                if (attr->type == cgltf_attribute_type_position) {
                    acc_POSITION = attr->data;
                } else if (attr->type == cgltf_attribute_type_normal) {
                    acc_NORMAL = attr->data;
                } else if (attr->type == cgltf_attribute_type_joints) {
                    acc_JOINTS = attr->data;
                } else if (attr->type == cgltf_attribute_type_weights) {
                    acc_WEIGHTS = attr->data;
                }
            }
            if (acc_POSITION && acc_JOINTS && acc_WEIGHTS) {
                gltf_apply_weights(node, acc_POSITION, acc_JOINTS, acc_WEIGHTS, acc_NORMAL, joint_transform);
            }
        }
    }
    return true;
}

static void gltf_apply_transforms(cgltf_data* data, std::unordered_map<std::string, cgltf_node*>& name_to_node)
{
    gltf_apply_transform_meshes(data);

    for (cgltf_size i = 0; i < data->scenes_count; ++i) {
        glm::mat4 identity = glm::mat4(1.f);
        const auto scene = &data->scenes[i];
        for (cgltf_size j = 0; j < scene->nodes_count; ++j) {
            gltf_apply_transform(scene->nodes[j], identity);
        }
    }

    // clear translation of hips parents
    const auto hips_found = name_to_node.find("hips");
    if (hips_found != name_to_node.end()) {
        const auto bone_hips = hips_found->second;
        glm::vec3 offset_translation = { 0, 0, 0 };
        auto node = bone_hips->parent;
        while (node != nullptr) {
            offset_translation.x += node->translation[0];
            offset_translation.y += node->translation[1];
            offset_translation.z += node->translation[2];

            node->translation[0] = 0;
            node->translation[1] = 0;
            node->translation[2] = 0;
            node = node->parent;
        }
        bone_hips->translation[0] += offset_translation.x;
        bone_hips->translation[1] += offset_translation.y;
        bone_hips->translation[2] += offset_translation.z;
    }
}

static void gltf_update_inverse_bind_matrices(cgltf_data* data)
{
    std::set<cgltf_accessor*> skin_done;
    for (cgltf_size i = 0; i < data->skins_count; ++i) {
        const auto skin = &data->skins[i];
        const auto accessor = skin->inverse_bind_matrices;

        if (skin_done.count(accessor) > 0) {
            continue;
        }
        skin_done.emplace(accessor);

        uint8_t* buffer_data = (uint8_t*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;

        accessor->max[0] = -FLT_MAX;
        accessor->max[1] = -FLT_MAX;
        accessor->max[2] = -FLT_MAX;
        accessor->min[0] = FLT_MAX;
        accessor->min[1] = FLT_MAX;
        accessor->min[2] = FLT_MAX;

        for (cgltf_size j = 0; j < skin->joints_count; ++j) {
            cgltf_node* node = skin->joints[j];
            cgltf_float* inverse_bind_matrix = (cgltf_float*)(buffer_data + accessor->stride * j);

            glm::mat4 inversed = glm::inverse(gltf_get_global_node_transform(node));

            gltf_from_mat4_to_floats(inversed, inverse_bind_matrix);

            gltf_f3_max(inverse_bind_matrix + 12, accessor->max, accessor->max);
            gltf_f3_min(inverse_bind_matrix + 12, accessor->min, accessor->min);
        }
    }
}
