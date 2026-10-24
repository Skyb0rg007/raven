#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

int raven_entry(void)
{
	HWND window = CreateWindowExA(0, "STATIC", "OpenGL pixels", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		0, 0, 128, 128, NULL, NULL, NULL, NULL);
	if (!window)
		return 1;
	HDC dc = GetDC(window);
	PIXELFORMATDESCRIPTOR format = {0};
	format.nSize = sizeof(format);
	format.nVersion = 1;
	format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	format.iPixelType = PFD_TYPE_RGBA;
	format.cColorBits = 32;
	format.cDepthBits = 24;
	int index = ChoosePixelFormat(dc, &format);
	if (!index || !DescribePixelFormat(dc, index, sizeof(format), &format) || !SetPixelFormat(dc, index, &format) ||
		GetPixelFormat(dc) != index)
		return 2;
	HGLRC context = wglCreateContext(dc);
	if (!context || !wglMakeCurrent(dc, context))
		return 3;
	OutputDebugStringA((const char *)glGetString(GL_RENDERER));
	glViewport(0, 0, 32, 32);
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	unsigned char pixel[4] = {0};
	glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	if (glGetError() || pixel[0] != 255 || pixel[1] || pixel[2] || pixel[3] != 255)
		return 4;
	if (!SwapBuffers(dc))
		return 5;
	OutputDebugStringA("OpenGL frame presented");
	Sleep(3000);
	if (!wglMakeCurrent(NULL, NULL) || !wglDeleteContext(context))
		return 6;
	ReleaseDC(window, dc);
	DestroyWindow(window);
	return 0;
}
