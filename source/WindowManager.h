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

// these go to ALL windows,  unlike SDL Events only go to active windows
enum class WindowMessage
{
    File_Added,
    File_Deleted,
    File_Renamed,
    Query_FileCount,
    Query_WindowCount,
    Query_Highlight,
    Query_FindFileWindow,
    Query_FindLockedLayout
};

#define WMF_Window 1                // send message to window (layout tabs)
#define WMF_Layout 2                // layouts can respond the message
#define WMF_Menu 4                 // menus can respond to the message
#define WMF_EarlyOut 8              // exit as soon as anyone responds (m_response > 0)
#define WMF_AreaCheck 16            // use layout area checks - only active tabs will get processed

struct WindowMessageStruct
{
    int m_flags = WMF_Window;       // just send to all windows by default
    WindowMessage m_type;
    class SourceFile* m_sourceFile = nullptr;
    int m_response = 0;             // response count - incremented whenever a window or layout respons, if WMF_EarlyOut, drop out after 1 response

    WindowTree* m_tree = nullptr;           // this can be initialised to restrict message to single tree
    WindowLayout* m_layout = nullptr;
    WindowBase* m_window = nullptr;

    int m_x = 0;
    int m_y = 0;

    void* m_query = nullptr;
};


class WindowManager : public Singleton<WindowManager>
{
public:
    void HandleEvent(SDL_Event* e);
    WindowTree* FindWindowByID(int id);

    void Message(WindowMessageStruct& msg);

    const WindowDockQuery& GetWindowDockQuery() { return m_mouseDockQuery; }
    const WindowHighlightQuery& GetWindowHighlightQuery() { return m_mouseHighlightQuery; }
    const WindowHighlightQuery& GetWindowSelectionQuery() { return m_mouseSelectionQuery; }

    WindowLayout* GetActiveWindowLayout() { return m_activeLayout; }
    WindowTree* GetActiveWindowTree() { return m_activeTree; }
    WindowBase* GetActiveWindowBase();

    bool IsMovingSplit() { return m_mouseMode == MouseMode_MovingSplit; }
    void Paint();
    void PaintAll();
    void PaintMenu() { m_menuList.Paint(); }
    void LayoutMenu();
    void SetActiveTree(WindowTree* tree);
    void LayoutWindows();
    void RemoveWindow(WindowBase* window);
    void AddWindow(WindowBase* window);
    void Tick();

    // allow remove windows deferred (so we can remove from recursive WM call trees)
    void QueueRemoveWindow(WindowBase* window)
    {
        m_windowsToRemove.push_back(window);
    }
    bool RemoveQueuedWindows()
    {
        bool removedAny = false;
        for (auto win : m_windowsToRemove)
        {
            removedAny = true;
            RemoveWindow(win);
        }
        m_windowsToRemove.clear();
        return removedAny;
    }

    void SetDefaultActiveWindow();

    // used for test data
    void AddWindowTree(WindowTree* tree) { m_windowTrees.push_back(tree); tree->LayoutWindows(); }
    void AddWindowMenu(WindowMenu* menu) { m_menuList.m_menus.push_back(menu); }

    ~WindowManager();

    void SaveWindowLayout();
    void LoadWindowLayout();

    void OnWindowDestruct(WindowBase* win)
    {
        if (m_activeWindow == win)
            m_activeWindow = nullptr;
    }

protected:
    std::vector<WindowTree*> m_windowTrees;
    std::vector<WindowBase*> m_windowsToRemove;
    WindowMenuList m_menuList;

    enum MouseMode
    {
        MouseMode_Idle,
        MouseMode_MovingWindow,
        MouseMode_ResizingWindow,
        MouseMode_SelectingTab,
        MouseMode_MovingSplit,
        MouseMode_UsingMenu,
        MouseMode_ScrollBar
    } m_mouseMode = MouseMode_Idle;
    Vec2i m_mouseGrabPos;
    Vec2i m_mouseInitial;
    WindowDockQuery m_mouseDockQuery;
    WindowHighlightQuery m_mouseHighlightQuery;
    WindowHighlightQuery m_mouseSelectionQuery;
    WindowTree* m_mouseTree = nullptr;
    WindowTree* m_mouseOriginateTree = nullptr;
    WindowTree* m_activeTree = nullptr;
    WindowLayout* m_activeLayout = nullptr;
    WindowBase* m_activeWindow = nullptr;
};
