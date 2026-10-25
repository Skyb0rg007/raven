#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE semaphore, start;

static DWORD WINAPI release(void *parameter)
{
	const DWORD marker = 0x1234 + (DWORD)(ULONG_PTR)parameter;
	if (WaitForSingleObject(start, 5000) != WAIT_OBJECT_0)
		return 20;
	SetLastError(marker);
	for (unsigned i = 0; i < 1000; ++i) {
		if (i % 250 == 0) {
			if (ReleaseSemaphore(NULL, 1, NULL) || GetLastError() != ERROR_INVALID_HANDLE)
				return 23;
			SetLastError(marker);
		}
		if (!ReleaseSemaphore(semaphore, 1, NULL))
			return 21;
		if (GetLastError() != marker)
			return 22;
	}
	return 0;
}

int raven_entry(void)
{
	semaphore = CreateSemaphoreW(NULL, 0, 4096, NULL);
	start = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (!semaphore || !start)
		return 1;
	struct { DWORD before; LONG count; DWORD after; } previous = {0x12345678, -1, 0x87654321};
	SetLastError(123);
	if (!ReleaseSemaphore(semaphore, 2, &previous.count) || previous.count != 0 || GetLastError() != 123 ||
		previous.before != 0x12345678 || previous.after != 0x87654321)
		return 2;
	if (WaitForSingleObject(semaphore, 0) != WAIT_OBJECT_0 || WaitForSingleObject(semaphore, 0) != WAIT_OBJECT_0 ||
		WaitForSingleObject(semaphore, 0) != WAIT_TIMEOUT)
		return 3;
	if (ReleaseSemaphore(NULL, 1, &previous.count) || GetLastError() != ERROR_INVALID_HANDLE || previous.count != 0)
		return 4;
	if (ReleaseSemaphore(semaphore, 4097, &previous.count) || GetLastError() != ERROR_TOO_MANY_POSTS || previous.count != 0)
		return 5;
	if (ReleaseSemaphore(semaphore, 0, NULL) || ReleaseSemaphore(semaphore, -1, NULL) ||
		WaitForSingleObject(semaphore, 0) != WAIT_TIMEOUT)
		return 6;
	LONG *output = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY);
	if (!output)
		return 7;
	if (ReleaseSemaphore(semaphore, 1, output) || GetLastError() != ERROR_NOACCESS ||
		WaitForSingleObject(semaphore, 0) != WAIT_TIMEOUT)
		return 8;
	VirtualFree(output, 0, MEM_RELEASE);
	HANDLE workers[4];
	for (unsigned i = 0; i < 4; ++i) {
		workers[i] = CreateThread(NULL, 0, release, (void *)(ULONG_PTR)i, 0, NULL);
		if (!workers[i])
			return 9;
	}
	if (!SetEvent(start))
		return 10;
	for (unsigned i = 0; i < 4000; ++i)
		if (WaitForSingleObjectEx(semaphore, 5000, FALSE) != WAIT_OBJECT_0)
			return 11;
	for (unsigned i = 0; i < 4; ++i) {
		DWORD code;
		if (WaitForSingleObject(workers[i], 5000) != WAIT_OBJECT_0 || !GetExitCodeThread(workers[i], &code))
			return 12;
		if (code)
			return (int)code;
		CloseHandle(workers[i]);
	}
	if (WaitForSingleObject(semaphore, 0) != WAIT_TIMEOUT)
		return 13;
	CloseHandle(semaphore);
	CloseHandle(start);
	return 0;
}
