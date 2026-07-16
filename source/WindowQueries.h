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
    void Reset() { m_tree = nullptr; m_menuIdx = -1; m_menuItemIdx = -1; m_subMenuItemIdx = -1; }
    struct WindowTree* m_tree = nullptr;
    int m_menuIdx = -1;
    int m_menuItemIdx = -1;
    int m_subMenuItemIdx = -1;
};

struct WindowScrollBarQuery
{
    struct WindowTree* m_tree = nullptr;
    struct WindowLayout* m_layout = nullptr;
    class WindowBase* m_window = nullptr;
    bool m_vertical = false;
    int m_grabOffset = 0;
};

enum class WindowHighlightType
{
    None,
    Menu,
    Tab,
    Icon,
    ScrollBar,
    Local
};

struct WindowHighlightQuery
{
    WindowHighlightType m_highlight = WindowHighlightType::None;
    struct WindowTree* m_tree = nullptr;
    struct WindowLayout* m_layout = nullptr;
    class WindowBase* m_window = nullptr;
    int m_localID = 0;
    union
    {
        struct
        {
            int m_menuIdx;
            int m_itemIdx;
            int m_subItemIdx;
            int m_prefixIcon;
        } m_menu;
        struct
        {
            int m_tabIdx;
        } m_tab;
        struct
        {
            int m_iconIdx;
        } m_icon;
        struct
        {
            bool m_vertical;
            bool m_bar;
        } m_scrollbar;
    };
};


