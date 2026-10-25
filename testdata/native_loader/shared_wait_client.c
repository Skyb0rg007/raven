#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef LONG (WINAPI *nt_wait)(HANDLE, BOOLEAN, LARGE_INTEGER *);

static BOOL contains(const WCHAR *text, const WCHAR *part)
{
	for (; *text; ++text) {
		unsigned index = 0;
		while (part[index] && text[index] == part[index]) ++index;
		if (!part[index]) return TRUE;
	}
	return FALSE;
}

static void append(WCHAR *text, const WCHAR *suffix)
{
	while (*text) ++text;
	do { *text++ = *suffix; } while (*suffix++);
}

int raven_entry(void)
{
	const WCHAR *arguments = GetCommandLineW();
	if (contains(arguments, L"--never")) {
		const BOOL shared = contains(arguments, L"--never-shared");
		HANDLE event = CreateEventW(NULL, FALSE, FALSE, shared ? L"Local\\RavenUnsignaledDeadline" : NULL);
		if (!event) return 92;
		OutputDebugStringA(shared ? "Shared wait entered" : "Private wait entered");
		return WaitForSingleObject(event, INFINITE) == WAIT_OBJECT_0 ? 90 : 91;
	}
	if (contains(arguments, L"--child")) {
		WCHAR name[160], ready_name[170];
		if (!GetEnvironmentVariableW(L"RAVEN_SHARED_WAIT", name, 160)) return 1;
		ready_name[0] = 0; append(ready_name, name); append(ready_name, L"-ready");
		HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, name);
		HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, ready_name);
		if (!event || !ready || !SetEvent(ready)) return 2;
		if (contains(arguments, L"--child-nt")) {
			nt_wait wait = (nt_wait)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtWaitForSingleObject");
			if (!wait || wait(event, FALSE, NULL)) return 3;
		} else if (WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0) return 4;
		CloseHandle(ready); CloseHandle(event);
		return 0;
	}
	WCHAR executable[1024];
	if (!GetModuleFileNameW(NULL, executable, 1024)) return 5;
	for (unsigned mode = 0; mode < 2; ++mode) {
		WCHAR name[160] = L"Local\\RavenSharedWait-", ready_name[170], command[1100] = L"\"";
		const WCHAR hex[] = L"0123456789abcdef";
		const DWORD pid = GetCurrentProcessId();
		for (unsigned index = 0; index < 8; ++index) {
			WCHAR digit[] = {hex[(pid >> ((7 - index) * 4)) & 15], 0};
			append(name, digit);
		}
		append(name, mode ? L"-nt" : L"-win32");
		ready_name[0] = 0; append(ready_name, name); append(ready_name, L"-ready");
		HANDLE event = CreateEventW(NULL, FALSE, FALSE, name);
		HANDLE ready = CreateEventW(NULL, FALSE, FALSE, ready_name);
		if (!event || !ready || !SetEnvironmentVariableW(L"RAVEN_SHARED_WAIT", name)) return 6;
		append(command, executable); append(command, mode ? L"\" --child-nt" : L"\" --child-win32");
		STARTUPINFOW startup = {0};
		PROCESS_INFORMATION child = {0};
		startup.cb = sizeof startup;
		if (!CreateProcessW(executable, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &child)) return 7;
		if (WaitForSingleObject(ready, 5000) != WAIT_OBJECT_0) return 8;
		Sleep(100);
		if (!SetEvent(event) || WaitForSingleObject(child.hProcess, 5000) != WAIT_OBJECT_0) return 9;
		DWORD code = 0;
		if (!GetExitCodeProcess(child.hProcess, &code) || code) return mode ? 11 : 10;
		CloseHandle(child.hThread); CloseHandle(child.hProcess); CloseHandle(ready); CloseHandle(event);
	}
	return 0;
}
