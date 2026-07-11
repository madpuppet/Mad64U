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

struct WindowMenuQuery
{
    void Reset() { m_tree = nullptr; m_menuIdx = -1; m_menuItemIdx = -1; }
    struct WindowTree* m_tree = nullptr;
    int m_menuIdx = 0;
    int m_menuItemIdx = 0;
};

struct WindowScrollBarQuery
{
    struct WindowTree* m_tree = nullptr;
    struct WindowLayout* m_layout = nullptr;
    class WindowBase* m_window = nullptr;
    bool m_vertical = false;
    int m_grabOffset = 0;
};
