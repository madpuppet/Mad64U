#include "common.h"
#include "WindowManager.h"
#include "WindowTree.h"
#include "Application.h"

WindowTree* WindowManager::FindWindowByID(int id)
{
    for (auto t : m_windowTrees)
    {
        if (t->m_windowID == id)
            return t;
    }
    return nullptr;
}

void WindowManager::LayoutMenu()
{
    if (m_activeTree)
    {
        m_menuList.Layout(m_activeTree);
    }
}

WindowBase* WindowManager::GetActiveWindowBase()
{
    if (m_activeLayout && m_activeLayout->m_splitType == WindowLayout::NoSplit && m_activeLayout->m_tabs.size() > 0)
        return m_activeLayout->m_tabs[m_activeLayout->m_activeTab];
    return nullptr;
}

void WindowManager::LayoutWindows()
{
    for (auto tree : m_windowTrees)
        tree->LayoutWindows();
}

void WindowManager::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            WindowTree* tree = FindWindowByID(e->button.windowID);
            if (tree)
            {
                tree->m_dirty = true;

                auto icon = tree->FindIcon((int)e->button.x, (int)e->button.y);
                if (icon != Icon_None)
                {
                    switch (icon)
                    {
                        case Icon_Fullscreen:
                        {
                            tree->MakeFullscreen();
                            return;
                        }
                        break;

                        case Icon_Windowed:
                        {
                            tree->MakeWindowed();
                            return;
                        }
                        break;

                        case Icon_Close:
                        {
                            auto it = std::find(m_windowTrees.begin(), m_windowTrees.end(), tree);
                            if (it != m_windowTrees.end()) {
                                m_windowTrees.erase(it);
                            }
                            delete tree;
                            if (m_windowTrees.empty())
                                Application::Instance().Quit();
                            return;
                        }
                        break;

                        case Icon_Resize:
                        {
                            int w, h;
                            SDL_GetWindowSizeInPixels(tree->m_window, &w, &h);
                            if ((w - e->button.x) < 15 && (h - e->button.y) < 15)
                            {
                                m_mouseMode = MouseMode_ResizingWindow;
                                m_mouseTree = tree;
                                m_mouseGrabPos.x = (int)e->button.x;
                                m_mouseGrabPos.y = (int)e->button.y;
                                m_mouseInitial.x = w;
                                m_mouseInitial.y = h;
                                SDL_CaptureMouse(true);
                            }
                            return;
                        }
                        break;

                        default:
                            break;
                    }
                }

                if (m_menuList.CheckForMenu(tree, (int)e->button.x, (int)e->button.y, m_mouseMenuQuery))
                {
                    auto menu = m_menuList.m_menus[m_mouseMenuQuery.m_menuIdx];
                    menu->m_open = true;
                    m_mouseMode = MouseMode_UsingMenu;
                    m_activeTree = tree;
                    SDL_CaptureMouse(true);
                    return;
                }

                // check for dragging the title bar
                if (!tree->m_fullscreen && e->button.y < WINDOW_TITLE_BAR_HEIGHT)
                {
                    int x, y;
                    SDL_GetWindowPosition(tree->m_window, &x, &y);

                    float mx, my;
                    SDL_GetGlobalMouseState(&mx, &my);
                    m_mouseMode = MouseMode_MovingWindow;
                    m_mouseTree = tree;
                    m_mouseOriginateTree = nullptr;
                    m_mouseGrabPos.x = (int)mx;
                    m_mouseGrabPos.y = (int)my;
                    m_mouseInitial.x = x;
                    m_mouseInitial.y = y;
                    SDL_CaptureMouse(true);
                    return;
                }

                // check for a tab
                m_mouseTabQuery.m_tabIndex = -1;
                if (tree->CheckForTab((int)e->button.x, (int)e->button.y, m_mouseTabQuery))
                {
                    m_activeTree = m_mouseTabQuery.m_tree;
                    m_activeLayout = m_mouseTabQuery.m_layout;
                    m_menuList.Layout(m_activeTree);

                    if (tree->CountWindows() == 1)
                    {
                        int x, y;
                        SDL_GetWindowPosition(tree->m_window, &x, &y);

                        float mx, my;
                        SDL_GetGlobalMouseState(&mx, &my);
                        m_mouseMode = MouseMode_MovingWindow;
                        m_mouseTree = tree;
                        m_mouseOriginateTree = nullptr;
                        m_mouseGrabPos.x = (int)mx;
                        m_mouseGrabPos.y = (int)my;
                        m_mouseInitial.x = x;
                        m_mouseInitial.y = y;
                        SDL_CaptureMouse(true);
                        return;
                    }

                    m_mouseMode = MouseMode_SelectingTab;
                    m_mouseTabQuery.m_layout->m_activeTab = m_mouseTabQuery.m_tabIndex;

                    float mx, my;
                    SDL_GetGlobalMouseState(&mx, &my);
                    m_mouseGrabPos.x = (int)mx;
                    m_mouseGrabPos.y = (int)my;
                    return;
                }

                // check for split
                m_mouseSplitQuery.m_foundSplit = false;
                if (tree->CheckForSplit((int)e->button.x, (int)e->button.y, m_mouseSplitQuery))
                {
                    m_mouseMode = MouseMode_MovingSplit;
                    float mx, my;
                    SDL_GetGlobalMouseState(&mx, &my);
                    m_mouseGrabPos.x = (int)mx;
                    m_mouseGrabPos.y = (int)my;
                    SDL_CaptureMouse(true);
                    return;
                }

                // check for layout select
                WindowLayout* layout;
                if (tree->CheckForLayout((int)e->button.x, (int)e->button.y, layout))
                {
                    m_activeTree = tree;
                    m_activeLayout = layout;
                    m_menuList.Layout(m_activeTree);
                }
            }
        }
        break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            switch (m_mouseMode)
            {
                case MouseMode_UsingMenu:
                    SDL_CaptureMouse(false);
                    if (m_mouseMenuQuery.m_menuIdx != -1)
                    {
                        auto menu = m_menuList.m_menus[m_mouseMenuQuery.m_menuIdx];
                        menu->m_open = false;
                        m_mouseMenuQuery.m_tree->m_dirty = true;
                        if (m_mouseMenuQuery.m_menuItemIdx != -1)
                        {
                            auto item = menu->m_items[m_mouseMenuQuery.m_menuItemIdx];
                            item->m_activate();
                        }
                    }
                    m_mouseMode = MouseMode_Idle;
                    break;

                case MouseMode_SelectingTab:
                    m_mouseMode = MouseMode_Idle;
                    break;

                case MouseMode_ResizingWindow:
                    SDL_CaptureMouse(false);
                    m_mouseMode = MouseMode_Idle;
                    break;

                case MouseMode_MovingSplit:
                    SDL_CaptureMouse(false);
                    m_mouseMode = MouseMode_Idle;
                    m_mouseSplitQuery.m_foundSplit = false;
                    m_mouseSplitQuery.m_tree->m_dirty = true;
                    break;

                case MouseMode_MovingWindow:
                    SDL_CaptureMouse(false);
                    m_mouseMode = MouseMode_Idle;
                    m_mouseTree->m_dirty = true;
                    if (m_mouseDockQuery.m_foundDock)
                    {
                        m_activeTree = nullptr;
                        m_activeLayout = nullptr;

                        m_mouseDockQuery.m_tree->m_dirty = true;
                        m_mouseDockQuery.m_foundDock = false;

                        // dock all the virtual windows of the old window tree
                        std::vector<WindowBase*> windows;
                        m_mouseTree->GatherWindows(windows);

                        if (m_mouseDockQuery.m_splitPosition == WindowDockQuery::SplitPosition_None)
                        {
                            for (auto vw : windows)
                                m_mouseDockQuery.m_layout->m_tabs.push_back(vw);

                            m_activeTree = m_mouseDockQuery.m_tree;
                            m_activeLayout = m_mouseDockQuery.m_layout;
                            m_menuList.Layout(m_activeTree);
                        }
                        else
                        {
                            m_mouseDockQuery.m_layout->m_splits[0] = new WindowLayout;
                            m_mouseDockQuery.m_layout->m_splits[1] = new WindowLayout;
                            int splitIdx = 0;
                            switch (m_mouseDockQuery.m_splitPosition)
                            {
                                case WindowDockQuery::SplitPosition_Left:
                                    m_mouseDockQuery.m_layout->m_splitType = WindowLayout::Horizontal;
                                    splitIdx = 0;
                                    break;
                                case WindowDockQuery::SplitPosition_Right:
                                    m_mouseDockQuery.m_layout->m_splitType = WindowLayout::Horizontal;
                                    splitIdx = 1;
                                    break;
                                case WindowDockQuery::SplitPosition_Top:
                                    m_mouseDockQuery.m_layout->m_splitType = WindowLayout::Vertical;
                                    splitIdx = 0;
                                    break;
                                case WindowDockQuery::SplitPosition_Bottom:
                                    m_mouseDockQuery.m_layout->m_splitType = WindowLayout::Vertical;
                                    splitIdx = 1;
                                    break;
                            }
                            for (auto vw : windows)
                                m_mouseDockQuery.m_layout->m_splits[splitIdx]->m_tabs.push_back(vw);
                            for (auto vw : m_mouseDockQuery.m_layout->m_tabs)
                                m_mouseDockQuery.m_layout->m_splits[1 - splitIdx]->m_tabs.push_back(vw);
                            m_mouseDockQuery.m_layout->m_tabs.clear();

                            m_activeTree = m_mouseDockQuery.m_tree;
                            m_activeLayout = m_mouseDockQuery.m_layout->m_splits[splitIdx];
                            m_menuList.Layout(m_activeTree);
                        }

                        // delete the old window tree now..
                        auto it = std::find(m_windowTrees.begin(), m_windowTrees.end(), m_mouseTree);
                        m_windowTrees.erase(it);
                        delete m_mouseTree;
                        m_mouseTree = nullptr;

                        m_mouseDockQuery.m_tree->LayoutWindows();
                    }
                    break;
            }
        }
        break;

        case SDL_EVENT_MOUSE_MOTION:
        {
            switch (m_mouseMode)
            {
                case MouseMode_UsingMenu:
                {
                    if (m_mouseMenuQuery.m_tree)
                        m_mouseMenuQuery.m_tree->m_dirty = true;

                    int oldMenuIdx = m_mouseMenuQuery.m_menuIdx;
                    m_menuList.CheckForMenu(m_activeTree, (int)e->button.x, (int)e->button.y, m_mouseMenuQuery);
                    if (m_mouseMenuQuery.m_menuIdx != -1)
                    {
                        if (oldMenuIdx != -1 && m_mouseMenuQuery.m_menuIdx != oldMenuIdx)
                        {
                            m_menuList.m_menus[oldMenuIdx]->m_open = false;
                            m_menuList.m_menus[m_mouseMenuQuery.m_menuIdx]->m_open = true;
                        }
                    }
                    else
                    {
                        m_mouseMenuQuery.m_menuIdx = oldMenuIdx;
                    }
                }
                break;

                case MouseMode_SelectingTab:
                {
                    float mx, my;
                    SDL_GetGlobalMouseState(&mx, &my);
                    int dist = Abs((int)mx - m_mouseGrabPos.x) + Abs((int)my - m_mouseGrabPos.y);
                    if (dist > 15)
                    {
                        // if this isn't the only tab in the tree, then we can drag it out
                        int count = m_mouseTabQuery.m_tree->CountWindows();
                        if (count > 1)
                        {
                            m_activeLayout = nullptr;

                            // drag out virtual window into a new window tree
                            // go to MovingWindow mode
                            auto vw = m_mouseTabQuery.m_layout->m_tabs[m_mouseTabQuery.m_tabIndex];
                            m_mouseTabQuery.m_layout->m_tabs.erase(m_mouseTabQuery.m_layout->m_tabs.begin() + m_mouseTabQuery.m_tabIndex);
                            m_mouseTabQuery.m_layout->m_activeTab = 0;
                            if (m_mouseTabQuery.m_layout->m_tabs.empty())
                            {
                                // collapse empty tab
                                m_mouseTabQuery.m_tree->CollapseEmptyLayouts();
                            }
                            m_mouseTabQuery.m_tree->LayoutWindows();

                            float mx, my;
                            SDL_GetGlobalMouseState(&mx, &my);
                            m_mouseMode = MouseMode_MovingWindow;
                            m_mouseOriginateTree = m_mouseTabQuery.m_tree;
                            m_mouseGrabPos.x = (int)mx + vw->m_tabArea.w / 2;
                            m_mouseGrabPos.y = (int)my + WINDOW_TITLE_BAR_HEIGHT + WINDOW_TAB_BAR_HEIGHT / 2;
                            m_mouseInitial.x = (int)mx;
                            m_mouseInitial.y = (int)my;
                            m_mouseDockQuery.m_foundDock = false;

                            auto tree = new WindowTree({ (int)mx - m_mouseGrabPos.x + m_mouseInitial.x, (int)my - m_mouseGrabPos.y + m_mouseInitial.y,640,512 });
                            tree->m_layout.m_splitType = WindowLayout::NoSplit;
                            tree->m_layout.m_tabs.push_back(vw);
                            tree->m_dirty = true;
                            m_activeTree = tree;
                            m_activeLayout = &tree->m_layout;
                            m_windowTrees.push_back(tree);
                            m_mouseTree = tree;
                            m_menuList.Layout(tree);
                            tree->LayoutWindows();
                        }
                    }
                }
                break;

                case MouseMode_ResizingWindow:
                {
                    m_mouseTree->m_dirty = true;
                    int newW = Max(128, (int)e->button.x - m_mouseGrabPos.x + m_mouseInitial.x);
                    int newH = Max(128, (int)e->button.y - m_mouseGrabPos.y + m_mouseInitial.y);
                    SDL_SetWindowSize(m_mouseTree->m_window, newW, newH);
                    SDL_SyncWindow(m_mouseTree->m_window);
                    SDL_SetRenderViewport(m_mouseTree->m_renderer, nullptr);
                    m_mouseTree->LayoutWindows();
                    m_mouseTree->Paint(nullptr);
                }
                break;

                case MouseMode_MovingSplit:
                {
                    float mx, my;
                    SDL_GetGlobalMouseState(&mx, &my);
                    switch (m_mouseSplitQuery.m_layout->m_splitType)
                    {
                        case WindowLayout::Vertical:
                        {
                            int offset = (int)my - m_mouseGrabPos.y;
                            int splitPos = m_mouseSplitQuery.m_splitPos + offset;
                            m_mouseSplitQuery.m_layout->m_splitPercentage = Clamp((float)(splitPos - m_mouseSplitQuery.m_layout->m_area.y) / (float)m_mouseSplitQuery.m_layout->m_area.h, 0.0f, 1.0f);
                        }
                        break;

                        case WindowLayout::Horizontal:
                        {
                            int offset = (int)mx - m_mouseGrabPos.x;
                            int splitPos = m_mouseSplitQuery.m_splitPos + offset;
                            m_mouseSplitQuery.m_layout->m_splitPercentage = Clamp((float)(splitPos - m_mouseSplitQuery.m_layout->m_area.x) / (float)m_mouseSplitQuery.m_layout->m_area.w, 0.0f, 1.0f);
                        }
                        break;
                    }
                    m_mouseSplitQuery.m_tree->LayoutWindows();
                }
                break;

                case MouseMode_MovingWindow:
                {
                    m_mouseTree->m_dirty = true;
                    float mx, my;
                    SDL_GetGlobalMouseState(&mx, &my);

                    int newX = (int)mx - m_mouseGrabPos.x + m_mouseInitial.x;
                    int newY = (int)my - m_mouseGrabPos.y + m_mouseInitial.y;
                    SDL_SetWindowPosition(m_mouseTree->m_window, newX, newY);

                    // check if position is over another window for docking
                    if (m_mouseDockQuery.m_foundDock)
                        m_mouseDockQuery.m_tree->m_dirty = true;

                    m_mouseDockQuery.m_foundDock = false;
                    for (auto t : m_windowTrees)
                    {
                        if (t != m_mouseTree /* && t != m_mouseOriginateTree*/ && t->CheckForDocking((int)mx, (int)my, m_mouseDockQuery))
                            break;
                    }
                    if (m_mouseDockQuery.m_foundDock)
                        m_mouseDockQuery.m_tree->m_dirty = true;
                }
                break;

                case MouseMode_Idle:
                    break;
            }
        }
        break;

        case SDL_EVENT_MOUSE_WHEEL:
            break;

        case SDL_EVENT_TEXT_INPUT:
            break;

        case SDL_EVENT_KEY_DOWN:
        {
            const SDL_KeyboardEvent& key = e->key;
            if (!key.repeat && key.key == SDLK_RETURN && (key.mod & SDL_KMOD_ALT))
            {
                if (m_activeTree)
                {
                }
            }
            break;
        }

        case SDL_EVENT_KEY_UP:
            break;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        {
            WindowTree* tree = FindWindowByID(e->window.windowID);
            if (tree)
            {
                tree->m_dirty = true;
            }
        }
        break;

        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            return;

        case SDL_EVENT_JOYSTICK_BUTTON_UP:
            return;

        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
            return;
    }
}

void WindowManager::SetActiveTree(WindowTree* tree)
{
    m_activeTree = tree;
    m_activeLayout = tree->FindFirstNonSplitLayout();
    m_menuList.Layout(m_activeTree);
}


void WindowManager::Paint()
{
    for (auto tree : m_windowTrees)
    {
        if (tree->m_dirty)
        {
            tree->Paint(nullptr);
            tree->m_dirty = false;
        }
    }
}

void WindowManager::PaintAll()
{
    for (auto tree : m_windowTrees)
    {
        tree->Paint(nullptr);
        tree->m_dirty = false;
    }
}

WindowManager::~WindowManager()
{
    for (auto tree : m_windowTrees)
        delete tree;
}

void WindowManager::RemoveWindow(WindowBase* window)
{
    for (auto tree : m_windowTrees)
    {
        int tabIdx = 0;
        auto layout = tree->FindLayoutFromWindow(window, tabIdx);
        if (layout)
        {
            if (layout->m_activeTab == tabIdx)
                layout->m_activeTab = 0;
            layout->m_tabs.erase(layout->m_tabs.begin() + tabIdx);
            if (layout->m_tabs.empty())
            {
                // collapse empty tab
                tree->CollapseEmptyLayouts();
            }
            tree->LayoutWindows();
            return;
        }
    }
}



