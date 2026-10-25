#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static BOOL argument(const WCHAR *wanted)
{
	for (const WCHAR *at = GetCommandLineW(); *at; ++at) {
		unsigned index = 0;
		while (wanted[index] && at[index] == wanted[index]) ++index;
		if (!wanted[index]) return TRUE;
	}
	return FALSE;
}

int raven_entry(void)
{
	if (argument(L" /s ")) {
		HANDLE file = CreateFileW(L"C:\\registration-count.bin", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		DWORD written = 0;
		if (file == INVALID_HANDLE_VALUE || SetFilePointer(file, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER ||
			!WriteFile(file, "x", 1, &written, NULL) || written != 1 || !CloseHandle(file)) return 2;
		return 1;
	}
	if (argument(L" /peer")) return 0;
	WCHAR path[512], line[540];
	DWORD length = GetModuleFileNameW(NULL, path, 512);
	if (!length || length >= 512) return 10;
	line[0] = L'"';
	for (DWORD index = 0; index < length; ++index) line[index + 1] = path[index];
	const WCHAR tail[] = L"\" /peer";
	for (unsigned index = 0; index < sizeof(tail) / sizeof(tail[0]); ++index) line[length + 1 + index] = tail[index];
	STARTUPINFOW startup = {sizeof(startup)};
	PROCESS_INFORMATION process;
	if (!CreateProcessW(path, line, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup, &process)) return 11;
	HANDLE primary, token, queried;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &primary) ||
		!DuplicateTokenEx(primary, TOKEN_IMPERSONATE | TOKEN_QUERY, NULL, SecurityImpersonation, TokenImpersonation, &token) ||
		!SetThreadToken(&process.hThread, token) || !OpenThreadToken(process.hThread, TOKEN_QUERY, TRUE, &queried)) return 12;
	DWORD code = 1;
	if (ResumeThread(process.hThread) != 1 || WaitForSingleObject(process.hProcess, 5000) != WAIT_OBJECT_0 ||
		!GetExitCodeProcess(process.hProcess, &code) || code) return 13;
	if (!CloseHandle(primary) || !CloseHandle(token) || !CloseHandle(queried) ||
		!CloseHandle(process.hThread) || !CloseHandle(process.hProcess)) return 14;
	return 0;
}
