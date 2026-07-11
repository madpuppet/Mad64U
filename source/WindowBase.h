#pragma once

#include <functional>
#include "WindowQueries.h"

class WindowBase
{
public:
    std::string m_name;
    Recti m_area;
    Recti m_tabArea;

    Vec2i m_clientContentSize{ 0,0 };     // size of client
    Vec2i m_clientContentOffset{ 0,0 };   // offset to start rendering client (adjust by scrollbars)
    Recti m_clientArea{ 0,0,0,0 };            // static screen area with scroll bars removed

    bool m_horizontalScrollbarVisible = false;
    bool m_verticalScrollbarVisible = false;
    Recti m_hsbBackgroundArea;
    Recti m_hsbBarArea;
    Recti m_vsbBackgroundArea;
    Recti m_vsbBarArea;

    void LayoutScrollbars();                 // layout scrollbars based on content size
    void PaintScrollbars(SDL_Renderer* renderer);        // paint scrollbars if visible
    bool CheckForScrollBar(int x, int y, WindowScrollBarQuery& query);
    void UpdateScrollBar(int offset, WindowScrollBarQuery& query);

    virtual void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) = 0;
    virtual void Close() = 0;
    virtual bool Tick() { return false; }
    virtual bool HandleEvent(SDL_Event* e) {
        return false;
    }
};

