#pragma once

#include "Singleton.h"
#include "WindowTree.h"
#include "WindowMenuList.h"

struct WindowHighlightState
{
    enum ObjectType
    {
        None,
        Menu,
        MenuItem,
        Icon
    };
    WindowTree* m_tree;
    WindowLayout* m_layout;
    Icons m_iconType;
};


class WindowManager : public Singleton<WindowManager>
{
public:
    void HandleEvent(SDL_Event* e);
    WindowTree* FindWindowByID(int id);

    const WindowDockQuery& GetWindowDockQuery() { return m_mouseDockQuery; }
    const WindowSplitQuery& GetWindowSplitQuery() { return m_mouseSplitQuery; }
    const WindowMenuQuery& GetWindowMenuQuery() { return m_mouseMenuQuery; }

    WindowLayout* GetActiveWindowLayout() { return m_activeLayout; }
    WindowTree* GetActiveWindowTree() { return m_activeTree; }

    bool IsMovingSplit() { return m_mouseMode == MouseMode_MovingSplit; }
    void Paint();
    void PaintMenu(const WindowMenuQuery& highlight) { m_menuList.Paint(highlight); }
    void LayoutMenu();
    void SetActiveTree(WindowTree* tree);

    // used for test data
    void AddWindowTree(WindowTree* tree) { m_windowTrees.push_back(tree); tree->LayoutWindows(); }
    void AddWindowMenu(WindowMenu* menu) { m_menuList.m_menus.push_back(menu); }

    ~WindowManager();

protected:
    std::vector<WindowTree*> m_windowTrees;
    WindowMenuList m_menuList;

    enum MouseMode
    {
        MouseMode_Idle,
        MouseMode_MovingWindow,
        MouseMode_ResizingWindow,
        MouseMode_SelectingTab,
        MouseMode_MovingSplit,
        MouseMode_UsingMenu
    } m_mouseMode = MouseMode_Idle;
    Vec2i m_mouseGrabPos;
    Vec2i m_mouseInitial;
    WindowDockQuery m_mouseDockQuery;
    WindowTabQuery m_mouseTabQuery;
    WindowSplitQuery m_mouseSplitQuery;
    WindowMenuQuery m_mouseMenuQuery;
    WindowTree* m_mouseTree = nullptr;
    WindowTree* m_mouseOriginateTree = nullptr;
    WindowTree* m_activeTree = nullptr;
    WindowLayout* m_activeLayout = nullptr;
};
