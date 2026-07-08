#pragma once

#include "WindowTree.h"

struct WindowMenuItem
{
    WindowMenuItem(const std::string& name, const std::function<void(void)> &activate) : m_name(name), m_activate(activate) {}

    std::string m_name;
    std::function<void(void)> m_activate;

    // dynamic
    Recti m_area;
};


struct WindowMenu
{
    void Layout(SDL_Renderer* renderer, const Vec2i &pos);
    void Paint(SDL_Renderer* renderer, int highlightItemIdx);

    std::string m_name;
    std::vector<WindowMenuItem*> m_items;
    bool m_open = false;

    // dynamic
    Recti m_area;
    Recti m_dropDownArea;
};

struct WindowMenuList
{
    void Layout(WindowTree* tree);
    void Paint(const WindowMenuQuery &highlight);

    bool CheckForMenu(WindowTree *tree, int x, int y, WindowMenuQuery& query);

    WindowTree* m_tree = nullptr;
    std::vector<WindowMenu*> m_menus;
};
