#include "gfx_text.hpp"

#include <array>

#include <glm/gtc/matrix_transform.hpp>

#include "cg_cgame.hpp"
#include "db_files.hpp"
#include "dvar.hpp"
#include "font.hpp"
#include "gfx_defs.hpp"
#include "gfx_shader.hpp"
#include "gfx_uniform.hpp"
#include "sys.hpp" 

extern dvar_t* vid_width;
extern dvar_t* vid_height;
FontDef r_defaultFont;

inline static const std::array<GfxSubTexDef, 6> s_subTexDefs = {
    GfxSubTexDef {.x = 0, .y = 0, .u = 0, .v = 1 },
    GfxSubTexDef {.x = 0, .y = 1, .u = 0, .v = 0 },
    GfxSubTexDef {.x = 1, .y = 1, .u = 1, .v = 0 },
    GfxSubTexDef {.x = 0, .y = 0, .u = 0, .v = 1 },
    GfxSubTexDef {.x = 1, .y = 1, .u = 1, .v = 0 },
    GfxSubTexDef {.x = 1, .y = 0, .u = 1, .v = 1 }
};

// ============================================================================
// DON'T TOUCH ANY OF THIS. DON'T EVEN LOOK AT IT THE WRONG WAY. LEAVE IT ALONE
// AND BE HAPPY YOU DIDN'T SPEND TEN HOURS GETTING IT TO WORK.
A_NO_DISCARD bool R_CreateTextureAtlas(A_INOUT FontDef& f) {
    if (!R_CreateShaderProgram(
        DB_LoadShader("text.vs.glsl"),
        DB_LoadShader("text.fs.glsl"),
        NULL, &f.prog)
        ) {
        return false;
    }

    for (const auto& g : f.Glyphs()) {
        f.atlas_width += g.width + 1;
        f.atlas_height = std::max(f.atlas_height, g.height);
    }

    GL_CALL(glUseProgram, r_defaultFont.prog.program);

    bool b = R_CreateVertexBuffer(
        s_subTexDefs.data(),
        s_subTexDefs.size() * sizeof(*s_subTexDefs.data()),
        s_subTexDefs.size() * sizeof(*s_subTexDefs.data()),
        0,
        &f.vb
    );
    assert(b);
    if (!b)
        return false;

    GL_CALL(glVertexAttribPointer,
        0, 2, GL_FLOAT, (GLboolean)GL_FALSE,
        (GLsizei)sizeof(GfxSubTexDef), (void*)offsetof(GfxSubTexDef, x)
    );
    GL_CALL(glVertexAttribPointer,
        1, 2, GL_FLOAT, (GLboolean)GL_FALSE,
        (GLsizei)sizeof(GfxSubTexDef), (void*)offsetof(GfxSubTexDef, u)
    );
    GL_CALL(glEnableVertexAttribArray, 0);
    GL_CALL(glEnableVertexAttribArray, 1);

    GL_CALL(glGenTextures, 1, &f.tex);
    GL_CALL(glActiveTexture, GL_TEXTURE0);

    GL_CALL(glBindTexture, GL_TEXTURE_2D, f.tex);
    GL_CALL(glTexImage2D,
        GL_TEXTURE_2D, 0, GL_RED, f.atlas_width,
        f.atlas_height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr
    );

    GLint unpackAlign = 0;
    GL_CALL(glGetIntegerv, GL_UNPACK_ALIGNMENT, &unpackAlign);
    GL_CALL(glPixelStorei, GL_UNPACK_ALIGNMENT, 1);

    GL_CALL(glTexParameteri,
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE
    );
    GL_CALL(glTexParameteri,
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE
    );
    GL_CALL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_CALL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int x = 0;
    for (auto& g : f.Glyphs()) {
        if (g.advance_x == 0)
            continue;

        if (g.pixels.size() > 0)
            GL_CALL(glTexSubImage2D,
                GL_TEXTURE_2D, 0, x, 0, g.width, g.height,
                GL_RED, GL_UNSIGNED_BYTE, g.pixels.data()
            );

        g.pixels.clear();
        g.atlas_x = (float)x / (float)f.atlas_width;
        g.atlas_y = 0.0f;

        x += g.width + 1;
    }

    R_SetUniform(f.prog.program, "uTex", 0);

    GL_CALL(glPixelStorei, GL_UNPACK_ALIGNMENT, unpackAlign);
    GL_CALL(glBindTexture, GL_TEXTURE_2D, 0);
    GL_CALL(glBindBuffer, GL_ARRAY_BUFFER, 0);
    GL_CALL(glBindVertexArray, 0);
    GL_CALL(glUseProgram, 0);

    return true;
}
// ============================================================================

void R_DrawText(
    size_t localClientNum, A_OPTIONAL_IN const FontDef* font, 
    const RectDef& rect, std::string_view text,
    float xscale, float yscale, const glm::vec3& color,
    bool right
) {
    if (font == nullptr)
        font = &r_defaultFont;

    cg_t& cg = CG_GetLocalClientGlobals(localClientNum);

    const float screenScaleX = cg.viewport.w * Dvar_GetInt(*vid_width);
    const float screenScaleY = cg.viewport.h * Dvar_GetInt(*vid_height);

    const float min_x = rect.x * screenScaleX;
    const float max_x = (rect.x + rect.w) * screenScaleX;
    const float min_y = (rect.y + rect.h) * screenScaleY - font->atlas_height;
    const float max_y = rect.y * screenScaleY;

    float x = min_x;
    float y = min_y;

    GL_CALL(glUseProgram,      font->prog.program);
    GL_CALL(glEnable,          GL_BLEND);
    GL_CALL(glBlendFunc,       GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GL_CALL(glBindVertexArray, font->vb.vao);
    GL_CALL(glBindBuffer,      GL_ARRAY_BUFFER, font->vb.vbo);
    GL_CALL(glActiveTexture,   GL_TEXTURE0);
    GL_CALL(glBindTexture,     GL_TEXTURE_2D, font->tex);

    R_SetUniform(font->prog.program, "uTextColor", color);

    R_SetUniform(
        font->prog.program, "uOrthoProjection", cg.camera.orthoProjection
    );

    char            last_c = '\0';
    float           last_w = 0, last_h = 0;
    const GlyphDef* last_g = nullptr;

    int i = right ? (int)text.size() - 1 : 0;
    const int end = right ? -1 : (int)text.size();

    while (i != end) {
        char c = text.at(i);
        // Make sure c != last_c so that g gets bound to a valid glyph instead
        // of getting bound to last_g and derefing NULL on the first iteration
        // of the loop.
        assert(c != '\0');

        if (c == '\n') {
            y -= font->atlas_height;
            if (y < max_y)
                break;
            x = min_x;
            right ? i-- : i++;
            continue;
        }

        if (c < 32 || c > 127)
            c = '#';

        // Reuse the last glyph if it's going to be rendered again.
        const GlyphDef* g = nullptr;
        if (c == last_c)
            g = last_g;
        else
            g = font->GetGlyph(c);

        // If the glyph couldn't be retrieved, use '#' as a placeholder.
        if (g == nullptr)
            g = font->GetGlyph('#');

        // If '#' couldn't be retrieved either, just move on to the next char.
        if (g == nullptr) {
            right ? i-- : i++;
            continue;
        }

        // Skip rendering a glyph if it has no advance
        if (g->advance_x == 0) {
            right ? i-- : i++;
            continue;
        }

        // Wrap the text if it goes beyond the boundaries of rect
        if (x + g->advance_x > max_x) {
            y -= font->atlas_height;
            if (y < max_y)
                break;

            x = min_x;
        }

        // Scale the width and height and update the last ones, or reuse the 
        // last ones if the same glyph is being rendered.
        float w = c == last_c ? last_w
            : g->width * xscale;
        float h = c == last_c ? last_h
            : g->height * yscale;

        last_w = w;
        last_h = h;

        // Glyphs can have no width or height because they have no pixel data
        // but a nonzero advance (i.e., space). Skip rendering on those and
        // just update the cursor position.
        if (g->width == 0 || g->height == 0) {
            if (right)
                x -= g->advance_x * xscale;
            else
                x += g->advance_x * xscale;
            last_c = c;
            last_g = g;
            right ? i-- : i++;
            continue;
        }

        float xpos = x + g->left;
        float ypos = y - (g->height - g->top);

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(xpos, ypos, 0.0f));
        model = glm::scale(model, glm::vec3(w, h, 0.0f));
        R_SetUniform(font->prog.program, "uModel", model);

        // If the same glyph is being rendered again, the coord uniform doesn't
        // need to be updated.
        if (c != last_c) {
            glm::vec4 atlasCoord(
                g->atlas_x, g->atlas_y,
                (float)g->width / (float)font->atlas_width,
                (float)g->height / (float)font->atlas_height
            );
            R_SetUniform(font->prog.program, "uAtlasCoord", atlasCoord);
        }

        GL_CALL(glDrawArrays, GL_TRIANGLES, 0, 6);

        if (right)
            x -= g->advance_x * xscale;
        else
            x += g->advance_x * xscale;
        last_c = c;
        last_g = g;
        right ? i-- : i++;
    }

    GL_CALL(glBindTexture, GL_TEXTURE_2D, 0);
    GL_CALL(glBindBuffer, GL_ARRAY_BUFFER, 0);
    GL_CALL(glBindVertexArray, 0);
    GL_CALL(glUseProgram, 0);
    GL_CALL(glDisable, GL_BLEND);
}

std::array<std::array<GfxTextDraw, 256>, MAX_LOCAL_CLIENTS> r_textDraws;

bool R_GetTextDraw(size_t localClientNum, size_t id, A_OUT GfxTextDraw*& draw) 
{
    draw = nullptr;
    assert(id < r_textDraws.at(localClientNum).size());

    draw = &r_textDraws.at(localClientNum).at(id);
    assert(draw);
    return !draw->free;
}


bool R_FindFreeTextDraw(
    size_t localClientNum, A_OUT size_t& index, A_OUT GfxTextDraw*& draw
) {
    index = (size_t)-1;
    draw = nullptr;

    for (int i = 0; i < (int)r_textDraws.at(localClientNum).size(); i++) {
        R_GetTextDraw(localClientNum, i, draw);
        if (draw->free) {
            index = i;
            return true;
        }
    }

    return false;
}

bool R_AddTextDrawDef(
    size_t localClientNum, FontDef* font, const RectDef& rect, 
    std::string text, float xscale, float yscale, glm::vec3 color, bool active,
    bool right, A_OUT size_t& id
) {
    GfxTextDraw* d = nullptr;
    if (!R_FindFreeTextDraw(localClientNum, id, d))
        return false;

    d->free   = false;
    d->font   = font;
    d->text   = text;
    d->rect   = rect;
    d->xscale = xscale;
    d->yscale = yscale;
    d->color  = color;
    d->active = active;
    d->right  = right;

    return true;
}

bool R_UpdateTextDrawDef(size_t localClientNum, size_t id, std::string text) {
    GfxTextDraw* d = nullptr;
    if (!R_GetTextDraw(localClientNum, id, d))
        return false;

    d->text = text;
    return true;
}

bool R_ActivateTextDrawDef(size_t localClientNum, size_t id, bool active) {
    GfxTextDraw* d = nullptr;
    R_GetTextDraw(localClientNum, id, d);
    bool wasActive = d->active;
    d->active = active;
    return wasActive;
}

bool R_RemoveTextDrawDef(size_t localClientNum, size_t id) {
    GfxTextDraw* d = nullptr;
    if (!R_GetTextDraw(localClientNum, id, d))
        return false;

    if (d->free == true)
        return false;

    d->free   = true;
    d->active = false;
    return true;
}

void R_ClearTextDrawDefs(size_t localClientNum) {
    for (auto& c : r_textDraws.at(localClientNum)) {
        c.active = false;
        c.free   = true;
    }
}

void R_DrawTextDrawDefs(size_t localClientNum) {
    for (const auto& c : r_textDraws.at(localClientNum)) {
        if (!c.free && c.active) {
            R_DrawText(
                localClientNum, c.font, c.rect, c.text,
                c.xscale, c.yscale, c.color, c.right
            );
        }
    }
}

void R_DeleteTextureAtlas(A_INOUT FontDef* f) {
    GL_CALL(glDeleteTextures,   1, &f->tex);
    GL_CALL(R_DeleteVertexBuffer,  &f->vb);
    GL_CALL(R_DeleteShaderProgram, &f->prog);
}
