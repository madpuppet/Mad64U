#pragma once

#include "WindowBase.h"

class TextBox
{
public:
    void SetInfo(const std::string title, const Recti& area);
    bool Contains(int x, int y)
    {
        return x >= m_area.x && x < m_area.x + m_area.w && y >= m_area.y && y < m_area.y + m_area.h;
    }

    void Focus(bool enable);
    bool IsFocused() { return m_focused; }

    void Tick();
    void SetCursorPos(int x);
    void MoveCursor(int xoffset) { m_cursor = Clamp(m_cursor + xoffset, 0, (int)m_text.size()); }
    void MoveCursorStart() { m_cursor = 0; }
    void MoveCursorEnd() { m_cursor = (int)m_text.size(); }

    bool m_focused = false;
    Vec2i m_stringPos;
    std::string m_title;

    Recti m_area;
    std::string m_text;
    int m_cursor = 0;
    int m_charWidth = 8;
    float m_animTime = 0.0f;
    void Paint(SDL_Renderer* renderer);
    bool HandleEvent(SDL_Event* e);
};

struct SearchResult
{
    int m_line;
    int m_startChar;
    int m_length;
};

class SearchWindow : public WindowBase
{
public:
    SearchWindow();
    ~SearchWindow();

    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;
    bool Tick() override;
    void MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg) override;
    void SetSearchActive();

    void SaveTokens(std::vector<std::string>& layoutTokens);
    static bool CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

    void SetSearchText(const std::string& text);

protected:
    void Search();
    void Replace();
    SourceFile* m_searchFile;
    std::vector<SearchResult> m_searchLines;

    TextBox m_searchBox;
    TextBox m_replaceBox;
    Recti m_searchLineArea;
    float m_animTime = 0.0f;
};

