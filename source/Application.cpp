#include "common.h"
#include "Application.h"
#include "WindowTree.h"
#include "IconRenderer.h"
#include "WindowBase.h"
#include "WindowMenuList.h"
#include "SourceFileManager.h"
#include <filesystem>

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


typedef void (SDLCALL* SDL_DialogFileCallback)(void* userdata, const char* const* filelist, int filter);



SDL_DialogFileFilter s_filters[] =
{
    { "Asm Files", "asm" },
    { "C Files", "c" },
    { "Text Files", "txt" },
    { "All Files", "*" }
};

void LoadFileCallback(void* userdata, const char* const* filelist, int filter)
{
    if (filelist && *filelist)
    {
        while (*filelist)
        {
            SourceFileManager::Instance().LoadFile(*filelist);
            filelist++;
        }
    }
    else
    {
        Log("No File Selected\n");
    }
}

void SaveFileCallback(void* userdata, const char* const* filelist, int filter)
{
    SourceFile* file = (SourceFile*)userdata;
    if (filelist && *filelist)
    {
        SourceFileManager::Instance().RenameFile(file, *filelist);
        SourceFileManager::Instance().SaveFile(file);
    }
    else
    {
        Log("No File Selected\n");
    }
}

void Application::CreateMenus()
{
    auto& wm = WindowManager::Instance();

    auto newFile = []()
        {
            SourceFileManager::Instance().NewFile("");
        };

    auto loadFile = []()
        {
            std::string cwd = std::filesystem::current_path().string();

            SDL_ShowOpenFileDialog(
                LoadFileCallback,
                nullptr,
                nullptr,        // parent window
                s_filters,
                SDL_arraysize(s_filters),
                cwd.c_str(),
                true
            );
        };

    auto saveFile = []()
        {
            auto window = WindowManager::Instance().GetActiveWindowBase();
            if (window)
            {
                auto file = SourceFileManager::Instance().GetFileFromWindow(window);
                if (file)
                {
                    if (file->m_path.empty())
                    {
                        std::string cwd = std::filesystem::current_path().string();

                        SDL_ShowSaveFileDialog(
                            SaveFileCallback,
                            file,
                            nullptr,        // parent window
                            s_filters,
                            SDL_arraysize(s_filters),
                            cwd.c_str()        // default location
                        );
                    }
                    else
                    {
                        SourceFileManager::Instance().SaveFile(file);
                    }
                }
            }
        };

    auto saveAsFile = []()
        {
            auto window = WindowManager::Instance().GetActiveWindowBase();
            if (window)
            {
                auto file = SourceFileManager::Instance().GetFileFromWindow(window);
                if (file)
                {
                    std::string cwd = std::filesystem::current_path().string();

                    SDL_ShowSaveFileDialog(
                        SaveFileCallback,
                        file,
                        nullptr,        // parent window
                        s_filters,
                        SDL_arraysize(s_filters),
                        cwd.c_str()        // default location
                    );
                }
            }
        };

    auto closeFile = []()
        {
            auto window = WindowManager::Instance().GetActiveWindowBase();
            if (window)
                window->Close();
        };

    auto fileMenu = new WindowMenu;
    fileMenu->m_name = "File";
    fileMenu->m_items.push_back(new WindowMenuItem("New", newFile));
    fileMenu->m_items.push_back(new WindowMenuItem("Load", loadFile));
    fileMenu->m_items.push_back(new WindowMenuItem("Save", saveFile));
    fileMenu->m_items.push_back(new WindowMenuItem("SaveAs", saveAsFile));
    fileMenu->m_items.push_back(new WindowMenuItem("Close", closeFile));
    wm.AddWindowMenu(fileMenu);

    auto lightTheme = []()
        {
            Application::Instance().SelectTheme(Theme::Light);
            WindowManager::Instance().PaintAll();
        };

    auto darkTheme = []()
        {
            Application::Instance().SelectTheme(Theme::Dark);
            WindowManager::Instance().PaintAll();
        };

    auto styleMenu = new WindowMenu;
    styleMenu->m_name = "Style";
    styleMenu->m_items.push_back(new WindowMenuItem("Theme: Light", lightTheme));
    styleMenu->m_items.push_back(new WindowMenuItem("Theme: Dark", darkTheme));
    wm.AddWindowMenu(styleMenu);

    auto buildMenu = new WindowMenu;
    buildMenu->m_name = "Build";
    buildMenu->m_items.push_back(new WindowMenuItem("Build", []() {Log("Build\n"); }));
    buildMenu->m_items.push_back(new WindowMenuItem("Run", []() {Log("Run\n"); }));
    buildMenu->m_items.push_back(new WindowMenuItem("Launch on U64", []() {Log("Launch\n"); }));
    wm.AddWindowMenu(buildMenu);

    wm.LayoutMenu();
}

void Application::CreateThemes()
{
    {
        auto &theme = m_themes[(int)Theme::Dark];
        theme.m_colors[(int)ThemeColor::TitleBar] = SDL_Color(64, 32, 16, 255);
        theme.m_colors[(int)ThemeColor::MenuText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackground] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackgroundSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::WindowBackground] = SDL_Color(16, 16, 16, 255);
        theme.m_colors[(int)ThemeColor::WindowClientEmpty] = SDL_Color(32, 32, 32, 255);
        theme.m_colors[(int)ThemeColor::TabBackground] = SDL_Color(48, 48, 48, 255);
        theme.m_colors[(int)ThemeColor::TabText] = SDL_Color(200, 200, 200, 255);
        theme.m_colors[(int)ThemeColor::TabTextSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::TabHighlight] = SDL_Color(255, 128, 255, 255);
        theme.m_colors[(int)ThemeColor::SourceBackground] = SDL_Color(32, 32, 32, 255);
        theme.m_colors[(int)ThemeColor::SourceBackgroundSelected] = SDL_Color(0, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::SourceText] = SDL_Color(180, 200, 240, 128);
        theme.m_colors[(int)ThemeColor::SourceTextSelected] = SDL_Color(180, 200, 240, 255);
    }

    {
        auto& theme = m_themes[(int)Theme::Light];
        theme.m_colors[(int)ThemeColor::TitleBar] = SDL_Color(64, 32, 16, 255);
        theme.m_colors[(int)ThemeColor::MenuText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackground] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackgroundSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::WindowBackground] = SDL_Color(64, 64, 64, 255);
        theme.m_colors[(int)ThemeColor::WindowClientEmpty] = SDL_Color(32, 32, 32, 255);
        theme.m_colors[(int)ThemeColor::TabBackground] = SDL_Color(100, 100, 100, 255);
        theme.m_colors[(int)ThemeColor::TabText] = SDL_Color(200, 200, 200, 255);
        theme.m_colors[(int)ThemeColor::TabTextSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::TabHighlight] = SDL_Color(255, 128, 255, 255);
        theme.m_colors[(int)ThemeColor::SourceBackground] = SDL_Color(180, 160, 130, 255);
        theme.m_colors[(int)ThemeColor::SourceBackgroundSelected] = SDL_Color(200, 180, 150, 255);
        theme.m_colors[(int)ThemeColor::SourceText] = SDL_Color(0, 0, 0, 128);
        theme.m_colors[(int)ThemeColor::SourceTextSelected] = SDL_Color(0, 0, 0, 255);
    }
}

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

    m_textFont = TTF_OpenFont("data/font.ttf", 16);
    if (m_textFont == nullptr)
    {
        Log("Unable to create fontc64.ttf\n");
        exit(0);
    }

    // create 
    FontRenderer::Startup();
    IconRenderer::Startup();
    WindowManager::Startup();
    SourceFileManager::Startup();

    auto& wm = WindowManager::Instance();

    // Create some test windows
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    auto win = new WindowTree(Recti{ 300,100,640,512 });
    wm.AddWindowTree(win);
    wm.SetActiveTree(win);

    CreateThemes();
    CreateMenus();

    SDL_Event e;
    while (!m_quit)
    {
        while (SDL_PollEvent(&e))
            wm.HandleEvent(&e);

        wm.Paint();
    }

    SourceFileManager::Shutdown();
    WindowManager::Shutdown();
    IconRenderer::Shutdown();
    FontRenderer::Shutdown();

    return 1;
}

