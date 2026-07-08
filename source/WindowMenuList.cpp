#include "common.h"
#include "WindowMenuList.h"
#include "FontRenderer.h"
#include "WindowQueries.h"

void WindowMenu::Paint(SDL_Renderer* renderer, int highlightItemIdx)
{
    SDL_Color col{ 255,255,255,128 };
    SDL_FRect area{ (float)m_area.x, (float)m_area.y, (float)m_area.w, (float)m_area.h };
    auto& fr = FontRenderer::Instance();
    fr.RenderText(renderer, m_name, col, m_area.x, m_area.y, FontRenderer::UIFont, nullptr, false);

    if (m_open)
    {
        SDL_Color itemCol{ 0,0,0,255 };
        SDL_FRect dropDownBody{ (float)m_dropDownArea.x, (float)m_dropDownArea.y, (float)m_dropDownArea.w, (float)m_dropDownArea.h };
        SDL_SetRenderDrawColor(renderer, 255, 240, 220, 255);
        SDL_RenderFillRect(renderer, &dropDownBody);
        SDL_SetRenderDrawColor(renderer, 255, 240, 220, 128);
        SDL_RenderLine(renderer, dropDownBody.x, dropDownBody.y, dropDownBody.x, dropDownBody.y + dropDownBody.h);
        SDL_RenderLine(renderer, dropDownBody.x, dropDownBody.y + dropDownBody.h, dropDownBody.x + dropDownBody.w, dropDownBody.y + dropDownBody.h);
        SDL_SetRenderDrawColor(renderer, 255, 240, 220, 128);
        SDL_RenderLine(renderer, dropDownBody.x + dropDownBody.w, dropDownBody.y, dropDownBody.x + dropDownBody.w, dropDownBody.y + dropDownBody.h);

        for (size_t i = 0; i < m_items.size(); i++)
        {
            auto item = m_items[i];
            if (i == highlightItemIdx)
            {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                SDL_FRect itemBody{ (float)m_dropDownArea.x, (float)item->m_area.y, (float)m_dropDownArea.w, (float)item->m_area.h };
                SDL_RenderFillRect(renderer, &itemBody);
            }

            fr.RenderText(renderer, item->m_name, itemCol, item->m_area.x, item->m_area.y, FontRenderer::UIFont, nullptr, false);
        }
    }
}

void WindowMenu::Layout(SDL_Renderer* renderer, const Vec2i& pos)
{
    auto& fr = FontRenderer::Instance();
    fr.CalcTextArea(renderer, m_name, pos, FontRenderer::UIFont, m_area);
    m_dropDownArea = m_area;
    int dx = m_dropDownArea.x + m_dropDownArea.w;
    int dy = m_dropDownArea.y + m_dropDownArea.h;

    Vec2i p = pos;
    p.y = m_area.y + m_area.h + 8;
    for (auto item : m_items)
    {
        fr.CalcTextArea(renderer, item->m_name, p, FontRenderer::UIFont, item->m_area);

        int x2 = item->m_area.x + item->m_area.w;
        int y2 = item->m_area.y + item->m_area.h;
        if (x2 > dx)
            dx = x2;
        if (y2 > dy)
            dy = y2;
        p.y = item->m_area.y + 24;
    }

    m_dropDownArea.x = m_dropDownArea.x - 8;
    m_dropDownArea.w = dx - m_dropDownArea.x + 16;
    m_dropDownArea.y = m_area.y + m_area.h;
    m_dropDownArea.h = dy - m_dropDownArea.y + 8;
}

void WindowMenuList::Paint(const WindowMenuQuery& highlight)
{
    if (m_tree)
    {
        for (size_t m = 0; m < m_menus.size(); m++)
        {
            auto menu = m_menus[m];
            menu->Paint(m_tree->m_renderer, highlight.m_menuIdx == m ? highlight.m_menuItemIdx : -1);

        }
    }
}

void WindowMenuList::Layout(WindowTree *tree)
{
    if (tree != m_tree)
    {
        m_tree = tree;
        Vec2i pos{ 16,2 };
        for (auto menu : m_menus)
        {
            menu->Layout(tree->m_renderer, pos);
            pos.x = menu->m_area.x + menu->m_area.w + 32;
        }
    }
}

bool WindowMenuList::CheckForMenu(WindowTree *tree, int x, int y, WindowMenuQuery& query)
{
    query.Reset();
    if (m_tree == tree)
    {
        query.m_tree = tree;
        for (size_t i = 0; i < m_menus.size(); i++)
        {
            auto menu = m_menus[i];
            if (menu->m_area.Contains(x, y))
            {
                query.m_menuIdx = (int)i;
                query.m_menuItemIdx = -1;
                return true;
            }
            else if (menu->m_open && menu->m_dropDownArea.Contains(x, y))
            {
                query.m_menuIdx = (int)i;
                for (size_t ii = 0; ii < menu->m_items.size(); ii++)
                {
                    auto item = menu->m_items[ii];
                    if (y < (item->m_area.y + item->m_area.h))
                    {
                        query.m_menuItemIdx = (int)ii;
                        return true;
                    }
                }
                return true;
            }
        }
    }
    return false;
}



