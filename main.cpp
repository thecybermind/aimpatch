/*
	This DLL removes the big ad at the bottom of the new AIM 7.5 window.

	It does this by simply finding the main AIM window and hiding all of its direct children.

	The DLL itself is loaded by pretending to be a normal DLL loaded by AIM (jgtktlk.dll), which
	happens to be one of the few which is loaded dynamically at runtime with GetProcAddress and
	does not have any resources. This DLL then loads the original DLL which has been renamed
	to ~jgtktlk.dll.

	In order to prevent the need for this DLL to export all of the functions that ~jgtktlk.dll
	does and route the calls to the original DLL, we hook GetProcAddress. Every time
	GetProcAddress is called with our DLL as its target, the hook blocks it and calls
	GetProcAddress with the original DLL as its target, completely bypassing this DLL when
	functions in the original need to be called.

	Incidentally, jgtktlk.dll is only loaded at sign-in, and is totally unloaded at sign-out,
	which obviously breaks things terribly with our GetProcAddress hook. I could have unloaded
	the GetProcAddress hook on DLL_PROCESS_DETACH as a simple fix, but I opted to hook
	FreeLibrary and block the call if the desired DLL to unload is this one. Blocking
	FreeLibrary is easier and keeps our DLL loaded the whole time which actually works out
	better, I think.

	Finally, when this DLL is loaded, it simply starts a thread which first finds the AIM window
	and then continuously hides its children.

	In short, getting our DLL loaded by default without the need for external DLL injectors took
	much more work than hiding the ad.
	*/
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "detours.h"

//tell linker to link in detours.lib
#pragma comment(lib, "detours")

//hook GetProcAddress
FARPROC WINAPI Detour_GetProcAddress(HMODULE, LPCSTR);
DETOUR_TRAMPOLINE(FARPROC WINAPI Trampoline_GetProcAddress(HMODULE, LPCSTR), GetProcAddress);

//hook FreeLibrary
void WINAPI Detour_FreeLibrary(HMODULE);
DETOUR_TRAMPOLINE(void WINAPI Trampoline_FreeLibrary(HMODULE), FreeLibrary);

//our worker thread's main function
DWORD WINAPI ThreadInit(LPVOID);

HMODULE g_mydll = NULL;			//this DLL's handle
HMODULE g_origdll = NULL;		//original DLL's handle
DWORD g_threadid = 0;			//worker thread id
HANDLE g_threadhandle = NULL;	//worker thread handle

//entry point, initialize detours
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
		g_mydll = hinstDLL;	//save our DLL's handle for later

		//start detour for GetProcAddress
		if (!DetourFunctionWithTrampoline((PBYTE)Trampoline_GetProcAddress, (PBYTE)Detour_GetProcAddress))
			return FALSE;
		//start detour for FreeLibrary
		if (!DetourFunctionWithTrampoline((PBYTE)Trampoline_FreeLibrary, (PBYTE)Detour_FreeLibrary))
			return FALSE;
	}

	return TRUE;
}

//GetProcAddress hook
FARPROC WINAPI Detour_GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
	//get original return value to pass on
	FARPROC orig = Trampoline_GetProcAddress(hModule, lpProcName);
	if (!g_mydll)
		return orig;

	//if this is first time we've been called, load original DLL
	if (!g_origdll)
		g_origdll = LoadLibraryA("~jgtktlk.dll");
	if (!g_origdll)	//if load failed
		return orig;

	//if looking for a function in our DLL, redirect to original DLL
	if (hModule == g_mydll)
		orig = Trampoline_GetProcAddress(g_origdll, lpProcName);

	//set up worker thread (only if stored id and handle are 0)
	if (g_threadhandle == NULL && g_threadid == 0)
		g_threadhandle = CreateThread(NULL, 0, ThreadInit, NULL, 0, &g_threadid);

	return orig;
}

//FreeLibrary hook
void WINAPI Detour_FreeLibrary(HMODULE hModule) {
	//only block FreeLibrary if it's being called on this DLL
	if (hModule != g_mydll)
		Trampoline_FreeLibrary(hModule);
}

//our main worker thread
DWORD WINAPI ThreadInit(LPVOID lpParam) {
	HWND aimWindow = NULL;
	HWND kidWindow = NULL;

	//find main aim window (either sign-in window or contact list)
	while (aimWindow == NULL) {
		Sleep(100);
		aimWindow = FindWindowEx(NULL, NULL, "__oxFrame.class__", "AIM");
		if (!aimWindow)
			aimWindow = FindWindowEx(NULL, NULL, "__oxFrame.class__", "AIM (Offline)");
	}

	//kill children
	while (1) {
		Sleep(100);
		kidWindow = FindWindowEx(aimWindow, kidWindow, "__oxFrame.class__", NULL);
		if (kidWindow)
			ShowWindow(kidWindow, SW_HIDE);
	}

	return 0;
}