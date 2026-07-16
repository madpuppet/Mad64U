#include "common.h"
#include "WindowBase.h"
#include "Application.h"

#define WINDOW_SCROLLBAR_SIZE 16
#define WINDOW_SCROLLBAR_MARGIN 2

void WindowBase::LayoutScrollbars()
{
    m_horizontalScrollbarVisible = m_clientContentSize.x > m_area.w;
    m_verticalScrollbarVisible = m_clientContentSize.y > m_area.h;

    m_clientArea = m_area;
    if (m_horizontalScrollbarVisible)
    {
        m_clientArea.h -= WINDOW_SCROLLBAR_SIZE;
        m_hsbBackgroundArea = Recti{ m_area.x, m_area.y + m_area.h - WINDOW_SCROLLBAR_SIZE, m_area.w, WINDOW_SCROLLBAR_SIZE };

        if (m_verticalScrollbarVisible)
            m_hsbBackgroundArea.w -= WINDOW_SCROLLBAR_SIZE;

        int maxOffset = Max(m_clientContentSize.x - m_clientArea.w, 0);
        float offsetPercent = (float)m_clientContentOffset.x / (float)maxOffset;
        int barSize = m_hsbBackgroundArea.w - WINDOW_SCROLLBAR_MARGIN * 2;
        int visibleBarSize = (int)((float)m_clientArea.w / (float)m_clientContentSize.x * (float)barSize);
        int maxBarOffset = barSize - visibleBarSize;
        int barOffset = (int)((float)maxBarOffset * offsetPercent);
        int barStart = m_hsbBackgroundArea.x + WINDOW_SCROLLBAR_MARGIN + barOffset;
        m_hsbBarArea = Recti(barStart, m_hsbBackgroundArea.y + WINDOW_SCROLLBAR_MARGIN, visibleBarSize, m_hsbBackgroundArea.h - WINDOW_SCROLLBAR_MARGIN * 2);
    }
    if (m_verticalScrollbarVisible)
    {
        m_clientArea.w -= WINDOW_SCROLLBAR_SIZE;
        m_vsbBackgroundArea = Recti{ m_area.x + m_area.w - WINDOW_SCROLLBAR_SIZE, m_area.y, WINDOW_SCROLLBAR_SIZE, m_area.h };

        if (m_horizontalScrollbarVisible)
            m_vsbBackgroundArea.h -= WINDOW_SCROLLBAR_SIZE;

        int maxOffset = Max(m_clientContentSize.y - m_clientArea.h, 0);
        float offsetPercent = (float)m_clientContentOffset.y / (float)maxOffset;
        int barSize = m_vsbBackgroundArea.h - WINDOW_SCROLLBAR_MARGIN * 2;
        int visibleBarSize = (int)((float)m_clientArea.h / (float)m_clientContentSize.y * (float)barSize);
        int maxBarOffset = barSize - visibleBarSize;
        int barOffset = (int)((float)maxBarOffset * offsetPercent);
        int barStart = m_vsbBackgroundArea.y + WINDOW_SCROLLBAR_MARGIN + barOffset;
        m_vsbBarArea = Recti(m_vsbBackgroundArea.x + WINDOW_SCROLLBAR_MARGIN, barStart, m_vsbBackgroundArea.w - WINDOW_SCROLLBAR_MARGIN * 2, visibleBarSize);
    }
}

void WindowBase::PaintScrollbars(SDL_Renderer* renderer)
{
    auto& tp = Application::Instance().GetThemeProperties();
    if (m_horizontalScrollbarVisible)
    {
        SDL_FRect backarea = m_hsbBackgroundArea.AsSDLFRect();
        tp.SetRenderDrawColor(renderer, ThemeColor::ScrollBarBackground);
        SDL_RenderFillRect(renderer, &backarea);

        SDL_FRect bararea = m_hsbBarArea.AsSDLFRect();
        tp.SetRenderDrawColor(renderer, ThemeColor::ScrollBar);
        SDL_RenderFillRect(renderer, &bararea);
    }
    if (m_verticalScrollbarVisible)
    {
        SDL_FRect backarea = m_vsbBackgroundArea.AsSDLFRect();
        tp.SetRenderDrawColor(renderer, ThemeColor::ScrollBarBackground);
        SDL_RenderFillRect(renderer, &backarea);

        SDL_FRect bararea = m_vsbBarArea.AsSDLFRect();
        tp.SetRenderDrawColor(renderer, ThemeColor::ScrollBar);
        SDL_RenderFillRect(renderer, &bararea);
    }
}

bool WindowBase::CheckForScrollBar(int x, int y, WindowScrollBarQuery& query)
{
    if (m_horizontalScrollbarVisible)
    {
        if (m_hsbBackgroundArea.Contains(x, y))
        {
            query.m_window = this;
            query.m_grabOffset = x - m_hsbBackgroundArea.x;
            query.m_vertical = false;
            return true;
        }
    }
    if (m_verticalScrollbarVisible)
    {
        if (m_vsbBackgroundArea.Contains(x, y))
        {
            query.m_window = this;
            query.m_grabOffset = y - m_vsbBackgroundArea.y;
            query.m_vertical = true;
            return true;
        }
    }
    return false;
}

void WindowBase::Close()
{
    WindowManager::Instance().RemoveWindow(this);
    delete this;
}

void WindowBase::UpdateScrollBar(int offset, WindowScrollBarQuery& query)
{
    if (query.m_vertical)
    {
        float offsetPercent = Clamp((float)(offset - m_vsbBackgroundArea.y) / (float)(m_vsbBackgroundArea.h - WINDOW_SCROLLBAR_MARGIN * 2), 0.0f, 1.0f);
        int contentOffset = (int)(offsetPercent * m_clientContentSize.y);
        m_clientContentOffset.y = Clamp(contentOffset - m_clientArea.h / 2, 0, m_clientContentSize.y - m_clientArea.h);
        LayoutScrollbars();
        query.m_tree->m_dirty = true;
    }
    else
    {
        float offsetPercent = Clamp((float)(offset - m_hsbBackgroundArea.x) / (float)(m_hsbBackgroundArea.w - WINDOW_SCROLLBAR_MARGIN * 2), 0.0f, 1.0f);
        int contentOffset = (int)(offsetPercent * m_clientContentSize.x);
        m_clientContentOffset.x = Clamp(contentOffset - m_clientArea.w / 2, 0, m_clientContentSize.x - m_clientArea.w);
        LayoutScrollbars();
        query.m_tree->m_dirty = true;
    }
}

bool WindowBase::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_WHEEL:
        {
            int scroll = e->wheel.integer_y * LINE_HEIGHT * 4;
            m_clientContentOffset.y -= scroll;
            m_clientContentOffset.y = Clamp(m_clientContentOffset.y, 0, Max(m_clientContentSize.y - m_clientArea.h, 0));
            if (m_clientContentSize.x < m_clientArea.w)
                m_clientContentOffset.x = 0;
            LayoutScrollbars();
            return true;
        }
        break;
    }

    return false;
}



