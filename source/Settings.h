#pragma once

#include "Singleton.h"
#include <unordered_map>
#include <mutex>

#define SETTING_FILES "files"
#define SETTING_THEME "theme"
#define SETTING_RENDERER "renderer"

class Settings : public Singleton<Settings>
{
public:
	Settings();
	~Settings();

	void Load();
	void Save();

	bool GetBool(const char* name);
	int GetInt(const char* name);
	float GetFloat(const char* name);
	SDL_Color GetColor(const char* name);
	std::string GetString(const char* name);
	std::vector<std::string> GetStringList(const char* name);

	void SetBool(const char* name, bool val);
	void SetInt(const char* name, int val);
	void SetFloat(const char* name, float val);
	void SetColor(const char* name, const SDL_Color &val);
	void SetString(const char* name, const std::string &val);
	void SetStringList(const char* name, const std::vector<std::string> &val);

private:
	struct SettingItem
	{
		SettingItem(const std::string& name) : m_name(name) {}

		std::string m_name;
		std::vector<std::string> m_tokens;
	};

	std::unordered_map<u64, SettingItem*> m_settings;
	std::mutex m_lock;

	SettingItem* Find(const char* name);
	SettingItem* FindOrCreate(const char* name);
};
