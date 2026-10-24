#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>

typedef BOOL (WINAPI *EnumProcessesFn)(DWORD *, DWORD, DWORD *);

static int image_name(void)
{
	WCHAR expected[512], path[513];
	char narrow[513], expected_a[512];
	const DWORD length = GetModuleFileNameW(0, expected, 512);
	DWORD size = 512;
	if (!length || length >= 512) return 1;
	path[512] = 0x4321;
	SetLastError(0x1234);
	if (!QueryFullProcessImageNameW(GetCurrentProcess(), 0, path, &size) || size != length ||
		path[length] || path[512] != 0x4321 || GetLastError() != 0x1234) return 2;
	for (unsigned index = 0; index < length; ++index)
		if (path[index] != expected[index]) return 3;
	const int bytes = WideCharToMultiByte(CP_ACP, 0, expected, -1, expected_a, sizeof expected_a, 0, 0);
	size = sizeof narrow;
	if (!bytes || !QueryFullProcessImageNameA(GetCurrentProcess(), 0, narrow, &size) || size != (DWORD)bytes - 1) return 4;
	for (unsigned index = 0; index < (unsigned)bytes; ++index)
		if (narrow[index] != expected_a[index]) return 5;
	size = length;
	path[0] = 0x4321;
	if (QueryFullProcessImageNameW(GetCurrentProcess(), 0, path, &size) || GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
		size != length || path[0] != 0x4321) return 6;
	size = 0;
	if (QueryFullProcessImageNameW(GetCurrentProcess(), 0, path, &size) || GetLastError() != ERROR_INSUFFICIENT_BUFFER) return 7;
	size = 512;
	if (!QueryFullProcessImageNameW(GetCurrentProcess(), PROCESS_NAME_NATIVE, path, &size) || size <= length ||
		path[0] != '\\' || path[1] != 'D' || path[size]) return 8;
	if (QueryFullProcessImageNameW(GetCurrentProcess(), 2, path, &size) || GetLastError() != ERROR_INVALID_PARAMETER) return 9;
	if (QueryFullProcessImageNameW(0, 0, path, &size) || GetLastError() != ERROR_INVALID_HANDLE) return 10;
	HANDLE denied = OpenProcess(PROCESS_VM_READ, FALSE, GetCurrentProcessId());
	if (!denied || QueryFullProcessImageNameW(denied, 0, path, &size) || GetLastError() != ERROR_ACCESS_DENIED) return 11;
	CloseHandle(denied);
	HANDLE allowed = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
	size = 512;
	if (!allowed || !QueryFullProcessImageNameW(allowed, 0, path, &size) || size != length) return 12;
	CloseHandle(allowed);
	if (QueryFullProcessImageNameW(GetCurrentProcess(), 0, 0, &size) || GetLastError() != ERROR_INVALID_PARAMETER ||
		QueryFullProcessImageNameW(GetCurrentProcess(), 0, path, 0) || GetLastError() != ERROR_INVALID_PARAMETER) return 13;
	size = 512;
	if (QueryFullProcessImageNameW(GetCurrentProcess(), 0, (WCHAR *)1, &size) || GetLastError() != ERROR_NOACCESS ||
		QueryFullProcessImageNameW(GetCurrentProcess(), 0, path, (DWORD *)1) || GetLastError() != ERROR_NOACCESS) return 14;
	return 0;
}

static int query(EnumProcessesFn enumerate)
{
	DWORD ids[32], used = 0;
	for (unsigned index = 0; index < 32; ++index) ids[index] = 0x87654321;
	SetLastError(0x1234);
	if (!enumerate(0, 0, &used) || used != 0 || GetLastError() != 0x1234) return 1;
	if (!enumerate(ids, 7, &used) || used != sizeof(DWORD) || ids[1] != 0x87654321) return 2;
	if (!enumerate(ids, sizeof ids, &used) || !used || used > sizeof ids || used % sizeof(DWORD)) return 3;
	const DWORD self = GetCurrentProcessId();
	BOOL found = FALSE;
	for (unsigned index = 0; index < used / sizeof(DWORD); ++index) {
		if (!ids[index]) return 4;
		if (ids[index] == self) found = TRUE;
		for (unsigned previous = 0; previous < index; ++previous)
			if (ids[previous] == ids[index]) return 5;
	}
	if (!found) return 6;
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, self);
	if (!process) return 7;
	CloseHandle(process);
	if (enumerate(0, sizeof(DWORD), &used) || GetLastError() != ERROR_INVALID_PARAMETER) return 8;
	if (enumerate(ids, sizeof ids, 0) || GetLastError() != ERROR_INVALID_PARAMETER) return 9;
	if (enumerate((DWORD *)1, sizeof(DWORD), &used) || GetLastError() != ERROR_NOACCESS) return 10;
	if (enumerate(ids, sizeof ids, (DWORD *)1) || GetLastError() != ERROR_NOACCESS) return 11;
	return 0;
}

DWORD raven_entry(void)
{
	const int image = image_name();
	if (image) return 50 + image;
	EnumProcessesFn modern = (EnumProcessesFn)GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32EnumProcesses");
	if (!modern) return 1;
	const int first = query(modern);
	if (first) return 10 + first;
	HMODULE psapi = LoadLibraryA("psapi.dll");
	EnumProcessesFn legacy = psapi ? (EnumProcessesFn)GetProcAddress(psapi, "EnumProcesses") : 0;
	if (!legacy) return 2;
	const int second = query(legacy);
	return second ? 30 + second : 0;
}
