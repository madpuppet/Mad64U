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

        int barSize = m_hsbBackgroundArea.w - WINDOW_SCROLLBAR_MARGIN * 2;
        int barStart = m_hsbBackgroundArea.x + WINDOW_SCROLLBAR_MARGIN + (int)(((float)m_clientContentOffset.x / (float)(m_clientContentSize.x - m_clientArea.w)) * (float)barSize);
        int barEnd = m_hsbBackgroundArea.x + (int)(((float)(m_clientContentOffset.x + m_clientArea.w) / (float)(m_clientContentSize.x)) * (float)barSize);
        m_hsbBarArea = Recti(barStart, m_hsbBackgroundArea.y + WINDOW_SCROLLBAR_MARGIN, barEnd - barStart, m_hsbBackgroundArea.h - WINDOW_SCROLLBAR_MARGIN * 2);
    }
    if (m_verticalScrollbarVisible)
    {
        m_clientArea.w -= WINDOW_SCROLLBAR_SIZE;
        m_vsbBackgroundArea = Recti{ m_area.x + m_area.w - WINDOW_SCROLLBAR_SIZE, m_area.y, WINDOW_SCROLLBAR_SIZE, m_area.h };

        if (m_horizontalScrollbarVisible)
            m_vsbBackgroundArea.h -= WINDOW_SCROLLBAR_SIZE;

        int barSize = m_vsbBackgroundArea.h - WINDOW_SCROLLBAR_MARGIN * 2;
        int barStart = m_vsbBackgroundArea.y + WINDOW_SCROLLBAR_MARGIN + (int)(((float)m_clientContentOffset.y / (float)(m_clientContentSize.y - m_clientArea.h)) * (float)barSize);
        int barEnd = m_vsbBackgroundArea.y + (int)(((float)(m_clientContentOffset.y + m_clientArea.h) / (float)(m_clientContentSize.y)) * (float)barSize);
        m_vsbBarArea = Recti(m_vsbBackgroundArea.x + WINDOW_SCROLLBAR_MARGIN, barStart, m_vsbBackgroundArea.w - WINDOW_SCROLLBAR_MARGIN * 2, barEnd-barStart);
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
