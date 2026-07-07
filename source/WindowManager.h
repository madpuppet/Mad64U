#pragma once

#include "Singleton.h"
#include "WindowTree.h"
#include "WindowMenuList.h"

class WindowManager : public Singleton<WindowManager>
{
public:
    void HandleEvent(SDL_Event* e);
    WindowTree* FindWindowByID(int id);
    const WindowDockQuery& GetWindowDockQuery() { return m_mouseDockQuery; }
    const WindowSplitQuery& GetWindowSplitQuery() { return m_mouseSplitQuery; }
    WindowLayout* GetActiveWindowLayout() { return m_activeLayout; }
    WindowTree* GetActiveWindowTree() { return m_activeTree; }
    bool IsMovingSplit() { return m_mouseMode == MouseMode_MovingSplit; }
    void Paint();
    void PaintMenu() { m_menuTree.Paint(); }
    void LayoutMenu();
    void SetActiveTree(WindowTree* tree);

    // used for test data
    void AddWindowTree(WindowTree* tree) { m_windowTrees.push_back(tree); tree->LayoutWindows(); }
    void AddWindowMenu(WindowMenu* menu) { m_menuTree.m_menus.push_back(menu); }

    ~WindowManager();

protected:
    std::vector<WindowTree*> m_windowTrees;
    WindowMenuList m_menuTree;

    enum MouseMode
    {
        MouseMode_Idle,
        MouseMode_MovingWindow,
        MouseMode_ResizingWindow,
        MouseMode_SelectingTab,
        MouseMode_MovingSplit
    } m_mouseMode = MouseMode_Idle;
    Vec2i m_mouseGrabPos;
    Vec2i m_mouseInitial;
    WindowDockQuery m_mouseDockQuery;
    WindowTabQuery m_mouseTabQuery;
    WindowSplitQuery m_mouseSplitQuery;
    WindowTree* m_mouseTree = nullptr;
    WindowTree* m_mouseOriginateTree = nullptr;
    WindowTree* m_activeTree = nullptr;
    WindowLayout* m_activeLayout = nullptr;
};
