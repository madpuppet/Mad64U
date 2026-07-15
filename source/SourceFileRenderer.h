#pragma once

#include "WindowBase.h"

class SourceFileRenderer : public WindowBase
{
public:
    SourceFileRenderer(class SourceFile* file);

    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    void Close() override;
    void BuildFragments(SDL_Renderer* renderer, class SourceLine* line);
    bool HandleEvent(SDL_Event* e) override;
    void CalcXYFromClientPos(int x, int y, int& col, int& row);
    int CalcXPos(int x, int y);
    void Compile() override;

    Vec2i m_cursor;
    int m_trackedColumn = 0;
    bool m_overwrite;
    SourceFile* m_sourceFile;

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
    Recti CalcCursorArea();
    float m_animTime = 0.0f;
    bool m_marking = false;
    bool m_marked = false;
    bool m_shiftDown = false;
    bool m_mouseLeftDown = false;
    Vec2i m_markStart{ 0,0 };
};

