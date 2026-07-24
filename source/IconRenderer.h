#pragma once

#include <unordered_map>
#include "Singleton.h"

enum class Icons
{
    None = -1,

    Close,
    Fullscreen,
    Windowed,
    Resize,
    Submenu,
    LayoutLocked,
    LayoutFree,
    Build,
    Run,
    Deploy,
    NextItem,
    PrevItem,
    ReplaceOne,
    ReplaceAll
};

class IconRenderer : public Singleton<IconRenderer>
{
public:
    IconRenderer();
    ~IconRenderer();

    void DrawIcon(SDL_Renderer *renderer, Icons icon, int x, int y);
    Recti CalcIconArea(Icons icon, int x, int y);

    void FlushRenderer(SDL_Renderer* renderer);

private:
    struct CachedIcon
    {
        Icons icon;
        SDL_Renderer* renderer;
        SDL_Texture* texture;
    };
    void LoadImage(const char *name);
    std::vector<SDL_Surface*> m_iconSurfaces;
    std::unordered_map<size_t, CachedIcon*> m_map;
    size_t Hash(SDL_Renderer* renderer, Icons icon);
};
