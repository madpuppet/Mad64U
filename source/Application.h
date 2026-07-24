#pragma once

#include "WindowTree.h"
#include "FontRenderer.h"
#include "IconRenderer.h"
#include "WindowManager.h"
#include "Singleton.h"
#include <array>

enum class ThemeColor
{
    TitleBar,
    MenuText,
    MenuItemText,
    MenuItemBackground,
    MenuItemBackgroundSelected,
    WindowBackground,
    WindowClientEmpty,
    LayoutSplit,
    TabBackground,
    TabText,
    TabTextModified,
    TabTextSelected,
    TabHighlight,
    SourceBackground,
    SourceBackgroundSelected,
    ScrollBarBackground,
    ScrollBar,
    ScrollBarSelected,
    Cursor,
    TextHighlight,
    HighlightArea,
    WindowEdgeLight,
    WindowEdgeDark,
    WindowEdgeLightSelected,
    WindowEdgeDarkSelected,

    SearchTitleBack,
    SearchTextBack,
    SearchTitle,
    SearchText,

    TextGeneral,
    TextOperator,
    TextString,
    TextLabel,
    TextComment,

    MAX
};
constexpr size_t NumThemeColor = static_cast<size_t>(ThemeColor::MAX);

struct ThemeProperties
{
    bool m_system = false;      // system themes don't get written to properties
    std::string m_name;
    std::array<SDL_Color, NumThemeColor> m_colors;

    const SDL_Color &Col(ThemeColor col)
    {
        return m_colors[(int)col];
    }
    void SetRenderDrawColor(SDL_Renderer *renderer, ThemeColor tc)
    {
        auto& c = m_colors[(int)tc];
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    }
};

class Application : public Singleton<Application>
{
public:
    int Run();

    void Quit() { m_quit = true; }

    void SelectTheme(const char *theme);
    ThemeProperties& GetThemeProperties() { return *m_activeTheme; }

    bool SendShellCommand(const std::string& command);

    void PostTestNetwork();

protected:
    void CreateShellProcess();
    void ProcessShellOutput();
    void DestroyShellProcess();

    void StartupNetwork();
    void ShutdownNetwork();

    void CreateSettings();
    void CreateMenus();
    void CreateThemes();
    void AddCustomEvents();
    void LoadThemesFromSettings();
    void SaveThemesToSettings();
    ThemeProperties* FindTheme(const char* name);

    bool m_quit = false;
    ThemeProperties* m_activeTheme;
    std::vector<ThemeProperties*> m_themes;

    SDL_Process* m_shellProcess;
    SDL_IOStream* m_shellInput;
    SDL_IOStream* m_shellOutput;
    std::string m_shellOutputLine;
};

