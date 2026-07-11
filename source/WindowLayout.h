#pragma once

#include "WindowBase.h"
#include "WindowQueries.h"

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
    std::vector<WindowBase*> m_tabs;
    int m_activeTab = 0;

    void Layout(SDL_Renderer* renderer, const Recti& area);
    void AddWindow(int x, int y, WindowBase* window);
    void AddWindow(WindowBase* window);
    void RemoveWindow(WindowBase* window);
    bool CheckForDocking(int x, int y, WindowDockQuery& query);
    void Paint(SDL_Renderer* renderer, const Recti& area);
    void GatherWindows(std::vector<WindowBase*>& windows);
    bool CheckForTab(int x, int y, WindowTabQuery& query);
    int CountWindows();
    void CollapseEmptyLayouts();
    bool CheckForSplit(int x, int y, WindowSplitQuery& query);
    bool CheckForLayout(int x, int y, WindowLayout*& layout);
    bool CheckForScrollBar(int x, int y, WindowScrollBarQuery& query);
    bool Tick();
    class WindowBase* GetActiveWindow();

    WindowLayout* FindFirstNonSplitLayout();
    WindowLayout* FindLayoutFromWindow(WindowBase* window, int& tabIdx);
};

