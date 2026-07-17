#include "common.h"
#include "Application.h"
#include "WindowTree.h"
#include "IconRenderer.h"
#include "WindowBase.h"
#include "WindowMenuList.h"
#include "Settings.h"
#include "SourceFileManager.h"
#include "LogManager.h"
#include "ProjectListWindow.h"
#include "OutputWindow.h"
#include "UndoBufferWindow.h"
#include <filesystem>

class TestWindow : public WindowBase
{
public:
    TestWindow(const std::string& name, const SDL_Color& col) : m_bgCol(col) { m_name = name; }
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override
    {
        SDL_SetRenderDrawColor(renderer, m_bgCol.r, m_bgCol.g, m_bgCol.b, m_bgCol.a);
        SDL_FRect body = m_clientArea.AsSDLFRect();
        SDL_RenderFillRect(renderer, &body);
    }
    void Close()
    {
        WindowManager::Instance().RemoveWindow(this);
        delete this;
    }
    SDL_Color m_bgCol;
};


typedef void (SDLCALL* SDL_DialogFileCallback)(void* userdata, const char* const* filelist, int filter);


static const char* s_themecolor_name[NumThemeColor] =
{
    "TitleBar",
    "MenuText",
    "MenuItemText",
    "MenuItemBackground",
    "MenuItemBackgroundSelected",
    "WindowBackground",
    "WindowClientEmpty",
    "TabBackground",
    "TabText",
    "TabTextModified",
    "TabTextSelected",
    "TabHighlight",
    "SourceBackground",
    "SourceBackgroundSelected",
    "ScrollBarBackground",
    "ScrollBar",
    "ScrollBarSelected",
    "Cursor",
    "TextHighlight",
    "TextGeneral",
    "TextOperator",
    "TextComment"
};


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
        std::vector<std::string> paths;
        while (*filelist)
        {
            paths.push_back(std::string(*filelist));
            filelist++;
        }

        SourceFileManager::Instance().RequestLoadFiles(paths);
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

    auto styleMenu = new WindowMenu;
    styleMenu->m_name = "Style";

    auto themeItem = new WindowMenuItem("Theme");
    for (auto theme : m_themes)
    {
        auto activateTheme = [theme]()
            {
                Application::Instance().SelectTheme(theme->m_name.c_str());
                WindowManager::Instance().PaintAll();
            };
        themeItem->m_subMenus.push_back(new WindowMenuItem(theme->m_name, activateTheme));
    }
    styleMenu->m_items.push_back(themeItem);
    wm.AddWindowMenu(styleMenu);

    auto newWindowProjectList = []()
        {
            WindowManager::Instance().AddWindow(new ProjectListWindow);
            WindowManager::Instance().LayoutWindows();
        };

    auto newWindowOutput = []()
        {
            WindowManager::Instance().AddWindow(new OutputWindow);
            WindowManager::Instance().LayoutWindows();
        };

    auto newWindowUndoBuffer = []()
        {
            WindowManager::Instance().AddWindow(new UndoBufferWindow);
            WindowManager::Instance().LayoutWindows();
        };

    auto windowMenu = new WindowMenu;
    windowMenu->m_name = "Windows";
    windowMenu->m_items.push_back(new WindowMenuItem("Project Files", newWindowProjectList ));
    windowMenu->m_items.push_back(new WindowMenuItem("Output", newWindowOutput));
    windowMenu->m_items.push_back(new WindowMenuItem("Undo Buffer", newWindowUndoBuffer));
    windowMenu->m_items.push_back(new WindowMenuItem("- - - - - - - - - -", []() {}));
    windowMenu->m_items.push_back(new WindowMenuItem("Save Window Layout", []() { WindowManager::Instance().SaveWindowLayout(); }));
    wm.AddWindowMenu(windowMenu);

    auto buildMenu = new WindowMenu;
    buildMenu->m_name = "Build";

    auto buildCB = []()
        {
            auto window = WindowManager::Instance().GetActiveWindowBase();
            window->Compile();
        };

    buildMenu->m_items.push_back(new WindowMenuItem("Build", buildCB));
    buildMenu->m_items.push_back(new WindowMenuItem("Run", []() {Log("Run\n"); }));
    buildMenu->m_items.push_back(new WindowMenuItem("Launch on U64", []() {Log("Launch\n"); }));
    wm.AddWindowMenu(buildMenu);

    wm.LayoutMenu();
}

ThemeProperties* Application::FindTheme(const char* name)
{
    for (auto theme : m_themes)
    {
        if (theme->m_name == name)
            return theme;
    }
    return nullptr;
}

std::pair<std::string, std::string> SplitNameItem(std::string_view text)
{
    const size_t separator = text.find('_');

    if (separator == std::string_view::npos)
        return { std::string(text), {} };

    return {
        std::string(text.substr(0, separator)),
        std::string(text.substr(separator + 1))
    };
}

void Application::LoadThemesFromSettings()
{
    auto& settings = Settings::Instance();
    std::vector<std::string> themeItems = settings.GetItemsStartWith(SETTING_THEME_PREFIX);
    for (auto themeItem : themeItems)
    {
        std::string itemStr = themeItem.substr(6);
        auto split = SplitNameItem(itemStr);

        auto theme = FindTheme(split.first.c_str());
        if (!theme)
        {
            theme = new ThemeProperties;
            theme->m_name = split.first;
            m_themes.push_back(theme);
        }

        for (int i = 0; i < NumThemeColor; i++)
        {
            if (split.second.c_str() == s_themecolor_name[i])
            {
                theme->m_colors[i] = settings.GetColor(themeItem.c_str());
                break;
            }
        }
    }

    auto themeName = settings.GetString(SETTING_ACTIVE_THEME);
    SelectTheme(themeName.c_str());
}
void Application::SaveThemesToSettings()
{
    auto& settings = Settings::Instance();
    for (auto theme : m_themes)
    {
        if (theme->m_system)
            continue;

        for (int i = 0; i < NumThemeColor; i++)
        {
            std::string colorName = std::format("theme_{}_{}", theme->m_name, s_themecolor_name[i]);
            settings.SetColor(colorName.c_str(), theme->m_colors[i]);
        }
    }
}

Uint32 TimerEventType = 0;
static Uint32 SDLCALL TimerCallback(void* userdata, SDL_TimerID timerID, Uint32 interval)
{
    SDL_Event event{};
    event.type = TimerEventType;
    event.user.code = 123;
    event.user.data1 = userdata;

    SDL_PushEvent(&event);

    // Return the next interval.
    // Return 0 to make this a one-shot timer.
    return interval;
}

void Application::AddTimerEvent()
{
    TimerEventType = SDL_RegisterEvents(1);

    if (TimerEventType == 0)
    {
        Log("SDL_RegisterEvents failed: %s\n", SDL_GetError());
        return;
    }

    SDL_TimerID timer = SDL_AddTimer(WINDOW_TICK_MS, TimerCallback, nullptr);
    if (timer == 0)
    {
        Log("SDL_AddTimer failed: %s\n", SDL_GetError());
    }
}

void Application::CreateSettings()
{
    Settings::Startup();

    auto &settings = Settings::Instance();
    settings.SetString(SETTING_RENDERER, "direct3d11");
    settings.SetString(SETTING_ACTIVE_THEME, "light" );

    // default themes
    // these can be overriden by settings.txt
    {
        auto& theme = *(new ThemeProperties);
        theme.m_system = true;
        theme.m_name = "dark";
        theme.m_colors[(int)ThemeColor::TitleBar] = SDL_Color(64, 32, 16, 255);
        theme.m_colors[(int)ThemeColor::MenuText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackground] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackgroundSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::WindowBackground] = SDL_Color(16, 16, 16, 255);
        theme.m_colors[(int)ThemeColor::WindowClientEmpty] = SDL_Color(32, 32, 32, 255);
        theme.m_colors[(int)ThemeColor::TabBackground] = SDL_Color(48, 48, 48, 255);
        theme.m_colors[(int)ThemeColor::TabText] = SDL_Color(200, 200, 200, 255);
        theme.m_colors[(int)ThemeColor::TabTextModified] = SDL_Color(255, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::TabTextSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::TabHighlight] = SDL_Color(255, 128, 255, 255);
        theme.m_colors[(int)ThemeColor::SourceBackground] = SDL_Color(32, 32, 32, 255);
        theme.m_colors[(int)ThemeColor::SourceBackgroundSelected] = SDL_Color(48, 48, 48, 255);
        theme.m_colors[(int)ThemeColor::ScrollBarBackground] = SDL_Color(0, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::ScrollBar] = SDL_Color(128, 128, 0, 255);
        theme.m_colors[(int)ThemeColor::ScrollBarSelected] = SDL_Color(255, 255, 0, 255);
        theme.m_colors[(int)ThemeColor::Cursor] = SDL_Color(255, 255, 128, 255);
        theme.m_colors[(int)ThemeColor::TextHighlight] = SDL_Color(128, 128, 128, 255);
        theme.m_colors[(int)ThemeColor::TextGeneral] = SDL_Color(230, 230, 230, 255);
        theme.m_colors[(int)ThemeColor::TextOperator] = SDL_Color(64, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::TextComment] = SDL_Color(128, 255, 128, 255);
        m_themes.push_back(&theme);
    }

    {
        auto& theme = *(new ThemeProperties);
        theme.m_system = true;
        theme.m_name = "c64blue";
        theme.m_colors[(int)ThemeColor::TitleBar] = SDL_Color(64, 32, 16, 255);
        theme.m_colors[(int)ThemeColor::MenuText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackground] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackgroundSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::WindowBackground] = SDL_Color(0, 0, 32, 255);
        theme.m_colors[(int)ThemeColor::WindowClientEmpty] = SDL_Color(8, 16, 48, 255);
        theme.m_colors[(int)ThemeColor::TabBackground] = SDL_Color(48, 48, 48, 255);
        theme.m_colors[(int)ThemeColor::TabText] = SDL_Color(200, 200, 200, 255);
        theme.m_colors[(int)ThemeColor::TabTextModified] = SDL_Color(255, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::TabTextSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::TabHighlight] = SDL_Color(255, 128, 255, 255);
        theme.m_colors[(int)ThemeColor::SourceBackground] = SDL_Color(0, 0, 64, 255);
        theme.m_colors[(int)ThemeColor::SourceBackgroundSelected] = SDL_Color(0, 0, 78, 255);
        theme.m_colors[(int)ThemeColor::ScrollBarBackground] = SDL_Color(0, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::ScrollBar] = SDL_Color(128, 128, 0, 255);
        theme.m_colors[(int)ThemeColor::ScrollBarSelected] = SDL_Color(255, 255, 0, 255);
        theme.m_colors[(int)ThemeColor::Cursor] = SDL_Color(255, 255, 128, 255);
        theme.m_colors[(int)ThemeColor::TextHighlight] = SDL_Color(128, 128, 128, 128);
        theme.m_colors[(int)ThemeColor::TextGeneral] = SDL_Color(0, 200, 255, 255);
        theme.m_colors[(int)ThemeColor::TextOperator] = SDL_Color(255, 128, 64, 255);
        theme.m_colors[(int)ThemeColor::TextComment] = SDL_Color(128, 255, 128, 255);
        m_themes.push_back(&theme);
    }

    {
        auto& theme = *(new ThemeProperties);
        theme.m_system = true;
        theme.m_name = "light";
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
        theme.m_colors[(int)ThemeColor::TabTextModified] = SDL_Color(255, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::TabHighlight] = SDL_Color(255, 128, 255, 255);
        theme.m_colors[(int)ThemeColor::SourceBackground] = SDL_Color(180, 170, 160, 255);
        theme.m_colors[(int)ThemeColor::SourceBackgroundSelected] = SDL_Color(200, 180, 150, 255);
        theme.m_colors[(int)ThemeColor::ScrollBarBackground] = SDL_Color(0, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::ScrollBar] = SDL_Color(128, 128, 0, 255);
        theme.m_colors[(int)ThemeColor::ScrollBarSelected] = SDL_Color(255, 255, 0, 255);
        theme.m_colors[(int)ThemeColor::Cursor] = SDL_Color(64, 64, 0, 255);
        theme.m_colors[(int)ThemeColor::TextHighlight] = SDL_Color(128, 128, 128, 255);
        theme.m_colors[(int)ThemeColor::TextGeneral] = SDL_Color(0, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::TextOperator] = SDL_Color(0, 0, 255, 255);
        theme.m_colors[(int)ThemeColor::TextComment] = SDL_Color(0, 128, 0, 255);
        m_themes.push_back(&theme);

        m_activeTheme = &theme;
    }

    // load settings from existing settings file
    Settings::Instance().Load();

    // pull any changed themes
    LoadThemesFromSettings();
}

int Application::Run()
{
    LogManager::Startup();

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

    // create 
    CreateSettings();
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

    CreateMenus();
    AddTimerEvent();

    wm.LoadWindowLayout();

    SDL_Event e;
    while (!m_quit)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == TimerEventType)
            {
                SourceFileManager::Instance().Tick();
                WindowManager::Instance().Tick();
            }
            else
                wm.HandleEvent(&e);
        }
        wm.Paint();
    }

    SourceFileManager::Shutdown();
    WindowManager::Shutdown();
    IconRenderer::Shutdown();
    FontRenderer::Shutdown();

    return 1;
}

void Application::SelectTheme(const char *themeName)
{
    auto theme = FindTheme(themeName);
    if (theme)
    {
        m_activeTheme = theme;
        Settings::Instance().SetString(SETTING_ACTIVE_THEME, m_activeTheme->m_name);
        Settings::Instance().Save();
    }
}
