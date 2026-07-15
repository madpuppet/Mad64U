#include "common.h"
#include "Settings.h"
#include <filesystem>
#include <fstream>
#include <sstream>

Settings::Settings()
{
}

Settings::~Settings()
{
}

void Settings::Load()
{
    std::filesystem::path path(SDL_GetPrefPath("Madpuppet", "Mad64U"));
    path /= "settings.txt";

    std::ifstream file(path);
    if (!file)
    {
        Log("Cannot open settings file: %s\n", path.c_str());
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream stream(line);
        std::string token;
        std::string name;
        std::vector<std::string> vals;

        if (stream >> name)
        {
            auto item = FindOrCreate(name.c_str());
            item->m_tokens.clear();
            while (stream >> token)
                item->m_tokens.push_back(std::move(token));
        }
    }
}

void Settings::Save()
{
    std::filesystem::path path(SDL_GetPrefPath("Madpuppet", "Mad64U"));
    path /= "settings.txt";

    std::ofstream file(path);
    if (!file)
    {
        Log("Cannot save settings file: %s\n", path.c_str());
        return;
    }

    for (const auto &item : m_settings)
    {
        file << item.second->m_name;
        for (const auto& val : item.second->m_tokens)
            file << ' ' << val;
        file << '\n';
    }
}

Settings::SettingItem* Settings::Find(const char* name)
{
    u64 hash = HashU8(0, name);
    const auto& it = m_settings.find(hash);
    return (it != m_settings.end()) ? it->second : nullptr;
}

Settings::SettingItem* Settings::FindOrCreate(const char* name)
{
    u64 hash = HashU8(0, name);
    const auto& it = m_settings.find(hash);
    if (it != m_settings.end())
        return it->second;
    auto item = new SettingItem(name);
    m_settings[hash] = item;
    return item;
}

bool Settings::Exists(const char* name)
{
    auto item = Find(name);
    return item != nullptr;
}

bool Settings::GetBool(const char* name)
{
    auto item = Find(name);
    if (item && item->m_tokens.size() > 0)
    {
        const auto& val = item->m_tokens[0];
        return val == "1" || val == "true";
    }
    return false;
}

int Settings::GetInt(const char* name)
{
    auto item = Find(name);
    if (item && item->m_tokens.size() > 0)
    {
        const auto& val = item->m_tokens[0];
        int value = 0;
        std::from_chars(val.data(), val.data() + val.size(), value);
        return value;
    }
    return 0;
}

float Settings::GetFloat(const char* name)
{
    auto item = Find(name);
    if (item && item->m_tokens.size() > 0)
    {
        const auto& val = item->m_tokens[0];
        float value = 0;
        std::from_chars(val.data(), val.data() + val.size(), value);
        return value;
    }
    return 0.0f;
}

SDL_Color Settings::GetColor(const char* name)
{
    auto item = Find(name);
    if (item && item->m_tokens.size() == 4)
    {
        const auto& val_r = item->m_tokens[0];
        const auto& val_g = item->m_tokens[1];
        const auto& val_b = item->m_tokens[2];
        const auto& val_a = item->m_tokens[3];
        SDL_Color col{ 255,255,255,255 };
        std::from_chars(val_r.data(), val_r.data() + val_r.size(), col.r);
        std::from_chars(val_g.data(), val_g.data() + val_g.size(), col.g);
        std::from_chars(val_b.data(), val_b.data() + val_b.size(), col.b);
        std::from_chars(val_a.data(), val_a.data() + val_a.size(), col.a);
        return col;
    }
    return SDL_Color{ 0,0,0,0 };
}

std::string Settings::GetString(const char* name)
{
    auto item = Find(name);
    if (item && item->m_tokens.size() > 0)
    {
        return item->m_tokens[0];
    }
    return "";
}

std::vector<std::string>Settings::GetStringList(const char* name)
{
    auto item = Find(name);
    if (item && item->m_tokens.size() > 0)
    {
        return item->m_tokens;
    }
    std::vector<std::string> emptyList;
    return emptyList;
}

std::vector<std::string> Settings::GetItemsStartWith(const char* name)
{
    std::vector<std::string> names;
    for (auto &item : m_settings)
    {
        if (item.second->m_name.starts_with(name))
            names.push_back(item.second->m_name);
    }
    return names;
}


void Settings::SetBool(const char* name, bool val)
{
    auto item = FindOrCreate(name);
    item->m_tokens.clear();
    item->m_tokens.push_back(std::format("{}", val ? 1 : 0));
}

void Settings::SetInt(const char* name, int val)
{
    auto item = FindOrCreate(name);
    item->m_tokens.clear();
    item->m_tokens.push_back(std::format("{}", val));
}

void Settings::SetFloat(const char* name, float val)
{
    auto item = FindOrCreate(name);
    item->m_tokens.clear();
    item->m_tokens.push_back(std::format("{}", val));
}

void Settings::SetColor(const char* name, const SDL_Color& val)
{
    auto item = FindOrCreate(name);
    item->m_tokens.clear();
    item->m_tokens.push_back(std::format("{} {} {} {}", val.r,val.g,val.b,val.a));
}

void Settings::SetString(const char* name, const std::string& val)
{
    auto item = FindOrCreate(name);
    item->m_tokens.clear();
    item->m_tokens.push_back(val);
}

void Settings::SetStringList(const char* name, const std::vector<std::string>& val)
{
    auto item = FindOrCreate(name);
    item->m_tokens.clear();
    item->m_tokens = val;
}
