#pragma once

#include <functional>
#include "IconRenderer.h"

#define WINDOW_TITLE_BAR_HEIGHT 24
#define WINDOW_TAB_BAR_HEIGHT 24
#define WINDOW_CLIENT_BORDER 5
#define TEXT_HBORDER 4

struct WindowDockQuery
{
    enum SplitPosition
    {
        SplitPosition_None,
        SplitPosition_Left,
        SplitPosition_Right,
        SplitPosition_Top,
        SplitPosition_Bottom
    } m_splitPosition;

    bool m_foundDock = false;
    struct WindowTree* m_tree = nullptr;
    struct WindowLayout* m_layout = nullptr;
    bool m_verticalSplit = false;
    Recti m_bodyArea;
    Recti m_splitArea;
};

struct WindowTabQuery
{
    struct WindowTree* m_tree = nullptr;
    struct WindowLayout* m_layout = nullptr;
    int m_tabIndex = -1;
};

struct VirtualWindow
{
    std::string m_name;
    Recti m_area;
    std::function<void(SDL_Renderer*, const Recti&)> m_paintCallback;
    Recti m_tabArea;
};

struct WindowLayout
{
    ~WindowLayout();

    enum SplitType
    {
        NoSplit,
        Vertical,
        Horizontal
    } m_splitType = NoSplit;

    Recti m_area{ 0,0,0,0 };
    WindowLayout* m_splits[2] = { 0,0 };
    float m_splitPercentage = 0.5f;
    std::vector<VirtualWindow*> m_tabs;
    int m_activeTab = 0;

    void Layout(SDL_Renderer *renderer, const Recti& area);
    void AddWindow(int x, int y, VirtualWindow* window);
    void RemoveWindow(VirtualWindow* window);
    bool CheckForDocking(int x, int y, WindowDockQuery &query);
    void Paint(SDL_Renderer* renderer, const Recti& area);
    void GatherVirtualWindows(std::vector<VirtualWindow*>& virtualWindows);
    bool CheckForTab(int x, int y, WindowTabQuery& query);
    int CountVirtualWindows();
    void CollapseEmptyLayouts();
};

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
    void AddWindow(int x, int y, VirtualWindow* window);
    void RemoveWindow(VirtualWindow* window);
    bool CheckForDocking(int x, int y, WindowDockQuery &query);
    void Paint(Recti *area);
    void GatherVirtualWindows(std::vector<VirtualWindow*>& virtualWindows);
    bool CheckForTab(int x, int y, WindowTabQuery& query);
    int CountVirtualWindows();
    void CollapseEmptyLayouts();
    Icons FindIcon(int x, int y);
};
