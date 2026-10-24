#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wmistr.h>
#include <evntrace.h>

static void clear(void *memory, unsigned count)
{
	volatile unsigned char *bytes = memory;
	for (unsigned index = 0; index < count; ++index) bytes[index] = 0;
}

DWORD raven_entry(void)
{
	EVENT_TRACE_PROPERTIES properties;
	clear(&properties, sizeof properties);
	const TRACEHANDLE missing = 0xfedcba9800000000ULL;
	const char *name = "raven-missing-trace";
	const WCHAR *wide = L"raven-missing-trace";
	SetLastError(0x1234);
	if (StopTraceA(0, 0, 0) != ERROR_INVALID_PARAMETER) return 1;
	if (StopTraceW(0, 0, &properties) != ERROR_INVALID_PARAMETER) return 2;
	if (StopTraceA(0, name, &properties) != ERROR_BAD_LENGTH) return 3;
	properties.Wnode.BufferSize = sizeof properties;
	if (StopTraceA(missing, 0, &properties) != ERROR_INVALID_PARAMETER) return 4;
	if (StopTraceW(missing, wide, 0) != ERROR_INVALID_PARAMETER) return 5;
	if (StopTraceA(0, "", &properties) != ERROR_INVALID_PARAMETER) return 6;
	if (ControlTraceW(missing, wide, &properties, 99) != ERROR_INVALID_PARAMETER) return 7;
	if (StopTraceA(0, name, (EVENT_TRACE_PROPERTIES *)1) != ERROR_NOACCESS) return 8;
	if (StopTraceW(0, (const WCHAR *)1, &properties) != ERROR_NOACCESS) return 9;
	for (unsigned index = 0; index < 128; ++index) {
		if (StopTraceA(missing, name, &properties) != ERROR_WMI_INSTANCE_NOT_FOUND) return 10;
		if (StopTraceW(missing, wide, &properties) != ERROR_WMI_INSTANCE_NOT_FOUND) return 11;
		if (ControlTraceA(missing, name, &properties, EVENT_TRACE_CONTROL_STOP) != ERROR_WMI_INSTANCE_NOT_FOUND) return 12;
		if (ControlTraceW(missing, wide, &properties, EVENT_TRACE_CONTROL_QUERY) != ERROR_WMI_INSTANCE_NOT_FOUND) return 13;
	}
	struct { TRACEHANDLE value; DWORD canary; } output = {0x1122334455667788ULL, 0x87654321};
	if (StartTraceA(0, name, &properties) != ERROR_INVALID_PARAMETER) return 14;
	if (StartTraceW(&output.value, 0, &properties) != ERROR_INVALID_PARAMETER) return 15;
	if (StartTraceA(&output.value, name, &properties) != ERROR_NOT_SUPPORTED) return 16;
	if (StartTraceW(&output.value, wide, &properties) != ERROR_NOT_SUPPORTED) return 17;
	if (output.value != 0x1122334455667788ULL || output.canary != 0x87654321) return 18;
	const unsigned char *bytes = (const unsigned char *)&properties;
	for (unsigned index = sizeof(DWORD); index < sizeof properties; ++index)
		if (bytes[index]) return 19;
	if (properties.Wnode.BufferSize != sizeof properties || GetLastError() != 0x1234) return 20;
	return 0;
}
