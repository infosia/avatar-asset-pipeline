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

static bool vrm0_override_material(json& overrides, cgltf_material* material)
{
    for (auto item : overrides.items()) {
        if (item.key() == "alphaMode") {
            const auto value = item.value().get<std::string>();
            if (value == "OPAQUE") {
                material->alpha_mode = cgltf_alpha_mode_opaque;
            }
        }
    }
    return true;
}

static bool vrm0_override_materials(json& materials_override, cgltf_data* data)
{
    (void)data;

    auto rules = materials_override["rules"];
    auto overrides = materials_override["values"];

    if (!rules.is_object() || !overrides.is_object())
        return false;

    for (cgltf_size i = 0; i < data->materials_count; ++i) {
        auto material = &data->materials[i];
        for (auto& rule : rules.items()) {
            const auto& key = rule.key();
            const auto& pattern = rule.value();
            if (!pattern.is_string())
                continue;

            std::regex re(pattern.get<std::string>());

            if (key == "name") {
                if (std::regex_match(material->name, re)) {
                    vrm0_override_material(overrides, material);
                }
            }
        }
    }

    return true;
}

static bool vrm0_override_parameters(json& overrides_object, cgltf_data* data)
{
    (void)data, overrides_object;
    auto materials_overrides = overrides_object["materials"];
    if (materials_overrides.is_array()) {
        for (auto item : materials_overrides) {
            vrm0_override_materials(item, data);
        }
    }

    return true;
}

static bool vrm0_validate_node_tree(cgltf_node* node, std::set<cgltf_node*>& parents)
{
    if (parents.find(node) != parents.end())
        return false;

    parents.emplace(node);
    for (cgltf_size i = 0; i < node->children_count; ++i) {
        if (!vrm0_validate_node_tree(node->children[i], parents))
            return false;
    }

    return true;
}

static cgltf_result vrm0_validate(cgltf_data* data)
{
    // should have at least 11 mandatory bones
    if (data->vrm_v0_0.humanoid.humanBones_count < 11) {
        return cgltf_result_data_too_short;
    }

    // detect wrong node tree that causes infinite loop
    for (cgltf_size i = 0; i < data->scenes_count; ++i) {
        const auto scene = &data->scenes[i];
        for (cgltf_size j = 0; j < scene->nodes_count; ++j) {
            std::set<cgltf_node*> parents;
            if (!vrm0_validate_node_tree(scene->nodes[i], parents)) {
                return cgltf_result_invalid_gltf;
            }
        }
    }

    return cgltf_result_success;
}

static bool vrm0_ensure_degreemap(cgltf_vrm_firstperson_degreemap_v0_0* degreemap)
{
    if (degreemap->curve_count == 0) {
        degreemap->curve_count = 8;
        degreemap->curve = (cgltf_float*)gltf_calloc(8, sizeof(cgltf_float));
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

static void vrm0_store_blendshapes(const std::string name, cgltf_size mesh, cgltf_size index, std::unordered_map<std::string, std::vector<cgltf_vrm_blendshape_bind_v0_0>>& blendshapes)
{
    cgltf_vrm_blendshape_bind_v0_0 item = { (cgltf_int)mesh, (cgltf_int)index, 100.f };
    auto iter = blendshapes.find(name);
    if (iter != blendshapes.end()) {
        iter->second.push_back(item);
    } else {
        std::vector<cgltf_vrm_blendshape_bind_v0_0> items = { item };
        blendshapes.emplace(name, items);
    }
}

static void vrm0_ensure_textureProperties(const json& materialProperties_object, cgltf_size i, cgltf_data* data)
{
    const auto vrm = &data->vrm_v0_0;
    auto textureProperties = materialProperties_object["textureProperties"];

    if (data->materials[i].normal_texture.texture != nullptr) {
        textureProperties["_BumpMap"] = (data->materials[i].normal_texture.texture - data->textures);
    }

    vrm->materialProperties[i].textureProperties_count = textureProperties.size() + 2;
    vrm->materialProperties[i].textureProperties_keys = (char**)gltf_calloc(vrm->materialProperties[i].textureProperties_count, sizeof(void*));
    vrm->materialProperties[i].textureProperties_values = (cgltf_int*)gltf_calloc(vrm->materialProperties[i].textureProperties_count, sizeof(cgltf_int));
    vrm->materialProperties[i].textureProperties_keys[0] = gltf_alloc_chars("_MainTex");
    vrm->materialProperties[i].textureProperties_keys[1] = gltf_alloc_chars("_ShadeTexture");
    vrm->materialProperties[i].textureProperties_values[0] = (cgltf_int)(data->materials[i].pbr_metallic_roughness.base_color_texture.texture - data->textures);
    vrm->materialProperties[i].textureProperties_values[1] = (cgltf_int)(data->materials[i].pbr_metallic_roughness.base_color_texture.texture - data->textures);
    cgltf_size j = 2;
    for (const auto item : textureProperties.items()) {
        vrm->materialProperties[i].textureProperties_keys[j] = gltf_alloc_chars(item.key().c_str());
        vrm->materialProperties[i].textureProperties_values[j] = item.value().get<cgltf_int>();
        j++;
    }
}

static void vrm0_ensure_floatProperties(const json& materialProperties_object, cgltf_size i, cgltf_data* data)
{
    const auto vrm = &data->vrm_v0_0;
    const auto floatProperties = materialProperties_object["floatProperties"];

    vrm->materialProperties[i].floatProperties_count = floatProperties.size();
    if (vrm->materialProperties[i].floatProperties_count > 0) {
        vrm->materialProperties[i].floatProperties_keys = (char**)gltf_calloc(vrm->materialProperties[i].floatProperties_count, sizeof(void*));
        vrm->materialProperties[i].floatProperties_values = (cgltf_float*)gltf_calloc(vrm->materialProperties[i].floatProperties_count, sizeof(cgltf_float));
        cgltf_size j = 0;
        for (const auto item : floatProperties.items()) {
            vrm->materialProperties[i].floatProperties_keys[j] = gltf_alloc_chars(item.key().c_str());
            vrm->materialProperties[i].floatProperties_values[j] = item.value().get<cgltf_float>();
            j++;
        }
    }
}

static void vrm0_ensure_vectorProperties(const json& materialProperties_object, cgltf_size i, cgltf_data* data)
{
    const auto vrm = &data->vrm_v0_0;
    const auto vectorProperties = materialProperties_object["vectorProperties"];

    vrm->materialProperties[i].vectorProperties_count = vectorProperties.size();
    if (vrm->materialProperties[i].vectorProperties_count > 0) {
        vrm->materialProperties[i].vectorProperties_keys = (char**)gltf_calloc(vrm->materialProperties[i].vectorProperties_count, sizeof(void*));
        vrm->materialProperties[i].vectorProperties_values = (cgltf_float**)gltf_calloc(vrm->materialProperties[i].vectorProperties_count, sizeof(cgltf_float*));
        vrm->materialProperties[i].vectorProperties_floats_size = (cgltf_size*)gltf_calloc(vrm->materialProperties[i].vectorProperties_count, sizeof(cgltf_size));
        cgltf_size j = 0;
        for (const auto item : vectorProperties.items()) {
            const auto values = item.value();
            vrm->materialProperties[i].vectorProperties_keys[j] = gltf_alloc_chars(item.key().c_str());
            vrm->materialProperties[i].vectorProperties_values[j] = (cgltf_float*)gltf_calloc(values.size(), sizeof(cgltf_float));
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

static void vrm0_ensure_mapProperties(const json& materialProperties_object, cgltf_size i, cgltf_data* data)
{
    const auto vrm = &data->vrm_v0_0;
    auto keywordMap = materialProperties_object["keywordMap"];
    auto tagMap = materialProperties_object["tagMap"];

    if (data->materials[i].normal_texture.texture != nullptr) {
        keywordMap["_NORMALMAP"] = true;
    }

    vrm->materialProperties[i].keywordMap_count = keywordMap.size();
    if (vrm->materialProperties[i].keywordMap_count > 0) {
        vrm->materialProperties[i].keywordMap_keys = (char**)gltf_calloc(vrm->materialProperties[i].keywordMap_count, sizeof(char*));
        vrm->materialProperties[i].keywordMap_values = (cgltf_bool*)gltf_calloc(vrm->materialProperties[i].keywordMap_count, sizeof(cgltf_bool));
        cgltf_size j = 0;
        for (const auto item : keywordMap.items()) {
            vrm->materialProperties[i].keywordMap_keys[j] = gltf_alloc_chars(item.key().c_str());
            vrm->materialProperties[i].keywordMap_values[j] = item.value().get<cgltf_bool>();
            j++;
        }
    }
    vrm->materialProperties[i].tagMap_count = tagMap.size();
    if (vrm->materialProperties[i].tagMap_count > 0) {
        vrm->materialProperties[i].tagMap_keys = (char**)gltf_calloc(vrm->materialProperties[i].tagMap_count, sizeof(char*));
        vrm->materialProperties[i].tagMap_values = (char**)gltf_calloc(vrm->materialProperties[i].tagMap_count, sizeof(char*));
        cgltf_size j = 0;
        for (const auto item : tagMap.items()) {
            vrm->materialProperties[i].tagMap_keys[j] = gltf_alloc_chars(item.key().c_str());
            vrm->materialProperties[i].tagMap_values[j] = gltf_alloc_chars(item.value().get<std::string>().c_str());
            j++;
        }
    }
}

static void vrm0_ensure_defaults(const json& output_config_object, cgltf_data* data)
{
    data->has_vrm_v0_0 = 1;

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
        vrm->firstPerson.meshAnnotations = (cgltf_vrm_firstperson_meshannotation_v0_0*)gltf_calloc(data->meshes_count, sizeof(cgltf_vrm_firstperson_meshannotation_v0_0));
        for (cgltf_size i = 0; i < data->meshes_count; ++i) {
            vrm->firstPerson.meshAnnotations[i].mesh = static_cast<cgltf_int>(i);
            vrm->firstPerson.meshAnnotations[i].firstPersonFlag = gltf_alloc_chars("Auto");
        }
    }

    if (vrm->firstPerson.firstPersonBoneOffset_count == 0) {
        vrm->firstPerson.firstPersonBoneOffset_count = 3;
        vrm->firstPerson.firstPersonBoneOffset = (cgltf_float*)gltf_calloc(3, sizeof(cgltf_float));
        auto firstPerson_object = output_config_object["firstPerson"];
        if (firstPerson_object.is_object() && firstPerson_object["firstPersonBoneOffset"].is_object()) {
            auto firstPersonBoneOffset = firstPerson_object["firstPersonBoneOffset"];
            if (firstPersonBoneOffset["x"].is_number()) {
                vrm->firstPerson.firstPersonBoneOffset[0] = firstPersonBoneOffset["x"].get<float>();
            }
            if (firstPersonBoneOffset["y"].is_number()) {
                vrm->firstPerson.firstPersonBoneOffset[1] = firstPersonBoneOffset["y"].get<float>();
            }
            if (firstPersonBoneOffset["z"].is_number()) {
                vrm->firstPerson.firstPersonBoneOffset[2] = firstPersonBoneOffset["z"].get<float>();
            }
        }
    }

    // Disable mipmap setup (because it did not work with VRoid Hub)
    for (cgltf_size i = 0; i < data->samplers_count; ++i) {
        const auto sampler = &data->samplers[i];
        if (sampler->min_filter >= 9984) {
            sampler->min_filter = sampler->mag_filter;
        }
    }

    // materials
    if (vrm->materialProperties_count == 0) {
        vrm->materialProperties_count = data->materials_count;
        vrm->materialProperties = (cgltf_vrm_material_v0_0*)gltf_calloc(data->materials_count, sizeof(cgltf_vrm_material_v0_0));
        for (cgltf_size i = 0; i < data->materials_count; ++i) {
            vrm->materialProperties[i].name = gltf_alloc_chars(data->materials[i].name);
            vrm->materialProperties[i].renderQueue = 2000;

            auto materialProperties_object = output_config_object["materialProperties"];
            if (materialProperties_object.is_object()) {
                vrm->materialProperties[i].shader = gltf_alloc_chars(materialProperties_object["shader"].get<std::string>().c_str());

                vrm0_ensure_textureProperties(materialProperties_object, i, data);
                vrm0_ensure_floatProperties(materialProperties_object, i, data);
                vrm0_ensure_vectorProperties(materialProperties_object, i, data);
                vrm0_ensure_mapProperties(materialProperties_object, i, data);

            } else {
                vrm->materialProperties[i].shader = gltf_alloc_chars("VRM_USE_GLTFSHADER");
            }
        }
    }

    // blendshapes
    std::unordered_map<std::string, std::vector<cgltf_vrm_blendshape_bind_v0_0>> blendshapes;
    for (cgltf_size i = 0; i < data->meshes_count; ++i) {
        const auto mesh = &data->meshes[i];
        for (cgltf_size j = 0; j < mesh->target_names_count; ++j) {
            const auto target_name = mesh->target_names[j];
            if (strcmp(target_name, "viseme_aa") == 0) {
                vrm0_store_blendshapes("A", i, j, blendshapes);
            } else if (strcmp(target_name, "viseme_I") == 0) {
                vrm0_store_blendshapes("I", i, j, blendshapes);
            } else if (strcmp(target_name, "viseme_U") == 0) {
                vrm0_store_blendshapes("U", i, j, blendshapes);
            } else if (strcmp(target_name, "viseme_E") == 0) {
                vrm0_store_blendshapes("E", i, j, blendshapes);
            } else if (strcmp(target_name, "viseme_O") == 0) {
                vrm0_store_blendshapes("O", i, j, blendshapes);
            } else if (strcmp(target_name, "eyeBlinkLeft") == 0) {
                vrm0_store_blendshapes("Blink", i, j, blendshapes);
                vrm0_store_blendshapes("Blink_L", i, j, blendshapes);
            } else if (strcmp(target_name, "eyeBlinkRight") == 0) {
                vrm0_store_blendshapes("Blink", i, j, blendshapes);
                vrm0_store_blendshapes("Blink_R", i, j, blendshapes);
            } else if (strcmp(target_name, "browInnerUp") == 0) {
                vrm0_store_blendshapes("Joy", i, j, blendshapes);
                vrm0_store_blendshapes("Sorrow", i, j, blendshapes);
            } else if (strcmp(target_name, "mouthSmile") == 0) {
                vrm0_store_blendshapes("Joy", i, j, blendshapes);
            } else if (strcmp(target_name, "browOuterUpLeft") == 0) {
                vrm0_store_blendshapes("Angry", i, j, blendshapes);
            } else if (strcmp(target_name, "browOuterUpRight") == 0) {
                vrm0_store_blendshapes("Angry", i, j, blendshapes);
            } else if (strcmp(target_name, "eyeSquintLeft") == 0) {
                vrm0_store_blendshapes("Angry", i, j, blendshapes);
            } else if (strcmp(target_name, "eyeSquintRight") == 0) {
                vrm0_store_blendshapes("Angry", i, j, blendshapes);
            } else if (strcmp(target_name, "mouthFrownLeft") == 0) {
                vrm0_store_blendshapes("Sorrow", i, j, blendshapes);
            } else if (strcmp(target_name, "mouthFrownRight") == 0) {
                vrm0_store_blendshapes("Sorrow", i, j, blendshapes);
            }
        }
    }
    data->vrm_v0_0.blendShapeMaster.blendShapeGroups_count = blendshapes.size();
    data->vrm_v0_0.blendShapeMaster.blendShapeGroups = (cgltf_vrm_blendshape_group_v0_0*)gltf_calloc(blendshapes.size(), sizeof(cgltf_vrm_blendshape_group_v0_0));
    cgltf_size index = 0;
    for (const auto item : blendshapes) {
        const auto values = item.second;
        const auto store_size = values.size() * sizeof(cgltf_vrm_blendshape_bind_v0_0);
        const auto binds = (cgltf_vrm_blendshape_bind_v0_0*)gltf_calloc(store_size, 1);
        memcpy_s(binds, store_size, values.data(), store_size);
        cgltf_vrm_blendshape_group_presetName_v0_0 preset_name;
        select_cgltf_vrm_blendshape_group_presetName_v0_0(gltf_str_tolower(item.first).c_str(), &preset_name);
        data->vrm_v0_0.blendShapeMaster.blendShapeGroups[index] = {
            gltf_alloc_chars(item.first.c_str()), preset_name,
            binds,
            item.second.size(),
            nullptr, 0, false
        };
        index++;
    }

    // check if skeleton is common root of joints
    for (cgltf_size i = 0; i < data->skins_count; ++i) {
        const auto skin = &data->skins[i];
        if (skin->skeleton == nullptr)
            continue;

        for (cgltf_size j = 0; j < skin->joints_count; ++j) {
            const auto joint = skin->joints[j];

            // it's ok to point to joint itself
            if (skin->skeleton == joint)
                continue;

            auto parent = joint->parent;
            bool found = false;
            GLTF_PARENT_LOOP_BEGIN(parent != nullptr)
            if (skin->skeleton == parent) {
                found = true;
                break;
            }
            parent = parent->parent;
            GLTF_PARENT_LOOP_END
            // SKIN_SKELETON_INVALID: Skeleton node is not a common root
            if (!found) {
                skin->skeleton = nullptr;
                break;
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
        } else if (key == "licenseName") {
            if (!select_cgltf_vrm_meta_licenseName_v0_0(value, &meta->licenseName)) {
                std::cout << "[ERROR] Unknown " << key << ": " << value << std::endl;
            }
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
    humanoid->humanBones = (cgltf_vrm_humanoid_bone_v0_0*)gltf_calloc(humanoid->humanBones_count, sizeof(cgltf_vrm_humanoid_bone_v0_0));

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

        dst->center = (cgltf_float*)gltf_calloc(3, sizeof(cgltf_float));
        dst->max = (cgltf_float*)gltf_calloc(3, sizeof(cgltf_float));
        dst->min = (cgltf_float*)gltf_calloc(3, sizeof(cgltf_float));

        const auto found = mappings->node_index_map.find(node->name);
        if (found != mappings->node_index_map.end()) {

            // VRM uses lower camel case
            auto bone_name_lower = bone_name;
            bone_name_lower[0] = (unsigned char)std::tolower(bone_name_lower[0]);

            dst->node = found->second;
            select_cgltf_vrm_humanoid_bone_bone_v0_0(bone_name_lower.c_str(), &dst->bone);

            if (bone_name == "Head") {
                vrm->firstPerson.firstPersonBone = found->second;
            }
        } else {
            std::cout << "[ERROR] bone is not found for " << bone_name << std::endl;
        }

        i++;
    }

    return true;
}
