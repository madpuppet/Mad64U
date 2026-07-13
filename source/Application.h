#pragma once

#include "WindowTree.h"
#include "FontRenderer.h"
#include "IconRenderer.h"
#include "WindowManager.h"
#include "Singleton.h"
#include <array>

enum class Theme
{
    Dark,
    Light,
    MAX
};
constexpr size_t NumThemes = static_cast<size_t>(Theme::MAX);

enum class ThemeColor
{
    TitleBar,
    MenuText,
    MenuItemText,
    MenuItemBackground,
    MenuItemBackgroundSelected,
    WindowBackground,
    WindowClientEmpty,
    TabBackground,
    TabText,
    TabTextSelected,
    TabHighlight,
    SourceBackground,
    SourceBackgroundSelected,
    ScrollBarBackground,
    ScrollBar,
    ScrollBarSelected,
    Cursor,
    TextHighlight,

    TextGeneral,
    TextOperator,
    TextComment,

    MAX
};
constexpr size_t NumThemeColor = static_cast<size_t>(ThemeColor::MAX);

struct ThemeProperties
{
    std::string m_name;
    std::array<SDL_Color, NumThemeColor> m_colors;

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

    void SelectTheme(Theme theme);
    ThemeProperties& GetThemeProperties() { return m_themes[(int)m_activeTheme]; }

protected:
    void InitDefaultSettings();
    void CreateMenus();
    void CreateThemes();
    void AddTimerEvent();

    bool m_quit = false;
    Theme m_activeTheme = Theme::Light;
    std::array<ThemeProperties, NumThemes> m_themes;
};

