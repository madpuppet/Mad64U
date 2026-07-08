#include "common.h"
#include "SourceFileManager.h"
#include "WindowManager.h"
#include "FontRenderer.h"
#include "Application.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define SOURCE_FILE_LINE_HEIGHT 16

SourceFileRenderer::SourceFileRenderer(SourceFile* file) : m_sourceFile(file)
{
    if (file->m_path.empty())
    {
        m_name = "<new-file>";
    }
    else
    {
        std::filesystem::path path = file->m_path;
        m_name = path.filename().string();
    }
}

void SourceFileRenderer::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    auto& tp = Application::Instance().GetThemeProperties();
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);

    SDL_FRect body = m_area.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    auto& fr = FontRenderer::Instance();

    SDL_Color col;
    if (window == this)
        col = tp.m_colors[(int)ThemeColor::SourceTextSelected];
    else
        col = tp.m_colors[(int)ThemeColor::SourceText];

    int x = m_area.x;
    int y = m_area.y;
    for (auto line : m_sourceFile->m_lines)
    {
        if (y + SOURCE_FILE_LINE_HEIGHT >= m_area.y)
            fr.RenderText(renderer, line->m_chars, col, x, y, FontRenderer::TextFont, nullptr, false);

        y += SOURCE_FILE_LINE_HEIGHT;
        if (y > m_area.y + m_area.h)
            break;
    }
}

void SourceFileRenderer::Close()
{
    SDL_MessageBoxButtonData buttons[] =
    {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "Cancel" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
        { 0, 0, "No" }
    };

    SDL_MessageBoxData data =
    {
        SDL_MESSAGEBOX_INFORMATION,
        nullptr,               // parent window
        "Confirm",
        "Do you want to save changes?",
        SDL_arraysize(buttons),
        buttons,
        nullptr                // color scheme
    };

    int buttonid = -1;

    if (m_sourceFile->m_modified)
    {
        if (SDL_ShowMessageBox(&data, &buttonid))
        {
            switch (buttonid)
            {
                case 2:
                    break;

                case 1:
                    SourceFileManager::Instance().SaveFile(m_sourceFile);
                    SourceFileManager::Instance().CloseFile(m_sourceFile);
                    break;

                case 0:
                    SourceFileManager::Instance().CloseFile(m_sourceFile);
                    break;
            }
        }
    }
}

bool SourceFileManager::RenameFile(SourceFile* file, const std::string& path)
{
    file->m_path = path;

    std::filesystem::path fspath = path;
    std::string name = fspath.filename().string();

    for (auto fr : m_sourceFileRenderers)
    {
        if (fr->m_sourceFile == file)
        {
            fr->m_name = name;
        }
    }

    WindowManager::Instance().LayoutWindows();
    return true;
}


bool SourceFileManager::NewFile(const std::string& path)
{
    auto sourceFile = new SourceFile(path);
    m_sourceFiles.push_back(sourceFile);

    auto sourceFileRenderer = new SourceFileRenderer(sourceFile);
    m_sourceFileRenderers.push_back(sourceFileRenderer);
    WindowManager::Instance().GetActiveWindowLayout()->AddWindow(sourceFileRenderer);
    WindowManager::Instance().GetActiveWindowTree()->LayoutWindows();
    return true;
}

SourceFile* SourceFileManager::GetFileFromWindow(WindowBase* window)
{
    for (auto sfr : m_sourceFileRenderers)
    {
        if (sfr == window)
        {
            return sfr->m_sourceFile;
        }
    }
    return nullptr;
}


bool SourceFileManager::LoadFile(const std::string& path)
{
    std::ifstream file(path);

    if (!file)
    {
        return false;
    }

    std::string line;
    auto sourceFile = new SourceFile(path);
    while (std::getline(file, line))
    {
        // Strip trailing CR if reading a Windows file on Linux/macOS.
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        SourceLine* sl = new SourceLine;
        sl->m_chars = std::move(line);
        sourceFile->m_lines.push_back(sl);
    }
    m_sourceFiles.push_back(sourceFile);

    auto sourceFileRenderer = new SourceFileRenderer(sourceFile);
    m_sourceFileRenderers.push_back(sourceFileRenderer);
    WindowManager::Instance().GetActiveWindowLayout()->AddWindow(sourceFileRenderer);
    WindowManager::Instance().GetActiveWindowTree()->LayoutWindows();

    return true;
}

bool SourceFileManager::SaveFile(SourceFile* file)
{
    return true;
}

bool SourceFileManager::CloseFile(SourceFile* file)
{
    return true;
}
