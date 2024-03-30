#include "gfx.hpp"

#include <cassert>
#include <cstdio>

#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <glm/gtc/matrix_transform.hpp>

#include "cg_cgame.hpp"
#include "db_files.hpp"
#include "dvar.hpp"
#include "font.hpp"
#include "gfx_backend.hpp"
#include "gfx_text.hpp"
#include "gfx_uniform.hpp"
#include "sys.hpp"

extern dvar_t* vid_width;
extern dvar_t* vid_height;
extern void R_DrawTextDraws(int localClientNum);
extern void R_ClearTextDraws(int localClientNum);

void GLAPIENTRY R_GlDebugOutput(
    GLenum source, GLenum type, unsigned int id, GLenum severity,
    GLsizei /*unused*/, const char* message, const void* /*unused*/
) {
    if (id == 131169 || id == 131185 ||
        id == 131218 || id == 131204 || id == 131139
    ) {
        return;
    }

    std::string_view src = GL_DEBUG_SOURCE_STR(source);
    std::string_view t   = GL_DEBUG_TYPE_STR(type);
    std::string_view sev = GL_DEBUG_SEVERITY_STR(severity);
    
    Com_DPrintln(
        "OpenGL debug message (source={}, type={}, severity={}): {}", 
        src, t, sev, message
    );
}

static void R_RegisterDvars();
static void R_InitCubePrim(INOUT GfxCubePrim& cubePrim);
static void R_DrawFrameInternal(int localClientNum);
static void R_InitLocalClient(int localClientNum);
static void R_UpdateLocalClientView(int localClientNum);

constexpr float NEAR_PLANE_DEFAULT = 0.1f;
constexpr float FAR_PLANE_DEFAULT  = 100.0f;

dvar_t* r_vsync;
dvar_t* r_fullscreen;
dvar_t* r_noBorder;

GfxCubePrim r_cubePrim;
extern FontDef r_defaultFont;

size_t r_testDrawId = 0;

void R_Init() {
    RB_Init();

    int imgInitFlags = IMG_INIT_JPG | IMG_INIT_PNG;
    if ((IMG_Init(imgInitFlags) & imgInitFlags) != imgInitFlags)
        Com_Errorln("SDL_image init failed.");

#if _DEBUG
    //GLint flags = 0;
    //GL_CALL(glGetIntegerv, GL_CONTEXT_FLAGS, &flags);
    //if ((flags & GL_CONTEXT_FLAG_DEBUG_BIT) == 0)
    //    Com_Errorln("Failed to initialize OpenGL debug context in debug build.");

    GL_CALL(glEnable, GL_DEBUG_OUTPUT);
    GL_CALL(glEnable, GL_DEBUG_OUTPUT_SYNCHRONOUS);
    GL_CALL(glDebugMessageCallback, R_GlDebugOutput, nullptr);
    GL_CALL(glDebugMessageControl, GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, (GLboolean)GL_TRUE);
#endif // _DEBUG

    GL_CALL(glClearColor, 0.2f, 0.3f, 0.3f, 1.0f);
    
    for (size_t i = 0; i < MAX_LOCAL_CLIENTS; i++) {
        R_InitLocalClient(i);
        R_ClearTextDraws(i);
    }

    R_RegisterDvars();

    bool b = Font_Load("consola.ttf", 0, 48, r_defaultFont);
    assert(b);
    
    R_AddTextDraw(
        0, nullptr, "This is a test.\nWhere the fuck i am?", 0.1f, 0.1f, 1.0f, 1.0f,
        glm::vec3(0.77, 0.77, 0.2), true, false, r_testDrawId
    );
    R_InitCubePrim(r_cubePrim);
     
    //glEnable(GL_POINT_SMOOTH);
    //glPointSize(4);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
}

static void R_InitLocalClient(int localClientNum) {
    cg_t& cg = CG_GetLocalClientGlobals(localClientNum);
    cg.nearPlane = NEAR_PLANE_DEFAULT;
    cg.farPlane = FAR_PLANE_DEFAULT;
    R_UpdateLocalClientView(localClientNum);
}

static void R_UpdateOrtho(int localClientNum) {
    cg_t& cg = CG_GetLocalClientGlobals(localClientNum);

    float left = cg.viewport.x * Dvar_GetInt(*vid_width);
    float right = cg.viewport.w * Dvar_GetInt(*vid_width) + left;
    float bottom = cg.viewport.y * Dvar_GetInt(*vid_height);
    float top = cg.viewport.h * Dvar_GetInt(*vid_height) + bottom;
    cg.camera.orthoProjection = glm::ortho(left, right, bottom, top);
}

void R_UpdateProjection(int localClientNum) {
    cg_t& cg = CG_GetLocalClientGlobals(localClientNum);

    float w = cg.viewport.w * Dvar_GetInt(*vid_width);
    float h = cg.viewport.h * Dvar_GetInt(*vid_height);
    cg.camera.perspectiveProjection = glm::perspective(
        glm::radians(cg.fovy), w / h, cg.nearPlane, cg.farPlane
    );
}

static void R_UpdateLocalClientView(int localClientNum) {
    R_UpdateOrtho(localClientNum);
    R_UpdateProjection(localClientNum);
}

static void R_RegisterDvars() {
    r_vsync      = &Dvar_RegisterBool("r_vsync", DVAR_FLAG_NONE, true);
    r_fullscreen = &Dvar_RegisterBool("r_fullscreen", DVAR_FLAG_NONE, false);
    r_noBorder   = &Dvar_RegisterBool("r_noBorder", DVAR_FLAG_NONE, false);
}

static void R_InitCubePrim(INOUT GfxCubePrim& cubePrim) {
    std::string vertSource = DB_LoadShader("vs.glsl");
    std::string fragSource = DB_LoadShader("fs.glsl");

    std::string errorLog;
    if (!R_CreateShaderProgram(
        vertSource, fragSource, &errorLog, r_cubePrim.prog
    )) {
        Com_Errorln(errorLog);
    }

    GL_CALL(glGenVertexArrays, 1, &cubePrim.vao);
    GL_CALL(glBindVertexArray, cubePrim.vao);

    GL_CALL(glGenBuffers, 1, &cubePrim.vbo);
    GL_CALL(glBindBuffer, GL_ARRAY_BUFFER, cubePrim.vbo);
    GL_CALL(glBufferData,
        GL_ARRAY_BUFFER, sizeof(GfxCubePrim::vertices), 
        GfxCubePrim::vertices, GL_STATIC_DRAW
    );

    GL_CALL(glVertexAttribPointer,
        0, 3, GL_FLOAT, (GLboolean)GL_FALSE, 
        5 * (GLsizei)sizeof(*GfxCubePrim::vertices), (void*)0
    );
    GL_CALL(glEnableVertexAttribArray, 0);

    GL_CALL(glVertexAttribPointer,
        1, 2, GL_FLOAT, (GLboolean)GL_FALSE, 
        5 * (GLsizei)sizeof(*GfxCubePrim::vertices),
        (void*)(3 * sizeof(*GfxCubePrim::vertices))
    );
    GL_CALL(glEnableVertexAttribArray, 1);

    bool b = R_CreateImage("container.jpg", NULL, NULL, cubePrim.tex);
    assert(b);

    GL_CALL(glUseProgram, cubePrim.prog.program);
    R_SetUniform(cubePrim.prog.program, "uContainerTex", 0);
    GL_CALL(glUseProgram, 0);
    GL_CALL(glBindBuffer, GL_ARRAY_BUFFER, 0);
    GL_CALL(glBindVertexArray, 0);
}

void R_DrawFrame(int localClientNum) {
    R_DrawFrameInternal(localClientNum);
}

void R_Frame() {
    RB_BeginFrame();
    GL_CALL(glEnable, GL_SCISSOR_TEST);
    for (size_t i = 0; i < MAX_LOCAL_CLIENTS; i++) {
        if (!CG_LocalClientIsActive(i))
            continue;

        R_DrawFrame(i);
    }
    GL_CALL(glDisable, GL_SCISSOR_TEST);
    RB_EndFrame();
}

void R_WindowResized() {
    for (size_t i = 0; i < MAX_LOCAL_CLIENTS; i++) {
        R_UpdateLocalClientView(i);
    }
}

NO_DISCARD bool R_CreateImage(
    std::string_view image_name, OPTIONAL_OUT int* width, 
    OPTIONAL_OUT int* height, OUT texture_t& tex
) {
    if(width)
        *width  = 0;
    if(height)
        *height = 0;

    GL_CALL(glActiveTexture, GL_TEXTURE0);
    GL_CALL(glGenTextures,   1, &tex);
    GL_CALL(glBindTexture,   GL_TEXTURE_2D, tex);

    GL_CALL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    GL_CALL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    GL_CALL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_CALL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    SdlSurface surf = SdlSurface(
        IMG_Load(DB_ImagePath(image_name).string().c_str())
    );

    if (!surf)
        return false;

    if (!surf->pixels)
        return false;

    if (surf->w < 1 || surf->h < 1)
        return false;

    image_format_t format = 0;
    switch (surf->format->format) {
    case SDL_PIXELFORMAT_RGB24:
        format = GL_RGB;
        break;
    case SDL_PIXELFORMAT_RGBA32:
        format = GL_RGBA;
        break;
    default:
        assert(false);
    }

    GL_CALL(glTexImage2D,
        GL_TEXTURE_2D, 0, GL_RGB, surf->w, surf->h, 
        0, format, GL_UNSIGNED_BYTE, surf->pixels
    );
    GL_CALL(glGenerateMipmap, GL_TEXTURE_2D);

    if (width)
        *width = surf->w;
    if (height)
        *height = surf->h;

    return true;
}

void R_DrawCube(const glm::vec3& pos, float angle, texture_t tex) {
    glEnable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(r_cubePrim.vao);
    glBindBuffer(GL_ARRAY_BUFFER, r_cubePrim.vbo);
    glUseProgram(r_cubePrim.prog.program);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0, 0.3, 0.5));
    R_SetUniform(r_cubePrim.prog.program, "uModel", model);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_DEPTH_TEST);
}

static void R_DrawFrameInternal(int localClientNum) {
    cg_t& cg = CG_GetLocalClientGlobals(localClientNum);

    GLint x = (GLint)(cg.viewport.x * Dvar_GetInt(*vid_width));
    GLint y = (GLint)(cg.viewport.y * Dvar_GetInt(*vid_height));
    GLsizei w = (GLsizei)(cg.viewport.w * Dvar_GetInt(*vid_width));
    GLsizei h = (GLsizei)(cg.viewport.h * Dvar_GetInt(*vid_height));

    if (Dvar_WasModified(*cg.fov)) {
        R_UpdateProjection(localClientNum);
        Dvar_ClearModified(*cg.fov);
    }

    GL_CALL(glViewport, x, y, w, h);
    GL_CALL(glScissor,  x, y, w, h);
    GL_CALL(glClear, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    static const std::array<glm::vec3, 10> cubePositions = {
        glm::vec3(0.0f,  0.0f,  0.0f),
        glm::vec3(2.0f,  5.0f, -15.0f),
        glm::vec3(-1.5f, -2.2f, -2.5f),
        glm::vec3(-3.8f, -2.0f, -12.3f),
        glm::vec3(2.4f, -0.4f, -3.5f),
        glm::vec3(-1.7f,  3.0f, -7.5f),
        glm::vec3(1.3f, -2.0f, -2.5f),
        glm::vec3(1.5f,  2.0f, -2.5f),
        glm::vec3(1.5f,  0.2f, -1.5f),
        glm::vec3(-1.3f,  1.0f, -1.5f)
    };

    /*
    R_DrawText(
        NULL, "Testing 123...", 
        25.0, 25.0, 1.0, 1.0, 
        glm::vec3(0.5, 0.8f, 0.2f)
    );
    */

    GL_CALL(glUseProgram, r_cubePrim.prog.program);

    glm::vec3 pos = cg.camera.pos;
    glm::vec3 center = pos + cg.camera.front;
    glm::vec3 up = cg.camera.up;
    glm::mat4 view = glm::lookAt(pos, center, up);
    R_SetUniform(r_cubePrim.prog.program, "uView", view);
    R_SetUniform(
        r_cubePrim.prog.program, "uPerspectiveProjection", 
        cg.camera.perspectiveProjection
    );

    for (int i = 0; i < (int)cubePositions.size(); i++) {
        R_DrawCube(cubePositions[i], 20.0f * (float)i, r_cubePrim.tex);
    }
    GL_CALL(glUseProgram, 0);

    R_DrawTextDraws(localClientNum);
}

static void R_UnregisterDvars() {
    Dvar_Unregister("r_noBorder");
    r_noBorder = nullptr;
    Dvar_Unregister("r_fullscreen");
    r_fullscreen = nullptr;
    Dvar_Unregister("r_vsync");
    r_vsync = nullptr;
}

void R_Shutdown() {
    for(size_t i = 0; i < MAX_LOCAL_CLIENTS; i++)
        R_ClearTextDraws(i);

    R_UnregisterDvars();
    IMG_Quit();
    RB_Shutdown();
}
