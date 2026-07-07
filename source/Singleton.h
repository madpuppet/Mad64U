#pragma once

template<class T>
class Singleton
{
	static T* s_instance;

public:
	Singleton()
	{
		s_instance = (T*)this;
	}

	~Singleton()
	{
		s_instance = NULL;
	}

	static T* Startup()
	{
		return new(T);
	}

	static void Shutdown()
	{
		delete(&Instance());
	}

	static bool Exists()
	{
		return (s_instance != NULL);
	}

	static T& Instance()
	{
		return *s_instance;
	}
};

template<class T> T* Singleton<T>::s_instance = NULL;
