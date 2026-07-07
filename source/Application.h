#pragma once

#include "WindowTree.h"
#include "FontRenderer.h"
#include "IconRenderer.h"
#include "WindowManager.h"
#include "Singleton.h"

class Application : public Singleton<Application>
{
public:
    int Run();

    TTF_Font* GetUIFont() { return m_uiFont; }
    TTF_Font* GetTextFont() { return m_textFont; }

    void Quit() { m_quit = true; }

protected:
    TTF_Font* m_uiFont = nullptr;
    TTF_Font* m_textFont = nullptr;
    bool m_quit = false;
};

