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
#include "MemViewWindow.h"
#include "OutputWindow.h"
#include "UndoBufferWindow.h"
#include "NetworkManager.h"
#include "SearchWindow.h"
#include "EmuScreenWindow.h"
#include "ViceBridge.h"
#include "FunctionsWindow.h"
#include "Cpu6502.h"
#include <filesystem>

extern "C" {
#include "src/main.h"
};

u32 CustomEvent_Timer = 0;

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
    "LayoutSplit",
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
    "HighlightLine",
    "HighlightArea",
    "WindowEdgeLight",
    "WindowEdgeDark",
    "WindowEdgeLightSelected",
    "WindowEdgeDarkSelected",
    "SearchTitleBack",
    "SearchTextBack",
    "SearchTextBackSelected",
    "SearchTitle",
    "SearchText",
    "TextGeneral",
    "TextOperator",
    "TextString",
    "TextLabel",
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
            auto& sm = SourceFileManager::Instance();
            auto file = sm.GetActiveSourceFile();
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
                    sm.SaveFile(file);
                }
            }
        };

    auto saveAsFile = []()
        {
            auto& sm = SourceFileManager::Instance();
            auto file = sm.GetActiveSourceFile();
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
        };

    auto closeFile = []()
        {
            auto window = WindowManager::Instance().GetActiveWindowBase();
            if (window)
                window->Close();
        };

    auto exitEditor = []()
        {
            Application::Instance().Quit();
        };

    auto fileMenu = new WindowMenu;
    fileMenu->m_name = "File";
    fileMenu->m_items.push_back(new WindowMenuItem("New", newFile));
    fileMenu->m_items.push_back(new WindowMenuItem("Load", loadFile));
    fileMenu->m_items.push_back(new WindowMenuItem("Save", saveFile));
    fileMenu->m_items.push_back(new WindowMenuItem("SaveAs", saveAsFile));
    fileMenu->m_items.push_back(new WindowMenuItem("Close", closeFile));
    fileMenu->m_items.push_back(new WindowMenuItem("Quit Mad64U", exitEditor));
    wm.AddWindowMenu(fileMenu);

    auto styleMenu = new WindowMenu;
    styleMenu->m_name = "Style";
    m_themeMenu = new WindowMenuItem("Theme");
    for (auto theme : m_themes)
    {
        auto activateTheme = [theme]()
            {
                Application::Instance().SelectTheme(theme->m_name.c_str());
                WindowManager::Instance().PaintAll();
            };
        m_themeMenu->m_subMenus.push_back(new WindowMenuItem(theme->m_name, activateTheme));
    }
    styleMenu->m_items.push_back(m_themeMenu);

    m_showLinesMenu = new WindowMenuItem("Line Numbers");
    auto showLinesTrue = []() { Application::Instance().ShowLines(true); };
    auto showLinesFalse = []() { Application::Instance().ShowLines(false); };
    m_showLinesMenu->m_subMenus.push_back(new WindowMenuItem("Show", showLinesTrue));
    m_showLinesMenu->m_subMenus.push_back(new WindowMenuItem("Hide", showLinesFalse));
    styleMenu->m_items.push_back(m_showLinesMenu);

    m_showBytesMenu = new WindowMenuItem("Code Bytes");
    auto showBytesTrue = []() { Application::Instance().ShowBytes(true); };
    auto showBytesFalse = []() { Application::Instance().ShowBytes(false); };
    m_showBytesMenu->m_subMenus.push_back(new WindowMenuItem("Show", showBytesTrue));
    m_showBytesMenu->m_subMenus.push_back(new WindowMenuItem("Hide", showBytesFalse));
    styleMenu->m_items.push_back(m_showBytesMenu);

    wm.AddWindowMenu(styleMenu);

    auto ultimateMenu = new WindowMenu;
    ultimateMenu->m_name = "C64 Ultimate";
    ultimateMenu->m_items.push_back(new WindowMenuItem("Reset", []() {NetworkManager::Instance().SendReset(); }));
    ultimateMenu->m_items.push_back(new WindowMenuItem("LoadStar", []()
        {
            NetworkManager::Instance().Message(new NMS_Command("machine:writemem?address=0277&data=4CCF222A222C382C310D"));
            NetworkManager::Instance().Message(new NMS_Command("machine:writemem?address=00C6&data=0A"));

            NetworkManager::Instance().Message(new NMS_Command("machine:writemem?address=0277&data=52554E0D"));
            NetworkManager::Instance().Message(new NMS_Command("machine:writemem?address=00C6&data=04"));
        }));

    auto screenCol = []()
        {
            auto& nm = NetworkManager::Instance();
            for (int i = 0; i < 64; i++)
            {
                int border = i & 15;
                int back = i / 4;
                std::string str = std::format("machine:writemem?address=D020&data={:02X}{:02X}", border, back);
                nm.Message(new NMS_Command(str));
            }
        };
    ultimateMenu->m_items.push_back(new WindowMenuItem("ScreenCol",screenCol));
    ultimateMenu->m_items.push_back(new WindowMenuItem("Pause", []() {NetworkManager::Instance().SendPause(); }));
    ultimateMenu->m_items.push_back(new WindowMenuItem("Resume", []() {NetworkManager::Instance().SendResume(); }));
    ultimateMenu->m_items.push_back(new WindowMenuItem("Power Off", []() {NetworkManager::Instance().SendPowerOff(); }));

    wm.AddWindowMenu(ultimateMenu);

    auto viceMenu = new WindowMenu;
    viceMenu->m_name = "Vice";

    m_viceZoomModeMenu = new WindowMenuItem("Zoom Mode");
    auto sizeToFitZoom = []() { Application::Instance().Vice_SetZoomMode(false); };
    auto zoomLevel1 = []() { Application::Instance().Vice_SetZoomLevel(1); };
    auto zoomLevel2 = []() { Application::Instance().Vice_SetZoomLevel(2); };
    auto zoomLevel4 = []() { Application::Instance().Vice_SetZoomLevel(4); };
    auto zoomLevel8 = []() { Application::Instance().Vice_SetZoomLevel(8); };
    m_viceZoomModeMenu->m_subMenus.push_back(new WindowMenuItem("Size To Fit", sizeToFitZoom));
    m_viceZoomModeMenu->m_subMenus.push_back(new WindowMenuItem("Fixed 1x", zoomLevel1));
    m_viceZoomModeMenu->m_subMenus.push_back(new WindowMenuItem("Fixed 2x", zoomLevel2));
    m_viceZoomModeMenu->m_subMenus.push_back(new WindowMenuItem("Fixed 4x", zoomLevel4));
    m_viceZoomModeMenu->m_subMenus.push_back(new WindowMenuItem("Fixed 8x", zoomLevel8));
    viceMenu->m_items.push_back(m_viceZoomModeMenu);

    m_viceVideoStandardMenu = new WindowMenuItem("Video Standard");
    auto setToPAL = []() { Application::Instance().Vice_EnablePAL(true); };
    auto setToNTSC = []() { Application::Instance().Vice_EnablePAL(false); };
    m_viceVideoStandardMenu->m_subMenus.push_back(new WindowMenuItem("PAL", setToPAL));
    m_viceVideoStandardMenu->m_subMenus.push_back(new WindowMenuItem("NTSC", setToNTSC));
    viceMenu->m_items.push_back(m_viceVideoStandardMenu);

    wm.AddWindowMenu(viceMenu);

    auto newWindowProjectList = []()
        {
            WindowManager::Instance().AddWindow(new ProjectListWindow);
            WindowManager::Instance().LayoutWindows();
        };

    auto newWindowMemView = []()
        {
            WindowManager::Instance().AddWindow(new MemViewWindow);
            WindowManager::Instance().LayoutWindows();
        };

    auto newWindowSystemOutput = []()
        {
            WindowManager::Instance().AddWindow(new OutputWindow(LogGroup::System));
            WindowManager::Instance().LayoutWindows();
        };

    auto newWindowBuildOutput = []()
        {
            WindowManager::Instance().AddWindow(new OutputWindow(LogGroup::Build));
            WindowManager::Instance().LayoutWindows();
        };

    auto newWindowUndoBuffer = []()
        {
            WindowManager::Instance().AddWindow(new UndoBufferWindow);
            WindowManager::Instance().LayoutWindows();
        };

    auto newWindowSearch = []()
        {
            WindowManager::Instance().AddWindow(new SearchWindow);
            WindowManager::Instance().LayoutWindows();
        };

    auto newWindowEmuScreen = []()
        {
            WindowManager::Instance().AddWindow(new EmuScreenWindow);
            WindowManager::Instance().LayoutWindows();
        };

    auto newWindowFunctions = []()
        {
            WindowManager::Instance().AddWindow(new FunctionsWindow);
            WindowManager::Instance().LayoutWindows();
        };

    auto toggleFrameLock = []()
        {
            auto layout = WindowManager::Instance().GetActiveWindowLayout();
            if (layout)
                layout->m_locked = !layout->m_locked;
        };

    auto windowMenu = new WindowMenu;
    windowMenu->m_name = "Windows";
    windowMenu->m_items.push_back(new WindowMenuItem("Project Files", newWindowProjectList ));
    windowMenu->m_items.push_back(new WindowMenuItem("Function List", newWindowFunctions));
    windowMenu->m_items.push_back(new WindowMenuItem("Debug Output", newWindowSystemOutput));
    windowMenu->m_items.push_back(new WindowMenuItem("Build Output", newWindowBuildOutput));
    windowMenu->m_items.push_back(new WindowMenuItem("Undo Buffer", newWindowUndoBuffer));
    windowMenu->m_items.push_back(new WindowMenuItem("Search And Replace", newWindowSearch));
    windowMenu->m_items.push_back(new WindowMenuItem("- - - - - - - - - -", []() {}));
    windowMenu->m_items.push_back(new WindowMenuItem("Emulator Screen", newWindowEmuScreen));
    windowMenu->m_items.push_back(new WindowMenuItem("Memory View", newWindowMemView));
    windowMenu->m_items.push_back(new WindowMenuItem("- - - - - - - - - -", []() {}));
    windowMenu->m_items.push_back(new WindowMenuItem("Toggle Frame Lock", toggleFrameLock));
    windowMenu->m_items.push_back(new WindowMenuItem("Save Window Layout", []() { WindowManager::Instance().SaveWindowLayout(); Settings::Instance().Save(); }));
    wm.AddWindowMenu(windowMenu);

    auto buildMenu = new WindowMenu;
    buildMenu->m_name = "Build";

    auto buildCB = []()
        {
            auto& sfm = SourceFileManager::Instance();
            auto file = sfm.GetActiveSourceFile();
            if (file)
            {
                sfm.Compile(file, false);
            }
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

static Uint32 SDLCALL TimerCallback(void* userdata, SDL_TimerID timerID, Uint32 interval)
{
    SDL_Event event{};
    event.type = CustomEvent_Timer;
    event.user.code = 123;
    event.user.data1 = userdata;

    SDL_PushEvent(&event);

    // Return the next interval.
    // Return 0 to make this a one-shot timer.
    return interval;
}

void Application::AddCustomEvents()
{
    u32 eventRange = SDL_RegisterEvents(1);
    if (eventRange == 0)
    {
        Log("SDL_RegisterEvents failed: %s\n", SDL_GetError());
        return;
    }
    CustomEvent_Timer = eventRange;

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
        theme.m_name = "Dark";
        theme.m_colors[(int)ThemeColor::TitleBar] = SDL_Color(64, 32, 16, 255);
        theme.m_colors[(int)ThemeColor::MenuText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackground] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackgroundSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::WindowBackground] = SDL_Color(16, 16, 16, 255);
        theme.m_colors[(int)ThemeColor::WindowClientEmpty] = SDL_Color(32, 32, 32, 255);
        theme.m_colors[(int)ThemeColor::LayoutSplit] = SDL_Color(255, 255, 0, 255);
        theme.m_colors[(int)ThemeColor::TabBackground] = SDL_Color(48, 48, 48, 255);
        theme.m_colors[(int)ThemeColor::TabText] = SDL_Color(200, 200, 200, 255);
        theme.m_colors[(int)ThemeColor::TabTextModified] = SDL_Color(255, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::TabTextSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::TabHighlight] = SDL_Color(255, 128, 255, 255);
        theme.m_colors[(int)ThemeColor::SourceBackground] = SDL_Color(16, 16, 16, 255);
        theme.m_colors[(int)ThemeColor::SourceBackgroundSelected] = SDL_Color(24, 24, 24, 255);
        theme.m_colors[(int)ThemeColor::ScrollBarBackground] = SDL_Color(32, 0, 32, 255);
        theme.m_colors[(int)ThemeColor::ScrollBar] = SDL_Color(64, 16, 64, 255);
        theme.m_colors[(int)ThemeColor::ScrollBarSelected] = SDL_Color(128, 128, 128, 255);
        theme.m_colors[(int)ThemeColor::Cursor] = SDL_Color(255, 255, 128, 255);
        theme.m_colors[(int)ThemeColor::TextHighlight] = SDL_Color(128, 128, 128, 255);
        theme.m_colors[(int)ThemeColor::HighlightLine] = SDL_Color(32, 32, 32, 255);
        theme.m_colors[(int)ThemeColor::HighlightArea] = SDL_Color(255, 255, 0, 32);
        theme.m_colors[(int)ThemeColor::WindowEdgeLight] = SDL_Color(64, 64, 64, 255);
        theme.m_colors[(int)ThemeColor::WindowEdgeDark] = SDL_Color(0, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::WindowEdgeLightSelected] = SDL_Color(96, 96, 96, 255);
        theme.m_colors[(int)ThemeColor::WindowEdgeDarkSelected] = SDL_Color(0, 0, 0, 255);

        theme.m_colors[(int)ThemeColor::SearchTitleBack] = SDL_Color(32, 32, 32, 255);
        theme.m_colors[(int)ThemeColor::SearchTextBack] = SDL_Color(0, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::SearchTextBackSelected] = SDL_Color(16, 16, 16, 255);
        theme.m_colors[(int)ThemeColor::SearchTitle] = SDL_Color(200, 190, 60, 255);
        theme.m_colors[(int)ThemeColor::SearchText] = SDL_Color(128, 128, 128, 255);

        theme.m_colors[(int)ThemeColor::TextGeneral] = SDL_Color(200, 200, 200, 255);
        theme.m_colors[(int)ThemeColor::TextOperator] = SDL_Color(240, 220, 100, 255);
        theme.m_colors[(int)ThemeColor::TextString] = SDL_Color(128, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::TextLabel] = SDL_Color(64, 64, 255, 255);
        theme.m_colors[(int)ThemeColor::TextComment] = SDL_Color(128, 255, 128, 255);
        m_themes.push_back(&theme);
    }

    {
        auto& theme = *(new ThemeProperties);
        theme.m_system = true;
        theme.m_name = "C64 Blue";
        theme.m_colors[(int)ThemeColor::TitleBar] = SDL_Color(64, 32, 16, 255);
        theme.m_colors[(int)ThemeColor::MenuText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackground] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackgroundSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::WindowBackground] = SDL_Color(0, 0, 32, 255);
        theme.m_colors[(int)ThemeColor::WindowClientEmpty] = SDL_Color(8, 16, 48, 255);
        theme.m_colors[(int)ThemeColor::LayoutSplit] = SDL_Color(255, 255, 0, 255);
        theme.m_colors[(int)ThemeColor::TabBackground] = SDL_Color(48, 48, 48, 255);
        theme.m_colors[(int)ThemeColor::TabText] = SDL_Color(200, 200, 200, 255);
        theme.m_colors[(int)ThemeColor::TabTextModified] = SDL_Color(255, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::TabTextSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::TabHighlight] = SDL_Color(255, 128, 255, 255);
        theme.m_colors[(int)ThemeColor::SourceBackground] = SDL_Color(0, 0, 64, 255);
        theme.m_colors[(int)ThemeColor::SourceBackgroundSelected] = SDL_Color(0, 0, 78, 255);
        theme.m_colors[(int)ThemeColor::ScrollBarBackground] = SDL_Color(0, 32, 132, 255);
        theme.m_colors[(int)ThemeColor::ScrollBar] = SDL_Color(0, 100, 255, 255);
        theme.m_colors[(int)ThemeColor::ScrollBarSelected] = SDL_Color(0, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::Cursor] = SDL_Color(255, 255, 128, 255);
        theme.m_colors[(int)ThemeColor::TextHighlight] = SDL_Color(128, 128, 128, 128);
        theme.m_colors[(int)ThemeColor::HighlightLine] = SDL_Color(0, 16, 96, 255);
        theme.m_colors[(int)ThemeColor::HighlightArea] = SDL_Color(255, 255, 0, 64);
        theme.m_colors[(int)ThemeColor::WindowEdgeLight] = SDL_Color(128, 196, 196, 64);
        theme.m_colors[(int)ThemeColor::WindowEdgeDark] = SDL_Color(0, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::WindowEdgeLightSelected] = SDL_Color(128, 220, 220, 64);
        theme.m_colors[(int)ThemeColor::WindowEdgeDarkSelected] = SDL_Color(0, 0, 0, 255);

        theme.m_colors[(int)ThemeColor::SearchTitleBack] = SDL_Color(16, 32, 64, 255);
        theme.m_colors[(int)ThemeColor::SearchTextBack] = SDL_Color(8, 16, 32, 255);
        theme.m_colors[(int)ThemeColor::SearchTextBackSelected] = SDL_Color(10, 20, 40, 255);
        theme.m_colors[(int)ThemeColor::SearchTitle] = SDL_Color(240, 220, 60, 255);
        theme.m_colors[(int)ThemeColor::SearchText] = SDL_Color(128, 140, 128, 255);

        theme.m_colors[(int)ThemeColor::TextGeneral] = SDL_Color(0, 200, 255, 255);
        theme.m_colors[(int)ThemeColor::TextOperator] = SDL_Color(255, 128, 64, 255);
        theme.m_colors[(int)ThemeColor::TextString] = SDL_Color(200, 170, 128, 255);
        theme.m_colors[(int)ThemeColor::TextLabel] = SDL_Color(255, 240, 64, 255);
        theme.m_colors[(int)ThemeColor::TextComment] = SDL_Color(128, 255, 128, 255);
        m_themes.push_back(&theme);
    }

    {
        auto& theme = *(new ThemeProperties);
        theme.m_system = true;
        theme.m_name = "Light";
        theme.m_colors[(int)ThemeColor::TitleBar] = SDL_Color(64, 32, 16, 255);
        theme.m_colors[(int)ThemeColor::MenuText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemText] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackground] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::MenuItemBackgroundSelected] = SDL_Color(255, 255, 255, 255);
        theme.m_colors[(int)ThemeColor::WindowBackground] = SDL_Color(64, 64, 64, 255);
        theme.m_colors[(int)ThemeColor::WindowClientEmpty] = SDL_Color(32, 32, 32, 255);
        theme.m_colors[(int)ThemeColor::LayoutSplit] = SDL_Color(255, 255, 0, 255);
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
        theme.m_colors[(int)ThemeColor::HighlightLine] = SDL_Color(170, 160, 150, 255);
        theme.m_colors[(int)ThemeColor::HighlightArea] = SDL_Color(255, 255, 0, 64);
        theme.m_colors[(int)ThemeColor::WindowEdgeLight] = SDL_Color(164, 164, 164, 64);
        theme.m_colors[(int)ThemeColor::WindowEdgeDark] = SDL_Color(0, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::WindowEdgeLightSelected] = SDL_Color(196, 196, 196, 64);
        theme.m_colors[(int)ThemeColor::WindowEdgeDarkSelected] = SDL_Color(64, 64, 64, 255);

        theme.m_colors[(int)ThemeColor::SearchTitleBack] = SDL_Color(16, 32, 64, 255);
        theme.m_colors[(int)ThemeColor::SearchTextBack] = SDL_Color(8, 16, 32, 255);
        theme.m_colors[(int)ThemeColor::SearchTextBackSelected] = SDL_Color(10, 20, 40, 255);
        theme.m_colors[(int)ThemeColor::SearchTitle] = SDL_Color(240, 220, 60, 255);
        theme.m_colors[(int)ThemeColor::SearchText] = SDL_Color(128, 200, 128, 255);


        theme.m_colors[(int)ThemeColor::TextGeneral] = SDL_Color(0, 0, 0, 255);
        theme.m_colors[(int)ThemeColor::TextOperator] = SDL_Color(0, 0, 255, 255);
        theme.m_colors[(int)ThemeColor::TextString] = SDL_Color(64, 48, 16, 255);
        theme.m_colors[(int)ThemeColor::TextLabel] = SDL_Color(64, 64, 255, 255);
        theme.m_colors[(int)ThemeColor::TextComment] = SDL_Color(0, 96, 0, 255);
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
    Log("START");

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

    CreateShellProcess();

    // create 
    CreateSettings();
    Cpu6502::Startup();
    NetworkManager::Startup();
    FontRenderer::Startup();
    IconRenderer::Startup();
    WindowManager::Startup();
    SourceFileManager::Startup();
    SourceFileManager::Instance().RestoreFilesFromSettings();

    auto& wm = WindowManager::Instance();

    // Create some test windows
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    auto win = new WindowTree(Recti{ 300,100,640,512 });
    win->m_layout.m_locked = true;
    wm.AddWindowTree(win);
    wm.SetActiveTree(win);

    CreateMenus();
    AddCustomEvents();

    auto themeName = Settings::Instance().GetString(SETTING_ACTIVE_THEME);
    SelectTheme(themeName.c_str());

    bool showLines = Settings::Instance().GetBool(SETTING_SHOW_LINES);
    bool showBytes = Settings::Instance().GetBool(SETTING_SHOW_BYTES);
    ShowLines(showLines);
    ShowBytes(showBytes);

    bool viceZoomMode = Settings::Instance().GetBool(SETTING_VICE_ZOOM_MODE);
    int viceZoomLevel = Settings::Instance().GetInt(SETTING_VICE_ZOOM_LEVEL);
    Vice_SetZoomLevel(viceZoomLevel);
    Vice_SetZoomMode(viceZoomMode);

    wm.LoadWindowLayout();

    auto& nm = NetworkManager::Instance();
    nm.Message(new NMS_SetIP("192.168.50.15"));

    ViceBridge::Startup();

    bool isPal = Settings::Instance().GetBool(SETTING_VICE_IS_PAL);

    auto emulator = [this]()
        {
            std::vector<const char*> args;
            args.push_back("mad64u.exe");
            args.push_back("-soundbufsize");
            args.push_back("200");
            args.push_back("-soundfragsize");
            args.push_back("3");
            args.push_back(m_vice_pal ? "-pal" : "-ntsc");
            args.push_back("-VICIImodel");
            args.push_back("6569");
            main_program((int)args.size(), (char**)args.data());
        };
    std::thread(emulator).detach();

    Vice_EnablePAL(isPal);

    SDL_Event e;
    while (!m_quit)
    {
        while (!m_quit && SDL_PollEvent(&e))
        {
            if (e.type == CustomEvent_Timer)
            {
                WindowManager::Instance().Tick();
                ProcessShellOutput();
                wm.SendDeferredMessages();
            }
            else
                wm.HandleEvent(&e);
        }
        ViceBridge::Instance().ExecuteMadCmds();

        wm.Paint();

        SourceFileManager::Instance().LoadRequestedFiles(true);
        SourceFileManager::Instance().UpdateDisassemblies();
    }

    DestroyShellProcess();
    SourceFileManager::Instance().SaveFilesToSettings();
    WindowManager::Instance().SaveWindowLayout();
    Settings::Instance().Save();

    SourceFileManager::Shutdown();
    WindowManager::Shutdown();
    IconRenderer::Shutdown();
    FontRenderer::Shutdown();
    NetworkManager::Shutdown();

    return 1;
}

void Application::SelectTheme(const char *themeName)
{
    auto theme = FindTheme(themeName);
    if (!theme)
    {
        theme = m_themes[1];
    }
    if (theme)
    {
        m_activeTheme = theme;
        m_themeMenu->m_name = std::format("Theme : {}", themeName);
        Settings::Instance().SetString(SETTING_ACTIVE_THEME, m_activeTheme->m_name);
        Settings::Instance().Save();
        WindowManager::Instance().LayoutMenu();
    }
}

void Application::Vice_SetZoomMode(bool fixed)
{
    int level = Settings::Instance().GetInt(SETTING_VICE_ZOOM_LEVEL);
    std::string result = fixed ? std::format("Fixed {}x", level) : "Scale To Fit";
    m_viceZoomModeMenu->m_name = std::format("Zoom Mode : {}", result);
    Settings::Instance().SetBool(SETTING_VICE_ZOOM_MODE, fixed);
    Settings::Instance().Save();
    WindowManager::Instance().LayoutMenu();
    m_vice_zoomFixed = fixed;
}

void Application::Vice_SetZoomLevel(int zoom)
{
    // force fixed if your gonna set the zoom level
    m_vice_zoomLevel = zoom;
    Settings::Instance().SetInt(SETTING_VICE_ZOOM_LEVEL, zoom);
    Vice_SetZoomMode(true);
}

void Application::Vice_EnablePAL(bool enable)
{
    auto cmd = new VBC_SetVideoStandard;
    cmd->m_palMode = enable;
    ViceBridge::Instance().SendMad2Vice(cmd);

    std::string mode = enable ? "PAL" : "NTSC";
    m_viceVideoStandardMenu->m_name = std::format("Video Standard : {}", mode);
    Settings::Instance().SetInt(SETTING_VICE_IS_PAL, enable);
    Settings::Instance().Save();
    WindowManager::Instance().LayoutMenu();
}

void Application::ShowLines(bool enable)
{
    std::string result = enable ? "Show" : "Hide";
    m_showLinesMenu->m_name = std::format("Line Numbers : {}", result);
    Settings::Instance().SetBool(SETTING_SHOW_LINES, enable);
    Settings::Instance().Save();
    WindowManager::Instance().LayoutMenu();
}

void Application::ShowBytes(bool enable)
{
    std::string result = enable ? "Show" : "Hide";
    m_showBytesMenu->m_name = std::format("Code Bytes : {}", result);
    Settings::Instance().SetBool(SETTING_SHOW_BYTES, enable);
    Settings::Instance().Save();
    WindowManager::Instance().LayoutMenu();
}


void Application::DestroyShellProcess()
{
    SDL_DestroyProcess(m_shellProcess);
}

void Application::CreateShellProcess()
{
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_CMDLINE_STRING, "cmd.exe");
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    m_shellProcess = SDL_CreateProcessWithProperties(props);
    m_shellInput = SDL_GetProcessInput(m_shellProcess);
    m_shellOutput = SDL_GetProcessOutput(m_shellProcess);
}

bool Application::SendShellCommand(const std::string& command)
{
    if (!m_shellInput)
        return false;

    std::string line = command;
    line += '\n';

    const size_t written =
        SDL_WriteIO(m_shellInput, line.data(), line.size());

    return written == line.size();
}

void Application::AddShellWatcher(OutputWatcherFunc watchFunc)
{
    m_watcherLock.lock();
    m_watchers.push_back(watchFunc);
    m_watcherLock.unlock();
}

void Application::AddShellWatcherOnce(OutputWatcherFunc watchFunc)
{
    m_watcherLock.lock();
    m_watchersOnce.push_back(watchFunc);
    m_watcherLock.unlock();
}

void Application::ProcessShellOutput()
{
    if (!m_shellOutput)
        return;

    char buffer[1024];
    size_t bytesRead = SDL_ReadIO(m_shellOutput, buffer, sizeof(buffer));
    for (int i = 0; i < bytesRead; i++)
    {
        if (buffer[i] == '\n')
        {
            Log(LogGroup::Build, m_shellOutputLine);

            m_watcherLock.lock();
            std::string line = m_shellOutputLine;
            for (auto& watch : m_watchers)
            {
                watch(line);
            }
            std::erase_if(m_watchersOnce, [line](auto watcher)->bool {return watcher(line); });
            m_watcherLock.unlock();
 
            m_shellOutputLine.clear();
        }
        else if (buffer[i] != '\t' && buffer[i] != '\r')
        {
            m_shellOutputLine.push_back(buffer[i]);
        }
    }
}

void Application::StartupNetwork()
{
}

void Application::PostTestNetwork()
{
}

void Application::ShutdownNetwork()
{
}


bool StrEqualNoCase(const std::string& a, const std::string& b)
{
    return std::ranges::equal(a, b,
        [](unsigned char lhs, unsigned char rhs)
        {
            return std::tolower(lhs) == std::tolower(rhs);
        });
}

