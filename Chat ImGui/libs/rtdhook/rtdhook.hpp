#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>			// VirtualProtect
#include <vector>

#ifdef _DEBUG
#include <assert.h>
#endif

#pragma warning(disable : 6387) // warning C6387: _Param_(4) may be 0: this does not adhere to the specification for the function "VirtualProtect"

class rtdhook {
	typedef unsigned char byte_t;
private:
	bool		mIsEnabled = false;

	size_t		mHookSize;

	uintptr_t	mDetourAddress;
	uintptr_t	mHookAddress;

	uintptr_t	mTrampoline;
	uintptr_t	mOriginalPrologue;

	inline void setEnabled(bool enabled) { mIsEnabled = enabled; }

public:
	/**
	 * @brief Конструктор
	 * @details В конструкторе записываются необходимые данные, выделяется область для трамплина и оригинального пролога, функция хукается позднее через метод install
	 * @param hook_address Адрес функции
	 * @param detour_function Функция, которая будет вызываться вмето оригинального
	 * @param prologue_size Размер пролога
	 */
	template <typename T> rtdhook(uintptr_t hook_address, T(*detour_function), size_t prologue_size = 5)
	{
		mHookSize = prologue_size;
		mDetourAddress = reinterpret_cast<uintptr_t>(detour_function);
		mHookAddress = hook_address;
		mOriginalPrologue = reinterpret_cast<uintptr_t>(new byte_t[mHookSize]);
		mTrampoline = reinterpret_cast<uintptr_t>(new byte_t[mHookSize + 5]);

		memcpy((void*)mOriginalPrologue, (void*)mHookAddress, mHookSize);
		memcpy((void*)mTrampoline, (void*)mOriginalPrologue, mHookSize);

		*reinterpret_cast<byte_t*>(mTrampoline + mHookSize + 0) = 0xE9;
		*reinterpret_cast<uintptr_t*>(mTrampoline + mHookSize + 1) = mHookAddress - mTrampoline - mHookSize + (mHookSize - 5);
	}
	
	/**
	 * @brief Деструктор
	 * @details В деструкторе снимается установленный хук, а после затираются указатели
	 */
	~rtdhook()
	{
		uninstall();

		delete[](reinterpret_cast<byte_t*>(mOriginalPrologue));
		delete[](reinterpret_cast<byte_t*>(mTrampoline));
	}

	/**
	 * @brief Установка хука на функцию
	 */
	void install()
	{
		if (isEnabled()) return;

		DWORD old_protection;
		VirtualProtect((LPVOID)mHookAddress, mHookSize, PAGE_EXECUTE_READWRITE, &old_protection);

		*reinterpret_cast<byte_t*>(mHookAddress) = 0xE9;
		*reinterpret_cast<uintptr_t*>(mHookAddress + 1) = mDetourAddress - mHookAddress - 5;

		memset((void*)(mHookAddress + 5), 0x90, mHookSize - 5);

		VirtualProtect((LPVOID)mHookAddress, mHookSize, old_protection, nullptr);

		setEnabled(true);
	}

	/**
	 * @brief Снятие хука с функции
	 */
	void uninstall()
	{
		// TODO: Current realisation will break other hooks installed by this address

		if (!isEnabled()) return;

		DWORD old_protection;
		VirtualProtect((LPVOID)mHookAddress, mHookSize, PAGE_EXECUTE_READWRITE, &old_protection);

		memcpy((void*)mHookAddress, (void*)mOriginalPrologue, mHookSize);

		VirtualProtect((LPVOID)mHookAddress, mHookSize, old_protection, nullptr);

		setEnabled(false);
	}

	inline bool isEnabled() { return mIsEnabled; }
	inline uintptr_t getHookAddress() { return mHookAddress; }
	inline uintptr_t getTrampoline() { return mTrampoline; }
};

class rtdhook_call {
	typedef unsigned char byte_t;
private:
	bool		mIsEnabled = false;

	uintptr_t	mDetourAddress;
	uintptr_t	mHookAddress;
	uintptr_t	mHookedFunctionAddress;

	inline void setEnabled(bool enabled) { mIsEnabled = enabled; }

public:
	/**
	 * @brief Конструктор
	 * @details В конструкторе записываются необходимые данные, хук происходит позже через метод install()
	 * @param hook_address Адрес функции
	 * @param detour_function Функция, которая будет вызываться вместо оригинального
	 */
	template <typename T> rtdhook_call(uintptr_t hook_address, T(*detour_function))
	{
#ifdef _DEBUG
		assert(*reinterpret_cast<byte_t*>(hook_address) == 0xE8);
#endif

		mDetourAddress = reinterpret_cast<uintptr_t>(detour_function);
		mHookAddress = hook_address;
		mHookedFunctionAddress = *reinterpret_cast<uintptr_t*>(mHookAddress + 1);
	}

	/**
	 * @brief Деструктор
	 * @details В деструкторе снимается установленный хук
	 */
	~rtdhook_call()
	{
		uninstall();
	}

	/**
	 * @brief Установка хука на вызов функции
	 */
	void install()
	{
		if (isEnabled()) return;

		DWORD old_protection;
		VirtualProtect((LPVOID)mHookAddress, 5, PAGE_EXECUTE_READWRITE, &old_protection);

		*reinterpret_cast<uintptr_t*>(mHookAddress + 1) = mDetourAddress - mHookAddress - 5;

		VirtualProtect((LPVOID)mHookAddress, 5, old_protection, nullptr);

		setEnabled(true);
	}

	/**
	 * @brief Снятие хука на вызов функции
	 */
	void uninstall()
	{
		// TODO: Current realisation will break other hooks installed by this address

		if (!isEnabled()) return;

		DWORD old_protection;
		VirtualProtect((LPVOID)mHookAddress, 5, PAGE_EXECUTE_READWRITE, &old_protection);

		*reinterpret_cast<uintptr_t*>(mHookAddress + 1) = mHookedFunctionAddress;

		VirtualProtect((LPVOID)mHookAddress, 5, old_protection, nullptr);

		setEnabled(false);
	}

	inline bool isEnabled() { return mIsEnabled; }
	inline uintptr_t getHookAddress() { return mHookAddress; }
	inline uintptr_t getHookedFunctionAddress() { return mHookedFunctionAddress + mHookAddress + 5; }
};

class rtdhook_vmt {
	typedef struct { int id; uintptr_t original; } vmt_method_t;
private:
	uintptr_t					mVmtAddress;
	std::vector<vmt_method_t>	mHookedList;

	/**
	 * @brief Перезапись указателя на виртуальный метод
	 * @param method_id Индекс виртуального метода
	 * @param detour_function Функция, которая будет вызываться вмето оригинального метода
	 */
	void rewrite_pointer(int method_id, uintptr_t detour_function)
	{
		DWORD old_protection;
		uintptr_t* vmt = *reinterpret_cast<uintptr_t**>(mVmtAddress);
		VirtualProtect(vmt + method_id, 4, PAGE_EXECUTE_READWRITE, &old_protection);
		*reinterpret_cast<uintptr_t*>(vmt + method_id) = detour_function;
		VirtualProtect(vmt + method_id, 4, old_protection, nullptr);
	}
public:
	/**
	 * @brief Конструктор
	 * @details В конструкторе записывается адрес виртуальной таблицы, методы заменяются позднее через метод install
	 * @param vmt_address Адрес виртуальной таблицы
	 */
	rtdhook_vmt(uintptr_t vmt_address) :
		mVmtAddress(vmt_address)
	{ /* Empty constructor */ }

	/**
	 * @brief Деструктор
	 */
	~rtdhook_vmt()
	{
		for (auto ptr = mHookedList.begin(); ptr < mHookedList.end();)
		{
			rewrite_pointer(ptr->id, ptr->original);
			ptr = mHookedList.erase(ptr);
		}
	}

	/**
	 * @brief Установка хука на виртуальный метод
	 * @param method_id Индекс виртуального метода
	 * @param detour_function Функция, которая будет вызываться вмето оригинального метода
	 * @return Успешность установки хука. Неуспех возможен только если на этот метод уже стоит хук
	 */
	template <typename T> bool install(int method_id, T(*detour_function))
	{
		if (is_installed(method_id)) return false;

		mHookedList.push_back({ method_id, *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t**>(mVmtAddress) + method_id) });

		rewrite_pointer(method_id, reinterpret_cast<uintptr_t>(detour_function));

		return true;
	}

	/**
	 * @brief Снятие хука на виртуальный метод
	 * @param method_id Индекс виртуального метода
	 * @return Успешность снятия хука. Неуспех возможен только если на этот метод не был установлен хук
	 */
	bool uninstall(int method_id)
	{
		// TODO: Current realisation will break other hooks installed by this method_id

		for (size_t i = 0; i < mHookedList.size(); i++) {
			if (mHookedList[i].id == method_id)
			{
				rewrite_pointer(method_id, mHookedList[i].original);
				mHookedList.erase(mHookedList.begin() + i);
				return true;
			}
		}
		return false;
	}

	/**
	 * @brief Установлен ли хук на виртуальный метод
	 * @param method_id Индекс виртуального метода
	 */
	bool is_installed(int method_id)
	{
		for (auto& method : mHookedList) {
			if (method.id == method_id)
				return true;
		}
		return false;
	}

	/**
	 * @brief Получение указателя на оригинальный виртуальный метод
	 * @param method_id Индекс виртуального метода
	 */
	uintptr_t getOriginalMethod(int method_id)
	{
		for (auto& method : mHookedList)
		{
			if (method.id == method_id)
				return method.original;
		}

		uintptr_t* vmt = *reinterpret_cast<uintptr_t**>(mVmtAddress);
		return *reinterpret_cast<uintptr_t*>(vmt + method_id);
	}
};

#pragma warning(default : 6387)
