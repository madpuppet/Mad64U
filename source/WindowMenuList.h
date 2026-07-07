#pragma once

#include "WindowTree.h"

struct WindowMenuItem
{
    WindowMenuItem(const std::string& name) : m_name(name) {}

    std::string m_name;

    // dynamic
    Recti m_area;
};


struct WindowMenu
{
    void Layout(SDL_Renderer* renderer, const Vec2i &pos);
    void Paint(SDL_Renderer* renderer);

    std::string m_name;
    std::vector<WindowMenuItem*> m_items;
    bool m_open = true;

    // dynamic
    Recti m_area;
    Recti m_dropDownArea;
};

struct WindowMenuList
{
    void Layout(WindowTree* tree);
    void Paint();

    WindowTree* m_tree = nullptr;
    std::vector<WindowMenu*> m_menus;
};
