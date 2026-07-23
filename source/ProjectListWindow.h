#pragma once

#include "WindowBase.h"
#include <filesystem>

extern std::vector<std::filesystem::path> FindC64Outputs(std::filesystem::path outPath);

class ProjectListWindow : public WindowBase
{
public:
    ProjectListWindow();
    ~ProjectListWindow();

    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;
    bool Tick() override;
    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);
    void MessageChild(WindowLayout *layout, struct WindowMessageStruct& msg) override;
    void IconPressed(Icons icon, int itemIdx);

protected:
    void RebuildFolders();
    struct ProjectLine
    {
        enum Type
        {
            Folder,
            Image_D64,
            Image_PRG,
            Image_CRT,
            Source_C,
            Source_ASM,
            Source_S,
            Text
        } m_type;
        class SourceFile *m_file;
        std::filesystem::path m_display;
        std::filesystem::path m_path;
    };
    std::vector<ProjectLine> m_lines;
};

