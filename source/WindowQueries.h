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
    LayoutSplit,
    Menu,
    Tab,
    WindowTitleBar,
    WindowTreeIcon,
    WindowLayoutIcon,
    ProjectListIcon,
    ScrollBar,
    ProjectListFile,
    ClientArea
};

struct WindowHighlightQuery
{
    void Reset()
    {
        m_highlight = WindowHighlightType::None;
    }

    WindowHighlightType m_highlight = WindowHighlightType::None;
    struct WindowTree* m_tree = nullptr;
    struct WindowLayout* m_layout = nullptr;
    class WindowBase* m_window = nullptr;
    int m_id = 0;                        // context specific identifier for icon, list index, file index, tab index, etc.
    Recti m_area{ 0,0,0,0 };
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
            bool m_vertical;
            bool m_bar;
        } m_scrollbar;
        struct
        {
            bool m_vertical;
            int m_splitPos;
        } m_split;
        struct
        {
            enum class Icons m_icon;
        } m_projectFiles;
    };

    bool IsEqual(const WindowHighlightQuery& o)
    {
        return o.m_highlight == m_highlight && o.m_tree == m_tree && o.m_area.x == m_area.x && o.m_area.y == m_area.y 
            && o.m_area.w == m_area.w && o.m_area.h == m_area.h;
    }
};


