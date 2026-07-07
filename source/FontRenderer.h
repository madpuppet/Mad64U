#pragma once

#include <unordered_map>
#include <list>
#include "SDL3/SDL.h"
#include "Singleton.h"

template <typename MyType>
class TLRUList
{
public:
    void Add(MyType* Item)
    {
        // If already tracked, just make it most recently used.
        auto Found = Lookup.find(Item);
        if (Found != Lookup.end())
        {
            Touch(Item);
            return;
        }

        List.push_back(Item);
        Lookup.emplace(Item, std::prev(List.end()));
    }

    void Touch(MyType* Item)
    {
        auto Found = Lookup.find(Item);

        // Move to back = most recently used.
        List.splice(List.end(), List, Found->second);
        Found->second = std::prev(List.end());
    }

    MyType* Pop()
    {
        // Take least recently used from front.
        auto It = List.begin();
        MyType* Item = *It;

        // Move it to back, making it most recently used.
        List.splice(List.end(), List, It);
        Lookup[Item] = std::prev(List.end());

        return Item;
    }

    template <typename ConditionType>
    void Flush(ConditionType Condition)
    {
        for (auto It = List.begin(); It != List.end(); )
        {
            MyType* Item = *It;

            if (Condition(Item))
            {
                Lookup.erase(Item);
                It = List.erase(It);
            }
            else
            {
                ++It;
            }
        }
    }

    void Remove(MyType* Item)
    {
        auto Found = Lookup.find(Item);
        if (Found == Lookup.end())
        {
            return;
        }

        List.erase(Found->second);
        Lookup.erase(Found);
    }

    template <typename ConditionType>
    void RemoveIf(ConditionType Condition)
    {
        for (auto It = List.begin(); It != List.end(); )
        {
            MyType* Item = *It;

            if (Condition(Item))
            {
                Lookup.erase(Item);
                It = List.erase(It);
            }
            else
            {
                ++It;
            }
        }
    }

    bool Contains(MyType* Item) const
    {
        return Lookup.find(Item) != Lookup.end();
    }

    bool IsEmpty() const
    {
        return List.empty();
    }

    size_t Num() const
    {
        return List.size();
    }

private:
    std::list<MyType*> List;
    std::unordered_map<MyType*, typename std::list<MyType*>::iterator> Lookup;
};



class FontRenderer : public Singleton<FontRenderer>
{
public:
    enum FontIDX
    {
        UIFont,
        TextFont
    };

    struct CachedString
    {
        SDL_Renderer* renderer;
        SDL_FRect rect;
        size_t hash;
        std::string str;
        SDL_Texture* tex;
        int w, h;
        std::list<CachedString*>::iterator it;
    };

    void RenderText(SDL_Renderer *renderer, const std::string& str, const SDL_Color& col, int x, int y, FontIDX fontIdx, SDL_Rect *outputQuad, bool bCalcSizeOnly);

    // split render into 2 phases, allowing you to do other things with the render rectangle
    // do not retain the cached string over long periods as it could be reused eventually (after 256 renders)
    struct CachedString *PrepareRender(SDL_Renderer* renderer, const std::string& str, int x, int y, FontIDX fontIdx);
    void Render(CachedString *cs, const SDL_Color& col);
    void RenderAt(CachedString* cs, const SDL_Color& col, int x, int y);

    // flush any cached strings for this renderer as we will be destroying the renderer
    void FlushRenderer(SDL_Renderer *renderer);

protected:
    size_t Hash(SDL_Renderer* renderer, const std::string& str, FontIDX fontIdx);
    TLRUList<CachedString> m_lru;
    std::unordered_map<size_t, CachedString*> m_map;
};
