#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

typedef BOOL (WINAPI *clip_fn)(const RECT *);
static clip_fn original;
static unsigned intercepted;

static BOOL WINAPI hooked(const RECT *rect)
{
	++intercepted;
	return original(rect);
}

static void jump(unsigned char *code, uintptr_t target)
{
	code[0] = 0x48;
	code[1] = 0xb8;
	for (unsigned index = 0; index < 8; ++index)
		code[2 + index] = target >> (index * 8);
	code[10] = 0xff;
	code[11] = 0xe0;
}

unsigned int raven_entry(void)
{
	HMODULE module = GetModuleHandleA("user32.dll");
	unsigned char *entry = (unsigned char *)GetProcAddress(module, "ClipCursor");
	unsigned char *trampoline = VirtualAlloc(NULL, 64, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!entry || !trampoline)
		return 1;
	unsigned char saved[14];
	for (unsigned index = 0; index < sizeof saved; ++index)
		saved[index] = trampoline[index] = entry[index];
	jump(trampoline + sizeof saved, (uintptr_t)entry + sizeof saved);
	original = (clip_fn)trampoline;
	DWORD protection;
	if (!VirtualProtect(entry, sizeof saved, PAGE_EXECUTE_READWRITE, &protection))
		return 2;
	jump(entry, (uintptr_t)hooked);
	entry[12] = entry[13] = 0x90;
	FlushInstructionCache(GetCurrentProcess(), entry, sizeof saved);
	RECT rect;
	if (!ClipCursor(NULL) || intercepted != 1 || !GetClipCursor(&rect) || rect.right <= rect.left || rect.bottom <= rect.top)
		return 3;
	for (unsigned index = 0; index < sizeof saved; ++index)
		entry[index] = saved[index];
	FlushInstructionCache(GetCurrentProcess(), entry, sizeof saved);
	if (!VirtualProtect(entry, sizeof saved, protection, &protection) || !ClipCursor(NULL) || intercepted != 1)
		return 4;
	if (!VirtualFree(trampoline, 0, MEM_RELEASE))
		return 5;
	return 0;
}
