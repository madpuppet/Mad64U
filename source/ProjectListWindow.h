
#pragma once

#pragma once

#include "WindowBase.h"

class ProjectListWindow : public WindowBase
{
public:
    ProjectListWindow();
    ~ProjectListWindow();

    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    void Close() override;
    bool HandleEvent(SDL_Event* e) override;
    bool Tick() override;

protected:
};

