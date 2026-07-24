#include "common.h"
#include "ProjectListWindow.h"
#include "SourceFileManager.h"
#include "FontRenderer.h"
#include "Application.h"
#include "SourceFileWindow.h"
#include <filesystem>
#include <unordered_map>
#include <vector>

void ProjectListWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    m_clientContentSize.y = (int)m_lines.size() * LINE_HEIGHT + LINE_HEIGHT;

    auto& sm = SourceFileManager::Instance();
    auto& fr = FontRenderer::Instance();
    auto& tp = Application::Instance().GetThemeProperties();
    auto& ir = IconRenderer::Instance();
    auto& wm = WindowManager::Instance();
    auto& highlight = wm.GetWindowHighlightQuery();

    // draw background
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    // render tab highlighted
    if (highlight.m_highlight == WindowHighlightType::ProjectListFile || highlight.m_highlight == WindowHighlightType::ProjectListIcon)
    {
        SDL_FRect area = highlight.m_area.AsSDLFRect();
        tp.SetRenderDrawColor(renderer, ThemeColor::HighlightArea);
        SDL_RenderFillRect(renderer, &area);
    }

    int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
    int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT, (int)m_lines.size());
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;
    int x = xBase;
    int y = yBase + firstLine * LINE_HEIGHT;
    int w = 0;
    for (int i = firstLine; i < lastLine; i++)
    {
        auto line = m_lines[i];
        switch (line.m_type)
        {
            case ProjectLine::Folder:
            {
                fr.RenderText(renderer, line.m_display.string(), tp.m_colors[(int)ThemeColor::TextOperator], x, y, FontType::Text);
                fr.RenderText(renderer, line.m_path.string(), tp.m_colors[(int)ThemeColor::TextComment], x + 300, y, FontType::Text);
                y += LINE_HEIGHT;
            }
            break;

            case ProjectLine::Image_D64:
            case ProjectLine::Image_PRG:
            case ProjectLine::Image_CRT:
            {
                ir.DrawIcon(renderer, Icons::Deploy, x + 16, y + 10);
                ir.DrawIcon(renderer, Icons::Run, x + 32, y + 10);
                fr.RenderText(renderer, line.m_display.string(), tp.m_colors[(int)ThemeColor::TextLabel], x + 52, y, FontType::Text);
                y += LINE_HEIGHT;
            }
            break;

            case ProjectLine::Source_C:
            case ProjectLine::Source_ASM:
            {
                ir.DrawIcon(renderer, Icons::Build, x + 16, y + 10);
                fr.RenderText(renderer, line.m_display.string(), tp.m_colors[(int)ThemeColor::TextGeneral], x + 52, y, FontType::Text);
                y += LINE_HEIGHT;
            }
            break;

            case ProjectLine::Source_S:
            case ProjectLine::Text:
            {
                fr.RenderText(renderer, line.m_display.string(), tp.m_colors[(int)ThemeColor::TextString], x + 52, y, FontType::Text);
                y += LINE_HEIGHT;
            }
            break;
        }
    }
}

ProjectListWindow::ProjectListWindow()
{
    m_name = "Project List";
    RebuildFolders();
}

ProjectListWindow::~ProjectListWindow()
{
}

bool ProjectListWindow::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            auto& selected = WindowManager::Instance().GetWindowSelectionQuery();
            if (selected.m_highlight == WindowHighlightType::ProjectListIcon && selected.m_window == this)
            {
                auto& line = m_lines[selected.m_id];
                switch (selected.m_projectFiles.m_icon)
                {
                    case Icons::Build:
                        if (line.m_file)
                        {
                            SourceFileManager::Instance().Compile(line.m_file);
                        }
                        break;

                    case Icons::Run:
                        if (line.m_file)
                        {
                            SourceFileManager::Instance().Run(line.m_file);
                        }
                        else
                        {
                            SourceFileManager::Instance().Run(line.m_path);
                        }
                        break;

                    case Icons::Deploy:
                        SourceFileManager::Instance().Deploy(line.m_path);
                        break;
                }
            }
            else if (selected.m_highlight == WindowHighlightType::ProjectListFile)
            {
                // check if file has a window
                auto& line = m_lines[selected.m_id];
                switch (line.m_type)
                {
                    case ProjectLine::Folder:
                        break;

                    case ProjectLine::Image_CRT:
                    case ProjectLine::Image_PRG:
                    case ProjectLine::Image_D64:
                        break;

                    case ProjectLine::Text:
                    case ProjectLine::Source_S:
                    case ProjectLine::Source_C:
                    case ProjectLine::Source_ASM:
                    {
                        if (line.m_file == nullptr)
                        {
                            std::vector<std::string> paths;
                            paths.push_back(line.m_path.string());
                            SourceFileManager::Instance().RequestLoadFiles(paths);
                            SourceFileManager::Instance().LoadRequestedFiles(true);
                        }
                        else
                        {
                            WindowMessageStruct msgFFW;
                            msgFFW.m_type = WindowMessage::Query_FindFileWindow;
                            msgFFW.m_sourceFile = line.m_file;
                            msgFFW.m_flags = WMF_EarlyOut | WMF_Window;
                            WindowManager::Instance().Message(msgFFW);
                            if (msgFFW.m_response > 0)
                            {
                                msgFFW.m_layout->ActivateWindow(msgFFW.m_window);
                                msgFFW.m_tree->m_dirty = true;
                                return true;
                            }

                            // check if there is a locked layout to open in
                            WindowMessageStruct msgFLL;
                            msgFLL.m_type = WindowMessage::Query_FindLockedLayout;
                            msgFLL.m_flags = WMF_EarlyOut | WMF_Layout;
                            WindowManager::Instance().Message(msgFLL);
                            if (msgFLL.m_response > 0)
                            {
                                msgFLL.m_layout->m_tabs.push_back(new SourceFileWindow(line.m_file));
                                msgFLL.m_layout->m_activeTab = (int)msgFLL.m_layout->m_tabs.size() - 1;
                            }
                            else
                            {
                                WindowManager::Instance().AddWindow(new SourceFileWindow(line.m_file));
                            }
                        }
                        WindowManager::Instance().LayoutWindows();
                    }
                    return true;
                }
            }
        }
        break;
    }



    return WindowBase::HandleEvent(e);
}

bool ProjectListWindow::Tick()
{
    return false;
}

void ProjectListWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("PROJECT");
}

bool ProjectListWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "PROJECT")
        return false;

    idx++;
    auto win = new ProjectListWindow;
    layout->m_tabs.push_back(win);
    return true;
}

void ProjectListWindow::RebuildFolders()
{
    auto& sm = SourceFileManager::Instance();

    // sort into folder groups
    struct ProjectFolder
    {
        std::filesystem::path m_path;
        std::vector<SourceFile *> m_files;
        std::vector<std::filesystem::path> m_outputs;
    };
    std::vector<ProjectFolder> folders;
    for (auto file : sm.m_sourceFiles)
    {
        std::filesystem::path p = file->m_path;
        std::filesystem::path dir = p.parent_path();

        if (dir.filename() == "out")
            continue;

        auto it = std::find_if(folders.begin(), folders.end(), [dir](const auto& item)->bool { return item.m_path == dir; });
        ProjectFolder* pf;
        if (it == folders.end())
        {
            ProjectFolder folder;
            folder.m_path = dir;
            folder.m_outputs = FindC64Outputs(dir/"out");
            folders.push_back(folder);
            pf = &folders.back();
        }
        else
        {
            pf = &(*it);
        }
        pf->m_files.push_back(file);
    }

    // flatten this
    m_lines.clear();
    for (auto& folder : folders)
    {
        ProjectLine pl;
        pl.m_type = ProjectLine::Folder;
        pl.m_file = nullptr;
        pl.m_path = folder.m_path;
        pl.m_display = folder.m_path.stem();
        m_lines.push_back(pl);

        for (auto& file : folder.m_files)
        {
            ProjectLine pl;
            std::filesystem::path path = file->m_path;
            auto ext = path.extension();
            if (ext == ".s")
                pl.m_type = ProjectLine::Source_S;
            else if (ext == ".asm")
                pl.m_type = ProjectLine::Source_ASM;
            else if (ext == ".c")
                pl.m_type = ProjectLine::Source_C;
            else
                pl.m_type = ProjectLine::Text;
            pl.m_file = file;
            pl.m_path = file->m_path;
            pl.m_display = pl.m_path.filename();
            m_lines.push_back(pl);
        }

        for (auto& output : folder.m_outputs)
        {
            ProjectLine pl;
            auto ext = output.extension();
            if (ext == ".prg")
                pl.m_type = ProjectLine::Image_PRG;
            else if (ext == ".d64")
                pl.m_type = ProjectLine::Image_D64;
            else if (ext == ".crt")
                pl.m_type = ProjectLine::Image_CRT;
            else if (ext == ".s")
                pl.m_type = ProjectLine::Source_S;
            pl.m_file = nullptr;
            pl.m_path = output;
            pl.m_display = output.filename();
            m_lines.push_back(pl);
        }
    }
}

void ProjectListWindow::MessageChild(WindowLayout *layout, struct WindowMessageStruct& msg)
{
    switch (msg.m_type)
    {
        case WindowMessage::File_Added:
        case WindowMessage::File_Deleted:
        case WindowMessage::File_Compiled:
            RebuildFolders();
            break;

        case WindowMessage::Query_Highlight:
        {
            auto& fr = FontRenderer::Instance();
            auto& ir = IconRenderer::Instance();
            auto query = (WindowHighlightQuery*)msg.m_query;
            int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
            int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT, (int)m_lines.size());
            int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
            int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;
            int mouseLine = (msg.m_y - yBase) / LINE_HEIGHT;
            int x = xBase;
            int y = yBase + mouseLine * LINE_HEIGHT;
            if (mouseLine >= 0 && mouseLine < m_lines.size())
            {
                auto testIcon = [this, layout, query, &msg](Icons icon, int x, int y, int line) -> bool
                    {
                        Recti area = IconRenderer::Instance().CalcIconArea(icon, x, y);
                        if (area.Contains(msg.m_x, msg.m_y))
                        {
                            query->m_area = area;
                            query->m_highlight = WindowHighlightType::ProjectListIcon;
                            query->m_id = line;
                            query->m_projectFiles.m_icon = icon;
                            query->m_tree = msg.m_tree;
                            query->m_layout = layout;
                            query->m_window = this;
                            msg.m_response++;
                            return true;
                        }
                        return false;
                    };

                auto line = m_lines[mouseLine];
                switch (line.m_type)
                {
                    case ProjectLine::Folder:
                    {
                        std::string stem = line.m_path.stem().string();
                        Recti folderArea;
                        fr.CalcTextArea(msg.m_tree->m_renderer, stem, { x,y }, FontType::Text, folderArea);
                        if (folderArea.Contains(msg.m_x, msg.m_y))
                        {
                            query->m_area = folderArea;
                            query->m_highlight = WindowHighlightType::ProjectListFile;
                            query->m_id = mouseLine;
                            msg.m_response++;
                            return;
                        }
                    }
                    break;

                    case ProjectLine::Image_CRT:
                    case ProjectLine::Image_PRG:
                    case ProjectLine::Image_D64:
                    {
                        if (testIcon(Icons::Deploy, x + 16, y + 10, mouseLine))
                            return;

                        if (testIcon(Icons::Run, x + 32, y + 10, mouseLine))
                            return;

                        Recti fileArea;
                        fr.CalcTextArea(msg.m_tree->m_renderer, line.m_path.string(), { x + 52, y }, FontType::Text, fileArea);
                        if (fileArea.Contains(msg.m_x, msg.m_y))
                        {
                            query->m_area = fileArea;
                            query->m_highlight = WindowHighlightType::ProjectListFile;
                            query->m_id = mouseLine;
                            query->m_tree = msg.m_tree;
                            query->m_layout = layout;
                            query->m_window = this;
                            msg.m_response++;
                            return;
                        }
                    }
                    break;

                    case ProjectLine::Source_C:
                    case ProjectLine::Source_ASM:
                    {
                        if (testIcon(Icons::Build, x + 16, y + 10, mouseLine))
                            return;

                        Recti fileArea;
                        fr.CalcTextArea(msg.m_tree->m_renderer, line.m_path.string(), { x + 36, y }, FontType::Text, fileArea);
                        if (fileArea.Contains(msg.m_x, msg.m_y))
                        {
                            query->m_area = fileArea;
                            query->m_highlight = WindowHighlightType::ProjectListFile;
                            query->m_id = mouseLine;
                            query->m_tree = msg.m_tree;
                            query->m_layout = layout;
                            query->m_window = this;
                            msg.m_response++;
                            return;
                        }
                    }
                    break;

                    case ProjectLine::Source_S:
                    case ProjectLine::Text:
                    {
                        Recti fileArea;
                        fr.CalcTextArea(msg.m_tree->m_renderer, line.m_path.string(), { x + 16, y }, FontType::Text, fileArea);
                        if (fileArea.Contains(msg.m_x, msg.m_y))
                        {
                            query->m_area = fileArea;
                            query->m_highlight = WindowHighlightType::ProjectListFile;
                            query->m_id = mouseLine;
                            query->m_tree = msg.m_tree;
                            query->m_layout = layout;
                            query->m_window = this;
                            msg.m_response++;
                            return;
                        }
                    }
                    break;
                }
            }

            if (m_clientArea.Contains(msg.m_x, msg.m_y))
            {
                msg.m_response++;
                query->m_area = m_clientArea;
                query->m_highlight = WindowHighlightType::ClientArea;
                query->m_tree = msg.m_tree;
                query->m_layout = layout;
                query->m_window = this;
                return;
            }
        }
        break;
    }
}


