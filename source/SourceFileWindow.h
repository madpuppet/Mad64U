#pragma once

#include "WindowBase.h"

class TextBox
{
public:
    void Enable(const Recti& area)
    {
        m_area = area;
        m_focused = true;
    }

    bool m_focused = false;
    Recti m_area;
    std::string m_text;
    int m_cursor = 0;
    void Paint(SDL_Renderer *renderer, float m_animateTimer);
    void HandleEvent(SDL_Event* e);
};



class SourceFileWindow : public WindowBase
{
public:
    SourceFileWindow(class SourceFile* file);

    static bool CreateFromLayoutTokens(WindowLayout *layout, const std::vector<std::string>& layoutTokens, size_t& idx);
    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    class SourceFile* GetSourceFile() { return m_sourceFile; }
    void Close() override;
    bool IsModified() override;
    void BuildFragments(SDL_Renderer* renderer, class SourceLine* line);
    bool HandleEvent(SDL_Event* e) override;
    void CalcXYFromClientPos(int x, int y, int& col, int& row);
    int CalcXPos(int x, int y);
    void MessageChild(WindowLayout *layout, struct WindowMessageStruct& msg) override;

    Vec2i m_cursor{ 0,0 };
    int m_trackedColumn = 0;
    bool m_overwrite = false;
    SourceFile* m_sourceFile;
    int m_charWidth = 24;

    bool Tick() override;

    // movement operations
    void MoveCursorLeft();
    void MoveCursorRight();
    void MoveCursorUp();
    void MoveCursorDown();
    void MoveCursorPageDown();
    void MoveCursorPageUp();
    void MoveCursorStartOfLine();
    void MoveCursorEndOfLine();
    void MoveCursorStartOfFile();
    void MoveCursorEndOfFile();
    void MoveCursorXY(int x, int y);
    void MakeCursorVisible();

    // undoable editing operations
    void DeleteCharBeforeCursor();
    void DeleteCharAfterCursor();
    void InsertNewLineAtCursor();
    void InsertTextAtCursor(const char* text);
    void DeleteSelected();

    // cut and paste
    void CopySelected();
    void PasteSelected();

    void StartMarking()
    {
        if (!m_marking)
        {
            m_marking = true;
            m_marked = true;
            m_markStart = m_cursor;
        }
    }
    void StopMarking()
    {
        m_marking = false;
    }
    void ClearMarking()
    {
        m_marking = false;
        m_marked = false;
    }
protected:
    void ClampCursor();
    Recti CalcCursorArea();
    float m_animTime = 0.0f;
    bool m_marking = false;
    bool m_marked = false;
    bool m_shiftDown = false;
    bool m_mouseLeftDown = false;
    Vec2i m_markStart{ 0,0 };
    Vec2i m_mouseDownPos{ 0,0 };

    // search/replace
    const int SearchModeWidth = 300;
    enum class SearchMode
    {
        Off,
        GotoLine,
        Search,
        Replace
    } m_searchMode = SearchMode::Replace;
    TextBox m_searchBox;
    TextBox m_replaceBox;
    void CalcSearchAndReplaceArea(Recti &fullArea, Recti &titleArea, Recti& searchArea, Recti& replaceArea);
    void PaintSearchWindow(SDL_Renderer* renderer);
};

