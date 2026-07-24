#include "common.h"
#include "WindowLayout.h"
#include "Application.h"
#include "LogManager.h"
#include "SourceFileWindow.h"
#include "OutputWindow.h"
#include "UndoBufferWindow.h"
#include "ProjectListWindow.h"
#include <format>

void WindowLayout::Layout(SDL_Renderer* renderer, const Recti& area)
{
    m_area = area;
    switch (m_splitType)
    {
        case NoSplit:
        {
            auto& fr = FontRenderer::Instance();
            SDL_Color col{ 255,255,255,255 };
            Recti clientArea{ area.x + WINDOW_CLIENT_BORDER, area.y + WINDOW_TAB_BAR_HEIGHT + WINDOW_CLIENT_BORDER, area.w - WINDOW_CLIENT_BORDER * 2, area.h - WINDOW_CLIENT_BORDER * 2 - WINDOW_TAB_BAR_HEIGHT };
            Recti tabArea{ area.x, area.y + WINDOW_CLIENT_BORDER, area.w, WINDOW_TAB_BAR_HEIGHT - WINDOW_CLIENT_BORDER };
            int tabX = area.x + WINDOW_CLIENT_BORDER + 20;
            int tabY = area.y + WINDOW_CLIENT_BORDER;

            for (auto t : m_tabs)
            {
                t->m_area = clientArea;
                Recti tabTextArea;
                fr.CalcTextArea(renderer, t->m_name, { tabX, tabY }, FontType::UI, tabTextArea);
                t->m_tabArea = { tabX, tabY, tabTextArea.w + TEXT_HBORDER * 2, tabTextArea.h };
                tabX = tabTextArea.x + tabTextArea.w + TEXT_HBORDER * 3;
                t->LayoutScrollbars();
            }
        }
        break;

        case Vertical:
        {
            int split = (int)((float)area.h * m_splitPercentage);
            m_splits[0]->Layout(renderer, Recti{ area.x,area.y,area.w,split });
            m_splits[1]->Layout(renderer, Recti{ area.x,area.y + split,area.w,area.h - split });
        }
        break;

        case Horizontal:
        {
            int split = (int)((float)area.w * m_splitPercentage);
            m_splits[0]->Layout(renderer, Recti{ area.x,area.y,split,area.h });
            m_splits[1]->Layout(renderer, Recti{ area.x + split,area.y,area.w - split,area.h });
        }
        break;
    }
}

void WindowLayout::AddWindow(WindowBase* window)
{
    if (m_splitType == NoSplit)
    {
        m_tabs.push_back(window);
        m_activeTab = (int)m_tabs.size() - 1;
    }
}

bool WindowLayout::Tick()
{
    if (m_splitType == NoSplit)
    {
        return (m_tabs.size() > 0) ? m_tabs[m_activeTab]->Tick() : false;
    }
    else
    {
        bool result = m_splits[0]->Tick();
        result |= m_splits[1]->Tick();
        return result;
    }
}

void WindowLayout::AddWindow(int x, int y, WindowBase* window)
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
            m_splits[0]->AddWindow(x, y, window);
        else
            m_splits[1]->AddWindow(x, y, window);
    }
}

void WindowLayout::RemoveWindow(WindowBase* window)
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

bool WindowLayout::CheckForDocking(int x, int y, WindowDockQuery& query)
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

void WindowLayout::Paint(SDL_Renderer* renderer, const Recti& area)
{
    auto& wm = WindowManager::Instance();
    auto& tp = Application::Instance().GetThemeProperties();
    auto& highlight = wm.GetWindowHighlightQuery();
    auto& selection = wm.GetWindowSelectionQuery();

    if (m_area.Overlaps(area))
    {
        if (highlight.m_highlight == WindowHighlightType::LayoutSplit && highlight.m_layout == this)
        {
            SDL_FRect area = highlight.m_area.AsSDLFRect();
            tp.SetRenderDrawColor(renderer, ThemeColor::HighlightArea);
            SDL_RenderFillRect(renderer, &area);
        }

        if (m_splitType == NoSplit)
        {
            Recti bodyRect{ m_area.x, m_area.y + WINDOW_TAB_BAR_HEIGHT + WINDOW_CLIENT_BORDER, m_area.w, m_area.h - WINDOW_TAB_BAR_HEIGHT - WINDOW_CLIENT_BORDER * 2 };
            IconRenderer::Instance().DrawIcon(renderer, m_locked ? Icons::LayoutLocked : Icons::LayoutFree, m_area.x + 14, m_area.y + 18);

            if (!m_tabs.empty())
            {
                // render tabs backgrounds
                auto& fr = FontRenderer::Instance();
                for (auto w : m_tabs)
                {
                    SDL_FRect sdlTabInnerRect{ (float)w->m_tabArea.x, (float)(w->m_tabArea.y + 2), (float)w->m_tabArea.w, (float)WINDOW_TAB_BAR_HEIGHT - 2 };
                    tp.SetRenderDrawColor(renderer, ThemeColor::TabBackground);
                    SDL_RenderFillRect(renderer, &sdlTabInnerRect);
                    SDL_Color col = tp.m_colors[(int)ThemeColor::TabText];
                    if (m_activeTab != -1 && m_tabs[m_activeTab] == w)
                    {
                        tp.SetRenderDrawColor(renderer, ThemeColor::TabHighlight);
                        SDL_RenderLine(renderer, (float)w->m_tabArea.x, (float)w->m_tabArea.y + 1, (float)w->m_tabArea.x + (float)w->m_tabArea.w, (float)w->m_tabArea.y + 1);
                    }
                }

                // render tab highlighted
                if (highlight.m_highlight == WindowHighlightType::Tab && highlight.m_layout == this)
                {
                    SDL_FRect area = highlight.m_area.AsSDLFRect();
                    tp.SetRenderDrawColor(renderer, ThemeColor::HighlightArea);
                    SDL_RenderFillRect(renderer, &area);
                }

                // render tab text
                for (auto w : m_tabs)
                {
                    SDL_Color col = tp.m_colors[(int)ThemeColor::TabText];
                    if (w->IsModified())
                        col = tp.m_colors[(int)ThemeColor::TabTextModified];
                    fr.RenderText(renderer, w->m_name, col, w->m_tabArea.x + TEXT_HBORDER, w->m_tabArea.y + 4, FontType::UI);
                    if (m_activeTab != -1 && m_tabs[m_activeTab] == w)
                    {
                        tp.SetRenderDrawColor(renderer, ThemeColor::TabHighlight);
                        SDL_RenderLine(renderer, (float)w->m_tabArea.x, (float)w->m_tabArea.y + 1, (float)w->m_tabArea.x + (float)w->m_tabArea.w, (float)w->m_tabArea.y + 1);
                    }
                }


                // render active tab body
                if (m_activeTab != -1)
                {
                    m_tabs[m_activeTab]->PaintScrollbars(renderer);
                    SDL_Rect clientArea = m_tabs[m_activeTab]->m_clientArea.AsSDLRect();
                    SDL_SetRenderClipRect(renderer, &clientArea);
                    m_tabs[m_activeTab]->Paint(renderer, area);
                    SDL_SetRenderClipRect(renderer, nullptr);
                }
            }
            else
            {
                // just clear the body
                SDL_FRect sdlBodyRect{ (float)bodyRect.x, (float)bodyRect.y, (float)bodyRect.w, (float)bodyRect.h };
                tp.SetRenderDrawColor(renderer, ThemeColor::WindowClientEmpty);
                SDL_RenderFillRect(renderer, &sdlBodyRect);
            }

            float bx1 = (float)(m_area.x + WINDOW_CLIENT_BORDER - 1);
            float bx2 = (float)(m_area.x + m_area.w - WINDOW_CLIENT_BORDER + 1);
            float by1 = (float)(m_area.y + WINDOW_TAB_BAR_HEIGHT + WINDOW_CLIENT_BORDER - 1);
            float by2 = (float)(m_area.y + m_area.h - WINDOW_CLIENT_BORDER + 1);
            bool selected = wm.GetActiveWindowLayout() == this;

            if (selected)
                tp.SetRenderDrawColor(renderer, ThemeColor::WindowEdgeLightSelected);
            else
                tp.SetRenderDrawColor(renderer, ThemeColor::WindowEdgeLight);

            SDL_RenderLine(renderer, bx1, by1, bx1, by2);
            SDL_RenderLine(renderer, bx1, by1, bx2, by1);

            if (selected)
                tp.SetRenderDrawColor(renderer, ThemeColor::WindowEdgeDarkSelected);
            else
                tp.SetRenderDrawColor(renderer, ThemeColor::WindowEdgeDark);

            SDL_RenderLine(renderer, bx2, by1, bx2, by2);
            SDL_RenderLine(renderer, bx1, by2, bx2, by2);
        }
        else
        {
            m_splits[0]->Paint(renderer, area);
            m_splits[1]->Paint(renderer, area);

            switch (m_splitType)
            {
                case WindowLayout::Vertical:
                {
                    int x1 = m_area.x;
                    int x2 = m_area.x + m_area.w;
                    int y = m_area.y + (int)(m_area.h * m_splitPercentage);
                    int y1 = y - 3;
                    int y2 = y + 3;

                    if (selection.m_highlight == WindowHighlightType::LayoutSplit && selection.m_layout == this)
                    {
                        SDL_FRect bodyArea{ (float)x1, (float)y1, (float)(x2 - x1),(float)(y2 - y1) };
                        tp.SetRenderDrawColor(renderer, ThemeColor::LayoutSplit);
                        SDL_RenderFillRect(renderer, &bodyArea);
                    }
                }
                break;

                case WindowLayout::Horizontal:
                {
                    int y1 = m_area.y;
                    int y2 = m_area.y + m_area.h;
                    int x = m_area.x + (int)(m_area.w * m_splitPercentage);
                    int x1 = x - 3;
                    int x2 = x + 3;

                    if (selection.m_highlight == WindowHighlightType::LayoutSplit && selection.m_layout == this)
                    {
                        SDL_FRect bodyArea = SDL_FRect{ (float)x1, (float)y1, (float)(x2 - x1),(float)(y2 - y1) };
                        tp.SetRenderDrawColor(renderer, ThemeColor::LayoutSplit);
                        SDL_RenderFillRect(renderer, &bodyArea);
                    }
                }
                break;
            }

            if (selection.m_highlight == WindowHighlightType::LayoutSplit && selection.m_layout == this)
            {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 32);
                SDL_FRect fullBody{ (float)m_area.x, (float)m_area.y, (float)m_area.w, (float)m_area.h };
                SDL_RenderFillRect(renderer, &fullBody);
            }
        }
    }
}

WindowLayout* WindowLayout::FindFirstNonSplitLayout()
{
    if (m_splitType == NoSplit)
        return this;
    else
    {
        auto layout = m_splits[0]->FindFirstNonSplitLayout();
        if (layout)
            return layout;
        return m_splits[1]->FindFirstNonSplitLayout();
    }
}

WindowLayout* WindowLayout::FindLayoutFromWindow(WindowBase* window, int& tabIdx)
{
    if (m_splitType == NoSplit)
    {
        for (size_t i = 0; i < m_tabs.size(); i++)
        {
            if (m_tabs[i] == window)
            {
                tabIdx = (int)i;
                return this;
            }
        }
    }
    else
    {
        auto layout = m_splits[0]->FindLayoutFromWindow(window, tabIdx);
        if (layout)
            return layout;
        return m_splits[1]->FindLayoutFromWindow(window, tabIdx);
    }
    return nullptr;
}

void WindowLayout::GatherWindows(std::vector<WindowBase*>& windows)
{
    if (m_splitType == NoSplit)
    {
        for (auto vw : m_tabs)
            windows.push_back(vw);
        m_tabs.clear();
    }
    else
    {
        m_splits[0]->GatherWindows(windows);
        m_splits[1]->GatherWindows(windows);
    }
}

WindowLayout::~WindowLayout()
{
    for (auto t : m_tabs)
        delete t;
    delete m_splits[0];
    delete m_splits[1];
}

int WindowLayout::CountWindows()
{
    if (m_splitType == NoSplit)
    {
        return (int)m_tabs.size();
    }
    else
    {
        return m_splits[0]->CountWindows() + m_splits[1]->CountWindows();
    }
}

void WindowLayout::CollapseEmptyLayouts()
{
    if (m_splitType != NoSplit)
    {
        m_splits[0]->CollapseEmptyLayouts();
        m_splits[1]->CollapseEmptyLayouts();

        bool empty0 = (m_splits[0] == nullptr) || (m_splits[0]->m_splitType == NoSplit && (m_splits[0]->m_tabs.empty() && !m_splits[0]->m_locked));
        bool empty1 = (m_splits[1] == nullptr) || (m_splits[1]->m_splitType == NoSplit && (m_splits[1]->m_tabs.empty() && !m_splits[1]->m_locked));

        if (empty0 && empty1)
        {
            m_splitType = NoSplit;
            delete m_splits[0];
            delete m_splits[1];
        }
        else if (empty0)
        {
            delete m_splits[0];
            m_splitType = m_splits[1]->m_splitType;
            m_tabs = std::move(m_splits[1]->m_tabs);
            auto new0 = m_splits[1]->m_splits[0];
            auto new1 = m_splits[1]->m_splits[1];
            m_splits[1]->m_splits[0] = nullptr;
            m_splits[1]->m_splits[1] = nullptr;
            m_splits[1]->m_splitType = NoSplit;
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
            m_splits[0]->m_splits[0] = nullptr;
            m_splits[0]->m_splits[1] = nullptr;
            m_splits[0]->m_splitType = NoSplit;
            delete m_splits[0];
            m_splits[0] = new0;
            m_splits[1] = new1;
        }
    }
}

WindowBase* WindowLayout::GetActiveWindow()
{
    if (m_splitType == NoSplit && m_tabs.size() > 0)
        return m_tabs[m_activeTab];
    return nullptr;
}

void WindowLayout::ActivateWindow(WindowBase* window)
{
    for (size_t i = 0; i < m_tabs.size(); i++)
    {
        if (m_tabs[i] == window)
        {
            m_activeTab = (int)i;
            return;
        }
    }
}


void WindowLayout::SaveLayout(std::vector<std::string>& layoutTokens)
{
    switch (m_splitType)
    {
        case NoSplit:
        {
            layoutTokens.push_back("N");
            layoutTokens.push_back(m_locked ? "L" : "F");
            layoutTokens.push_back(std::format("{}", m_tabs.size()));
            layoutTokens.push_back(std::format("{}", m_activeTab));
            for (auto file : m_tabs)
            {
                file->SaveTokens(layoutTokens);
            }
        }
        break;

        case Horizontal:
        case Vertical:
            layoutTokens.push_back(m_splitType == Horizontal ? "H" : "V");
            layoutTokens.push_back(std::format("{}", m_splitPercentage));
            m_splits[0]->SaveLayout(layoutTokens);
            m_splits[1]->SaveLayout(layoutTokens);
            break;
    }
}

void WindowLayout::LoadLayout(const std::vector<std::string>& layoutTokens, size_t& idx)
{
    std::string typeStr = layoutTokens[idx++];
    if (typeStr == "N")
    {
        m_splitType = NoSplit;
        m_locked = layoutTokens[idx++] == "L";
        int tabCount = std::stoi(layoutTokens[idx++]);
        m_activeTab = std::stoi(layoutTokens[idx++]);
        for (int i = 0; i < tabCount; i++)
        {
            if (!SourceFileWindow::CreateFromLayoutTokens(this, layoutTokens, idx))
            {
                if (!OutputWindow::CreateFromLayoutTokens(this, layoutTokens, idx))
                {
                    if (!UndoBufferWindow::CreateFromLayoutTokens(this, layoutTokens, idx))
                    {
                        if (!ProjectListWindow::CreateFromLayoutTokens(this, layoutTokens, idx))
                        {
                            Log(LogGroup::System, "Unknown window token: {}", layoutTokens[idx]);
                        }
                    }
                }
            }
        }
    }
    else if (typeStr == "V")
    {
        m_splitType = Vertical;
        m_splitPercentage = std::stof(layoutTokens[idx++]);
        m_splits[0] = new WindowLayout;
        m_splits[1] = new WindowLayout;
        m_splits[0]->LoadLayout(layoutTokens, idx);
        m_splits[1]->LoadLayout(layoutTokens, idx);
    }
    else if (typeStr == "H")
    {
        m_splitType = Horizontal;
        m_splitPercentage = std::stof(layoutTokens[idx++]);
        m_splits[0] = new WindowLayout;
        m_splits[1] = new WindowLayout;
        m_splits[0]->LoadLayout(layoutTokens, idx);
        m_splits[1]->LoadLayout(layoutTokens, idx);
    }
    else
    {
        Assert(false, "Expected N,V,H");
    }
}

void WindowLayout::Message(struct WindowMessageStruct& msg)
{
    msg.m_layout = this;
    if (msg.m_flags & WMF_AreaCheck)
    {
        if (!m_area.Contains(msg.m_x, msg.m_y))
            return;
    }

    if (msg.m_flags & WMF_Layout)
    {
        switch (msg.m_type)
        {
            case WindowMessage::Query_FindLockedLayout:
                if (m_splitType == NoSplit && m_locked)
                    msg.m_response++;
                break;

            case WindowMessage::Query_Highlight:
            {
                switch (m_splitType)
                {
                    case Horizontal:
                    {
                        int splitX = m_area.x + (int)(m_area.w * m_splitPercentage);
                        Recti splitArea{ splitX - 6, m_area.y, 12, m_area.h };
                        if (splitArea.Contains(msg.m_x, msg.m_y))
                        {
                            auto query = (WindowHighlightQuery*)msg.m_query;
                            query->m_area = splitArea;
                            query->m_highlight = WindowHighlightType::LayoutSplit;
                            query->m_tree = msg.m_tree;
                            query->m_layout = this;
                            query->m_split.m_vertical = false;
                            query->m_split.m_splitPos = splitX;
                            msg.m_response++;
                        }
                    }
                    break;

                    case Vertical:
                    {
                        int splitY = m_area.y + (int)(m_area.h * m_splitPercentage);
                        Recti splitArea{ m_area.x, splitY - 6, m_area.w,  12 };
                        if (splitArea.Contains(msg.m_x, msg.m_y))
                        {
                            auto query = (WindowHighlightQuery*)msg.m_query;
                            query->m_area = splitArea;
                            query->m_highlight = WindowHighlightType::LayoutSplit;
                            query->m_tree = msg.m_tree;
                            query->m_layout = this;
                            query->m_split.m_vertical = false;
                            query->m_split.m_splitPos = splitY;
                            msg.m_response++;
                        }
                    }
                    break;
                }
            }
            break;
        }
        if ((msg.m_flags & WMF_EarlyOut) && msg.m_response > 0)
            return;
    }

    if (m_splitType == NoSplit)
    {
        if (msg.m_flags & WMF_Layout)
        {
            // handle tab checks...
            switch (msg.m_type)
            {
                case WindowMessage::Query_Highlight:
                {
                    auto query = (WindowHighlightQuery*)msg.m_query;
                    Recti lockArea = IconRenderer::Instance().CalcIconArea(Icons::LayoutLocked, m_area.x + 14, m_area.y + 18);
                    if (lockArea.Contains(msg.m_x, msg.m_y))
                    {
                        query->m_highlight = WindowHighlightType::WindowLayoutIcon;
                        query->m_id = (int)Icons::LayoutLocked;
                        query->m_tree = msg.m_tree;
                        query->m_layout = this;
                        query->m_area = lockArea;
                        msg.m_response++;
                        return;
                    }

                    for (int i = 0; i < m_tabs.size(); i++)
                    {
                        if (m_tabs[i]->m_tabArea.Contains(msg.m_x, msg.m_y))
                        {
                            query->m_area = m_tabs[i]->m_tabArea;
                            query->m_highlight = WindowHighlightType::Tab;
                            query->m_tree = msg.m_tree;
                            query->m_layout = this;
                            query->m_id = i;
                            msg.m_response++;
                            return;
                        }
                    }
                }
                break;
            }

            if ((msg.m_flags & WMF_EarlyOut) && msg.m_response > 0)
                return;
        }

        if (msg.m_flags & WMF_Window)
        {
            if (msg.m_flags & WMF_AreaCheck)
            {
                if (!m_tabs.empty())
                    m_tabs[m_activeTab]->Message(this, msg);
            }
            else
            {
                if (msg.m_type == WindowMessage::Query_WindowCount)
                {
                    msg.m_response += (int)m_tabs.size();
                }

                for (auto tab : m_tabs)
                {
                    tab->Message(this, msg);

                    if ((msg.m_flags & WMF_EarlyOut) && msg.m_response > 0)
                        return;
                }
            }
        }
    }
    else
    {
        m_splits[0]->Message(msg);
        if ((msg.m_flags & WMF_EarlyOut) && msg.m_response > 0)
            return;

        m_splits[1]->Message(msg);
        if ((msg.m_flags & WMF_EarlyOut) && msg.m_response > 0)
            return;
    }
}

bool WindowLayout::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            auto& selection = WindowManager::Instance().GetWindowSelectionQuery();
            switch (selection.m_highlight)
            {
                case WindowHighlightType::WindowLayoutIcon:
                    m_locked = !m_locked;
                    selection.m_tree->m_dirty;
                    return true;
            }
        }
        break;
    }
    return false;
}

