#pragma once

#include "WindowBase.h"

class TextBox
{
public:
    void Enable(const std::string title, const Recti& area);
    void Disable();

    bool m_visible = false;
    bool m_focused = false;
    Vec2i m_stringPos;
    std::string m_title;

    Recti m_area;
    std::string m_text;
    int m_cursor = 0;
    void Paint(SDL_Renderer* renderer, float m_animateTimer);
    void HandleEvent(SDL_Event* e);
};


class SearchWindow : public WindowBase
{
public:
    SearchWindow();
    ~SearchWindow();

    enum class SearchMode
    {
        Goto,
        Search,
        Replace
    };

    void SetMode(SearchMode mode);

    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;
    bool Tick() override;
    void MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg) override;

    void SaveTokens(std::vector<std::string>& layoutTokens);
    static bool CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

protected:
    struct SearchResult
    {
        SourceFile* m_sourceFile;
        int m_line;
        int m_startChar;
        int m_length;
    };
    std::vector<SearchResult> m_results;

    SearchMode m_searchMode = SearchMode::Replace;
    TextBox m_searchBox;
    TextBox m_replaceBox;
    float m_animTime = 0.0f;
};

