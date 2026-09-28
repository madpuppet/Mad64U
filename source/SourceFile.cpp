#include "common.h"
#include "SourceFile.h"
#include "SourceFileCmdBuffer.h"
#include <filesystem>
#include "FontRenderer.h"
#include "SourceFileManager.h"

static int s_uniqueID = 1;

SourceFile::SourceFile(const std::string& path) : m_path(path)
{
    m_fileID = s_uniqueID++;
    m_cmdBuffer = new SourceFileCmdBuffer;

    std::filesystem::path p = path;
    std::filesystem::path ext = p.extension();
    if (ext == ".asm")
        m_sourceType = SourceType::Asm;
    else if (ext == ".c")
        m_sourceType = SourceType::C;
    else if (ext == ".s")
        m_sourceType = SourceType::S;
    else
        m_sourceType = SourceType::Text;
}

SourceFile::~SourceFile()
{
    for (auto line : m_lines)
        delete line;
    delete m_cmdBuffer;
}

void SourceLine::BuildFragments(SDL_Renderer* renderer, SourceType sourceType)
{
    auto& sm = SourceFileManager::Instance();
    auto& fr = FontRenderer::Instance();

    int charWidth = fr.GetCharactorWidth(FontType::Text, ' ');
    int x = 0;
    int y = 0;
    m_fragmentsDirty = false;
    char frag[1024];
    m_fragments.clear();
    int i = 0;
    int charCount = (int)m_chars.size();
    FragmentType ft = FragmentType::General;
    int chIdx = 0;
    while (i < charCount)
    {
        // skip white space
        while (i < charCount)
        {
            u32 ch = (u32)m_chars[i];     //TODO: UTF8 process
            if (ch == '\t')
            {
                chIdx = ((chIdx / TAB_SIZE) + 1) * TAB_SIZE;
                i++;
            }
            else if (ch == ' ')
            {
                chIdx++;
                i++;
            }
            else break;
        }

        // gather a fragment
        int c = 0;
        bool alted = false;
        frag[0] = 0;

        if (ft == FragmentType::Operator)
            ft = FragmentType::General;
        while (true)
        {
            if (i == charCount)
            {
                if (c > 0 && ft != FragmentType::String && ft != FragmentType::Comment && sm.IsKeyword(sourceType, frag))
                {
                    ft = FragmentType::Operator;
                }
                break;
            }

            char ch = m_chars[i];
            if (ch == '\t' || ch == ' ')
            {
                if (ft != FragmentType::String && ft != FragmentType::Comment && sm.IsKeyword(sourceType, frag))
                {
                    ft = FragmentType::Operator;
                }
                break;
            }

            if (c > 0 && ft != FragmentType::Comment && ft != FragmentType::String)
            {
                char last = tolower(frag[c - 1]);
                char next = tolower(ch);
                bool lastIsAlphaNumeric = (last >= 'a' && last <= 'z') || (last >= '0' && last <= '9');
                bool nextIsAlphaNumeric = (next >= 'a' && next <= 'z') || (next >= '0' && next <= '9');
                if (sourceType == SourceType::Asm || sourceType == SourceType::S)
                {
                    lastIsAlphaNumeric |= last == '.';
                    nextIsAlphaNumeric |= next == '.';
                }

                if (lastIsAlphaNumeric != nextIsAlphaNumeric)
                {
                    if (sm.IsKeyword(sourceType, frag))
                        ft = FragmentType::Operator;
                    break;
                }
            }

            frag[c++] = (u8)ch;
            frag[c] = 0;
            i++;

            // next quote won't start/end a string
            alted = false;
            if (ch == '\\')
            {
                alted = true;
            }

            if (ft != FragmentType::Comment && ch == '"' && !alted)
            {
                if (ft == FragmentType::String)
                    break;
                ft = FragmentType::String;
            }

            // check for double char tokens
            if (ft != FragmentType::Comment && ft != FragmentType::String && c == 2)
            {
                char oneFrag[2];
                oneFrag[0] = frag[0];
                oneFrag[1] = 0;

                if (sm.IsKeyword(sourceType, frag))
                {
                    ft = FragmentType::Operator;
                    break;
                }
                else if (sm.IsKeyword(sourceType, oneFrag))
                {
                    frag[1] = 0;
                    --i;
                    --c;
                    ft = FragmentType::Operator;
                    break;
                }
            }
        }

        if ((sourceType == SourceType::S) && strcmp(frag, ";") == 0)
            ft = FragmentType::Comment;

        if ((sourceType == SourceType::C || sourceType == SourceType::Asm) && strcmp(frag, "//") == 0)
            ft = FragmentType::Comment;

        SourceLineRenderFragment fragment;
        fragment.m_fragType = ft;
        fragment.m_chars = frag;
        x = chIdx * charWidth;
        fr.CalcTextArea(renderer, fragment.m_chars, Vec2i(x, y), FontType::Text, fragment.m_area);
        x = fragment.m_area.x + fragment.m_area.w;
        m_fragments.emplace_back(fragment);
        chIdx += c;
    }
}

int SourceLine::CharIndexToColumn(int idx)
{
    int out = 0;
    for (int i = 0; i < idx && i < m_chars.size(); i++)
    {
        if (m_chars[i] == '\t')
            out += TAB_SIZE;
        else
            out++;
    }
    return out;
}
