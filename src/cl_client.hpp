#pragma once

#include <string_view>

#include "acommon/acommon.h"
#include "acommon/a_math.h"

#include "fs_files.h"

enum : uint32_t {
	MAP_HEADER_HEAD = A_MAKE_FOURCC('h', 'e', 'a', 'd'),
	MAP_HEADER_FOOT = A_MAKE_FOURCC('f', 'o', 'o', 't'),
	TAGS_FOOT = A_MAKE_FOURCC('t', 'a', 'g', 's'),

	TAGS_MAX_SIZE_XBOX = 22 * 1024 * 1024,
	TAGS_MAX_SIZE_GEARBOX = 23 * 1024 * 1024,

	TAGS_BASE_ADDR_XBOX = 0x803A6000,
	TAGS_BASE_ADDR_GEARBOX = 0x40440000
};

typedef enum MapEngine : uint32_t {
	MAP_ENGINE_XBOX = 5,
	MAP_ENGINE_GEARBOX = 7
} MapEngine;

typedef enum ScenarioType : uint16_t {
	SCENARIO_TYPE_SINGELPLAYER,
	SCENARIO_TYPE_MULTILPLAYER,
	SCENARIO_TYPE_UI,
	SCENARIO_TYPE_COUNT
} ScenarioType;

typedef enum TagFourCC : uint32_t {
	TAG_FOURCC_SCENARIO = A_MAKE_FOURCC('s', 'c', 'n', 'r'),
	TAG_FOURCC_SBSP = A_MAKE_FOURCC('s', 'b', 's', 'p'),
	TAG_FOURCC_BITMAP = A_MAKE_FOURCC('b', 'i', 't', 'm'),
	TAG_FOURCC_SHADER = A_MAKE_FOURCC('s', 'h', 'd', 'r'),
	TAG_FOURCC_SHADER_ENVIRONMENT = A_MAKE_FOURCC('s', 'e', 'n', 'v')
} TagFourCC;

A_PACK(struct MapHeader {
	uint32_t     head_magic;
	MapEngine    engine;
	uint32_t     decompressed_file_size, compressed_padding;
	uint32_t     tag_data_offset, tag_data_size;
	uint64_t     __pad1;
	char         name[32], build[32];
	ScenarioType type;
	uint16_t     __pad2;
	uint32_t     crc32;
	char         __pad3[1940];
	uint32_t     foot_magic;
});
A_STATIC_ASSERT(sizeof(MapHeader) == 2048);

union TagId {
	uint32_t id;
	uint16_t index;
};
A_STATIC_ASSERT(sizeof(TagId) == 4);

A_PACK(struct Tag {
	uint32_t primary_class, secondary_class, tertiary_class;
	TagId    tag_id;
	uint32_t tag_path;
	uint32_t tag_data;
	uint32_t external;
	uint32_t __pad;
});
A_STATIC_ASSERT(sizeof(Tag) == 32);

A_PACK(struct TagDependency {
	uint32_t fourcc, path_pointer, path_size;
	TagId id;
});
A_STATIC_ASSERT(sizeof(TagDependency) == 16);

A_PACK(struct TagDataOffset {
	uint32_t size, external, file_offset;
	void* pointer;
#if A_POINTER_SIZE == 32
	uint32_t            __pad;
#elif A_POINTER_SIZE != 64
#error "pointer size???"
#endif
});
A_STATIC_ASSERT(sizeof(TagDataOffset) == 20);

A_PACK(struct TagReflexive {
	uint32_t count;
	void* pointer;
#if A_POINTER_SIZE == 32
	uint32_t            __pad;
#elif A_POINTER_SIZE != 64
#error "pointer size???"
#endif
});
A_STATIC_ASSERT(sizeof(TagReflexive) == 12);

A_PACK(struct TagHeaderPC {
	uint32_t tag_ptr;
	TagId scenario_tag_id;
	uint32_t checksum, tag_count, model_part_count, model_data_file_offset;
	uint32_t model_part_count_2, vertex_data_size, model_data_size;
	uint32_t magic;
});
A_STATIC_ASSERT(sizeof(TagHeaderPC) == 40);

A_PACK(struct TagHeaderXbox {
	uint32_t tag_ptr;
	TagId scenario_tag_id;
	uint32_t checksum, tag_count, model_part_count, vertex_data_ptr;
	uint32_t model_part_count_2, triangle_data_ptr;
	uint32_t magic;
});
A_STATIC_ASSERT(sizeof(TagHeaderXbox) == 36);

A_PACK(struct TagHeaderCommon {
	uint32_t tag_ptr;
	TagId scenario_tag_id;
	uint32_t checksum, tag_count, model_part_count;
});
A_STATIC_ASSERT(sizeof(TagHeaderCommon) == 20);

A_PACK(struct ScenarioBSP {
	uint32_t bsp_start, bsp_size, bsp_address, __pad;
	TagDependency structure_bsp;
});
A_STATIC_ASSERT(sizeof(ScenarioBSP) == 32);

enum ScenarioPlayerSpawnType: uint16_t {
	SCENARIO_PLAYER_SPAWN_TYPE_NONE,
	// ...
};

A_PACK(struct ScenarioPlayerSpawn {
	apoint3f_t              pos;
	float                   facing;
	uint16_t                team_index;
	uint16_t                bsp_index;
	ScenarioPlayerSpawnType type0, type1, type2, type3;
	char                    __pad[24];
});
A_STATIC_ASSERT(sizeof(ScenarioPlayerSpawn) == 52);

A_PACK(struct BSPHeader {
	uint32_t pointer;
	uint32_t lightmap_material_count;
	uint32_t rendered_vertices;
	uint32_t lightmap_material_count_2;
	uint32_t lightmap_vertices;
	uint32_t magic;
});
A_STATIC_ASSERT(sizeof(BSPHeader) == 24);

union TagHeader {
	TagHeaderPC     pc;
	TagHeaderXbox   xbox;
	TagHeaderCommon common;
};

A_PACK(struct BSPSurf {
	uint16_t verts[3];
});
A_STATIC_ASSERT(sizeof(BSPSurf) == 6);

A_PACK(struct BSPRenderedVertex {
	apoint3f_t pos;
	avec3f_t   normal, binormal, tangent;
	apoint2f_t tex_coords;
});
A_STATIC_ASSERT(sizeof(BSPRenderedVertex) == 56);

A_PACK(struct BSPRenderedVertexCompressed {
	apoint3f_t pos;
	uint32_t   normal, binormal, tangent;
	apoint2f_t tex_coords;
});
A_STATIC_ASSERT(sizeof(BSPRenderedVertexCompressed) == 32);

A_PACK(struct BSPLightmapVertex {
	avec3f_t   normal;
	apoint2f_t tex_coords;
});
A_STATIC_ASSERT(sizeof(BSPLightmapVertex) == 20);

A_PACK(struct BSPLightmapVertexCompressed {
	uint32_t   normal;
	apoint2f_t tex_coords;
});
A_STATIC_ASSERT(sizeof(BSPLightmapVertexCompressed) == 12);

A_PACK(struct BSPCollSurf {
	uint32_t plane, first_edge;
	uint8_t  flags;
	int8_t   breakable_surface;
	uint16_t material;
});
A_STATIC_ASSERT(sizeof(BSPCollSurf) == 12);

A_PACK(struct BSPCollEdge {
	uint32_t start_vert, end_vert;
	uint32_t forward_edge, reverse_edge;
	uint32_t left_surf, right_surf;
});

A_PACK(struct BSPCollVertex {
	avec3f_t point;
	uint32_t first_edge;
});

enum BSPMaterialType: uint16_t {
	BSP_MATERIAL_DIRT,
	BSP_MATERIAL_SAND,
	BSP_MATERIAL_STONE,
	BSP_MATERIAL_SNOW,
	BSP_MATERIAL_WOOD,
	BSP_MATERIAL_METAL_HOLLOW,
	BSP_MATERIAL_METAL_THIN,
	BSP_MATERIAL_METAL_THICK,
	BSP_MATERIAL_RUBBER,
	BSP_MATERIAL_GLASS,
	BSP_MATERIAL_FORCE_FIELD,
	BSP_MATERIAL_GRUNT,
	BSP_MATERIAL_HUNTER_ARMOR,
	BSP_MATERIAL_HUNTER_SKIN,
	BSP_MATERIAL_ELITE,
	BSP_MATERIAL_JACKAL,
	BSP_MATERIAL_JACKAL_ENERGY_SHIELD,
	BSP_MATERIAL_ENGINEER_SKIN,
	BSP_MATERIAL_ENGINERR_FORCE_FIELD,
	BSP_MATERIAL_FLOOD_COMBAT_FORM,
	BSP_MATERIAL_FLOOD_CARRIER_FORM,
	BSP_MATERIAL_CYBORG_ARMOR,
	BSP_MATERIAL_CYBORT_ENERGY_SHIELD,
	BSP_MATERIAL_HUMAN_ARMOR,
	BSP_MATERIAL_HUMAN_SKIN,
	BSP_MATERIAL_SENTINEL,
	BSP_MATERIAL_MONITOR,
	BSP_MATERIAL_PLASTIC,
	BSP_MATERIAL_WATER,
	BSP_MATERIAL_LEAVES,
	BSP_MATERIAL_ELITE_ENERGY_SHIELD,
	BSP_MATERIAL_ICE,
	BSP_MATERIAL_HUNTER_SHIELD,
	BSP_MATERIAL_COUNT,
};

enum BSPBitmapDataType: uint16_t {
	BSP_BITMAP_DATA_TYPE_2D_TEXTURE,
	BSP_BITMAP_DATA_TYPE_3D_TEXTURE,
	BSP_BITMAP_DATA_TYPE_CUBE_MAP,
	BSP_BITMAP_DATA_TYPE_WHITE,
	BSP_BITMAP_DATA_TYPE_COUNT
};

enum BSPBitmapDataFormat: uint16_t {
	BSP_BITMAP_DATA_FORMAT_A8,
	BSP_BITMAP_DATA_FORMAT_Y8,
	BSP_BITMAP_DATA_FORMAT_AY8,
	BSP_BITMAP_DATA_FORMAT_A8Y8,
	BSP_BITMAP_DATA_FORMAT_R5G6B5 = 6,
	BSP_BITMAP_DATA_FORMAT_A1R5G5B5 = 8,
	BSP_BITMAP_DATA_FORMAT_A4R4G4B4,
	BSP_BITMAP_DATA_FORMAT_X8R8G8B8,
	BSP_BITMAP_DATA_FORMAT_A8R8G8B8,
	BSP_BITMAP_DATA_FORMAT_DXT1 = 14,
	BSP_BITMAP_DATA_FORMAT_DXT3,
	BSP_BITMAP_DATA_FORMAT_DXT5,
	BSP_BITMAP_DATA_FORMAT_P8_BUMP,
	BSP_BITMAP_DATA_FORMAT_BC7,
	BSP_BITMAP_DATA_FORMAT_COUNT
};

enum BSPBitmapType: uint16_t {
	BSP_BITMAP_TYPE_2D_TEXTURES,
    BSP_BITMAP_TYPE_3D_TEXTURES,
    BSP_BITMAP_TYPE_CUBE_MAPS,
    BSP_BITMAP_TYPE_SPRITES,
    BSP_BITMAP_TYPE_UI,
    BSP_BITMAP_TYPE_COUNT
};

enum BSPBitmapFormat: uint16_t {
	BSP_BITMAP_FORMAT_DXT1,
    BSP_BITMAP_FORMAT_DXT3,
    BSP_BITMAP_FORMAT_DXT5,
    BSP_BITMAP_FORMAT_16_BIT,
    BSP_BITMAP_FORMAT_32_BIT,
    BSP_BITMAP_FORMAT_MONOCHROME,
    BSP_BITMAP_FORMAT_BC7,
	BSP_BITMAP_FORMAT_COUNT
};

enum BSPBitmapUsage: uint16_t {
    BSP_BITMAP_USAGE_ALPHA_BLEND,
    BSP_BITMAP_USAGE_DEFAULT,
    BSP_BITMAP_USAGE_HEIGHT_MAP,
    BSP_BITMAP_USAGE_DETAIL_MAP,
    BSP_BITMAP_USAGE_LIGHT_MAP,
    BSP_BITMAP_USAGE_VECTOR_MAP,
    BSP_BITMAP_USAGE_COUNT
};

enum BSPBitmapSpriteBudgetSize: uint16_t {
    BSP_BITMAP_SPRITE_BUDGET_SIZE_32x32,
    BSP_BITMAP_SPRITE_BUDGET_SIZE_64x64,
    BSP_BITMAP_SPRITE_BUDGET_SIZE_128x128,
    BSP_BITMAP_SPRITE_BUDGET_SIZE_256x256,
    BSP_BITMAP_SPRITE_BUDGET_SIZE_512x512,
    BSP_BITMAP_SPRITE_BUDGET_SIZE_1024x1024,
    BSP_BITMAP_SPRITE_BUDGET_SIZE_COUNT
};

enum BSPBitmapSpriteUsage: uint16_t {
    BSP_BITMAP_SPRITE_USAGE_BLEND_ADD_SUBTRACT_MAX,
    BSP_BITMAP_SPRITE_USAGE_MULTIPLY_MIN,
    BSP_BITMAP_SPRITE_USAGE_DOUBLE_MULTIPLY,
    BSP_BITMAP_SPRITE_USAGE_COUNT
};

enum BSPShaderDetailLevel: uint16_t {
	BSP_SHADER_DETAIL_LEVEL_HIGH,
	BSP_SHADER_DETAIL_LEVEL_MEDIUM,
	BSP_SHADER_DETAIL_LEVEL_LOW,
	BSP_SHADER_DETAIL_LEVEL_AWFUL
};

enum BSPShaderType: uint16_t {
	// TODO
};

enum BSPShaderEnvironmentType: uint16_t {
	BSP_SHADER_ENVIRONMENT_TYPE_NORMAL,
	BSP_SHADER_ENVIRONMENT_TYPE_BLENDED,
	BSP_SHADER_ENVIRONMENT_TYPE_BLENDED_BASE_SPECULAR
};

enum BSPShaderDetailFunction: uint16_t {
	BSP_SHADER_DETAIL_FUNCTION_DOUBLE_BIASED_MULTIPLY,
	BSP_SHADER_DETAIL_FUNCTION_MULTIPLY,
	BSP_SHADER_DETAIL_FUNCTION_DOUBLE_BIASED_ADD,
	BSP_SHADER_DETAIL_FUNCTION_COUNT
};

// #define DISCRIMINANT_TO_STRING(a) [a] = A_STRINGIFY(a)

// inline const char* BSPMaterialType_to_string(BSPMaterialType type) {
// 	static const char* names[] = {
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_DIRT),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_SAND),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_STONE),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_SNOW),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_WOOD),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_METAL_HOLLOW),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_METAL_THIN),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_METAL_THICK),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_RUBBER),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_GLASS),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_FORCE_FIELD),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_GRUNT),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_HUNTER_ARMOR),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_HUNTER_SKIN),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_ELITE),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_JACKAL),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_JACKAL_ENERGY_SHIELD),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_ENGINEER_SKIN),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_ENGINERR_FORCE_FIELD),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_FLOOD_COMBAT_FORM),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_FLOOD_CARRIER_FORM),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_CYBORG_ARMOR),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_CYBORT_ENERGY_SHIELD),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_HUMAN_ARMOR),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_HUMAN_SKIN),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_SENTINEL),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_MONITOR),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_PLASTIC),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_WATER),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_LEAVES),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_ELITE_ENERGY_SHIELD),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_ICE),
// 		DISCRIMINANT_TO_STRING(BSP_MATERIAL_HUNTER_SHIELD)
// 	};

// 	if(type >= BSP_MATERIAL_COUNT) 
// 		return NULL;
	
// 	return names[type];
// }

// inline const char* BSPBitmapDataType_to_string(BSPBitmapDataType type) {
// 	assert(type < BSP_BITMAP_DATA_TYPE_COUNT && "invalid type");
	
// 	static const char* names[] = {
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_TYPE_2D_TEXTURE),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_TYPE_3D_TEXTURE),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_TYPE_CUBE_MAP),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_TYPE_WHITE)
// 	};

// 	if (type >= BSP_BITMAP_DATA_TYPE_COUNT)
// 		return NULL;

// 	return names[type];
// }

// inline const char* BSPBitmapType_to_string(BSPBitmapType type) {
// 	assert(type < BSP_BITMAP_TYPE_COUNT && "invalid type");

// 	static const char* names[] = {
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_TYPE_2D_TEXTURES),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_TYPE_3D_TEXTURES),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_TYPE_CUBE_MAPS),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_TYPE_SPRITES),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_TYPE_UI)
// 	};

// 	return names[type];
// }

// inline const char* BSPBitmapDataFormat_to_string(BSPBitmapDataFormat format) {
// 	assert(format < BSP_BITMAP_DATA_FORMAT_COUNT && "invalid format");

// 	static const char* names[] = {
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_A8),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_Y8),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_AY8),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_A8Y8),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_R5G6B5),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_A1R5G5B5),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_A4R4G4B4),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_X8R8G8B8),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_A8R8G8B8),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_DXT1),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_DXT3),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_DXT5),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_P8_BUMP),
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_DATA_FORMAT_BC7)
// 	};

// 	return names[format];
// }

// inline const char* BSPBitmapFormat_to_string(BSPBitmapFormat format) {
// 	assert(format < BSP_BITMAP_FORMAT_COUNT && "invalid format");

// 	static const char* names[] = {
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_FORMAT_DXT1),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_FORMAT_DXT3),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_FORMAT_DXT5),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_FORMAT_16_BIT),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_FORMAT_32_BIT),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_FORMAT_MONOCHROME),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_FORMAT_BC7)
// 	};

// 	return names[format];
// }

// inline const char* BSPBitmapUsage_to_string(BSPBitmapUsage usage) {
// 	assert(usage < BSP_BITMAP_USAGE_COUNT && "invalid usage");

// 	static const char* names[] = {
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_USAGE_ALPHA_BLEND),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_USAGE_DEFAULT),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_USAGE_HEIGHT_MAP),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_USAGE_DETAIL_MAP),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_USAGE_LIGHT_MAP),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_USAGE_VECTOR_MAP)
// 	};

// 	return names[usage];
// }

// inline const char* BSPBitmapSpriteBudgetSize_to_string(BSPBitmapSpriteBudgetSize size) {
// 	assert(size < BSP_BITMAP_SPRITE_BUDGET_SIZE_COUNT && "invalid size");

// 	static const char* names[] = {
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_SPRITE_BUDGET_SIZE_32x32),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_SPRITE_BUDGET_SIZE_64x64),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_SPRITE_BUDGET_SIZE_128x128),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_SPRITE_BUDGET_SIZE_256x256),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_SPRITE_BUDGET_SIZE_512x512),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_SPRITE_BUDGET_SIZE_1024x1024)
// 	};

// 	return names[size];
// }

// inline const char* BSPBitmapSpriteUsage_to_string(BSPBitmapSpriteUsage usage) {
// 	assert(usage < BSP_BITMAP_SPRITE_USAGE_COUNT && "invalid usage");

// 	static const char* names[] = {
// 		DISCRIMINANT_TO_STRING(BSP_BITMAP_SPRITE_USAGE_BLEND_ADD_SUBTRACT_MAX),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_SPRITE_USAGE_MULTIPLY_MIN),
//     	DISCRIMINANT_TO_STRING(BSP_BITMAP_SPRITE_USAGE_DOUBLE_MULTIPLY)
// 	};

// 	return names[usage];
// }

// #undef DISCRIMINANT_TO_STRING

A_PACK(struct BSPCollisionMaterial {
	TagDependency shader;
	uint16_t         __pad;
	BSPMaterialType  material;
});
A_STATIC_ASSERT(sizeof(BSPCollisionMaterial) == 20);

A_PACK(struct BSPBitmapData {
	uint32_t            klass;
	uint16_t            width, height, depth;
	BSPBitmapDataType   type;
	BSPBitmapDataFormat format;
	uint16_t            flags;
	apoint2s_t          registration_point;
	uint16_t            mipmap_count;
	uint16_t            __pad1;
	uint32_t            pixel_data_offset;
	uint32_t            pixel_data_size;
	TagId               bitmap_tag_id;
	void*               pixels;
#if A_POINTER_SIZE == 32
	uint32_t            __pad2;
#elif A_POINTER_SIZE != 64
#error "pointer size???"
#endif
	uint32_t           actual_size;
});
A_STATIC_ASSERT(sizeof(BSPBitmapData) == 48);

A_PACK(struct BSPBitmap {
	BSPBitmapType             type;
	BSPBitmapFormat           format;
	BSPBitmapUsage            usage;
	uint16_t                  flags;
	float                     detail_fade_factor, sharpen_amount, bump_height;
	BSPBitmapSpriteBudgetSize size;
	uint16_t                  sprite_budget_count;
	uint16_t                  color_palate_width, color_palate_height;
	TagDataOffset             compressed_color_palate_data;
	TagDataOffset             processed_pixel_data;
	float                     blur_filter_size, alpha_bias;
	uint16_t                  mipmap_count;
	BSPBitmapSpriteUsage      sprite_usage;
	uint16_t                  sprite_spacing;
	uint16_t                  __pad;
	TagReflexive              bitmap_group_sequence, bitmap_data;
});
A_STATIC_ASSERT(sizeof(BSPBitmap) == 108);

enum BSPVertexType: uint16_t {
	BSP_VERTEX_TYPE_UNCOMPRESSED_RENDERED,
	BSP_VERTEX_TYPE_COMPRESSED_RENDERED,
	BSP_VERTEX_TYPE_UNCOMPRESSED_LIGHTMAP,
	BSP_VERTEX_TYPE_COMPRESSED_LIGHTMAP,
	BSP_VERTEX_TYPE_UNCOMPRESSED_MODEL,
	BSP_VERTEX_TYPE_COMPRESSED_MODEL,
	BSP_VERTEX_TYPE_COUNT
};

A_PACK(struct BSPMaterial {
	TagDependency shader;
	uint16_t shader_permutation;
	uint16_t flags;
	uint32_t surfaces, surface_count;
	apoint3f_t centroid;
	acolor_rgb_t ambient_color;
	uint16_t distant_light_count;
	uint16_t __pad1;
	acolor_rgb_t distant_light_0_color;
	avec3f_t distant_light_0_direction;
	acolor_rgb_t distant_light_1_color;
	avec3f_t distant_light_1_direction;
	uint8_t __pad2[12];
	acolor_argb_t reflection_tint;
	avec3f_t shadow_vector;
	acolor_rgb_t shadow_color;
	aplane3f_t plane;
	uint16_t breakable_surface;
	uint16_t __pad3;
	BSPVertexType rendered_vertices_type;
	uint16_t __pad4;
	uint32_t rendered_vertices_count, rendered_vertices_offset;
	uint32_t __pad5;
	uint32_t rendered_vertices_index_pointer;
	BSPVertexType lightmap_vertices_type;
	uint16_t __pad6;
	uint32_t lightmap_vertices_count, lightmap_vertices_offset;
	uint32_t __pad7;
	uint32_t lightmap_vertices_index_pointer;
	TagDataOffset uncompressed_vertices, compressed_vertices;
});
A_STATIC_ASSERT(sizeof(BSPMaterial) == 256);

A_PACK(struct BSPLightmap {
	uint16_t bitmap_data_index;
	uint8_t __pad[18];
	TagReflexive materials;
});
A_STATIC_ASSERT(sizeof(BSPLightmap) == 32);

A_PACK(struct BSPShader {
	uint16_t             flags;
	BSPShaderDetailLevel detail_level;
	float                power;
	acolor_rgb_t         emitted_light_color, tint_color;
	uint16_t             physics_flags;
	BSPMaterialType      material_type;
	BSPShaderType        type;
	uint16_t             __pad;
});
A_STATIC_ASSERT(sizeof(BSPShader) == 40);

enum ShaderWaveFunction: uint16_t {
	SHADER_WAVE_FUNCTION_ONE
	// ...
};

A_PACK(struct BSPShaderEnvironment {
	BSPShader                   base;
	uint16_t                    flags;
	BSPShaderEnvironmentType    type;
	float                       lens_flare_spacing;
	TagDependency               lens_flare;
	char                        __pad1[44];
	uint16_t                    diffuse_flags;
	char                        __pad2[26];
	TagDependency               base_map;
	char                        __pad3[24];
	BSPShaderDetailFunction     detail_map_function;
	uint16_t                    __pad4;
	float                       primary_detail_map_scale;
	TagDependency               primary_detail_map;
	float                       secondary_detail_map_scale;
	TagDependency               secondary_detail_map;
	char                        __pad5[24];
	BSPShaderDetailFunction     micro_detail_map_function;
	uint16_t                    __pad6;
	float                       micro_detail_map_scale;
	TagDependency               micro_detail_map;
	acolor_rgb_t                material_color;
	char                        __pad7[12];
	float                       bump_map_scale;
	TagDependency               bump_map;
	apoint2f_t                  bump_map_scale_xy;
	char                        __pad8[16];
	ShaderWaveFunction          u_anim_fn;
	uint16_t                    __pad9;
	float                       u_anim_period, u_anim_scale;
	ShaderWaveFunction          v_anim_function;
	uint16_t                    __pad10;
	float                       v_anim_period, v_anim_scale;
	char                        __pad11[24];
	uint16_t                    self_illumination_flags;
	char                        __pad12[26];
	acolor_rgb_t                primary_on_color, primary_off_color;
	ShaderWaveFunction          primary_anim_function;
	uint16_t                    __pad13;
	float                       primary_anim_period, primary_anim_phase;
	char                        __pad14[24];
	acolor_rgb_t                secondary_on_color, secondary_off_color;
	ShaderWaveFunction          secondary_anim_function;
	uint16_t                    __pad15;
	float                       secondary_anim_period, secondary_anim_phase;
	char                        __pad16[24];
	acolor_rgb_t                plasma_on_color, plasma_off_color;
	ShaderWaveFunction          plasma_anim_function;
	uint16_t                    __pad17;
	float                       plasma_anim_period, plasma_anim_phase;
	char                        __pad18[24];
	float                       map_scale;
	TagDependency               map;
	char                        __pad19[224];
});
A_STATIC_ASSERT(sizeof(BSPShaderEnvironment) == 836);

A_EXTERN_C Tag*               CL_Map_Tag(TagId id);
A_EXTERN_C BSPSurf*       	  CL_Map_Surfs(void);
A_EXTERN_C uint32_t       	  CL_Map_SurfCount(void);
A_EXTERN_C BSPRenderedVertex* CL_Map_RenderedVertices(void);
A_EXTERN_C BSPLightmapVertex* CL_Map_LightmapVertices(void);
A_EXTERN_C BSPCollSurf*   	  CL_Map_CollSurfs(void);
A_EXTERN_C uint32_t       	  CL_Map_CollSurfCount(void);
A_EXTERN_C BSPCollEdge*   	  CL_Map_CollEdges(void);
A_EXTERN_C uint32_t       	  CL_Map_CollEdgeCount(void);
A_EXTERN_C BSPCollVertex* 	  CL_Map_CollVertices(void);
A_EXTERN_C uint32_t       	  CL_Map_CollVertexCount(void);
A_EXTERN_C BSPLightmap*       CL_Map_Lightmap(uint16_t i);
A_EXTERN_C uint32_t           CL_Map_LightmapCount(void);

A_EXTERN_C bool     CL_BitmapDataFormatIsCompressed(BSPBitmapDataFormat format);
A_EXTERN_C size_t   CL_BitmapDataFormatBPP(BSPBitmapDataFormat format);
A_EXTERN_C bool     CL_LoadMap			  (std::string_view map_name);
