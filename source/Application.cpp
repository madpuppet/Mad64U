#include "common.h"
#include "Application.h"
#include "WindowTree.h"
#include "IconRenderer.h"
#include "WindowBase.h"

class TestWindow : public WindowBase
{
public:
    TestWindow(const std::string& name, const Colori& col) : m_bgCol(col) { m_name = name; }
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override
    {
        SDL_SetRenderDrawColor(renderer, m_bgCol.r, m_bgCol.g, m_bgCol.b, m_bgCol.a);
        SDL_FRect body = m_area.AsSDLFRect();
        SDL_RenderFillRect(renderer, &body);
    }
    Colori m_bgCol;
};



int Application::Run()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK))
    {
        Log("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        exit(0);
    }

    // dump renderers
    for (int i = 0; i < SDL_GetNumRenderDrivers(); i++)
    {
        auto name = SDL_GetRenderDriver(i);
        Log("RENDERER: %s\n", name);
    }

    // init fonts
    TTF_Init();
    m_uiFont = TTF_OpenFont("data/font.ttf", 16);
    if (m_uiFont == nullptr)
    {
        Log("Unable to create font.ttf\n");
        exit(0);
    }

    m_textFont = TTF_OpenFont("data/fontc64.ttf", 16);
    if (m_textFont == nullptr)
    {
        Log("Unable to create fontc64.ttf\n");
        exit(0);
    }

    // create 
    FontRenderer::Startup();
    IconRenderer::Startup();
    WindowManager::Startup();

    auto& wm = WindowManager::Instance();

    // Create some test windows
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    auto win = new WindowTree(Recti{ 300,100,640,512 });
    win->m_layout.m_tabs.push_back(new TestWindow("poop.asm", Colori{ 255,0,0,32 }));
    win->m_layout.m_tabs.push_back(new TestWindow("another.asm", Colori{ 255,255,0,32 }));
    win->m_layout.m_tabs.push_back(new TestWindow("another.c", Colori{ 0,0,255,32 }));
    win->m_layout.m_tabs.push_back(new TestWindow("another.txt", Colori{ 0,255,0,32 }));
    wm.AddWindowTree(win);

    SDL_Event e;
    while (!m_quit)
    {
        while (SDL_PollEvent(&e))
            wm.HandleEvent(&e);

        wm.Paint();
    }

    WindowManager::Shutdown();
    IconRenderer::Shutdown();
    FontRenderer::Shutdown();

    return 1;
}

