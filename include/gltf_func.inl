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
#include <codecvt>
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

// avoid infinite loop on node tree traversal
#define GLTF_PARENT_LOOP_BEGIN(CONDITION) { std::uint16_t COUNTER=0; while (CONDITION) { ++COUNTER; if (COUNTER > 256) {AVATAR_PIPELINE_LOG("[WARNING] infinite loop detected at parent loop"); break;}
#define GLTF_PARENT_LOOP_END }}

#define gltf_calloc(N, SIZE) (pipeline_leackcheck_enabled ? stb_leakcheck_calloc(N * SIZE, __FILE__, __LINE__) : calloc(N, SIZE))
#define gltf_free(P) (pipeline_leackcheck_enabled ? stb_leakcheck_free(P) : free(P))

static void* gltf_leakcheck_malloc(void* user, cgltf_size size)
{
    (void)user;
    return stb_leakcheck_malloc(size, __FILE__, __LINE__);
}

static void gltf_leackcheck_free(void* user, void* ptr)
{
    (void)user;
    stb_leakcheck_free(ptr);
}

static char* gltf_alloc_chars(const char* str)
{
    if (str == nullptr)
        return nullptr;

    const auto length = strlen(str);
    if (length == 0)
        return (char*)gltf_calloc(1, 1);

    auto dst = (char*)gltf_calloc(length + 1, sizeof(char));
    if (dst > 0)
        strncpy(dst, str, length);

    return dst;
}

static std::string gltf_str_tolower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (unsigned char)std::tolower(c); });
    return s;
}

static cgltf_result gltf_wstring_file_read(const struct cgltf_memory_options* memory_options, const struct cgltf_file_options* file_options, const std::wstring path, cgltf_size* size, void** data)
{
    (void)file_options;
    void* (*memory_alloc)(void*, cgltf_size) = memory_options->alloc ? memory_options->alloc : &cgltf_default_alloc;
    void (*memory_free)(void*, void*) = memory_options->free ? memory_options->free : &cgltf_default_free;

    FILE* file = _wfopen(path.c_str(), L"rb");

    if (!file) {
        return cgltf_result_file_not_found;
    }

    cgltf_size file_size = size ? *size : 0;

    if (file_size == 0) {
        fseek(file, 0, SEEK_END);

        long length = ftell(file);
        if (length < 0) {
            fclose(file);
            return cgltf_result_io_error;
        }

        fseek(file, 0, SEEK_SET);
        file_size = (cgltf_size)length;
    }

    char* file_data = (char*)memory_alloc(memory_options->user_data, file_size);
    if (!file_data) {
        fclose(file);
        return cgltf_result_out_of_memory;
    }

    cgltf_size read_size = fread(file_data, 1, file_size, file);

    fclose(file);

    if (read_size != file_size) {
        memory_free(memory_options->user_data, file_data);
        return cgltf_result_io_error;
    }

    if (size) {
        *size = file_size;
    }
    if (data) {
        *data = file_data;
    }

    return cgltf_result_success;
}

static cgltf_result gltf_file_read(const struct cgltf_memory_options* memory_options, const struct cgltf_file_options* file_options, const char* path, cgltf_size* size, void** data)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return gltf_wstring_file_read(memory_options, file_options, converter.from_bytes(path), size, data);
}

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

static bool gltf_remove_animation(cgltf_data* data)
{
    if (data->animations_count > 0) {
        for (cgltf_size i = 0; i < data->animations_count; ++i) {
            data->memory.free(data->memory.user_data, data->animations[i].name);
            for (cgltf_size j = 0; j < data->animations[i].samplers_count; ++j) {
                cgltf_free_extensions(data, data->animations[i].samplers[j].extensions, data->animations[i].samplers[j].extensions_count);
            }
            data->memory.free(data->memory.user_data, data->animations[i].samplers);

            for (cgltf_size j = 0; j < data->animations[i].channels_count; ++j) {
                cgltf_free_extensions(data, data->animations[i].channels[j].extensions, data->animations[i].channels[j].extensions_count);
            }
            data->memory.free(data->memory.user_data, data->animations[i].channels);

            cgltf_free_extensions(data, data->animations[i].extensions, data->animations[i].extensions_count);
        }
        data->memory.free(data->memory.user_data, data->animations);

        data->animations = nullptr;
        data->animations_count = 0;

        return true;
    }
    return false;
}

static bool gltf_update_joint_buffer(cgltf_accessor* joints)
{
    // check if joint buffer needs to be updated
    if (joints->component_type == cgltf_component_type_r_16u)
        return false;

    const cgltf_size new_buffer_view_size = joints->count * 4 * sizeof(std::uint16_t);
    std::uint16_t* joints_data = (std::uint16_t*)gltf_calloc(1, new_buffer_view_size);
    for (cgltf_size i = 0; i < joints->count; ++i) {
        cgltf_uint out[4];
        if (!cgltf_accessor_read_uint(joints, i, out, 4)) {
            gltf_free(joints_data);
            return false;
        }
        auto data = (joints_data + (i * 4));
        data[0] = static_cast<std::uint16_t>(out[0]);
        data[1] = static_cast<std::uint16_t>(out[1]);
        data[2] = static_cast<std::uint16_t>(out[2]);
        data[3] = static_cast<std::uint16_t>(out[3]);
    }
    joints->buffer_view->size = new_buffer_view_size;
    joints->buffer_view->data = joints_data;
    joints->component_type = cgltf_component_type_r_16u;

    return true;
}

static bool gltf_create_buffer(cgltf_data* data)
{
    // this should not happen but for safety
    if (data->buffers_count == 0)
        return false;

    for (cgltf_size i = 0; i < data->buffers_count; ++i) {

        auto buffer = &data->buffers[i];

        cgltf_size total_size = 0;
        for (cgltf_size j = 0; j < data->buffer_views_count; ++j) {
            if (data->buffer_views[i].buffer == buffer) {
                total_size += (data->buffer_views[j].size + 3) & ~3;
            }
        }

        auto buffer_data = (uint8_t*)gltf_calloc(total_size, sizeof(uint8_t)); // will be freed at cgltf_free()
        if (buffer_data == nullptr)
            return false;

        auto buffer_dst = buffer_data;
        for (cgltf_size j = 0; j < data->buffer_views_count; ++j) {
            const auto buffer_view = &data->buffer_views[j];
            if (data->buffer_views[i].buffer == buffer) {
                const auto size_to_copy = buffer_view->size;
                if (buffer_view->data != nullptr) {
                    memcpy_s(buffer_dst, size_to_copy, buffer_view->data, size_to_copy);
                } else {
                    memcpy_s(buffer_dst, size_to_copy, ((uint8_t*)buffer_view->buffer->data + buffer_view->offset), size_to_copy);
                }
                buffer_view->offset = (buffer_dst - buffer_data);
                buffer_dst += (size_to_copy + 3) & ~3;

                if (buffer_view->data != nullptr) {
                    gltf_free(buffer_view->data);
                    buffer_view->data = nullptr;
                }
            }
        }

        // check if buffer has been updated from original, need to free in that case
        if (buffer->data != data->bin) {
            gltf_free(buffer->data);
        }

        buffer->size = total_size;
        buffer->data = buffer_data;
    }

    return true;
}

static bool gltf_upcast_joints(cgltf_data* data)
{
    cgltf_size update_count = 0;
    std::set<cgltf_accessor*> accessor_coord_done;
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const auto node = &data->nodes[i];
        if (node->mesh == nullptr)
            continue;

        const auto mesh = node->mesh;

        for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
            const auto primitive = &mesh->primitives[j];

            for (cgltf_size k = 0; k < primitive->attributes_count; ++k) {
                const auto attr = &primitive->attributes[k];
                const auto accessor = attr->data;
                if (accessor_coord_done.count(accessor) > 0) {
                    continue;
                }
                if (attr->name && strcmp(attr->name, "JOINTS_0") == 0) {
                    if (gltf_update_joint_buffer(accessor))
                        ++update_count;
                }
                accessor_coord_done.emplace(accessor);
            }
        }
    }

    return (update_count > 0 ? gltf_create_buffer(data) : true);
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

static std::string gltf_get_json(cgltf_options* options, cgltf_data* data)
{
    auto size = cgltf_write(options, NULL, 0, data);
    auto buffer = (char*)gltf_calloc(size, sizeof(char*));
    cgltf_write(options, buffer, size, data);

    // compress JSON
    auto j = nlohmann::json::parse(buffer, nullptr, false, true);

    data->memory.free(data->memory.user_data, buffer);

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

static bool gltf_write_file(cgltf_options* options, cgltf_data* data, std::string output)
{
    std::ofstream fout(output, std::ios::trunc | std::ios::binary);
    if (fout.fail()) {
        return false;
    }

    const auto in_json = gltf_get_json(options, data);
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

        const std::int8_t zero = 0;
        for (uint32_t j = 0; j < align; ++j) {
            fout.write(reinterpret_cast<const char*>(&zero), 1);
        }
    }

    fout.close();

    return true;
}

static bool gltf_write_json(cgltf_options* options, cgltf_data* data, std::string output)
{
    return cgltf_write_file(options, output.c_str(), data) == cgltf_result_success;
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
    GLTF_PARENT_LOOP_BEGIN (parent != nullptr)
        m = gltf_get_node_transform(parent) * m;
        parent = parent->parent;
    GLTF_PARENT_LOOP_END

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

static bool gltf_apply_weight(cgltf_node* skin_node, cgltf_float* positions, cgltf_uint* joints, cgltf_float* weights, cgltf_float* normals)
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
        glm::mat4 inverseBindMatrixForJoint = glm::make_mat4(ibm_mat);
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

    skin_node->scale[0] = 1;
    skin_node->scale[1] = 1;
    skin_node->scale[2] = 1;

    skin_node->rotation[0] = 0;
    skin_node->rotation[1] = 0;
    skin_node->rotation[2] = 0;
    skin_node->rotation[3] = 1;

    return true;
}

static bool gltf_apply_weights(cgltf_node* skin_node, cgltf_accessor* positions, cgltf_accessor* joints, cgltf_accessor* weights, cgltf_accessor* normals)
{
    cgltf_uint* joints_data = (cgltf_uint*)gltf_calloc(joints->count * 4, sizeof(cgltf_uint));
    for (cgltf_size i = 0; i < joints->count; ++i) {
        cgltf_accessor_read_uint(joints, i, joints_data + (i * 4), 4);
    }

    cgltf_size unpack_count = weights->count * 4;
    cgltf_float* weights_data = (cgltf_float*)gltf_calloc(unpack_count, sizeof(cgltf_float));
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

        gltf_apply_weight(skin_node, position, joints_data + (i * 4), weights_data + (i * 4), normal);

        gltf_f3_max(position, positions->max, positions->max);
        gltf_f3_min(position, positions->min, positions->min);
    }

    gltf_free(joints_data);
    gltf_free(weights_data);

    return true;
}

static bool gltf_skinning(cgltf_data* data)
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
                gltf_apply_weights(node, acc_POSITION, acc_JOINTS, acc_WEIGHTS, acc_NORMAL);
            }
        }
    }
    return true;
}

static void gltf_apply_transforms(cgltf_data* data, std::unordered_map<std::string, cgltf_node*>& name_to_node)
{
    gltf_apply_transform_meshes(data);

    for (cgltf_size i = 0; i < data->scenes_count; ++i) {
        const auto scene = &data->scenes[i];
        for (cgltf_size j = 0; j < scene->nodes_count; ++j) {
            glm::mat4 identity = glm::mat4(1.f);
            gltf_apply_transform(scene->nodes[j], identity);
        }
    }

    // clear translation of hips parents so bone position matches node position (VRM requirement)
    const auto hips_found = name_to_node.find("Hips");
    if (hips_found != name_to_node.end()) {
        const auto bone_hips = hips_found->second;
        glm::vec3 offset_translation = { 0, 0, 0 };
        auto node = bone_hips->parent;
        GLTF_PARENT_LOOP_BEGIN (node != nullptr)
            offset_translation.x += node->translation[0];
            offset_translation.y += node->translation[1];
            offset_translation.z += node->translation[2];

            node->translation[0] = 0;
            node->translation[1] = 0;
            node->translation[2] = 0;
            node = node->parent;
        GLTF_PARENT_LOOP_END
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

static std::string gltf_get_image_mimetype(const std::string& ext)
{
    if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if (ext == ".png")
        return "image/png";

    return "";
}

static bool gltf_is_mimetype_jpeg(const char* mime_type)
{
    if (mime_type == nullptr)
        return false;

    if (strcmp(mime_type, "image/jpg") == 0 || strcmp(mime_type, "image/jpeg") == 0
        || strcmp(mime_type, "image\\/jpeg") == 0 || strcmp(mime_type, "image\\/jpg") == 0) {
        return true;
    }
    return false;
}

static void gltf_png_write_func(void* context, void* buffer, int size)
{
    cgltf_image* image = (cgltf_image*)context;

    // check if buffer data has been updated. need to free in that case
    if (image->buffer_view->data != nullptr)
        gltf_free(image->buffer_view->data);

    uint8_t* buffer_dst = (uint8_t*)gltf_calloc(1, size);
    memcpy_s(buffer_dst, size, buffer, size);

    image->buffer_view->data = buffer_dst;
    image->buffer_view->size = size;
}

static bool gltf_images_jpg_to_png(cgltf_data* data)
{
    for (cgltf_size i = 0; i < data->images_count; ++i) {
        const auto image = &data->images[i];
        if (!gltf_is_mimetype_jpeg(image->mime_type))
            continue;

        const auto buffer = ((uint8_t*)image->buffer_view->buffer->data + image->buffer_view->offset);
        int x, y, n;
        const auto image_data = stbi_load_from_memory(buffer, (int)image->buffer_view->size, &x, &y, &n, 0);
        if (image_data == nullptr) {
            return false;
        }
        stbi_write_png_to_func(gltf_png_write_func, image, x, y, n, image_data, x * n);
        stbi_image_free(image_data); // safe to free here because buffer has been already copied to image buffer

        // assign new mime type
        gltf_free(image->mime_type);
        image->mime_type = gltf_alloc_chars("image/png");
    }

    gltf_create_buffer(data);

    return true;
}