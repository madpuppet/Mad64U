#include "common.h"
#include "WindowTree.h"
#include "Settings.h"
#include <format>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include "Application.h"

void EnableDropShadow(SDL_Window* window)
{
    SDL_PropertiesID props = SDL_GetWindowProperties(window);

    HWND hwnd = (HWND)SDL_GetPointerProperty(
        props,
        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        nullptr);

    if (!hwnd)
        return;

    // Let DWM render non-client effects such as shadow.
    DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
    DwmSetWindowAttribute(
        hwnd,
        DWMWA_NCRENDERING_POLICY,
        &policy,
        sizeof(policy));

    // Windows 11: ask for normal rounded corners too.
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(
        hwnd,
        DWMWA_WINDOW_CORNER_PREFERENCE,
        &corner,
        sizeof(corner));

    // Often helps DWM treat the window as having a tiny frame.
    MARGINS margins = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}


void WindowTree::LayoutWindows()
{
    int w, h;
    SDL_GetWindowSize(m_window, &w, &h);
    Recti area{ 0,WINDOW_TITLE_BAR_HEIGHT,w,h - WINDOW_TITLE_BAR_HEIGHT };
    m_layout.Layout(m_renderer, area);
    m_dirty = true;
}

void WindowTree::AddWindow(int x, int y, WindowBase* window)
{
    m_layout.AddWindow(x, y, window);
    LayoutWindows();
}


void WindowTree::RemoveWindow(WindowBase* window)
{
    m_layout.RemoveWindow(window);
    LayoutWindows();
}

bool WindowTree::CheckForDocking(int x, int y, WindowDockQuery &query)
{
    int wx, wy;
    SDL_GetWindowPosition(m_window, &wx, &wy);

    query.m_tree = this;
    query.m_foundDock = m_layout.CheckForDocking(x-wx, y-wy, query);
    return query.m_foundDock;
}

WindowTree::WindowTree(const Recti& area)
{
    static int s_unique = 0;

    std::string name = std::format("MAD64U {} #{}", VERSION, s_unique++);
    m_window = SDL_CreateWindow(name.c_str(), area.w, area.h, SDL_WINDOW_BORDERLESS);
    if (m_window == NULL)
    {
        Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(0);
    }
    SDL_SetWindowPosition(m_window, area.x, area.y);
    m_windowID = SDL_GetWindowID(m_window);

    m_renderer = SDL_CreateRenderer(m_window, Settings::Instance().GetString(SETTING_RENDERER).c_str());
    if (m_renderer == NULL)
    {
        Log("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(0);
    }

    Log("Window %p,  Create Renderer: %p\n", m_window, m_renderer);

    EnableDropShadow(m_window);
}

WindowTree::~WindowTree()
{
    FontRenderer::Instance().FlushRenderer(m_renderer);
    IconRenderer::Instance().FlushRenderer(m_renderer);

    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
}

void WindowTree::Paint(Recti* area)
{
    auto& wm = WindowManager::Instance();
    auto& ir = IconRenderer::Instance();
    auto& tp = Application::Instance().GetThemeProperties();

    tp.SetRenderDrawColor(m_renderer, ThemeColor::WindowBackground);
    SDL_RenderClear(m_renderer);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    Recti windowArea;
    windowArea.x = 0;
    windowArea.y = 0;
    SDL_GetWindowSizeInPixels(m_window, &windowArea.w, &windowArea.h);

    Recti refreshArea;
    if (area)
    {
        int wx, wy;
        SDL_GetWindowPosition(m_window, &wx, &wy);
        refreshArea.x = area->x - wx;
        refreshArea.y = area->y - wy;
        refreshArea.w = area->w;
        refreshArea.h = area->h;
    }
    else
    {
        refreshArea = windowArea;
    }
    m_layout.Paint(m_renderer, refreshArea);

    // draw title bar and resize tab
    SDL_FRect sdlBarRect{ 0, 0, (float)windowArea.w, (float)WINDOW_TITLE_BAR_HEIGHT };
    tp.SetRenderDrawColor(m_renderer, ThemeColor::TitleBar);
    SDL_RenderFillRect(m_renderer, &sdlBarRect);

    // render dock guide
    auto &dockQuery = wm.GetWindowDockQuery();
    if (dockQuery.m_foundDock && dockQuery.m_tree == this)
    {
        SDL_FRect bodyArea{ (float)dockQuery.m_bodyArea.x,(float)dockQuery.m_bodyArea.y,(float)dockQuery.m_bodyArea.w,(float)dockQuery.m_bodyArea.h };
        SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 128);
        SDL_RenderFillRect(m_renderer, &bodyArea);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 128);
        bodyArea.x += 3.0f;
        bodyArea.y += 3.0f;
        bodyArea.w -= 6.0f;
        bodyArea.h -= 6.0f;
        SDL_RenderFillRect(m_renderer, &bodyArea);
    }

    ir.DrawIcon(m_renderer, Icon_Close, windowArea.w - 12, WINDOW_TITLE_BAR_HEIGHT/2);
    if (m_fullscreen)
        ir.DrawIcon(m_renderer, Icon_Windowed, windowArea.w - 30, WINDOW_TITLE_BAR_HEIGHT / 2);
    else
    {
        ir.DrawIcon(m_renderer, Icon_Fullscreen, windowArea.w - 30, WINDOW_TITLE_BAR_HEIGHT / 2);
        ir.DrawIcon(m_renderer, Icon_Resize, windowArea.w - 8, windowArea.h - 8);
    }

    if (wm.GetActiveWindowTree() == this)
    {
        wm.PaintMenu(wm.GetWindowMenuQuery());
    }

    SDL_RenderPresent(m_renderer);
}

WindowLayout* WindowTree::FindFirstNonSplitLayout()
{
    return m_layout.FindFirstNonSplitLayout();
}

void WindowTree::GatherWindows(std::vector<WindowBase*>& windows)
{
    m_layout.GatherWindows(windows);
}

bool WindowTree::CheckForSplit(int x, int y, WindowSplitQuery& query)
{
    query.m_tree = this;
    return m_layout.CheckForSplit(x, y, query);
}

bool WindowTree::CheckForTab(int x, int y, WindowTabQuery& query)
{
    query.m_tree = this;
    return m_layout.CheckForTab(x, y, query);
}

int WindowTree::CountWindows()
{
    return m_layout.CountWindows();
}

void WindowTree::CollapseEmptyLayouts()
{
    m_layout.CollapseEmptyLayouts();
    LayoutWindows();
}

Icons WindowTree::FindIcon(int x, int y)
{
    int w, h;
    SDL_GetWindowSizeInPixels(m_window, &w, &h);
    Recti closeIcon{ w - 12 - 8, WINDOW_TITLE_BAR_HEIGHT / 2 - 8, 16, 16 };
    Recti windowIcon{ w - 30 - 8, WINDOW_TITLE_BAR_HEIGHT / 2 - 8, 16, 16 };
    Recti resizeIcon{ w - 16, h - 16, 16, 16 };
    if (closeIcon.Contains(x, y))
        return Icon_Close;
    if (m_fullscreen)
    {
        if (windowIcon.Contains(x, y))
            return Icon_Windowed;
    }
    else
    {
        if (windowIcon.Contains(x, y))
            return Icon_Fullscreen;
        if (resizeIcon.Contains(x, y))
            return Icon_Resize;
    }
    return Icon_None;
}

SDL_DisplayID GetMouseDisplay()
{
    SDL_FPoint pt;
    SDL_GetGlobalMouseState(&pt.x, &pt.y);
    SDL_Point point{ (int)pt.x,(int)pt.y };
    return SDL_GetDisplayForPoint(&point);
}

void WindowTree::MakeFullscreen()
{
    if (!m_fullscreen)
    {
        SDL_GetWindowPosition(m_window, &m_windowedRect.x, &m_windowedRect.y);
        SDL_GetWindowSize(m_window, &m_windowedRect.w, &m_windowedRect.h);

        SDL_Rect rect;
        SDL_GetDisplayBounds(GetMouseDisplay(), &rect);
        SDL_SetWindowPosition(m_window, rect.x, rect.y);
        SDL_SetWindowSize(m_window, rect.w, rect.h);
        SDL_SyncWindow(m_window);
        m_fullscreen = true;
        LayoutWindows();
    }
}

void WindowTree::MakeWindowed()
{
    SDL_SetWindowPosition(m_window, m_windowedRect.x, m_windowedRect.y);
    SDL_SetWindowSize(m_window, m_windowedRect.w, m_windowedRect.h);
    SDL_SyncWindow(m_window);
    m_fullscreen = false;
    LayoutWindows();
}

bool WindowTree::CheckForLayout(int x, int y, WindowLayout*& layout)
{
    return m_layout.CheckForLayout(x, y, layout);
}

WindowLayout* WindowTree::FindLayoutFromWindow(WindowBase* window, int& tabIdx)
{
    return m_layout.FindLayoutFromWindow(window, tabIdx);
}

bool WindowTree::CheckForScrollBar(int x, int y, WindowScrollBarQuery& query)
{
    query.m_tree = this;
    return m_layout.CheckForScrollBar(x, y, query);
}

void WindowTree::Tick()
{
    m_dirty |= m_layout.Tick();
}


