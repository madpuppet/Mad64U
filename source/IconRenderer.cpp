#include "common.h"
#include "IconRenderer.h"
#include "SDL3/SDL.h"
#include "SDL3_image/SDL_image.h"
#include <unordered_map>
#include <algorithm>

IconRenderer::IconRenderer()
{
    LoadImage("data/icon_close.png");
    LoadImage("data/icon_fullscreen.png");
    LoadImage("data/icon_windowed.png");
    LoadImage("data/icon_resize.png");
    LoadImage("data/icon_submenu.png");
    LoadImage("data/icon_layoutLocked.png");
    LoadImage("data/icon_layoutFree.png");
    LoadImage("data/icon_build.png");
    LoadImage("data/icon_run.png");
}

IconRenderer::~IconRenderer()
{
}

void IconRenderer::LoadImage(const char* name)
{
    m_iconSurfaces.push_back(IMG_Load(name));
}

void IconRenderer::FlushRenderer(SDL_Renderer *renderer)
{
    std::erase_if(m_map, [renderer](const auto& Pair) { return Pair.second->renderer == renderer; });
}

size_t IconRenderer::Hash(SDL_Renderer* renderer, Icons icon)
{
        size_t hash = (intptr_t)renderer;
        hash = HashU8(hash, (u8)icon);
        return hash;
}

void IconRenderer::DrawIcon(SDL_Renderer* renderer, Icons icon, int x, int y)
{
    CachedIcon* ci = nullptr;
    size_t hash = Hash(renderer, icon);
    auto it = m_map.find(hash);
    if (it != m_map.end())
    {
        ci = it->second;
    }
    else
    {
        ci = new CachedIcon;
        ci->icon = icon;
        ci->renderer = renderer;
        ci->texture = SDL_CreateTextureFromSurface(renderer, m_iconSurfaces[(int)icon]);
        m_map[hash] = ci;
    }

    float w, h;
    SDL_GetTextureSize(ci->texture, &w, &h);

    SDL_FRect dest{ x - w/2,y - w/2, w, h };
    SDL_RenderTexture(renderer, ci->texture, nullptr, &dest);
}

Recti IconRenderer::CalcIconArea(Icons icon, int x, int y)
{
    int w = 16;
    int h = 16;
    return Recti{ x - w / 2,y - w / 2, w, h };
}

