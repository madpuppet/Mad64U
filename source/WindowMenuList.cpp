#include "common.h"
#include "WindowMenuList.h"
#include "FontRenderer.h"
#include "WindowQueries.h"

#define MENU_LINE_HEIGHT 24
#define MENU_ITEM_MARGIN 8

void WindowMenu::Paint(SDL_Renderer* renderer, int highlightItemIdx)
{
    int subMenuIdx = (highlightItemIdx >> 8) & 255;
    int menuIdx = highlightItemIdx & 255;

    SDL_Color col{ 255,255,255,128 };
    SDL_FRect area{ (float)m_area.x, (float)m_area.y, (float)m_area.w, (float)m_area.h };
    auto& fr = FontRenderer::Instance();
    fr.RenderText(renderer, m_name, col, m_area.x, m_area.y, FontType::UI);

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
            if (i == menuIdx)
            {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                SDL_FRect itemBody{ dropDownBody.x, (float)item->m_area.y, dropDownBody.w, (float)item->m_area.h };
                SDL_RenderFillRect(renderer, &itemBody);
            }
            fr.RenderText(renderer, item->m_name, itemCol, item->m_area.x, item->m_area.y, FontType::UI);

            if (item->m_subMenus.size() > 0)
                IconRenderer::Instance().DrawIcon(renderer, Icons::Submenu, item->m_area.x + item->m_area.w + 16, item->m_area.y + 10);
        }

        for (size_t i = 0; i < m_items.size(); i++)
        {
            auto item = m_items[i];
            if (item->m_subMenuOpen)
            {
                SDL_FRect subMenuBody{ (float)item->m_subArea.x, (float)item->m_subArea.y, (float)item->m_subArea.w, (float)item->m_subArea.h };
                SDL_SetRenderDrawColor(renderer, 255, 240, 220, 255);
                SDL_RenderFillRect(renderer, &subMenuBody);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderRect(renderer, &subMenuBody);

                for (size_t ii = 0; ii < item->m_subMenus.size(); ii++)
                {
                    auto subItem = item->m_subMenus[ii];
                    if (ii == subMenuIdx)
                    {
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_FRect subItemBody{ subMenuBody.x, (float)subItem->m_area.y, subMenuBody.w, (float)subItem->m_area.h };
                        SDL_RenderFillRect(renderer, &subItemBody);
                    }
                    fr.RenderText(renderer, subItem->m_name, itemCol, subItem->m_area.x, subItem->m_area.y, FontType::UI);
                }
                break;
            }
        }
    }
}

void WindowMenu::Layout(SDL_Renderer* renderer, const Vec2i &pos)
{
    auto& fr = FontRenderer::Instance();
    fr.CalcTextArea(renderer, m_name, pos, FontType::UI, m_area);

    m_dropDownArea = Recti{ m_area.x, m_area.y + m_area.h, 0, (int)m_items.size() * MENU_LINE_HEIGHT + 16};

    Vec2i p = pos;
    p.x += MENU_ITEM_MARGIN;
    p.y += MENU_LINE_HEIGHT + MENU_ITEM_MARGIN;
    for (auto item : m_items)
    {
        fr.CalcTextArea(renderer, item->m_name, p, FontType::UI, item->m_area);
        m_dropDownArea.w = Max(m_dropDownArea.w, item->m_area.x + item->m_area.w - m_dropDownArea.x);
        p.y += MENU_LINE_HEIGHT;

        if (!item->m_subMenus.empty())
        {
            Vec2i sub_p{ item->m_area.x + item->m_area.w + MENU_ITEM_MARGIN, item->m_area.y - MENU_ITEM_MARGIN };

            item->m_subArea.x = sub_p.x;
            item->m_subArea.y = sub_p.y;
            item->m_subArea.w = 0;
            item->m_subArea.h = (int)item->m_subMenus.size() * MENU_LINE_HEIGHT + 16;

            sub_p.x += MENU_ITEM_MARGIN;
            sub_p.y += MENU_ITEM_MARGIN;
            for (auto subitem : item->m_subMenus)
            {
                fr.CalcTextArea(renderer, subitem->m_name, sub_p, FontType::UI, subitem->m_area);
                item->m_subArea.w = Max(item->m_subArea.w, subitem->m_area.x + subitem->m_area.w - item->m_subArea.x);
                sub_p.y += MENU_LINE_HEIGHT;
            }

            item->m_subArea.w += MENU_ITEM_MARGIN;
        }

        m_dropDownArea.w += MENU_ITEM_MARGIN;
    }
}

void WindowMenuList::Paint(const WindowMenuQuery& highlight)
{
    if (m_tree)
    {
        int highlightIdx = (highlight.m_menuItemIdx & 255) | ((highlight.m_subMenuItemIdx & 255) << 8);
        for (size_t m = 0; m < m_menus.size(); m++)
        {
            auto menu = m_menus[m];
            menu->Paint(m_tree->m_renderer, highlight.m_menuIdx == m ? highlightIdx : -1);
        }
    }
}

void WindowMenuList::Layout(WindowTree *tree)
{
    m_tree = tree;
    Vec2i pos{ 16,2 };
    for (auto menu : m_menus)
    {
        menu->Layout(tree->m_renderer, pos);
        pos.x = menu->m_area.x + menu->m_area.w + 32;
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
            else if (menu->m_open)
            {
                // check against any open submenus first
                for (size_t ii = 0; ii < menu->m_items.size(); ii++)
                {
                    auto item = menu->m_items[ii];
                    if (item->m_subMenuOpen && item->m_subArea.Contains(x, y))
                    {
                        query.m_menuIdx = (int)i;
                        for (size_t s = 0; s < item->m_subMenus.size(); s++)
                        {
                            auto subItem = item->m_subMenus[s];
                            if (y < (subItem->m_area.y + subItem->m_area.h))
                            {
                                query.m_menuItemIdx = (int)ii;
                                query.m_subMenuItemIdx = (int)s;
                                return true;
                            }
                        }
                    }
                }

                // check against the menu items
                if (menu->m_dropDownArea.Contains(x, y))
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
                }
                return true;
            }
        }
    }
    return false;
}



