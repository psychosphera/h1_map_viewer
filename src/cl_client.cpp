#include "cl_client.hpp"
#include "cl_client.h"

#include <cassert>

#include <zlib.h>

#include "acommon/a_format.hpp"
#include "acommon/z_mem.h"

#include "cg_cgame.h"
#include "com_print.hpp"
#include "db_files.hpp"
#include "dvar.h"
#include "gfx.h"
#include "gfx_text.h"
#include "in_input.h"

#define BSP_MAX_COLLISION_MATERIALS 512

A_EXTERN_C dvar_t* vid_width;
A_EXTERN_C dvar_t* vid_height;

A_EXTERN_C Tag* CL_Map_Tag(TagId id);

struct cll_t {
	KeyFocus keyfocus    = KF_GAME;
	bool drawDevGui      = false;
	size_t fpsTextDrawId = (size_t)-1;
};

std::array<cll_t, MAX_LOCAL_CLIENTS> s_cll;
struct cl_t {
	dvar_t* drawfps  = NULL;
	bool hasKbmFocus = false;
};

std::array<cl_t, MAX_LOCAL_CLIENTS> s_cl;

dvar_t* cl_splitscreen = NULL;

static uint64_t s_lastFpsDrawTime;
static uint64_t s_lastFpsDrawDelta;

cll_t* CL_GetLocalClientLocals(size_t localClientNum) {
	return &s_cll.at(localClientNum);
}

cl_t* CL_GetLocalClientGlobals(size_t localClientNum) {
	return &s_cl.at(localClientNum);
}

struct MapLoadData {
	StreamFile  		  f;
	std::string 		  map_name;
	void*       		  p;
	size_t      		  n;
	FileMapping           bitmaps_map;

	Tag*                  tags;
	uint32_t              tag_count;
	BSPSurf*              surfs;
	uint32_t              surf_count;
	BSPRenderedVertex*    rendered_vertices;
	BSPLightmapVertex*    lightmap_vertices;
	BSPCollSurf*    	  coll_surfs;
	uint32_t    		  coll_surf_count;
	BSPCollEdge*    	  coll_edges;
	uint32_t    		  coll_edge_count;
	BSPCollVertex*  	  coll_vertices;
	uint32_t    		  coll_vertex_count;
	BSPCollisionMaterial* collision_materials;
	uint32_t              lightmap_count;
	BSPLightmap*          lightmaps;

	Invader::HEK::ScenarioStructureBSP<Invader::HEK::NativeEndian>* bsp_ptr;
} g_load;

A_EXTERN_C void CL_Init(void) {
	A_memset((void*)&g_load, 0, sizeof(g_load));
	g_load.bitmaps_map = DB_LoadMap_Mmap("bitmaps.map");
	assert(g_load.bitmaps_map.p && g_load.bitmaps_map.n > 0);
	Com_DPrintln(
		"CL_Init: Successfully mapped bitmaps.map at 0x{:08X} ({} bytes)",
		(size_t)g_load.bitmaps_map.p, g_load.bitmaps_map.n
	);

	cl_splitscreen = Dvar_RegisterBool("cl_splitscreen", DVAR_FLAG_NONE, false);

	for (size_t i = 0; i < MAX_LOCAL_CLIENTS; i++) {
		cl_t* cl = CL_GetLocalClientGlobals(i);
		cl->drawfps = Dvar_RegisterLocalBool(i, "cl_drawfps", DVAR_FLAG_NONE, false);
		CL_GetLocalClientLocals(i)->drawDevGui = false;
		CL_SetKeyFocus(i, KF_GAME);
		RectDef rect = { .x = 0.985f, .y = 0.99f, .w = 0.0498f, .h = 0.0498f };
		acolor_rgb_t color = A_color_rgb(0.5, 0.8f, 0.2f);
		bool b = R_AddTextDrawDef(
			i, NULL, &rect,
			A_Format("FPS: {:.0f}", 1000.0f / s_lastFpsDrawDelta).c_str(),
			0.5f, 0.5f,
			color, Dvar_GetBool(cl->drawfps), true,
			&CL_GetLocalClientLocals(i)->fpsTextDrawId
		);
		assert(b);
		(void)b;
	}

	CL_GiveKbmFocus(0);
	
	bool b = CL_LoadMap("c40_xbox.map");
	assert(b);
	(void)b;
}

A_EXTERN_C void CL_EnableFpsCounter(size_t localClientNum, bool enable) {
	Dvar_SetBool(CL_GetLocalClientGlobals(localClientNum)->drawfps, enable);
}

A_EXTERN_C void CL_DrawFps(size_t localClientNum) {
	R_UpdateTextDrawDef(
		localClientNum, CL_GetLocalClientLocals(localClientNum)->fpsTextDrawId,
		A_Format("FPS: {:.0f}", 1000.0f / (float)s_lastFpsDrawDelta).c_str()
	);
}

A_EXTERN_C void CL_Frame(void) {
	for (size_t i = 0; i < MAX_LOCAL_CLIENTS; i++) {
		cl_t*  cl  = CL_GetLocalClientGlobals(i);
		cll_t* cll = CL_GetLocalClientLocals(i);
		R_ActivateTextDrawDef(i, cll->fpsTextDrawId, Dvar_GetBool(cl->drawfps));

		// Updating the FPS counter too often makes it flicker
		if (Sys_Milliseconds() - s_lastFpsDrawTime > 40) {
			s_lastFpsDrawTime  = Sys_Milliseconds();
			s_lastFpsDrawDelta = Com_LastFrameTimeDelta();
		}

		if (Dvar_GetBool(cl->drawfps))
			CL_DrawFps(i);

		cll->drawDevGui = IN_Key_IsToggled(i, IN_KEYCODE_TILDE);
		if (cll->drawDevGui)
			CL_SetKeyFocus(i, KF_DEVGUI);
		else
			CL_SetKeyFocus(i, KF_GAME);
	}	
} 

A_EXTERN_C void CL_EnterSplitscreen(size_t activeLocalClient) {
	cg_t* cg0 = CG_GetLocalClientGlobals(0);

	cg0->viewport.x = 0.0f;
	cg0->viewport.y = 0.5f;
	cg0->viewport.w = 1.0f;
	cg0->viewport.h = 0.5f;

	float w0 = cg0->viewport.w * Dvar_GetInt(vid_width);
	float h0 = cg0->viewport.h * Dvar_GetInt(vid_height);
	cg0->fovy = R_FovHorzToVertical(Dvar_GetFloat(cg0->fov), h0 / w0);

	cg_t* cg1 = CG_GetLocalClientGlobals(1);

	cg1->viewport.x = 0.0f;
	cg1->viewport.y = 0.0f;
	cg1->viewport.w = 1.0f;
	cg1->viewport.h = 0.5f;

	float w1 = cg1->viewport.w * Dvar_GetInt(vid_width);
	float h1 = cg1->viewport.h * Dvar_GetInt(vid_height);
	cg1->fovy = R_FovHorzToVertical(Dvar_GetFloat(cg1->fov), h1 / w1);

	if (activeLocalClient != 0)
		CG_ActivateLocalClient(0);
	if (activeLocalClient != 1)
		CG_ActivateLocalClient(1);
}

A_EXTERN_C void CL_LeaveSplitscreen(size_t activeLocalClient) {
	for (size_t i = 0; i < MAX_LOCAL_CLIENTS; i++) {
		cg_t* cg = CG_GetLocalClientGlobals(i);
		cg->viewport.x = 0.0f;
		cg->viewport.y = 0.0f;
		cg->viewport.w = 1.0f;
		cg->viewport.h = 1.0f;

		float w = cg->viewport.w * Dvar_GetInt(vid_width);
		float h = cg->viewport.h * Dvar_GetInt(vid_height);
		cg->fovy = R_FovHorzToVertical(Dvar_GetFloat(cg->fov), h / w);

		if (i != activeLocalClient)
			CG_DectivateLocalClient(i);
	}
}


A_EXTERN_C void CL_GiveKbmFocus(size_t localClientNum) {
	for (size_t i = 0; i < MAX_LOCAL_CLIENTS; i++)
		CL_GetLocalClientGlobals(i)->hasKbmFocus = i == localClientNum;
}

A_EXTERN_C bool CL_HasKbmFocus(size_t localClientNum) {
	return CL_GetLocalClientGlobals(localClientNum)->hasKbmFocus;
}

A_EXTERN_C size_t CL_ClientWithKbmFocus() {
	for (size_t i = 0; i < MAX_LOCAL_CLIENTS; i++) {
		if (CL_HasKbmFocus(i))
			return i;
	}

	assert(false);
	return (size_t)-1;
}

A_EXTERN_C KeyFocus CL_KeyFocus(size_t localClientNum) {
	return CG_GetLocalClientGlobals(localClientNum)->keyfocus;
}

A_EXTERN_C void CL_SetKeyFocus(size_t localClientNum, KeyFocus f) {
	CG_GetLocalClientGlobals(localClientNum)->keyfocus = f;
}

//static const void* CL_BitmapOffsetToPointer(size_t off, MapEngine engine, void* bsp_base) {
//	assert(g_load.bitmaps_map.p);
//	assert(g_load.bitmaps_map.n >= off);
//
//	if (engine == MAP_ENGINE_XBOX) 
//		return (const void*)((char*)bsp_base + off);
//
//	return (const void*)((const char*)g_load.bitmaps_map.p + off);
//}

A_EXTERN_C bool CL_BitmapDataFormatIsCompressed(BSPBitmapDataFormat format) {
	switch (format) {
	case BSP_BITMAP_DATA_FORMAT_DXT1:
	case BSP_BITMAP_DATA_FORMAT_DXT3:
	case BSP_BITMAP_DATA_FORMAT_DXT5:
	case BSP_BITMAP_DATA_FORMAT_BC7:
		return true;
	default:
		return false;
	};
}

#define CL_DECOMPRESS_FLOAT_SIGN_BIT(bits) (1 << ((bits) - 1))
#define CL_DECOMPRESS_FLOAT_MASK(bits) (CL_DECOMPRESS_FLOAT_SIGN_BIT(bits) - 1)
#define CL_DECOMPRESS_FLOAT(f, bits) (                         \
(float)(                                                       \
	((f) & CL_DECOMPRESS_FLOAT_MASK(bits)) /                   \
		   CL_DECOMPRESS_FLOAT_MASK(bits)                      \
) * ((f) & CL_DECOMPRESS_FLOAT_SIGN_BIT(bits) ? -1.0f : 1.0f)) 

static void CL_DecompressVector(A_OUT avec3f_t* decompressed, uint32_t compressed) {
	// uint32_t compressed_x = compressed >> 0;
	uint32_t compressed_y = compressed >> 11;
	uint32_t compressed_z = compressed >> 22;
	
	decompressed->x = CL_DECOMPRESS_FLOAT(compressed,   11);
	decompressed->y = CL_DECOMPRESS_FLOAT(compressed_y, 11);
	decompressed->z = CL_DECOMPRESS_FLOAT(compressed_z, 10);

	*decompressed = A_vec3f_normalize(*decompressed);
}

static void CL_DecompressRenderedVertex(
	A_OUT BSPRenderedVertex* decompressed, 
	const BSPRenderedVertexCompressed* compressed
) {
	decompressed->pos        = compressed->pos;
	decompressed->tex_coords = compressed->tex_coords;
	CL_DecompressVector   (&decompressed->normal,        compressed->normal);
	CL_DecompressVector   (&decompressed->binormal,      compressed->binormal);
	CL_DecompressVector   (&decompressed->tangent,       compressed->tangent);
}

static void CL_DecompressLightmapVertex(
	A_OUT BSPLightmapVertex* decompressed, 
	const BSPLightmapVertexCompressed* compressed
) {
	CL_DecompressVector(&decompressed->normal, compressed->normal);
	decompressed->tex_coords = compressed->tex_coords;
}

static bool CL_LoadMap_Header(A_OUT MapHeader* header) {
	long long pos = FS_SeekStream(&g_load.f, FS_SEEK_BEGIN, 0);
	assert(pos == 0);
	(void)pos;
	if (!FS_ReadStream(&g_load.f, &header, sizeof(header))) {
		Com_Errorln("CL_LoadMap: Read failed.");
		return false;
	}

	if ((uint32_t)header->head_magic != MAP_HEADER_HEAD) {
		Com_Errorln("CL_LoadMap: Invalid header head magic.");
		return false;
	}

	if ((uint32_t)header->foot_magic != MAP_HEADER_FOOT) {
		Com_Errorln("CL_LoadMap: Invalid header foot magic.");
		return false;
	}
	return true;
}

static bool CL_LoadMap_Decompress(
	const char* map_name, 
	const MapHeader* header
) {
	const std::string decompressed_map_name = std::string("decompressed_") + std::string(map_name);
	if (!FS_FileExists(decompressed_map_name.data())) {
		FileMapping f = DB_LoadMap_Mmap(map_name);
		size_t decompressed_map_size =
			(size_t)header->decompressed_file_size;
		void* decompressed = Z_Zalloc(decompressed_map_size - sizeof(header));

		z_stream stream;
		stream.zalloc    = Z_NULL;
		stream.zfree     = Z_NULL;
		stream.opaque    = Z_NULL;
		stream.avail_in  = f.n - sizeof(header);
		stream.next_in   = (Bytef*)f.p + sizeof(header);
		stream.avail_out = decompressed_map_size - sizeof(header);
		stream.next_out  = (Bytef*)decompressed;
		int ret = inflateInit2(&stream, -8);
		assert(ret == Z_OK);
		ret = inflate(&stream, Z_FINISH);
		assert(ret == Z_STREAM_END);
		ret = inflateEnd(&stream);
		assert(ret == Z_OK);

		Com_DPrintln(
			"CL_LoadMap: Decompressed {} bytes ({} {}), expected {}.",
			stream.total_out, ret, stream.msg ? stream.msg : "<NULL>",
			decompressed_map_size - sizeof(header));
		StreamFile decompressed_map = FS_StreamFile(
			decompressed_map_name.data(), FS_SEEK_BEGIN,
			FS_STREAM_READ_WRITE_NEW, 0
		);
		assert(decompressed_map.f);
		bool b = FS_WriteStream(&decompressed_map, &header, sizeof(header));
		assert(b);
		(void)b;
		b = FS_WriteStream(&decompressed_map, decompressed, decompressed_map_size - sizeof(header));
		assert(b);
		FS_CloseStream(&g_load.f);
		g_load.f = decompressed_map;
		Z_Free(decompressed);
		b = Z_UnmapFile(&f);
		assert(b);
	}
	else {
		g_load.f = FS_StreamFile(
			decompressed_map_name.data(), FS_SEEK_BEGIN,
			FS_STREAM_READ_EXISTING, 0
		);
		assert(g_load.f.f);
	}

	return true;
}

A_EXTERN_C bool CL_LoadMap_TagHeader(
	const MapHeader* header, bool is_xbox,
	A_OUT TagHeader* tag_header
) {
	size_t tag_data_offset = header->tag_data_offset;
	Com_DPrintln(
		"CL_LoadMap: tag data: offset=0x{:08X}, size={}", 
		tag_data_offset, header->tag_data_size
	);

	long long pos = FS_SeekStream(&g_load.f, FS_SEEK_BEGIN, tag_data_offset);
	assert(pos == (long long)tag_data_offset);
	(void)pos;

	if (is_xbox) {
		if (!FS_ReadStream(&g_load.f, &tag_header->xbox, sizeof(tag_header->xbox))) {
			Com_Errorln("CL_LoadMap: Failed to load tag header.");
			return false;
		}

		if (tag_header->xbox.magic != TAGS_FOOT) {
			Com_Errorln("CL_LoadMap: Invalid tag header.");
			return false;
		}
	}
	else {
		if (!FS_ReadStream(&g_load.f, &tag_header->pc, sizeof(tag_header->pc))) {
			Com_Errorln("CL_LoadMap: Failed to load tag header.");
			return false;
		}

		if (tag_header->pc.magic != TAGS_FOOT) {
			Com_Errorln("CL_LoadMap: Invalid tag header.");
			return false;
		}
	}
	return true;
}

A_EXTERN_C bool CL_LoadMap_TagData(
	const MapHeader* header,
	const TagHeader* tag_header, 
	bool is_xbox
) {
	const void* tag_base = is_xbox ?
		(const void*)TAGS_BASE_ADDR_XBOX : (const void*)TAGS_BASE_ADDR_GEARBOX;
	size_t total_tag_space = is_xbox ? 
		TAGS_MAX_SIZE_XBOX : TAGS_MAX_SIZE_GEARBOX;
	uint32_t tag_array_offset = 
		(uint32_t)((intptr_t)tag_header->common.tag_ptr - (intptr_t)tag_base);
	Com_DPrintln("CL_LoadMap: tag_array_offset=0x{:08X}", tag_array_offset);
	size_t tag_header_size = is_xbox ?
		sizeof(tag_header->xbox) : sizeof(tag_header->pc);
	uint32_t off = tag_array_offset - tag_header_size;
	FS_SeekStream(&g_load.f, FS_SEEK_CUR, off);
	std::vector<std::byte> v;
	v.resize(header->tag_data_size);
	bool b = FS_ReadStream(&g_load.f, v.data(), (size_t)header->tag_data_size);
	assert(b);
	(void)b;
	assert(v.size() > 0);
	Com_DPrintln("CL_LoadMap: allocating {} bytes for tag data.", 
		         total_tag_space);
	g_load.n = total_tag_space;
	g_load.p = Z_AllocAt(tag_base, g_load.n);
	assert(g_load.p == tag_base);
	A_memcpy(g_load.p, &tag_header, tag_header_size);
	A_memcpy(
		(void*)((intptr_t)g_load.p + 
			(intptr_t)tag_header_size + (intptr_t)off),
		v.data(), v.size()
	);

	g_load.tags      = (Tag*)tag_header->common.tag_ptr;
	g_load.tag_count =       tag_header->common.tag_count;
	return true;
}

A_EXTERN_C bool CL_LoadMap_Scenario(
	const TagHeader* tag_header, 
	A_OUT Invader::HEK::Scenario<Invader::HEK::NativeEndian>** scenario
) {
	Tag* scenario_tag = NULL;
	for (size_t i = 0; i < tag_header->common.tag_count; i++) {
		if (g_load.tags[i].tag_id.id == 
			tag_header->common.scenario_tag_id.id
		) {
			scenario_tag = &g_load.tags[i];
			break;
		}
	}

	if (scenario_tag == NULL) {
		Com_Errorln("CL_LoadMap: Unable to locate scenario tag.");
		return false;
	}

	if (scenario_tag->primary_class != TAG_FOURCC_SCENARIO) {
		Com_Errorln(
			"CL_LoadMap: Invalid scenario tag fourcc ({}).",
			(uint32_t)scenario_tag->primary_class
		);
		return false;
	}
	
	*scenario = (Invader::HEK::Scenario<Invader::HEK::NativeEndian>*)scenario_tag->tag_data;

	if (!*scenario) {
		Com_Errorln("CL_LoadMap: NULL scenario pointer.");
		return false;
	}

	Com_DPrintln("CL_LoadMap: Scenario at {}", (void*)*scenario);

	return true;
}

A_EXTERN_C bool CL_LoadMap_BSPHeader(const ScenarioBSP* sbsp,
	                      A_OUT BSPHeader** bsp_header) 
{
	long long pos = FS_SeekStream(&g_load.f, FS_SEEK_BEGIN, sbsp->bsp_start);
	assert(pos == sbsp->bsp_start);
	(void)pos;
	*bsp_header = (BSPHeader*)sbsp->bsp_address;
	bool b = FS_ReadStream(&g_load.f, *bsp_header, sizeof(**bsp_header));
	assert(b);
	(void)b;
	assert((*bsp_header)->pointer);

	void* bsp_data = (void*)sbsp->bsp_address;
	Com_DPrintln(
		"CL_LoadMap: BSP at {} (offset=0x{:08X}, size={})",
		bsp_data, sbsp->bsp_start, sbsp->bsp_size
	);

	if ((uint32_t)(*bsp_header)->magic != TAG_FOURCC_SBSP) {
		Com_Errorln("CL_LoadMap: Invalid signature for BSP header.");
		return false;
	}

	if ((*bsp_header)->lightmap_vertices == 0 || 
		(*bsp_header)->rendered_vertices == 0) 
	{
		Com_Errorln("CL_LoadMap: BSP has no renderable vertices.");
		return false;
	}

	return true;
}

A_EXTERN_C bool CL_LoadMap_BSP(
	const ScenarioBSP* sbsp,
	const BSPHeader* bsp_header,
	A_OUT Invader::HEK::ScenarioStructureBSP<Invader::HEK::NativeEndian>** bsp
) {
	void* bsp_data = (void*)(sbsp->bsp_address + sizeof(*bsp_header));
	bool b = FS_ReadStream(&g_load.f, bsp_data, sbsp->bsp_size - sizeof(*bsp_header));
	assert(b);
	(void)b;

	*bsp = (Invader::HEK::ScenarioStructureBSP<Invader::HEK::NativeEndian>*)bsp_header->pointer;

	if ((uint32_t)(*bsp)->lightmaps_bitmap.tag_fourcc != TAG_FOURCC_BITMAP) {
		Com_Errorln(
			"CL_LoadMap: Invalid lightmaps bitmap fourcc (0x{:08X}).", 
			(uint32_t)(*bsp)->lightmaps_bitmap.tag_fourcc
		);
		return false;
	}

	return true;
}

A_EXTERN_C bool CL_LoadMap_CBSBVertices(
	const Invader::HEK::ModelCollisionGeometryBSP<Invader::HEK::NativeEndian>* cbsp, 
	A_OUT Invader::HEK::ModelCollisionGeometryBSPVertex<Invader::HEK::NativeEndian>** vertices
) {
	*vertices =
		(Invader::HEK::ModelCollisionGeometryBSPVertex<Invader::HEK::NativeEndian>*)
		((uint32_t)cbsp->vertices.pointer);
	uint32_t coll_vertices_count = cbsp->vertices.count.read();
	Com_DPrintln("CBSP has {} vertices ({}).", coll_vertices_count, (void*)vertices);
	return true;
}

A_EXTERN_C bool CL_LoadMap_CBSP(
	const Invader::HEK::ScenarioStructureBSP< Invader::HEK::NativeEndian>* bsp,
	A_OUT Invader::HEK::ModelCollisionGeometryBSPVertex<Invader::HEK::NativeEndian>** vertices,
	A_OUT Invader::HEK::ModelCollisionGeometryBSP<Invader::HEK::NativeEndian>** cbsp
) {
	uint32_t cbsp_count = bsp->collision_bsp.count.read();
	if (bsp->collision_bsp.count.read() > 1) {
		Com_Errorln("CL_LoadMap: More than one collision BSP ({}).", cbsp_count);
		return false;
	}

	if (bsp->collision_bsp.count.read() < 1) {
		Com_Errorln("CL_LoadMap: No collision BSPs.");
		return false;
	}

	*cbsp = 
		(Invader::HEK::ModelCollisionGeometryBSP<Invader::HEK::NativeEndian>*)
			((uint32_t)bsp->collision_bsp.pointer.read());

	if (!CL_LoadMap_CBSBVertices(*cbsp, vertices)) return false;
	return true;
}

A_EXTERN_C bool CL_LoadMap_CollisionMaterials(
	const Invader::HEK::ScenarioStructureBSP< Invader::HEK::NativeEndian>* bsp, 
	A_OUT BSPCollisionMaterial** collision_materials
) {
	*collision_materials =
		(BSPCollisionMaterial*)bsp->collision_materials.pointer.read();
	uint32_t collision_materials_count = bsp->collision_materials.count;
	assert(collision_materials_count < BSP_MAX_COLLISION_MATERIALS);
	(void)collision_materials_count;
	return true;
}

A_EXTERN_C size_t CL_BitmapDataSize(const BSPBitmapData* bitmap_data) {
	assert(bitmap_data->type == BSP_BITMAP_DATA_TYPE_2D_TEXTURE ||
		bitmap_data->type == BSP_BITMAP_DATA_TYPE_3D_TEXTURE);

	if (bitmap_data->type == BSP_BITMAP_DATA_TYPE_2D_TEXTURE)
		assert(bitmap_data->depth == 1);

	size_t bpp = CL_BitmapDataFormatBPP(bitmap_data->format);

	size_t size = (bitmap_data->width * bitmap_data->height * bitmap_data->depth * bpp) / 8;
	assert(A_IS_MULTIPLE_OF(size, 8));
	return size;
}

A_EXTERN_C bool CL_LoadMap_Bitmap(TagId tag_id) {
	Tag* bitmap_tag = CL_Map_Tag(tag_id);
	assert(bitmap_tag);
	assert(bitmap_tag->primary_class == TAG_FOURCC_BITMAP);

	BSPBitmap* bitmap = (BSPBitmap*)bitmap_tag->tag_data;
	assert(bitmap);

	BSPBitmapData* bitmap_data = (BSPBitmapData*)bitmap->bitmap_data.pointer;
	for (uint32_t i = 0; i < bitmap->bitmap_data.count; i++) {
		bitmap_data[i].pixels = NULL;

		if (bitmap_data[i].type != BSP_BITMAP_DATA_TYPE_2D_TEXTURE &&
			bitmap_data[i].type != BSP_BITMAP_DATA_TYPE_3D_TEXTURE
			) {
			continue;
		}

		bitmap_data->actual_size = CL_BitmapDataSize(&bitmap_data[i]);
		assert(bitmap_data->actual_size < 4 * 1024 * 1024);
		bitmap_data[i].pixels = Z_Alloc(bitmap_data->actual_size);
		assert(bitmap_data[i].pixels);
		long long pos = FS_SeekStream(&g_load.f, FS_SEEK_BEGIN, bitmap_data[i].pixel_data_offset);
		assert(pos == bitmap_data[i].pixel_data_offset);
		(void)pos;

		bool b = FS_ReadStream(&g_load.f, bitmap_data[i].pixels, bitmap_data->actual_size);
		assert(b);
		(void)b;
	}
	return true;
}

A_EXTERN_C size_t CL_BitmapDataFormatBPP(BSPBitmapDataFormat format) {
	switch (format) {
	case BSP_BITMAP_DATA_FORMAT_A8R8G8B8:
	case BSP_BITMAP_DATA_FORMAT_X8R8G8B8:
		return 32;
	case BSP_BITMAP_DATA_FORMAT_R5G6B5:
	case BSP_BITMAP_DATA_FORMAT_A1R5G5B5:
	case BSP_BITMAP_DATA_FORMAT_A4R4G4B4:
	case BSP_BITMAP_DATA_FORMAT_A8Y8:
		return 16;
	case BSP_BITMAP_DATA_FORMAT_P8_BUMP:
	case BSP_BITMAP_DATA_FORMAT_A8:
	case BSP_BITMAP_DATA_FORMAT_AY8:
	case BSP_BITMAP_DATA_FORMAT_Y8:
	case BSP_BITMAP_DATA_FORMAT_DXT5:
	case BSP_BITMAP_DATA_FORMAT_DXT3:
	case BSP_BITMAP_DATA_FORMAT_BC7:
		return 8;
	case BSP_BITMAP_DATA_FORMAT_DXT1:
		return 4;
	default:
		assert(false);
		return 0;
	}
}

A_EXTERN_C bool CL_LoadMap(std::string_view map_name) {
	g_load.f = DB_LoadMap_Stream(map_name);
	if (g_load.f.f == NULL) {
		return false;
	}

	MapHeader header;
	if (!CL_LoadMap_Header(&header)) return false;

	bool is_xbox    = header.engine == MAP_ENGINE_XBOX;
	bool is_gearbox = header.engine == MAP_ENGINE_GEARBOX;
	
	if (!is_gearbox) {
		if (!is_xbox) {
			Com_Errorln("CL_LoadMap: Wrong engine version.");
			return false;
		}

		CL_LoadMap_Decompress(map_name.data(), &header);
	}
	
	TagHeader tag_header = {};
	if (!CL_LoadMap_TagHeader(&header, is_xbox, &tag_header)) return false;
	g_load.tag_count = tag_header.common.tag_count;
	if (!CL_LoadMap_TagData  (&header, &tag_header, is_xbox)) return false;

	Invader::HEK::Scenario<Invader::HEK::NativeEndian>* scenario = NULL;
	if (!CL_LoadMap_Scenario(&tag_header, &scenario)) return false;

	ScenarioPlayerSpawn* spawns = 
		(ScenarioPlayerSpawn*)scenario->player_starting_locations.pointer.read();

	for (size_t localClientNum = 0;
		localClientNum < MAX_LOCAL_CLIENTS;
		localClientNum++
	) {
		apoint3f_t pos = A_point3f_swap_yz(spawns[0].pos);
		CG_SetSpawn(localClientNum, pos, spawns[0].facing);
	}

	const ScenarioBSP* sbsps =
		(ScenarioBSP*)scenario->structure_bsps.pointer.read();
	BSPHeader* bsp_header = NULL;
	if (!CL_LoadMap_BSPHeader(&sbsps[0], &bsp_header)) return false;
	g_load.rendered_vertices = (BSPRenderedVertex*)bsp_header->rendered_vertices;
	g_load.lightmap_vertices = (BSPLightmapVertex*)bsp_header->lightmap_vertices;

	Invader::HEK::ScenarioStructureBSP<Invader::HEK::NativeEndian>* bsp = NULL;
	if (!CL_LoadMap_BSP(&sbsps[0], bsp_header, &bsp)) return false;
	assert(bsp);
	g_load.bsp_ptr = bsp;
	g_load.surfs = (BSPSurf*)(uint32_t)bsp->surfaces.pointer.read();
	g_load.surf_count = bsp->surfaces.count.read();
	g_load.lightmaps = (BSPLightmap*)bsp->lightmaps.pointer.read();
	g_load.lightmap_count = bsp->lightmaps.count.read();

	Invader::HEK::ModelCollisionGeometryBSPVertex<Invader::HEK::NativeEndian>* vertices = NULL;
	Invader::HEK::ModelCollisionGeometryBSP<Invader::HEK::NativeEndian>* cbsp = NULL;
	if (!CL_LoadMap_CBSP(bsp, &vertices, &cbsp)) return false;
	assert(vertices && cbsp);
	g_load.coll_surfs = (BSPCollSurf*)(uint32_t)cbsp->surfaces.pointer.read();
	g_load.coll_surf_count = cbsp->surfaces.count;
	g_load.coll_edges = (BSPCollEdge*)(uint32_t)cbsp->edges.pointer.read();
	g_load.coll_edge_count = cbsp->edges.count;
	g_load.coll_vertices = (BSPCollVertex*)vertices;
	g_load.coll_vertex_count = cbsp->vertices.count;

	BSPCollisionMaterial* collision_materials = NULL;
	if (!CL_LoadMap_CollisionMaterials(bsp, &collision_materials)) return false;
	assert(collision_materials);
	g_load.collision_materials = collision_materials;

	size_t total_vertex_count = 0;
	Invader::HEK::ScenarioStructureBSPLightmap<Invader::HEK::NativeEndian>* lightmaps =
		(Invader::HEK::ScenarioStructureBSPLightmap<Invader::HEK::NativeEndian>*)bsp->lightmaps.pointer.read();
	for(uint32_t i = 0; i < bsp->lightmaps.count; i++) {
		Invader::HEK::ScenarioStructureBSPLightmap<Invader::HEK::NativeEndian>* lightmap = &lightmaps[i];
		BSPMaterial* materials = (BSPMaterial*)lightmap->materials.pointer.read();
		for(uint32_t j = 0; j < lightmap->materials.count; j++) {
			BSPMaterial* material = &materials[j];
			uint32_t rendered_vertices_count = 
				material->rendered_vertices_count;
			uint32_t lightmap_vertices_count = 
				material->lightmap_vertices_count;

			if ((rendered_vertices_count == 0 || lightmap_vertices_count == 0) && 
				i < bsp->lightmaps.count.read() - 1
			) {
				Com_DPrintln("Lightmap {} Material {}: bitmap == -1", i, j);
				assert(false);
			}

			if(material->shader.id.index == 0xFFFF)
					continue;
			Tag* shader_tag = CL_Map_Tag(material->shader.id);
			const char* shader_path = (const char *)shader_tag->tag_path;
			assert(shader_tag);
			assert(shader_path);
			
			total_vertex_count += rendered_vertices_count;
			const BSPRenderedVertexCompressed* compressed_rendered_vertices = 
				(const BSPRenderedVertexCompressed*)
					material->compressed_vertices.pointer;
			const BSPLightmapVertexCompressed* compressed_lightmap_vertices = 
				(const BSPLightmapVertexCompressed*)(
					(const BSPRenderedVertexCompressed*)
						material->compressed_vertices.pointer + 
							(uint64_t)material->rendered_vertices_count
				);

			size_t decompressed_rendered_vertices_size = 
				rendered_vertices_count * sizeof(BSPRenderedVertex);
			size_t decompressed_lightmap_vertices_size = 
				lightmap_vertices_count * sizeof(BSPLightmapVertex);
			size_t decompressed_vertices_size = 
				decompressed_rendered_vertices_size + decompressed_lightmap_vertices_size;

			void*  decompressed_vertices = Z_Alloc(decompressed_vertices_size);
			material->uncompressed_vertices.pointer = decompressed_vertices;
			BSPRenderedVertex* rendered_vertices = 
				(BSPRenderedVertex*)decompressed_vertices;
			for (uint32_t k = 0; k < rendered_vertices_count; k++)
				CL_DecompressRenderedVertex(&rendered_vertices[k], 
					                        &compressed_rendered_vertices[k]);
			BSPLightmapVertex* lightmap_vertices = 
				(BSPLightmapVertex*)
					((char*)decompressed_vertices + 
						decompressed_rendered_vertices_size);
			for (uint32_t k = 0; k < material->lightmap_vertices_count; k++)
				CL_DecompressLightmapVertex(&lightmap_vertices[k], 
					                        &compressed_lightmap_vertices[k]);

			for (int32_t k = material->surfaces; 
				 k < material->surfaces + material->surface_count; 
				k++
			) {
				for (int l = 0; l < 3; l++) {
					if (g_load.surfs[k].verts[l] >= rendered_vertices_count)
						Com_DPrintln(
							"CL_LoadMap: Lightmap {} Material {}: out of bounds vertex index {} for surf {}, vert {}", 
							i, j, (uint16_t)g_load.surfs[k].verts[l], k, l
						);
					assert(g_load.surfs[k].verts[l] < rendered_vertices_count);
				}
			}

			assert(shader_tag->tag_data);
			assert(shader_tag->secondary_class == TAG_FOURCC_SHADER);
			if (shader_tag->primary_class == TAG_FOURCC_SHADER_ENVIRONMENT) {
				BSPShaderEnvironment* shader = 
					(BSPShaderEnvironment*)shader_tag->tag_data;
				assert(shader->detail_map_function < BSP_SHADER_DETAIL_FUNCTION_COUNT);
				
				bool b = true;
				if (shader->map.id.index != 0xFFFF)
					b = CL_LoadMap_Bitmap(shader->map.id);
				if (shader->base_map.id.index != 0xFFFF) 
					b = CL_LoadMap_Bitmap(shader->base_map.id);
				assert(b);
				if (shader->primary_detail_map.id.index != 0xFFFF)
					b = CL_LoadMap_Bitmap(shader->primary_detail_map.id);
				assert(b);
				if (shader->secondary_detail_map.id.index != 0xFFFF)
					b = CL_LoadMap_Bitmap(shader->secondary_detail_map.id);
				if (shader->micro_detail_map.id.index != 0xFFFF)
					b = CL_LoadMap_Bitmap(shader->micro_detail_map.id);

				assert(b);
				(void)b;
			}
		}
	}
	Com_DPrintln("CL_LoadMap: Total Vertex Count={}.", total_vertex_count);

	R_LoadMap();

	for (size_t localClientNum = 0; 
		 localClientNum < MAX_LOCAL_CLIENTS; 
		 localClientNum++
	) {
		CG_Respawn(localClientNum);
	}
	
	return true;
}

A_EXTERN_C Tag* CL_Map_Tag(TagId id) {
	assert(g_load.tags);
	if(id.index >= g_load.tag_count)
		Com_DPrintln("invalid tag ({} > g_load.tag_count {})",
			         id.index, g_load.tag_count);
	assert(id.index < g_load.tag_count);
	return &g_load.tags[id.index];
}

A_EXTERN_C BSPSurf* CL_Map_Surfs(void) {
	assert(g_load.surfs);
	return g_load.surfs;
}

A_EXTERN_C uint32_t CL_Map_SurfCount(void) {
	assert(g_load.surfs);
	return g_load.surf_count;
}

A_EXTERN_C BSPRenderedVertex* CL_Map_RenderedVertices(void) {
	assert(g_load.rendered_vertices);
	return g_load.rendered_vertices;
}

A_EXTERN_C BSPLightmapVertex* CL_Map_LightmapVertices(void) {
	assert(g_load.lightmap_vertices);
	return g_load.lightmap_vertices;
}

A_EXTERN_C BSPCollSurf* CL_Map_CollSurfs(void) {
	assert(g_load.coll_surfs);
	return g_load.coll_surfs;
}

A_EXTERN_C uint32_t CL_Map_CollSurfCount(void) {
	assert(g_load.coll_surfs);
	return g_load.coll_surf_count;
}

A_EXTERN_C BSPCollEdge* CL_Map_CollEdges(void) {
	assert(g_load.coll_edges);
	return g_load.coll_edges;
}

A_EXTERN_C uint32_t CL_Map_CollEdgeCount(void) {
	assert(g_load.coll_edges);
	return g_load.coll_edge_count;
}

A_EXTERN_C BSPCollVertex* CL_Map_CollVertices(void) {
	assert(g_load.coll_vertices);
	return g_load.coll_vertices;
}

A_EXTERN_C uint32_t CL_Map_CollVertexCount(void) {
	assert(g_load.coll_vertices);
	return g_load.coll_vertex_count;
}

A_EXTERN_C BSPCollisionMaterial* CL_Map_CollisionMaterial(uint16_t i) {
	assert(g_load.collision_materials);
	assert(i < BSP_MAX_COLLISION_MATERIALS);
	return &g_load.collision_materials[i];
}

A_EXTERN_C BSPLightmap* CL_Map_Lightmap(uint16_t i) {
	assert(g_load.lightmaps);
	assert(i < g_load.lightmap_count);
	return &g_load.lightmaps[i];
}

A_EXTERN_C uint32_t CL_Map_LightmapCount(void) {
	assert(g_load.lightmaps);
	return g_load.lightmap_count;
}


A_EXTERN_C bool CL_UnloadMap(void) {
	R_UnloadMap();

	if (g_load.f.f)
		FS_CloseStream(&g_load.f);

	if (!g_load.bsp_ptr)
		return false;

	Invader::HEK::ScenarioStructureBSPLightmap<Invader::HEK::NativeEndian>* lightmaps =
		(Invader::HEK::ScenarioStructureBSPLightmap<Invader::HEK::NativeEndian>*)g_load.bsp_ptr->lightmaps.pointer.read();
	for (uint32_t i = 0; i < g_load.bsp_ptr->lightmaps.count; i++) {
		Invader::HEK::ScenarioStructureBSPLightmap<Invader::HEK::NativeEndian>* lightmap = &lightmaps[i];
		BSPMaterial* materials = (BSPMaterial*)lightmap->materials.pointer.read();
		for (uint32_t j = 0; j < lightmap->materials.count; j++) {
			BSPMaterial* material = &materials[j];
			Z_Free(material->uncompressed_vertices.pointer);

			if (material->shader.id.index == 0xFFFF)
				continue;
			Tag* shader_tag = CL_Map_Tag(material->shader.id);
			assert(shader_tag);
			assert(shader_tag->tag_data);

			assert(shader_tag->secondary_class == TAG_FOURCC_SHADER);
			if (shader_tag->primary_class == TAG_FOURCC_SHADER_ENVIRONMENT) {
				BSPShaderEnvironment* shader = 
					(BSPShaderEnvironment*)shader_tag->tag_data;

				if (shader->base_map.id.index != 0xFFFF) {
					Tag* base_map_tag = CL_Map_Tag(shader->base_map.id);
					assert(base_map_tag->primary_class == TAG_FOURCC_BITMAP);
					BSPBitmap* base_map_bitmap = (BSPBitmap*)base_map_tag->tag_data;
					BSPBitmapData* bitmap_data = 
						(BSPBitmapData*)base_map_bitmap->bitmap_data.pointer;
					for (uint32_t k = 0; 
						k < base_map_bitmap->bitmap_data.count;
						k++
					) {
						if (bitmap_data[k].type != 
								BSP_BITMAP_DATA_TYPE_2D_TEXTURE &&
							bitmap_data[k].type != 
								BSP_BITMAP_DATA_TYPE_3D_TEXTURE
							) {
							continue;
						}

						assert(bitmap_data[k].pixels && 
							   bitmap_data[k].pixels != (void*)0xFFFFFFFF);
						Z_Free(bitmap_data[k].pixels);
					}
				}

				if (shader->primary_detail_map.id.index != 0xFFFF) {
					Tag* primary_detail_map_tag = 
						CL_Map_Tag(shader->primary_detail_map.id);
					assert(primary_detail_map_tag->primary_class == 
						   TAG_FOURCC_BITMAP);
					BSPBitmap* primary_detail_map_bitmap = 
						(BSPBitmap*)primary_detail_map_tag->tag_data;
					BSPBitmapData* bitmap_data = 
						(BSPBitmapData*)
							primary_detail_map_bitmap->bitmap_data.pointer;
					for (uint32_t k = 0; 
						k < primary_detail_map_bitmap->bitmap_data.count; 
						k++
					) {
						if (bitmap_data[k].type !=
								BSP_BITMAP_DATA_TYPE_2D_TEXTURE &&
							bitmap_data[k].type != 
								BSP_BITMAP_DATA_TYPE_3D_TEXTURE
						) {
							continue;
						}

						assert(bitmap_data[k].pixels && 
							   bitmap_data[k].pixels != (void*)0xFFFFFFFF);
						Z_Free(bitmap_data[k].pixels);
					}
				}
				
				if (shader->secondary_detail_map.id.index != 0xFFFF) {
					Tag* secondary_detail_map_tag = 
						CL_Map_Tag(shader->secondary_detail_map.id);
					assert(secondary_detail_map_tag->primary_class == 
						   TAG_FOURCC_BITMAP);
					BSPBitmap* secondary_detail_map_bitmap = 
						(BSPBitmap*)secondary_detail_map_tag->tag_data;
					BSPBitmapData* bitmap_data = 
						(BSPBitmapData*)
							secondary_detail_map_bitmap->bitmap_data.pointer;
					for (uint32_t k = 0; 
						k < secondary_detail_map_bitmap->bitmap_data.count;
						k++
					) {
						if (bitmap_data[k].type != BSP_BITMAP_DATA_TYPE_2D_TEXTURE &&
							bitmap_data[k].type != BSP_BITMAP_DATA_TYPE_3D_TEXTURE
							) {
							continue;
						}

						assert(bitmap_data[k].pixels && 
							   bitmap_data[k].pixels != (void*)0xFFFFFFFF);
						Z_Free(bitmap_data[k].pixels);
					}
				}

				if (shader->micro_detail_map.id.index != 0xFFFF) {
					Tag* micro_detail_map_tag = 
						CL_Map_Tag(shader->micro_detail_map.id);
					assert(micro_detail_map_tag->primary_class == 
						   TAG_FOURCC_BITMAP);
					BSPBitmap* micro_detail_map_bitmap = 
						(BSPBitmap*)micro_detail_map_tag->tag_data;
					BSPBitmapData* bitmap_data = 
						(BSPBitmapData*)
							micro_detail_map_bitmap->bitmap_data.pointer;
					for (uint32_t k = 0; 
						 k < micro_detail_map_bitmap->bitmap_data.count;
						 k++
					) {
						if (bitmap_data[k].type != BSP_BITMAP_DATA_TYPE_2D_TEXTURE &&
							bitmap_data[k].type != BSP_BITMAP_DATA_TYPE_3D_TEXTURE
							) {
							continue;
						}

						assert(bitmap_data[k].pixels && 
							   bitmap_data[k].pixels != (void*)0xFFFFFFFF);
						Z_Free(bitmap_data[k].pixels);
					}
				}
			}
		}
	}

	if (g_load.p && g_load.n > 0) {
		Z_FreeAt(g_load.p, g_load.n);
		g_load.p			       = NULL;
		g_load.n			       = 0;
		g_load.collision_materials = NULL;
		g_load.rendered_vertices   = NULL;
		g_load.surfs               = NULL;
		g_load.coll_surfs	       = NULL;
		g_load.coll_edges	       = NULL;
		g_load.coll_vertices       = NULL;
		g_load.lightmaps           = NULL;
	}

	return true;
}

A_EXTERN_C bool CL_IsMapLoaded(void) {
	return g_load.f.f && g_load.p && g_load.n > 0;
}

A_EXTERN_C void CL_Shutdown(void) {
	Z_UnmapFile(&g_load.bitmaps_map);
	CL_UnloadMap();
	A_memset((void*)&g_load, 0, sizeof(g_load));

	Dvar_SetBool(cl_splitscreen, false);
	Dvar_Unregister("cl_splitscreen");
	cl_splitscreen = NULL;

	for (size_t i = 0; i < MAX_LOCAL_CLIENTS; i++) {
		cl_t* cl = CL_GetLocalClientGlobals(i);
		Dvar_SetBool(cl->drawfps, false);
		Dvar_UnregisterLocal(i, "cl_drawfps");
		cl->drawfps = NULL;

		cll_t* cll = CL_GetLocalClientLocals(i);
		cll->drawDevGui = false;
		cll->keyfocus = KF_GAME;
	}
}
