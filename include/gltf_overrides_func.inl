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

#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;

static bool gltf_override_material_values(json& values, cgltf_material* material)
{
    for (auto item : values.items()) {
        const auto key = item.key();
        const auto value_object = item.value();
        if (item.key() == "alphaMode" && value_object.is_string()) {
            const auto value = value_object.get<std::string>();
            if (value == "OPAQUE") {
                material->alpha_mode = cgltf_alpha_mode_opaque;
            } else if (value == "MASK") {
                material->alpha_mode = cgltf_alpha_mode_mask;
            } else if (value == "BLEND") {
                material->alpha_mode = cgltf_alpha_mode_blend;
            } else {
                std::cout << "[WARNING] Unknown alphaMode: " << value << std::endl;
            }
        }
    }
    return true;
}

static bool gltf_read_image_from_file(fs::path file, cgltf_data* data, cgltf_image* image, cgltf_buffer_view* buffer_view)
{
    // this should not happen but for safety
    if (data->buffers_count == 0)
        return false;

    const auto mime_type = gltf_get_image_mimetype(file.extension().u8string());
    if (!mime_type.empty())
        image->mime_type = gltf_alloc_chars(mime_type.c_str());

    // remove "uri"
    if (image->uri != nullptr) {
        gltf_free(image->uri);
        image->uri = nullptr;
    }

    if (image->buffer_view == nullptr) {

    } else {
    
    }

    return true;
}

static bool gltf_override_find_missing_textures(json& rules, cgltf_data* data, cgltf_material* material, AvatarBuild::cmd_options* options)
{
    json from_object = rules["find_missing_textures_from"];
    std::string from = from_object.is_string() ? from_object.get<std::string>() : "";

    // search missing textures based on:
    // 1. image name, 2. texture name, 3. material name
    // also take suffix into account: "_color", "_normal", "_specular"

    std::unordered_map<std::string, cgltf_image*> images;
    for (cgltf_size i = 0; i < data->images_count; ++i) {
        const auto image = &data->images[i];
        const std::string image_name = image->name == nullptr ? "" : image->name;
        if (!image_name.empty()) {
            images.emplace(image->name, image);
            images.emplace(std::string(image->name) + "_color", image);
        }
    }

    std::unordered_map<std::string, cgltf_texture*> textures;
    for (cgltf_size i = 0; i < data->textures_count; ++i) {
        const auto texture = &data->textures[i];
        const std::string texture_name = texture->name == nullptr ? "" : texture->name;
        if (!texture_name.empty()) {
            textures.emplace(texture_name, texture);
            textures.emplace(texture_name + "_color", texture);
        }
    }

    const fs::path parent_path = fs::path(options->output_config).parent_path();
    const fs::path path_from = parent_path / from;

    for (const auto& entry : fs::directory_iterator(path_from)) {
        const auto file = entry.path();
        const auto ext = file.extension();

        if (ext != ".png" && ext != ".jpg" && ext != ".jpeg")
            continue;

        const auto stem = file.stem().u8string();

        const auto images_found = images.find(stem);
        if (images_found != images.end()) {
            gltf_read_image_from_file(file, data, images_found->second);
            break;
        }

        const auto textures_found = textures.find(stem);
        if (textures_found != textures.end()) {
            gltf_read_image_from_file(file, data, textures_found->second->image);
            break;
        }

        if (material->name != nullptr) {
            std::string name = material->name;
            if (stem == name || stem == (name + "_color") && 
                material->has_pbr_metallic_roughness && material->pbr_metallic_roughness.base_color_texture.texture != nullptr) {
                gltf_read_image_from_file(file, data, material->pbr_metallic_roughness.base_color_texture.texture->image);
                break;
            }
        }
    }

    // create new buffer view for images (needs allocation)
    // cgltf_buffer* buffer = (cgltf_buffer*)gltf_calloc(1, sizeof(cgltf_buffer)); // will be freed at cgltf_free()

    return true;
}

static bool gltf_override_materials(json& materials_override, cgltf_data* data, AvatarBuild::cmd_options* options)
{
    (void)data;

    auto rules = materials_override["rules"];
    auto values = materials_override["values"];

    if (!rules.is_object())
        return false;

    for (cgltf_size i = 0; i < data->materials_count; ++i) {
        auto material = &data->materials[i];
        bool maches = true; // true when all rules matched (or there's no rules found)
        for (auto& rule : rules.items()) {
            const auto& key = rule.key();
            const auto& pattern = rule.value();
            if (!pattern.is_string())
                continue;

            std::regex re(pattern.get<std::string>());

            if (key == "name") {
                if (!std::regex_match(material->name, re)) {
                    maches = false;
                    break;
                }
            }
        }
        if (maches) {
            if (values.is_object()) {
                gltf_override_material_values(values, material);
            }
            gltf_override_find_missing_textures(rules, data, material, options);
        }
    }

    return true;
}

static bool gltf_override_parameters(json& overrides_object, cgltf_data* data, AvatarBuild::cmd_options* options)
{
    (void)data, overrides_object;
    auto materials_overrides = overrides_object["materials"];
    if (materials_overrides.is_array()) {
        for (auto item : materials_overrides) {
            gltf_override_materials(item, data, options);
        }
    }

    return true;
}
