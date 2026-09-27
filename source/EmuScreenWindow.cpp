#include "common.h"
#include "EmuScreenWindow.h"
#include "WindowLayout.h"

EmuScreenWindow::EmuScreenWindow()
{
    m_name = "EmuScreen";
}

EmuScreenWindow::~EmuScreenWindow()
{
}

void EmuScreenWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
}

bool EmuScreenWindow::HandleEvent(SDL_Event* e)
{
    return false;
}

void EmuScreenWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("EMUSCREEN");
}

void EmuScreenWindow::UpdateTexture(int w, int h)
{
}

bool EmuScreenWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "EMUSCREEN")
        return false;

    idx++;
    auto win = new EmuScreenWindow();
    layout->m_tabs.push_back(win);
    return true;
}


