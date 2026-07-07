#pragma once

#include <unordered_map>
#include "Singleton.h"

enum Icons
{
    Icon_None = -1,

    Icon_Close,
    Icon_Fullscreen,
    Icon_Windowed,
    Icon_Resize
};

class IconRenderer : public Singleton<IconRenderer>
{
public:
    IconRenderer();
    ~IconRenderer();

    void DrawIcon(SDL_Renderer *renderer, Icons icon, int x, int y);
    void FlushRenderer(SDL_Renderer* renderer);

private:
    struct CachedIcon
    {
        int icon;
        SDL_Renderer* renderer;
        SDL_Texture* texture;
    };
    void LoadImage(const char *name);
    std::vector<SDL_Surface*> m_iconSurfaces;
    std::unordered_map<size_t, CachedIcon*> m_map;
    size_t Hash(SDL_Renderer* renderer, Icons icon);
};
