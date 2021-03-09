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

static bool vrm0_ensure_degreemap(cgltf_vrm_firstperson_degreemap_v0_0* degreemap)
{
    if (degreemap->curve_count == 0) {
        degreemap->curve_count = 8;
        degreemap->curve = (cgltf_float*)calloc(8, sizeof(cgltf_float));
        degreemap->xRange = 90;
        degreemap->yRange = 10;

        degreemap->curve[0] = 0;
        degreemap->curve[1] = 0;
        degreemap->curve[2] = 0;
        degreemap->curve[3] = 1;
        degreemap->curve[4] = 1;
        degreemap->curve[5] = 1;
        degreemap->curve[6] = 1;
        degreemap->curve[7] = 0;
    }
    return true;
}

static void vrm0_ensure_defaults(cgltf_data* data)
{
    data->has_vrm_v0_0 = true;

    const auto vrm = &data->vrm_v0_0;
    vrm->exporterVersion = gltf_alloc_chars("cgltf+vrm 1.9");
    vrm->specVersion = gltf_alloc_chars("0.0");

    // VRM does not use animation
    if (data->animations_count > 0) {
        data->memory.free(data->memory.user_data, data->animations);
        data->animations_count = 0;
        data->animations = nullptr;
    }

    vrm0_ensure_degreemap(&vrm->firstPerson.lookAtHorizontalInner);
    vrm0_ensure_degreemap(&vrm->firstPerson.lookAtHorizontalOuter);
    vrm0_ensure_degreemap(&vrm->firstPerson.lookAtVerticalDown);
    vrm0_ensure_degreemap(&vrm->firstPerson.lookAtVerticalUp);

    if (vrm->firstPerson.meshAnnotations_count == 0) {
        vrm->firstPerson.meshAnnotations_count = data->meshes_count;
        vrm->firstPerson.meshAnnotations = (cgltf_vrm_firstperson_meshannotation_v0_0*)calloc(data->meshes_count, sizeof(cgltf_vrm_firstperson_meshannotation_v0_0));
        for (cgltf_size i = 0; i < data->meshes_count; ++i) {
            vrm->firstPerson.meshAnnotations[i].mesh = static_cast<cgltf_int>(i);
            vrm->firstPerson.meshAnnotations[i].firstPersonFlag = gltf_alloc_chars("Auto");
        }
    }

    if (vrm->firstPerson.firstPersonBoneOffset_count == 0) {
        vrm->firstPerson.firstPersonBoneOffset_count = 3;
        vrm->firstPerson.firstPersonBoneOffset = (cgltf_float*)calloc(3, sizeof(cgltf_float));
    }

    if (vrm->materialProperties_count == 0) {
        vrm->materialProperties_count = data->materials_count;
        vrm->materialProperties = (cgltf_vrm_material_v0_0*)calloc(data->materials_count, sizeof(cgltf_vrm_material_v0_0));
        for (cgltf_size i = 0; i < data->materials_count; ++i) {
            vrm->materialProperties[i].name = gltf_alloc_chars(data->materials[i].name);
            vrm->materialProperties[i].shader = gltf_alloc_chars("VRM_USE_GLTFSHADER");
            vrm->materialProperties[i].renderQueue = 2000;
        }
    }
}

static bool vrm0_update_meta(const json& meta_object, cgltf_vrm_v0_0* vrm)
{
    if (!meta_object.is_object()) {
        return false;
    }

	auto meta_object_items = meta_object.items();
    const auto meta = &vrm->meta;
    for (const auto item : meta_object_items) {
        const auto key = item.key();
        const auto value_str = item.value().get<std::string>();
        const auto value = value_str.c_str();
        if (key == "title") {
            meta->title = gltf_alloc_chars(value);
        } else if (key == "version") {
            meta->version = gltf_alloc_chars(value);
        } else if (key == "author") {
            meta->author = gltf_alloc_chars(value);
        } else if (key == "contactInformation") {
            meta->contactInformation = gltf_alloc_chars(value);
        } else if (key == "reference") {
            meta->reference = gltf_alloc_chars(value);
        } else if (key == "otherPermissionUrl") {
            meta->otherPermissionUrl = gltf_alloc_chars(value);
        } else if (key == "otherLicenseUrl") {
            meta->otherLicenseUrl = gltf_alloc_chars(value);
        } else if (key == "allowedUserName") {
            if (!select_cgltf_vrm_meta_allowedUserName_v0_0(value, &meta->allowedUserName)) {
                std::cout << "[ERROR] Unknown " << key << ": " << value << std::endl;
            }
        } else if (key == "violentUssageName") {
            if (!select_cgltf_vrm_meta_violentUssageName_v0_0(value, &meta->violentUssageName)) {
                std::cout << "[ERROR] Unknown " << key << ": " << value << std::endl;
            }
        } else if (key == "sexualUssageName") {
            if (!select_cgltf_vrm_meta_sexualUssageName_v0_0(value, &meta->sexualUssageName)) {
                std::cout << "[ERROR] Unknown " << key << ": " << value << std::endl;
            }
        } else if (key == "commercialUssageName") {
            if (!select_cgltf_vrm_meta_commercialUssageName_v0_0(value, &meta->commercialUssageName)) {
                std::cout << "[ERROR] Unknown " << key << ": " << value << std::endl;
            }
        }
    }
    return true;
}

static bool vrm0_update_bones(AvatarBuild::bone_mappings* mappings, cgltf_data* data)
{
    const auto vrm = &data->vrm_v0_0;
    const auto humanoid = &vrm->humanoid;

    humanoid->humanBones_count = mappings->name_to_node.size();
    humanoid->humanBones = (cgltf_vrm_humanoid_bone_v0_0*)calloc(humanoid->humanBones_count, sizeof(cgltf_vrm_humanoid_bone_v0_0));

    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const auto node = &data->nodes[i];
        // fill in default values
        if (!node->has_translation) {
            node->translation[0] = 0;
            node->translation[1] = 0;
            node->translation[2] = 0;
            node->has_translation = true;
        }
        if (!node->has_rotation) {
            node->rotation[0] = 0;
            node->rotation[1] = 0;
            node->rotation[2] = 0;
            node->rotation[3] = 1;
            node->has_rotation = true;
        }
        if (!node->has_scale) {
            node->scale[0] = 1;
            node->scale[1] = 1;
            node->scale[2] = 1;
            node->has_scale = true;
        }
    }

    cgltf_size i = 0;
    for (const auto bone : mappings->name_to_node) {
        const auto bone_name = bone.first;
        const auto node = bone.second;

        auto dst = &humanoid->humanBones[i];

        // defaults
        dst->axisLength = 0;
        dst->useDefaultValues = true;
        dst->center_count = 1;
        dst->min_count = 1;
        dst->max_count = 1;
        dst->node = 0;

        dst->center = (cgltf_float*)calloc(3, sizeof(cgltf_float));
        dst->max = (cgltf_float*)calloc(3, sizeof(cgltf_float));
        dst->min = (cgltf_float*)calloc(3, sizeof(cgltf_float));

        const auto found = mappings->node_index_map.find(node->name);
        if (found != mappings->node_index_map.end()) {
            dst->node = found->second;
            select_cgltf_vrm_humanoid_bone_bone_v0_0(bone_name.c_str(), &dst->bone);

            if (bone_name == "head") {
                vrm->firstPerson.firstPersonBone = found->second;
            }
        } else {
            std::cout << "[ERROR] bone is not found for " << bone_name << std::endl;
        }

        i++;
    }

    return true;
}
