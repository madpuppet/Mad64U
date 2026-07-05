#include "common.h"
#include "WindowTree.h"
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


void WindowLayout::Layout(SDL_Renderer *renderer, const Recti& area)
{
    m_area = area;
    switch (m_splitType)
    {
        case NoSplit:
        {
            auto& fr = Application::Instance().GetFontRenderer();
            SDL_Color col{ 255,255,255,255 };
            Recti clientArea{ area.x + WINDOW_CLIENT_BORDER, area.y + WINDOW_TAB_BAR_HEIGHT + WINDOW_CLIENT_BORDER, area.w - WINDOW_CLIENT_BORDER * 2, area.h - WINDOW_CLIENT_BORDER * 2 - WINDOW_TAB_BAR_HEIGHT };
            Recti tabArea{ area.x, area.y, area.w, WINDOW_TAB_BAR_HEIGHT };
            int tabX = area.x;
            int tabY = area.y;
            for (auto t : m_tabs)
            {
                t->m_area = clientArea;
                SDL_Rect tabTextArea;
                fr.RenderText(renderer, t->m_name, col, tabX, tabY, CachedFontRenderer::UIFont, &tabTextArea, true);
                t->m_tabArea = { tabX, tabY, tabTextArea.w + TEXT_HBORDER * 2, tabTextArea.h };
                tabX = tabTextArea.x + tabTextArea.w + TEXT_HBORDER*3;
            }
        }
        break;

        case Vertical:
        {
            int split = (int)((float)area.h * m_splitPercentage);
            m_splits[0]->Layout(renderer, Recti{ area.x,area.y,area.w,split});
            m_splits[1]->Layout(renderer, Recti{ area.x,area.y + split,area.w,area.h - split });
        }
        break;

        case Horizontal:
        {
            int split = (int)((float)area.w * m_splitPercentage);
            m_splits[0]->Layout(renderer, Recti{ area.x,area.y,split,area.h });
            m_splits[1]->Layout(renderer, Recti{ area.x + split,area.y,area.w-split,area.h});
        }
        break;
    }
}

void WindowLayout::AddWindow(int x, int y, VirtualWindow* window)
{
    if (m_splitType == NoSplit)
    {
        // calculate split type (noSplit, horizontal, vertical)
        float xPercent = (float)(x - m_area.x) / (float)m_area.w;
        float yPercent = (float)(y - m_area.y) / (float)m_area.h;

        SplitType newSplit = NoSplit;
        int splitIndex = -1;
        if (xPercent < 0.25f)
        {
            newSplit = Horizontal;
            splitIndex = 0;
        }
        else if (xPercent > 0.75f)
        {
            newSplit = Horizontal;
            splitIndex = 1;
        }
        else if (yPercent < 0.25f)
        {
            newSplit = Vertical;
            splitIndex = 0;
        }
        else if (yPercent > 0.75f)
        {
            newSplit = Vertical;
            splitIndex = 1;
        }

        if (newSplit != NoSplit && !m_tabs.empty())
        {
            m_splits[0] = new WindowLayout;
            m_splits[1] = new WindowLayout;
            m_splitType = newSplit;
            m_splits[splitIndex]->m_tabs.push_back(window);
            m_splits[1 - splitIndex]->m_tabs = std::move(m_tabs);
            m_splitPercentage = 0.5f;
        }
        else
        {
            // add to body
            m_tabs.push_back(window);
        }
    }
    else
    {
        if (m_splits[0]->m_area.Contains(x, y))
            m_splits[0]->AddWindow(x,y,window);
        else
            m_splits[1]->AddWindow(x,y,window);
    }
}

void WindowLayout::RemoveWindow(VirtualWindow* window)
{
    if (m_splitType == NoSplit)
    {
        auto It = std::find(m_tabs.begin(), m_tabs.end(), window);
        if (It != m_tabs.end())
        {
            m_tabs.erase(It);
        }
    }
    else
    {
        m_splits[0]->RemoveWindow(window);
        m_splits[1]->RemoveWindow(window);
    }
}

bool WindowLayout::CheckForDocking(int x, int y, WindowDockQuery &query)
{
    if (!m_area.Contains(x, y))
        return false;

    if (m_splitType == NoSplit)
    {
        float xPercent = (float)(x - m_area.x) / (float)m_area.w;
        float yPercent = (float)(y - m_area.y) / (float)m_area.h;
        if (xPercent < 0.25f)
        {
            int hw = m_area.w / 2;
            query.m_layout = this;
            query.m_splitPosition = WindowDockQuery::SplitPosition_Left;
            query.m_bodyArea = Recti{ m_area.x, m_area.y, hw, m_area.h };
            query.m_splitArea = Recti{ m_area.x + hw, m_area.y, m_area.w - hw, m_area.h };
            return true;
        }
        else if (xPercent > 0.75f)
        {
            int hw = m_area.w / 2;
            query.m_layout = this;
            query.m_splitPosition = WindowDockQuery::SplitPosition_Right;
            query.m_splitArea = Recti{ m_area.x, m_area.y, hw, m_area.h };
            query.m_bodyArea = Recti{ m_area.x + hw, m_area.y, m_area.w - hw, m_area.h };
            return true;
        }
        else if (yPercent < 0.25f)
        {
            int hh = m_area.h / 2;
            query.m_layout = this;
            query.m_splitPosition = WindowDockQuery::SplitPosition_Top;
            query.m_bodyArea = Recti{ m_area.x, m_area.y, m_area.w, hh };
            query.m_splitArea = Recti{ m_area.x, m_area.y + hh, m_area.w, m_area.h - hh };
            return true;
        }
        else if (yPercent > 0.75f)
        {
            int hh = m_area.h / 2;
            query.m_layout = this;
            query.m_splitPosition = WindowDockQuery::SplitPosition_Bottom;
            query.m_splitArea = Recti{ m_area.x, m_area.y, m_area.w, hh };
            query.m_bodyArea = Recti{ m_area.x, m_area.y + hh, m_area.w, m_area.h - hh };
            return true;
        }
        else
        {
            query.m_layout = this;
            query.m_splitPosition = WindowDockQuery::SplitPosition_None;
            query.m_bodyArea = m_area;
            return true;
        }
    }
    else
    {
        if (m_splits[0]->CheckForDocking(x, y, query))
            return true;
        return m_splits[1]->CheckForDocking(x, y, query);
    }
}

void WindowLayout::Paint(SDL_Renderer *renderer, const Recti& area)
{
    if (m_area.Overlaps(area))
    {
        if (m_splitType == NoSplit)
        {
            // render tab bar
            SDL_FRect sdlBarRect{ (float)m_area.x, (float)m_area.y, (float)m_area.w, (float)WINDOW_TAB_BAR_HEIGHT };
            SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
            SDL_RenderFillRect(renderer, &sdlBarRect);

            Recti bodyRect{ m_area.x, m_area.y + WINDOW_TAB_BAR_HEIGHT, m_area.w, m_area.h - WINDOW_TAB_BAR_HEIGHT };
            if (!m_tabs.empty())
            {
                // render tabs
                auto& fr = Application::Instance().GetFontRenderer();
                for (auto w : m_tabs)
                {
                    SDL_FRect sdlTabInnerRect{ (float)w->m_tabArea.x, (float)(w->m_tabArea.y+2), (float)w->m_tabArea.w, (float)WINDOW_TAB_BAR_HEIGHT-2 };
                    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                    SDL_RenderFillRect(renderer, &sdlTabInnerRect);

                    SDL_Color col{ 200,200,200,255 };
                    if (m_activeTab != -1 && m_tabs[m_activeTab] == w)
                        col = SDL_Color{ 255,255,255,255 };

                    fr.RenderText(renderer, w->m_name, col, w->m_tabArea.x + TEXT_HBORDER, w->m_tabArea.y+4, CachedFontRenderer::UIFont, nullptr, false);

                    if (m_activeTab != -1 && m_tabs[m_activeTab] == w)
                    {
                        SDL_SetRenderDrawColor(renderer, 255, 128, 255, 255);
                        SDL_RenderLine(renderer, (float)w->m_tabArea.x, (float)w->m_tabArea.y+1, (float)w->m_tabArea.x + (float)w->m_tabArea.w, (float)w->m_tabArea.y+1);
                    }
                }

                // render active tab body
                if (m_activeTab != -1)
                    m_tabs[m_activeTab]->m_paintCallback(renderer, area);
            }
            else
            {
                // just clear the body
                SDL_FRect sdlBodyRect{ (float)bodyRect.x, (float)bodyRect.y, (float)bodyRect.w, (float)bodyRect.h };
                SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
                SDL_RenderFillRect(renderer, &sdlBodyRect);
            }
        }
        else
        {
            m_splits[0]->Paint(renderer, area);
            m_splits[1]->Paint(renderer, area);
        }
    }
}

bool WindowLayout::CheckForTab(int x, int y, WindowTabQuery& query)
{
    if (!m_area.Contains(x, y))
        return false;

    if (m_splitType == NoSplit)
    {
        for (int i=0; i<m_tabs.size(); i++)
        {
            if (m_tabs[i]->m_tabArea.Contains(x, y))
            {
                query.m_layout = this;
                query.m_tabIndex = i;
                return true;
            }
        }
        return false;
    }
    else
    {
        if (m_splits[0]->CheckForTab(x, y, query))
            return true;
        return m_splits[1]->CheckForTab(x, y, query);
    }
}

void WindowLayout::GatherVirtualWindows(std::vector<VirtualWindow*>& virtualWindows)
{
    if (m_splitType == NoSplit)
    {
        for (auto vw : m_tabs)
            virtualWindows.push_back(vw);
        m_tabs.clear();
    }
    else
    {
        m_splits[0]->GatherVirtualWindows(virtualWindows);
        m_splits[1]->GatherVirtualWindows(virtualWindows);
    }
}

WindowLayout::~WindowLayout()
{
    for (auto t : m_tabs)
        delete t;
    delete m_splits[0];
    delete m_splits[1];
}

int WindowLayout::CountVirtualWindows()
{
    if (m_splitType == NoSplit)
    {
        return (int)m_tabs.size();
    }
    else
    {
        return m_splits[0]->CountVirtualWindows() + m_splits[1]->CountVirtualWindows();
    }
}

void WindowLayout::CollapseEmptyLayouts()
{
    if (m_splitType != NoSplit)
    {
        m_splits[0]->CollapseEmptyLayouts();
        m_splits[1]->CollapseEmptyLayouts();

        bool empty0 = (m_splits[0] == nullptr) || (m_splits[0]->m_splitType == NoSplit && m_splits[0]->m_tabs.empty());
        bool empty1 = (m_splits[1] == nullptr) || (m_splits[1]->m_splitType == NoSplit && m_splits[1]->m_tabs.empty());

        if (empty0 && empty1)
        {
            m_splitType = NoSplit;
        }
        else if (empty0)
        {
            delete m_splits[0];
            m_splitType = m_splits[1]->m_splitType;
            m_tabs = std::move(m_splits[1]->m_tabs);
            auto new0 = m_splits[1]->m_splits[0];
            auto new1 = m_splits[1]->m_splits[1];
            delete m_splits[1];
            m_splits[0] = new0;
            m_splits[1] = new1;
        }
        else if (empty1)
        {
            delete m_splits[1];
            m_splitType = m_splits[0]->m_splitType;
            m_tabs = std::move(m_splits[0]->m_tabs);
            auto new0 = m_splits[0]->m_splits[0];
            auto new1 = m_splits[0]->m_splits[1];
            delete m_splits[0];
            m_splits[0] = new0;
            m_splits[1] = new1;
        }
    }
}

void WindowTree::LayoutWindows()
{
    int w, h;
    SDL_GetWindowSize(m_window, &w, &h);
    Recti area{ 0,WINDOW_TITLE_BAR_HEIGHT,w,h - WINDOW_TITLE_BAR_HEIGHT };
    m_layout.Layout(m_renderer, area);
}

void WindowTree::AddWindow(int x, int y, VirtualWindow* window)
{
    m_layout.AddWindow(x, y, window);
    LayoutWindows();
}


void WindowTree::RemoveWindow(VirtualWindow* window)
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

    m_renderer = SDL_CreateRenderer(m_window, "gpu");
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
    Application::Instance().GetFontRenderer().FlushRenderer(m_renderer);
    Application::Instance().GetIconRenderer().FlushRenderer(m_renderer);

    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
}

void WindowTree::Paint(Recti* area)
{
    SDL_SetRenderDrawColor(m_renderer, 64, 64, 64, 255);
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
    SDL_SetRenderDrawColor(m_renderer, 64, 32, 16, 255);
    SDL_RenderFillRect(m_renderer, &sdlBarRect);
    if (!m_fullscreen)
    {
        int x0 = windowArea.w;
        int x1 = windowArea.w - 5;
        int x2 = windowArea.w - 10;
        int x3 = windowArea.w - 15;
        int y0 = windowArea.h;
        int y1 = windowArea.h - 5;
        int y2 = windowArea.h - 10;
        int y3 = windowArea.h - 15;
        SDL_SetRenderDrawColor(m_renderer, 128, 128, 128, 255);
        SDL_RenderLine(m_renderer, (float)x3, (float)y0, (float)x0, (float)y3);
        SDL_RenderLine(m_renderer, (float)x2, (float)y0, (float)x0, (float)y2);
        SDL_RenderLine(m_renderer, (float)x1, (float)y0, (float)x0, (float)y1);
    }

    // render dock guide
    auto &dockQuery = Application::Instance().GetWindowDockQuery();
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

    Application::Instance().GetIconRenderer().DrawIcon(m_renderer, Icon_Close, windowArea.w - 12, WINDOW_TITLE_BAR_HEIGHT/2);
    if (m_fullscreen)
        Application::Instance().GetIconRenderer().DrawIcon(m_renderer, Icon_Windowed, windowArea.w - 30, WINDOW_TITLE_BAR_HEIGHT / 2);
    else
    {
        Application::Instance().GetIconRenderer().DrawIcon(m_renderer, Icon_Fullscreen, windowArea.w - 30, WINDOW_TITLE_BAR_HEIGHT / 2);
        Application::Instance().GetIconRenderer().DrawIcon(m_renderer, Icon_Resize, windowArea.w - 8, windowArea.h - 8);
    }

    SDL_RenderPresent(m_renderer);
}

void WindowTree::GatherVirtualWindows(std::vector<VirtualWindow*>& virtualWindows)
{
    m_layout.GatherVirtualWindows(virtualWindows);
}

bool WindowTree::CheckForTab(int x, int y, WindowTabQuery& query)
{
    query.m_tree = this;
    return m_layout.CheckForTab(x, y, query);
}

int WindowTree::CountVirtualWindows()
{
    return m_layout.CountVirtualWindows();
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
