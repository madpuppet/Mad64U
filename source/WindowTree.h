#pragma once

#include <functional>
#include "WindowQueries.h"
#include "WindowLayout.h"
#include "WindowBase.h"
#include "IconRenderer.h"

#define WINDOW_TITLE_BAR_HEIGHT 24
#define WINDOW_TAB_BAR_HEIGHT 24
#define WINDOW_CLIENT_BORDER 5
#define TEXT_HBORDER 4

struct WindowTree
{
    WindowTree(const Recti& area);
    ~WindowTree();

    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    WindowLayout m_layout;
    Recti m_windowedRect{ 100,100,640,512 };
    u32 m_windowID = 0;
    bool m_dirty = true;
    bool m_fullscreen = false;

    void LayoutWindows();
    void AddWindow(int x, int y, WindowBase* window);
    void RemoveWindow(WindowBase* window);
    bool CheckForDocking(int x, int y, WindowDockQuery &query);
    void Paint(Recti *area);
    void GatherWindows(std::vector<WindowBase*>& windows);
    bool CheckForTab(int x, int y, WindowTabQuery& query);
    int CountWindows();
    void CollapseEmptyLayouts();
    Icons FindIcon(int x, int y);
    bool CheckForSplit(int x, int y, WindowSplitQuery& query);
    bool CheckForLayout(int x, int y, WindowLayout*& layout);

    void MakeFullscreen();
    void MakeWindowed();
};

