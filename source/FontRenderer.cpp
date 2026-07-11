#include "common.h"
#include "FontRenderer.h"
#include <algorithm>
#include "Application.h"
#include "SDL3/SDL.h"
#include "SDL3_ttf/SDL_ttf.h"

#define MAX_ITEMS 8192

//FontRenderer* FontRenderer::s_instance = nullptr;

size_t FontRenderer::Hash(SDL_Renderer* renderer, const std::string &str, FontType fontIdx)
{
    size_t hash = (intptr_t)renderer;
    for (int i = 0; i < str.size(); i++)
    {
        hash = HashU8(hash, str[i]);
    }
    hash = HashU8(hash, (u8)fontIdx);
    return hash;
}

std::vector<u16>& StringToUnicode(const std::string& str)
{
    static std::vector<u16> convert_unicode;
    size_t len = str.size() + 1;
    convert_unicode.resize(len);
    for (size_t i = 0; i < len; i++)
    {
        convert_unicode[i] = (u16)((u8)str[i]);
    }
    return convert_unicode;
}

// split render into 2 phases, allowing you to do other things with the render rectangle
FontRenderer::CachedString *FontRenderer::PrepareRender(SDL_Renderer * renderer, const std::string & str, int x, int y, FontType fontIdx)
{
    TTF_Font* font = m_fonts[(int)fontIdx];
    CachedString* cs = nullptr;
    size_t hash = Hash(renderer, str, fontIdx);

    auto it = m_map.find(hash);
    if (it != m_map.end())
    {
        // check for collision
        if (it->second->str != str || it->second->renderer != renderer)
        {
            Log("Collision: %s -> %s\n", str.c_str(), it->second->str.c_str());
        }
        cs = it->second;

        m_lru.Touch(cs);

        cs->rect.x = (float)x;
        cs->rect.y = (float)y;
    }
    else
    {
        if (m_lru.Num() == MAX_ITEMS)
        {
            // list is full, grab the least recently used one, move it the most recent position
            cs = m_lru.Pop();
            m_map.erase(cs->hash);
            SDL_DestroyTexture(cs->tex);
        }
        else
        {
            // list is not full, create a new element and add it to the queue
            cs = new CachedString();
            m_lru.Add(cs);
        }

        cs->str = str;
        cs->hash = hash;
        cs->renderer = renderer;
        cs->rect.x = (float)x;
        cs->rect.y = (float)y;

        m_map[hash] = cs;

        SDL_Color white = { 255, 255, 255, 255 };
        SDL_Surface* surface = TTF_RenderText_Blended(font, str.c_str(), str.size(), white);
        cs->tex = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_SetTextureBlendMode(cs->tex, SDL_BLENDMODE_BLEND);
        SDL_DestroySurface(surface);
        auto propertyID = SDL_GetTextureProperties(cs->tex);
        cs->rect.w = (float)SDL_GetNumberProperty(propertyID, SDL_PROP_TEXTURE_WIDTH_NUMBER, 16);
        cs->rect.h = (float)SDL_GetNumberProperty(propertyID, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 16);
    }
    return cs;
}

FontRenderer::FontRenderer()
{
    TTF_Init();
    m_fonts[(int)FontType::UI] = TTF_OpenFont("data/font.ttf", 16);
    if (m_fonts[(int)FontType::UI] == nullptr)
    {
        Log("Unable to create ui font: font.ttf\n");
        exit(0);
    }
    m_fonts[(int)FontType::Text] = TTF_OpenFont("data/font.ttf", 16);
    if (m_fonts[(int)FontType::Text] == nullptr)
    {
        Log("Unable to create text font: font.ttf\n");
        exit(0);
    }
}

FontRenderer::~FontRenderer()
{
    TTF_CloseFont(m_fonts[(int)FontType::UI]);
    TTF_CloseFont(m_fonts[(int)FontType::Text]);
}

int FontRenderer::GetCharactorWidth(FontType fontIdx, u32 ch)
{
    TTF_Font* font = m_fonts[(int)fontIdx];

    int advance = 0;
    TTF_GetGlyphMetrics(font, ch, nullptr, nullptr, nullptr, nullptr, &advance);
    return advance;
}

void FontRenderer::FlushRenderer(SDL_Renderer* renderer)
{
    // Remove all entries where the value is empty.
    std::erase_if(m_map, [renderer](const auto& Pair) { return Pair.second->renderer == renderer; });
    m_lru.RemoveIf([renderer](const auto& Item) { return Item->renderer == renderer; });
}

void FontRenderer::Render(CachedString* cs, const SDL_Color& col)
{
    // got tex, now render it
    SDL_SetTextureColorMod(cs->tex, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(cs->tex, col.a);
    SDL_RenderTexture(cs->renderer, cs->tex, NULL, &cs->rect);
}

void FontRenderer::RenderAt(CachedString* cs, const SDL_Color& col, int x, int y)
{
    // got tex, now render it
    SDL_FRect quad = { (float)x, (float)y, (float)cs->rect.w, (float)cs->rect.h };
    SDL_SetTextureColorMod(cs->tex, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(cs->tex, col.a);
    SDL_RenderTexture(cs->renderer, cs->tex, NULL, &quad);
}

void FontRenderer::RenderText(SDL_Renderer* renderer, const std::string& str, const SDL_Color& col, int x, int y, FontType fontIdx)
{
    auto cs = PrepareRender(renderer, str, x, y, fontIdx);
    Render(cs, col);
}

void FontRenderer::CalcTextArea(SDL_Renderer* renderer, const std::string& str, const Vec2i &pos, FontType fontIdx, Recti &area)
{
    auto cs = PrepareRender(renderer, str, pos.x, pos.y, fontIdx);
    area = Recti{ (int)cs->rect.x, (int)cs->rect.y, (int)cs->rect.w, (int)cs->rect.h };
}
