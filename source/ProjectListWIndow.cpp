#include "common.h"
#include "ProjectListWindow.h"
#include "SourceFileManager.h"
#include "FontRenderer.h"
#include "Application.h"
#include <filesystem>
#include <unordered_map>
#include <vector>

#define LINE_HEIGHT 24

struct FolderContents
{
    std::filesystem::path folder;
    std::vector<std::filesystem::path> files;
};

void ProjectListWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    auto& sm = SourceFileManager::Instance();
    auto& fr = FontRenderer::Instance();
    auto& tp = Application::Instance().GetThemeProperties();

    // draw background
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    // build list of files by folder
    std::vector<FolderContents*> folders;
    for (auto file : sm.m_sourceFiles)
    {
        std::filesystem::path p = file->m_path;
        std::filesystem::path dir = p.parent_path();
        std::filesystem::path filename = p.filename();

        auto it = std::find_if(folders.begin(), folders.end(), [dir](const auto& item)->bool { return item->folder == dir; });
        FolderContents* contents;
        if (it == folders.end())
        {
            contents = new FolderContents;
            contents->folder = dir;
            folders.push_back(contents);
        }
        else
        {
            contents = *it;
        }
        contents->files.push_back(filename.string());
    }

    int x = m_clientArea.x;
    int y = m_clientArea.y;
    for (auto contents : folders)
    {
        fr.RenderText(renderer, contents->folder.string(), tp.m_colors[(int)ThemeColor::TextOperator], x, y, FontType::Text);
        y += LINE_HEIGHT;

        for (const auto &file : contents->files)
        {
            fr.RenderText(renderer, file.string(), tp.m_colors[(int)ThemeColor::TextGeneral], x + 16, y, FontType::Text);
            y += LINE_HEIGHT;
        }
    }

    // clean up
    for (auto contents : folders)
        delete contents;
}

ProjectListWindow::ProjectListWindow()
{
    m_name = "Project List";
}

ProjectListWindow::~ProjectListWindow()
{
}

void ProjectListWindow::Close()
{
    WindowManager::Instance().RemoveWindow(this);
    delete this;
}

bool ProjectListWindow::HandleEvent(SDL_Event* e)
{
    return true;
}

bool ProjectListWindow::Tick()
{
    return false;
}


