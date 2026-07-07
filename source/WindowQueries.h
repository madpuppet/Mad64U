#pragma once

struct WindowSplitQuery
{
    bool m_foundSplit = false;
    struct WindowTree* m_tree;
    struct WindowLayout* m_layout;
    int m_grabOffset;
    int m_splitPos;
};

struct WindowDockQuery
{
    enum SplitPosition
    {
        SplitPosition_None,
        SplitPosition_Left,
        SplitPosition_Right,
        SplitPosition_Top,
        SplitPosition_Bottom
    } m_splitPosition;

    bool m_foundDock = false;
    struct WindowTree* m_tree = nullptr;
    struct WindowLayout* m_layout = nullptr;
    bool m_verticalSplit = false;
    Recti m_bodyArea;
    Recti m_splitArea;
};

struct WindowTabQuery
{
    struct WindowTree* m_tree = nullptr;
    struct WindowLayout* m_layout = nullptr;
    int m_tabIndex = -1;
};

