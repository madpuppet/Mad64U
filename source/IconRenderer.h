#pragma once

#include <unordered_map>
#include "Singleton.h"

enum class Icons
{
    None = -1,

    Highlight,
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
    ReplaceAll,
    Breakpoint,
    Pause,
    SingleStep,
    Zoom,
    Multicolour,
    Aligned,
    TabClose,
    ColourBall,
    Digit1,
    Digit2,
    Digit3,
    Digit4
};

class IconRenderer : public Singleton<IconRenderer>
{
public:
    IconRenderer();
    ~IconRenderer();

    void SetIconColor(SDL_Renderer* renderer, Icons icon, u32 color32);
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
