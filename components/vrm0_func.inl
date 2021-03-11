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

static void vrm0_ensure_defaults(const json& materialProperties_object, cgltf_data* data)
{
    data->has_vrm_v0_0 = true;

    const auto vrm = &data->vrm_v0_0;
    vrm->exporterVersion = gltf_alloc_chars("cgltf+vrm 1.9");
    vrm->specVersion = gltf_alloc_chars("0.0");

    // VRM does not support animation
    gltf_remove_animation(data);

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
            vrm->materialProperties[i].renderQueue = 2000;

            if (materialProperties_object.is_object() && materialProperties_object["default"].is_object()) {

                auto props_default = materialProperties_object["default"];
                const auto floatProperties = props_default["floatProperties"];
                const auto vectorProperties = props_default["vectorProperties"];
                const auto textureProperties = props_default["textureProperties"];
                const auto keywordMap = props_default["keywordMap"];
                const auto tagMap = props_default["tagMap"];

                vrm->materialProperties[i].shader = gltf_alloc_chars(props_default["shader"].get<std::string>().c_str());
                vrm->materialProperties[i].textureProperties_count = textureProperties.size() + 2;
                vrm->materialProperties[i].textureProperties_keys = (char**)calloc(vrm->materialProperties[i].textureProperties_count, sizeof(void*));
                vrm->materialProperties[i].textureProperties_values = (cgltf_int*)calloc(vrm->materialProperties[i].textureProperties_count, sizeof(cgltf_int));
                vrm->materialProperties[i].textureProperties_keys[0] = gltf_alloc_chars("_MainTex");
                vrm->materialProperties[i].textureProperties_keys[1] = gltf_alloc_chars("_ShadeTexture");
                vrm->materialProperties[i].textureProperties_values[0] = (cgltf_int)(data->materials[i].pbr_metallic_roughness.base_color_texture.texture - data->textures);
                vrm->materialProperties[i].textureProperties_values[1] = (cgltf_int)(data->materials[i].pbr_metallic_roughness.base_color_texture.texture - data->textures);
                {
                    cgltf_size j = 2;
                    for (const auto item : textureProperties.items()) {
                        vrm->materialProperties[i].textureProperties_keys[j] = gltf_alloc_chars(item.key().c_str());
                        vrm->materialProperties[i].textureProperties_values[j] = item.value().get<cgltf_int>();
                        j++;
                    }
                }
                {
                    vrm->materialProperties[i].floatProperties_count = floatProperties.size();
                    if (vrm->materialProperties[i].floatProperties_count > 0) {
                        vrm->materialProperties[i].floatProperties_keys = (char**)calloc(vrm->materialProperties[i].floatProperties_count, sizeof(void*));
                        vrm->materialProperties[i].floatProperties_values = (cgltf_float*)calloc(vrm->materialProperties[i].floatProperties_count, sizeof(cgltf_float));
                        cgltf_size j = 0;
                        for (const auto item : floatProperties.items()) {
                            vrm->materialProperties[i].floatProperties_keys[j] = gltf_alloc_chars(item.key().c_str());
                            vrm->materialProperties[i].floatProperties_values[j] = item.value().get<cgltf_float>();
                            j++;
                        }
                    }
                }
                {
                    vrm->materialProperties[i].vectorProperties_count = vectorProperties.size();
                    if (vrm->materialProperties[i].vectorProperties_count > 0) {
                        vrm->materialProperties[i].vectorProperties_keys = (char**)calloc(vrm->materialProperties[i].vectorProperties_count, sizeof(void*));
                        vrm->materialProperties[i].vectorProperties_values = (cgltf_float**)calloc(vrm->materialProperties[i].vectorProperties_count, sizeof(cgltf_float*));
                        vrm->materialProperties[i].vectorProperties_floats_size = (cgltf_size*)calloc(vrm->materialProperties[i].vectorProperties_count, sizeof(cgltf_size));
                        cgltf_size j = 0;
                        for (const auto item : vectorProperties.items()) {
                            const auto values = item.value();
                            vrm->materialProperties[i].vectorProperties_keys[j] = gltf_alloc_chars(item.key().c_str());
                            vrm->materialProperties[i].vectorProperties_values[j] = (cgltf_float*)calloc(values.size(), sizeof(cgltf_float));
                            vrm->materialProperties[i].vectorProperties_floats_size[j] = values.size();
                            cgltf_size k = 0;
                            for (const auto value : values) {
                                vrm->materialProperties[i].vectorProperties_values[j][k] = value.get<cgltf_float>();
                                k++;
                            }
                            j++;
                        }
                    }
                }
                {
                    vrm->materialProperties[i].keywordMap_count = keywordMap.size();
                    if (vrm->materialProperties[i].keywordMap_count > 0) {
                        vrm->materialProperties[i].keywordMap_keys = (char**)calloc(vrm->materialProperties[i].keywordMap_count, sizeof(void*));
                        vrm->materialProperties[i].keywordMap_values = (cgltf_bool*)calloc(vrm->materialProperties[i].keywordMap_count, sizeof(cgltf_bool));
                        cgltf_size j = 0;
                        for (const auto item : keywordMap.items()) {
                            vrm->materialProperties[i].keywordMap_keys[j] = gltf_alloc_chars(item.key().c_str());
                            vrm->materialProperties[i].keywordMap_values[j] = item.value().get<bool>();
                            j++;
                        }
                    }
                }
                {
                    vrm->materialProperties[i].tagMap_count = tagMap.size();
                    if (vrm->materialProperties[i].tagMap_count > 0) {
                        vrm->materialProperties[i].tagMap_keys = (char**)calloc(vrm->materialProperties[i].tagMap_count, sizeof(void*));
                        vrm->materialProperties[i].tagMap_values = (char**)calloc(vrm->materialProperties[i].tagMap_count, sizeof(void*));
                        cgltf_size j = 0;
                        for (const auto item : tagMap.items()) {
                            vrm->materialProperties[i].tagMap_keys[j] = gltf_alloc_chars(item.key().c_str());
                            vrm->materialProperties[i].tagMap_values[j] = gltf_alloc_chars(item.value().get<std::string>().c_str());
                            j++;
                        }
                    }
                }

            } else {
                vrm->materialProperties[i].shader = gltf_alloc_chars("VRM_USE_GLTFSHADER");
            }
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
