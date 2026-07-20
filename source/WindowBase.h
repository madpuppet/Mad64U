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
    void UpdateScrollBar(int offset, bool vertical, struct WindowTree* tree);
    void MakeRowVisible(int row);
    void Message(struct WindowLayout *layout, struct WindowMessageStruct& msg);

    virtual ~WindowBase();
    virtual void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) = 0;
    virtual void Close();
    virtual bool Tick() { return false; }
    virtual bool HandleEvent(SDL_Event* e);
    virtual bool IsModified() { return false; }
    virtual class SourceFile* GetSourceFile() { return nullptr; }
    virtual void MessageChild(struct WindowLayout *layout, struct WindowMessageStruct& msg) {}

    // enough info to recreate the window from static CreateFromTokens() function each window type should have
    virtual void SaveTokens(std::vector<std::string>& layoutTokens) = 0;
};

