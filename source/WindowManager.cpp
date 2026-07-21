#include "common.h"
#include "WindowManager.h"
#include "WindowTree.h"
#include "Application.h"
#include "Settings.h"
#include "SourceFileManager.h"
#include "LogManager.h"
#include <filesystem>

std::vector<std::string> GetSourceFiles(const std::string& pathStr)
{
    namespace fs = std::filesystem;

    std::vector<std::string> files;
    fs::path path(pathStr);

    if (!fs::exists(path))
        return files;

    // Does it exist and is it a directory?
    if (!fs::is_directory(path))
    {
        files.push_back(pathStr);
        return files;
    }

    for (const auto& entry : fs::directory_iterator(path))
    {
        if (!entry.is_regular_file())
            continue;

        fs::path file = entry.path();
        std::string ext = file.extension().string();

        if (ext == ".asm" || ext == ".c")
            files.push_back(file.string());
    }

    return files;
}



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

void WindowManager::Tick()
{
    for (auto tree : m_windowTrees)
        tree->Tick();
}


void WindowManager::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            if (e->button.button == 1)
            {
                m_mouseHighlightQuery.Reset();
                m_mouseSelectionQuery.Reset();

                WindowMessageStruct msg;
                msg.m_type = WindowMessage::Query_Highlight;
                msg.m_flags = WMF_Menu | WMF_AreaCheck | WMF_Layout | WMF_Window | WMF_EarlyOut;
                msg.m_query = &m_mouseSelectionQuery;
                msg.m_tree = FindWindowByID(e->button.windowID);
                if (!msg.m_tree)
                    break;

                msg.m_x = (int)e->button.x;
                msg.m_y = (int)e->button.y;
                Message(msg);

                switch (m_mouseSelectionQuery.m_highlight)
                {
                    case WindowHighlightType::ProjectListFile:
                    case WindowHighlightType::ProjectListFolder:
                    case WindowHighlightType::ProjectListIcon:
                        m_mouseSelectionQuery.m_window->HandleEvent(e);
                        return;

                    case WindowHighlightType::WindowLayoutIcon:
                        m_mouseSelectionQuery.m_layout->HandleEvent(e);
                        return;

                    case WindowHighlightType::WindowTreeIcon:
                        switch ((Icons)m_mouseSelectionQuery.m_id)
                        {
                            case Icons::Fullscreen:
                            {
                                m_mouseSelectionQuery.m_tree->MakeFullscreen();
                            }
                            return;

                            case Icons::Windowed:
                            {
                                m_mouseSelectionQuery.m_tree->MakeWindowed();
                            }
                            return;

                            case Icons::Close:
                            {
                                if (m_windowTrees.size() == 1)
                                {
                                    Application::Instance().Quit();
                                }
                                else
                                {
                                    auto it = std::find(m_windowTrees.begin(), m_windowTrees.end(), m_mouseSelectionQuery.m_tree);
                                    if (it != m_windowTrees.end()) {
                                        m_windowTrees.erase(it);
                                    }
                                    delete m_mouseSelectionQuery.m_tree;
                                    WindowManager::Instance().IndexWindows();
                                }
                                m_mouseSelectionQuery.Reset();
                            }
                            return;

                            case Icons::Resize:
                            {
                                int w, h;
                                SDL_GetWindowSizeInPixels(m_mouseSelectionQuery.m_tree->m_window, &w, &h);
                                if ((w - e->button.x) < 15 && (h - e->button.y) < 15)
                                {
                                    m_mouseMode = MouseMode_ResizingWindow;
                                    m_mouseTree = m_mouseSelectionQuery.m_tree;
                                    m_mouseGrabPos.x = (int)e->button.x;
                                    m_mouseGrabPos.y = (int)e->button.y;
                                    m_mouseInitial.x = w;
                                    m_mouseInitial.y = h;
                                    SDL_CaptureMouse(true);
                                }
                            }
                            return;
                        }
                        break;

                    case WindowHighlightType::Menu:
                    {
                        auto menu = m_menuList.m_menus[m_mouseSelectionQuery.m_menu.m_menuIdx];
                        menu->m_open = true;
                        m_mouseMode = MouseMode_UsingMenu;
                        SetActiveTree(m_mouseSelectionQuery.m_tree);
                        SDL_CaptureMouse(true);
                        m_mouseHighlightQuery.Reset();
                    }
                    return;

                    case WindowHighlightType::WindowTitleBar:
                    {
                        SetActiveTree(m_mouseSelectionQuery.m_tree);
                        int x, y;
                        SDL_GetWindowPosition(m_mouseSelectionQuery.m_tree->m_window, &x, &y);
                        float mx, my;
                        SDL_GetGlobalMouseState(&mx, &my);
                        m_mouseMode = MouseMode_MovingWindow;
                        m_mouseTree = m_mouseSelectionQuery.m_tree;
                        m_mouseOriginateTree = nullptr;
                        m_mouseGrabPos.x = (int)mx;
                        m_mouseGrabPos.y = (int)my;
                        m_mouseInitial.x = x;
                        m_mouseInitial.y = y;
                        SDL_CaptureMouse(true);
                    }
                    return;

                    case WindowHighlightType::Tab:
                    {
                        SetActiveTree(m_mouseSelectionQuery.m_tree);
                        m_activeLayout = m_mouseSelectionQuery.m_layout;
                        m_menuList.Layout(m_activeTree);

                        WindowMessageStruct msgLLC;
                        msgLLC.m_type = WindowMessage::Query_FindLockedLayout;
                        msgLLC.m_flags = WMF_Layout | WMF_EarlyOut;
                        msgLLC.m_tree = m_mouseSelectionQuery.m_tree;
                        Message(msgLLC);

                        WindowMessageStruct msgWC;
                        msgWC.m_type = WindowMessage::Query_WindowCount;
                        msgWC.m_flags = WMF_Window;
                        msgWC.m_tree = m_mouseSelectionQuery.m_tree;
                        Message(msgWC);

                        if (msgLLC.m_response == 0 && msgWC.m_response <= 1)
                        {
                            int x, y;
                            SDL_GetWindowPosition(m_mouseSelectionQuery.m_tree->m_window, &x, &y);

                            float mx, my;
                            SDL_GetGlobalMouseState(&mx, &my);
                            m_mouseMode = MouseMode_MovingWindow;
                            m_mouseTree = m_mouseSelectionQuery.m_tree;
                            m_mouseOriginateTree = nullptr;
                            m_mouseGrabPos.x = (int)mx;
                            m_mouseGrabPos.y = (int)my;
                            m_mouseInitial.x = x;
                            m_mouseInitial.y = y;
                            SDL_CaptureMouse(true);
                            return;
                        }

                        m_mouseMode = MouseMode_SelectingTab;
                        m_mouseSelectionQuery.m_layout->m_activeTab = m_mouseSelectionQuery.m_id;
                        m_activeWindow = m_activeLayout->GetActiveWindow();

                        if (m_activeWindow)
                        {
                            auto file = m_activeWindow->GetSourceFile();
                            if (file)
                                SourceFileManager::Instance().SetActiveSourceFile(file);
                        }

                        float mx, my;
                        SDL_GetGlobalMouseState(&mx, &my);
                        m_mouseGrabPos.x = (int)mx;
                        m_mouseGrabPos.y = (int)my;
                    }
                    return;

                    case WindowHighlightType::LayoutSplit:
                    {
                        m_mouseMode = MouseMode_MovingSplit;
                        float mx, my;
                        SDL_GetGlobalMouseState(&mx, &my);
                        m_mouseGrabPos.x = (int)mx;
                        m_mouseGrabPos.y = (int)my;
                        SDL_CaptureMouse(true);
                    }
                    return;

                    case WindowHighlightType::ScrollBar:
                    {
                        SetActiveTree(m_mouseSelectionQuery.m_tree);
                        m_activeLayout = m_mouseSelectionQuery.m_layout;
                        m_activeWindow = m_activeLayout->GetActiveWindow();

                        bool vertical = m_mouseSelectionQuery.m_scrollbar.m_vertical;
                        int offset = vertical ? (int)e->button.y : (int)e->button.x;
                        m_mouseSelectionQuery.m_window->UpdateScrollBar(offset, vertical, m_mouseSelectionQuery.m_tree);
                        m_mouseSelectionQuery.m_tree->m_dirty = true;

                        m_menuList.Layout(m_activeTree);
                        SDL_CaptureMouse(true);
                        m_mouseMode = MouseMode_ScrollBar;
                        float mx, my;
                        SDL_GetGlobalMouseState(&mx, &my);
                        m_mouseInitial.x = (int)mx;
                        m_mouseInitial.y = (int)my;
                    }
                    return;

                    case WindowHighlightType::ClientArea:
                    {
                        if (m_mouseSelectionQuery.m_window != m_activeWindow)
                        {
                            SetActiveTree(m_mouseSelectionQuery.m_tree);
                            m_activeLayout = m_mouseSelectionQuery.m_layout;
                            m_activeWindow = m_mouseSelectionQuery.m_window;
                            m_menuList.Layout(m_activeTree);
                            m_mouseSelectionQuery.m_tree->m_dirty;
                        }
                    }
                    break;
                }
            }
        }
        break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            if (e->button.button == 1)
            {
                switch (m_mouseMode)
                {
                    case MouseMode_ScrollBar:
                        SDL_CaptureMouse(false);
                        m_mouseMode = MouseMode_Idle;
                        break;

                    case MouseMode_UsingMenu:
                        SDL_CaptureMouse(false);
                        if (m_mouseSelectionQuery.m_highlight == WindowHighlightType::Menu)
                        {
                            auto menu = m_menuList.m_menus[m_mouseSelectionQuery.m_menu.m_menuIdx];
                            menu->m_open = false;
                            m_activeTree->m_dirty = true;
                            if (m_mouseSelectionQuery.m_menu.m_itemIdx != -1)
                            {
                                auto item = menu->m_items[m_mouseSelectionQuery.m_menu.m_itemIdx];
                                item->m_subMenuOpen = false;
                                if (m_mouseSelectionQuery.m_menu.m_subItemIdx != -1)
                                {
                                    auto subitem = item->m_subMenus[m_mouseSelectionQuery.m_menu.m_subItemIdx];
                                    if (subitem->m_activate)
                                        subitem->m_activate();
                                }
                                else
                                {
                                    if (item->m_activate)
                                        item->m_activate();
                                }
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
                        m_mouseSelectionQuery.m_tree->m_dirty = true;
                        m_mouseSelectionQuery.Reset();
                        break;

                    case MouseMode_MovingWindow:
                        SDL_CaptureMouse(false);
                        m_mouseMode = MouseMode_Idle;
                        m_mouseTree->m_dirty = true;
                        if (m_mouseDockQuery.m_foundDock)
                        {
                            SetActiveTree(nullptr);
                            m_activeLayout = nullptr;
                            m_activeWindow = nullptr;

                            m_mouseDockQuery.m_tree->m_dirty = true;
                            m_mouseDockQuery.m_foundDock = false;

                            // dock all the virtual windows of the old window tree
                            std::vector<WindowBase*> windows;
                            m_mouseTree->GatherWindows(windows);

                            if (m_mouseDockQuery.m_splitPosition == WindowDockQuery::SplitPosition_None)
                            {
                                for (auto vw : windows)
                                    m_mouseDockQuery.m_layout->m_tabs.push_back(vw);
                                m_mouseDockQuery.m_layout->m_activeTab = (int)m_mouseDockQuery.m_layout->m_tabs.size() - 1;

                                SetActiveTree(m_mouseDockQuery.m_tree);
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

                                SetActiveTree(m_mouseDockQuery.m_tree);
                                m_activeLayout = m_mouseDockQuery.m_layout->m_splits[splitIdx];
                                m_activeWindow = m_activeLayout->GetActiveWindow();
                                m_menuList.Layout(m_activeTree);
                            }

                            // delete the old window tree now..
                            auto it = std::find(m_windowTrees.begin(), m_windowTrees.end(), m_mouseTree);
                            m_windowTrees.erase(it);
                            delete m_mouseTree;
                            m_mouseTree = nullptr;
                            WindowManager::Instance().IndexWindows();

                            m_mouseDockQuery.m_tree->LayoutWindows();
                            WindowManager::Instance().SaveWindowLayout();
                        }
                        break;
                }
            }
        }
        break;

        case SDL_EVENT_MOUSE_MOTION:
        {
            switch (m_mouseMode)
            {
                case MouseMode_Idle:
                {
                    WindowHighlightQuery query;
                    WindowMessageStruct msg;
                    msg.m_type = WindowMessage::Query_Highlight;
                    msg.m_flags = WMF_Menu | WMF_AreaCheck | WMF_Layout | WMF_Window | WMF_EarlyOut;
                    msg.m_query = &query;
                    msg.m_tree = FindWindowByID(e->button.windowID);
                    if (msg.m_tree)
                    {
                        msg.m_x = (int)e->button.x;
                        msg.m_y = (int)e->button.y;
                        Message(msg);
                        if (!m_mouseHighlightQuery.IsEqual(query))
                        {
                            m_mouseHighlightQuery = query;
                            msg.m_tree->m_dirty = true;
                        }
                    }
                }
                break;

                case MouseMode_ScrollBar:
                {
                    bool vertical = m_mouseSelectionQuery.m_scrollbar.m_vertical;
                    int offset = vertical ? (int)e->button.y : (int)e->button.x;
                    m_mouseSelectionQuery.m_window->UpdateScrollBar(offset, vertical, m_mouseSelectionQuery.m_tree);
                    m_mouseSelectionQuery.m_tree->m_dirty = true;
                }
                break;

                case MouseMode_UsingMenu:
                {
                    int oldMenuIdx = m_mouseSelectionQuery.m_menu.m_menuIdx;
                    int oldMenuItemIdx = m_mouseSelectionQuery.m_menu.m_itemIdx;
                    auto oldTree = m_mouseSelectionQuery.m_tree;

                    m_activeTree->m_dirty = true;
                    m_mouseSelectionQuery.Reset();
                    WindowMessageStruct msg;
                    msg.m_type = WindowMessage::Query_Highlight;
                    msg.m_flags = WMF_Menu | WMF_AreaCheck | WMF_EarlyOut;
                    msg.m_query = &m_mouseSelectionQuery;
                    msg.m_tree = oldTree;
                    msg.m_x = (int)e->button.x;
                    msg.m_y = (int)e->button.y;
                    Message(msg);

                    // close the old menus
                    if (oldMenuIdx != -1)
                    {
                        m_menuList.m_menus[oldMenuIdx]->m_open = false;
                        if (oldMenuItemIdx != -1)
                        {
                            m_menuList.m_menus[oldMenuIdx]->m_items[oldMenuItemIdx]->m_subMenuOpen = false;
                        }
                    }

                    // open the new menus
                    if (m_mouseSelectionQuery.m_highlight == WindowHighlightType::Menu)
                    {
                        m_menuList.m_menus[m_mouseSelectionQuery.m_menu.m_menuIdx]->m_open = true;
                        if (m_mouseSelectionQuery.m_menu.m_itemIdx != -1)
                        {
                            m_menuList.m_menus[m_mouseSelectionQuery.m_menu.m_menuIdx]->m_items[m_mouseSelectionQuery.m_menu.m_itemIdx]->m_subMenuOpen = true;
                        }
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
                        WindowMessageStruct msg;
                        msg.m_tree = FindWindowByID(e->button.windowID);
                        msg.m_flags = WMF_Layout | WMF_EarlyOut;
                        msg.m_type = WindowMessage::Query_FindLockedLayout;
                        Message(msg);

                        WindowMessageStruct msgCW;
                        msgCW.m_tree = msg.m_tree;
                        msgCW.m_flags = WMF_Window;
                        msgCW.m_type = WindowMessage::Query_WindowCount;
                        Message(msgCW);
                        if (msg.m_response > 0 || msgCW.m_response > 1)
                        {
                            m_activeLayout = nullptr;
                            m_activeWindow = nullptr;

                            // drag out virtual window into a new window tree
                            // go to MovingWindow mode
                            auto vw = m_mouseSelectionQuery.m_layout->m_tabs[m_mouseSelectionQuery.m_id];
                            m_mouseSelectionQuery.m_layout->m_tabs.erase(m_mouseSelectionQuery.m_layout->m_tabs.begin() + m_mouseSelectionQuery.m_id);
                            m_mouseSelectionQuery.m_layout->m_activeTab = 0;
                            if (m_mouseSelectionQuery.m_layout->m_tabs.empty())
                            {
                                // collapse empty tab
                                m_mouseSelectionQuery.m_tree->CollapseEmptyLayouts();
                            }
                            m_mouseSelectionQuery.m_tree->LayoutWindows();

                            float mx, my;
                            SDL_GetGlobalMouseState(&mx, &my);
                            m_mouseMode = MouseMode_MovingWindow;
                            m_mouseOriginateTree = m_mouseSelectionQuery.m_tree;
                            m_mouseGrabPos.x = (int)mx + vw->m_tabArea.w / 2;
                            m_mouseGrabPos.y = (int)my + WINDOW_TITLE_BAR_HEIGHT + WINDOW_TAB_BAR_HEIGHT / 2;
                            m_mouseInitial.x = (int)mx;
                            m_mouseInitial.y = (int)my;
                            m_mouseDockQuery.m_foundDock = false;

                            auto tree = new WindowTree({ (int)mx - m_mouseGrabPos.x + m_mouseInitial.x, (int)my - m_mouseGrabPos.y + m_mouseInitial.y,640,512 });
                            tree->m_layout.m_splitType = WindowLayout::NoSplit;
                            tree->m_layout.m_tabs.push_back(vw);
                            tree->m_dirty = true;
                            SetActiveTree(tree);
                            m_activeLayout = &tree->m_layout;
                            m_activeWindow = m_activeLayout->GetActiveWindow();
                            m_windowTrees.push_back(tree);
                            WindowManager::Instance().IndexWindows();
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
                }
                break;

                case MouseMode_MovingSplit:
                {
                    float mx, my;
                    SDL_GetGlobalMouseState(&mx, &my);
                    switch (m_mouseSelectionQuery.m_layout->m_splitType)
                    {
                        case WindowLayout::Vertical:
                        {
                            int offset = (int)my - m_mouseGrabPos.y;
                            int splitPos = m_mouseSelectionQuery.m_split.m_splitPos + offset;
                            m_mouseSelectionQuery.m_layout->m_splitPercentage = Clamp((float)(splitPos - m_mouseSelectionQuery.m_layout->m_area.y) / (float)m_mouseSelectionQuery.m_layout->m_area.h, 0.0f, 1.0f);
                        }
                        break;

                        case WindowLayout::Horizontal:
                        {
                            int offset = (int)mx - m_mouseGrabPos.x;
                            int splitPos = m_mouseSelectionQuery.m_split.m_splitPos + offset;
                            m_mouseSelectionQuery.m_layout->m_splitPercentage = Clamp((float)(splitPos - m_mouseSelectionQuery.m_layout->m_area.x) / (float)m_mouseSelectionQuery.m_layout->m_area.w, 0.0f, 1.0f);
                        }
                        break;
                    }
                    m_mouseSelectionQuery.m_tree->LayoutWindows();
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
            }
        }
        break;

        case SDL_EVENT_KEY_DOWN:
        {
            const SDL_KeyboardEvent& key = e->key;
            if (!key.repeat && key.key == SDLK_RETURN && (key.mod & SDL_KMOD_ALT))
            {
                if (m_activeTree)
                {
                    Log("ALT_RETURN\n");
                    return;
                }
            }
            break;
        }

        case SDL_EVENT_DROP_FILE:
        {
            // This pointer is only valid while processing this event,
            // so copy it if you need to retain the path.
            std::vector<std::string> fileList;
            fileList.push_back(e->drop.data);



            SourceFileManager::Instance().RequestLoadFiles(fileList);
        }
        return;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        {
            WindowTree* tree = FindWindowByID(e->window.windowID);
            if (tree)
            {
                tree->m_dirty = true;
            }
            return;
        }
        break;
    }

    if (m_mouseMode == MouseMode_Idle && m_activeWindow)
    {
        if (m_activeWindow->HandleEvent(e))
            m_activeTree->m_dirty = true;
    }
}

void WindowManager::SetActiveTree(WindowTree* tree)
{
    if (m_activeTree == tree)
        return;

    if (m_activeTree)
    {
        SDL_StopTextInput(m_activeTree->m_window);
    }

    m_activeTree = tree;
    if (tree)
    {
        m_activeLayout = tree->FindFirstNonSplitLayout();
        m_activeWindow = m_activeLayout->GetActiveWindow();

        if (m_activeWindow)
        {
            auto sourceFile = m_activeWindow->GetSourceFile();
            if (sourceFile)
                SourceFileManager::Instance().SetActiveSourceFile(sourceFile);
        }

        m_menuList.Layout(m_activeTree);
    }
    else
    {
        m_activeLayout = nullptr;
        m_activeWindow = nullptr;
    }

    if (m_activeTree)
    {
        SDL_StartTextInput(m_activeTree->m_window);
    }
}


void WindowManager::Paint()
{
    // todo: dirty system is ANYTHING-DIRTY - just redraw all.
    // might as well just do WindowManager dirty flag...
    // or do a dirty system per renderer & per file, with a paint ID if we really need a smart dirty system
    bool anyDirty = false;
    for (auto tree : m_windowTrees)
        anyDirty |= tree->m_dirty;

    if (anyDirty)
    {
        for (auto tree : m_windowTrees)
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

            if (m_activeWindow == window)
            {
                m_activeWindow = layout->GetActiveWindow();
            }
            return;
        }
    }
}

void WindowManager::SetDefaultActiveWindow()
{
    if (m_windowTrees.empty())
    {
        m_activeTree = nullptr;
        m_activeLayout = nullptr;
        m_activeWindow = nullptr;
        return;
    }

    if (!m_activeTree)
    {
        m_activeTree = m_windowTrees[0];
        m_activeLayout = nullptr;
        m_activeWindow = nullptr;
    }

    if (m_activeTree && !m_activeLayout)
    {
        m_activeLayout = m_activeTree->FindFirstNonSplitLayout();
        m_activeWindow = nullptr;
    }

    if (m_activeLayout)
    {
        m_activeWindow = m_activeLayout->GetActiveWindow();
    }
}

void WindowManager::AddWindow(WindowBase* window)
{
    SetDefaultActiveWindow();

    Assert(m_activeTree, "Attempted to add a window without an active window tree\n");
    Assert(m_activeLayout, "Attempted to add a window without an active window layout\n");

    m_activeLayout->AddWindow(window);
    m_activeWindow = m_activeLayout->GetActiveWindow();
}

#define WINDOW_LAYOUT_VERSION "v001"

void WindowManager::SaveWindowLayout()
{
    std::vector<std::string> layoutTokens;
    layoutTokens.push_back(WINDOW_LAYOUT_VERSION);

    for (auto tree : m_windowTrees)
    {
        int x, y, w, h;
        bool isFullscreen = tree->m_fullscreen;
        if (isFullscreen)
        {
            x = tree->m_windowedRect.x;
            y = tree->m_windowedRect.y;
            w = tree->m_windowedRect.w;
            h = tree->m_windowedRect.h;
        }
        else
        {
            SDL_GetWindowPosition(tree->m_window, &x, &y);
            SDL_GetWindowSize(tree->m_window, &w, &h);
        }

        layoutTokens.push_back("T");
        layoutTokens.push_back(std::format("{}", (isFullscreen ? 1 : 0)));
        layoutTokens.push_back(std::format("{}", x));
        layoutTokens.push_back(std::format("{}", y));
        layoutTokens.push_back(std::format("{}", w));
        layoutTokens.push_back(std::format("{}", h));

        tree->m_layout.SaveLayout(layoutTokens);
    }
    Log(LogGroup::System, "Save Window Layout : {} windows, {} tokens", m_windowTrees.size(), layoutTokens.size());

    Settings::Instance().SetStringList(SETTING_WINDOWS, layoutTokens);
}

void WindowManager::LoadWindowLayout()
{
    std::vector<std::string> layoutTokens = Settings::Instance().GetStringList(SETTING_WINDOWS);
    if (layoutTokens.empty() || layoutTokens[0] != WINDOW_LAYOUT_VERSION)
        return;

    // close all the windows because we'll want to recreate new ones
    for (auto tree : m_windowTrees)
        delete tree;
    m_windowTrees.clear();

    // create new layout from tokens   
    size_t idx = 1;
    while (idx < layoutTokens.size())
    {
        Assert(layoutTokens[idx++] == "T", "Expected TREE token");
        int isFullscreen = std::stoi(layoutTokens[idx++]);
        int x = std::stoi(layoutTokens[idx++]);
        int y = std::stoi(layoutTokens[idx++]);
        int w = std::stoi(layoutTokens[idx++]);
        int h = std::stoi(layoutTokens[idx++]);

        auto tree = new WindowTree(Recti{ x,y,w,h });
        tree->m_fullscreen = isFullscreen;
        m_windowTrees.push_back(tree);
        WindowManager::Instance().IndexWindows();

        tree->m_layout.LoadLayout(layoutTokens, idx);

        SetActiveTree(tree);
        tree->LayoutWindows();
    }
}

void WindowManager::IndexWindows()
{
    for (int i = 0; i < m_windowTrees.size(); i++)
        m_windowTrees[i]->m_windowIdx = i;
}


void WindowManager::Message(WindowMessageStruct& msg)
{
    if (msg.m_flags & WMF_Menu)
    {
        if (msg.m_tree && msg.m_tree == m_activeTree)
        {
            m_menuList.Message(msg);
            if ((msg.m_response > 0) && (msg.m_flags & WMF_EarlyOut))
                return;
        }
    }

    if (msg.m_tree)
    {
        msg.m_tree->Message(msg);
    }
    else
    {
        for (auto tree : m_windowTrees)
        {
            msg.m_tree = tree;
            tree->Message(msg);
            if ((msg.m_response > 0) && (msg.m_flags & WMF_EarlyOut))
                return;
        }
    }
}






