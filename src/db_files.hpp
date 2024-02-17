#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

#include "com_defs.hpp"

NO_DISCARD std::filesystem::path  DB_ImagePath (std::string_view image_name );
NO_DISCARD std::string            DB_LoadShader(std::string_view shader_name);
NO_DISCARD std::vector<std::byte> DB_LoadFont  (std::string_view font_name  );
NO_DISCARD std::vector<std::byte> DB_LoadImage (std::string_view image_name );