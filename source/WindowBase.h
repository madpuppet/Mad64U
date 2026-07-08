#pragma once

#include <functional>

class WindowBase
{
public:
    std::string m_name;
    Recti m_area;
    Recti m_tabArea;

    virtual void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) = 0;
    virtual void Close() = 0;
};

