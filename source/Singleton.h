#pragma once

template<class T>
class Singleton
{
	static T* s_instance;

public:
	Singleton()
	{
		Assert(s_instance == nullptr, "Singleton already exists!\n");
		s_instance = (T*)this;
	}

	~Singleton()
	{
		s_instance = NULL;
	}

	// Prevent copying
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;

	// Prevent moving
	Singleton(Singleton&&) = delete;
	Singleton& operator=(Singleton&&) = delete;

	static T* Startup()
	{
		return new T;
	}

	static void Shutdown()
	{
		delete s_instance;
	}

	static bool Exists()
	{
		return (s_instance != NULL);
	}

	static T& Instance()
	{
		Assert(s_instance != nullptr, "Singleton does not exist!\n");
		return *s_instance;
	}
};

template<class T> T* Singleton<T>::s_instance = NULL;

