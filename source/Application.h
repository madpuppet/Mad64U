#pragma once

#include "WindowTree.h"
#include "CachedFontRenderer.h"
#include "IconRenderer.h"

class Application
{
public:
    static Application* s_instance;
    static Application& Instance() { return *s_instance; };

    Application() { s_instance = this; }
    int Run(int argc);
    void HandleEvent(SDL_Event* e);
    ~Application() {}

    TTF_Font* GetUIFont() { return m_uiFont; }
    TTF_Font* GetTextFont() { return m_textFont; }
    const WindowDockQuery& GetWindowDockQuery() { return m_mouseDockQuery; }
    CachedFontRenderer& GetFontRenderer() { return *m_fontRenderer; }
    IconRenderer& GetIconRenderer() { return *m_iconRenderer; }

protected:
    std::vector<WindowTree*> m_windowTrees;
    TTF_Font* m_uiFont = nullptr;
    TTF_Font* m_textFont = nullptr;
    bool m_quit = false;
    WindowTree* FindWindow(int id);

    enum MouseMode
    {
        MouseMode_Idle,
        MouseMode_MovingWindow,
        MouseMode_ResizingWindow,
        MouseMode_SelectingTab
    } m_mouseMode = MouseMode_Idle;
    Vec2i m_mouseGrabPos;
    Vec2i m_mouseInitial;
    WindowDockQuery m_mouseDockQuery;
    WindowTabQuery m_mouseTabQuery;
    WindowTree* m_mouseTree = nullptr;
    WindowTree* m_mouseOriginateTree = nullptr;
    CachedFontRenderer* m_fontRenderer;
    IconRenderer* m_iconRenderer;
};

