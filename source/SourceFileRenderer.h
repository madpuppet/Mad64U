#pragma once

#include "WindowBase.h"

class SourceFileRenderer : public WindowBase
{
public:
    SourceFileRenderer(class SourceFile* file);

    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    void Close() override;
    void BuildFragments(SDL_Renderer* renderer, class SourceLine* line, int x, int y);
    bool HandleEvent(SDL_Event* e) override;

    Vec2i m_cursor;
    int m_trackedColumn = 0;
    bool m_overwrite;
    SourceFile* m_sourceFile;

    bool Tick() override;

    void MoveCursorLeft();
    void MoveCursorRight();
    void MoveCursorUp();
    void MoveCursorDown();
    void MoveCursorPageDown();
    void MoveCursorPageUp();
    void MoveCursorStart();
    void MoveCursorEnd();
    void MoveCursorXY(int x, int y);
    void MakeCursorVisible();

    void CalcXYFromClientPos(int x, int y, int& col, int& row);

protected:
    Recti CalcCursorArea();
    float m_animTime = 0.0f;
};

