#include <windows.h>
#include "simpleApplication.h"

LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
HWND hWnd;
HDC hdc;
HGLRC hrc;
HINSTANCE hInstance;
const TCHAR szName[]=TEXT("win");

HANDLE actThread = NULL, frameThread = NULL;
bool thread1End, thread2End;
DWORD WINAPI ActThreadRun(LPVOID param);
DWORD WINAPI FrameThreadRun(LPVOID param);
void CreateThreads();
void ReleaseThreads();
bool dataPrepared = false;
HANDLE mutex = NULL;
void InitMutex();
void DeleteMutex();
DWORD currentTime = 0;

SimpleApplication* app = NULL;
void CreateApplication();
void ReleaseApplication();

void KillWindow() {
	DeleteMutex();
	ReleaseThreads();
	ReleaseApplication();
	ShowCursor(true);
	wglMakeCurrent(NULL,NULL);
	wglDeleteContext(hrc);
	ReleaseDC(hWnd,hdc);
	DestroyWindow(hWnd);
	UnregisterClass(szName,hInstance);
}

void ResizeWindow(int width,int height) {
	app->resize(width, height);
}

void DrawWindow() {
	WaitForSingleObject(mutex, INFINITE);
	dataPrepared = true;
	ReleaseMutex(mutex);
	app->draw();
	WaitForSingleObject(mutex, INFINITE);
	dataPrepared = false;
	ReleaseMutex(mutex);
	Sleep(0);
}

DWORD WINAPI ActThreadRun(LPVOID param) {
	DWORD last = 0;
	while (!app->willExit) {
		currentTime = GetTickCount();
		if(currentTime - last >= 1) {
			app->moveCamera();
			last = currentTime;
		}
	}
	thread1End = true;
	return 1;
}

DWORD WINAPI FrameThreadRun(LPVOID param) {
	DWORD startTime = GetTickCount();
	while (!app->willExit) {
		Sleep(0);
		if(!dataPrepared) {
			app->act(startTime, currentTime);
			app->prepare();
			app->animate(startTime, currentTime);

			WaitForSingleObject(mutex, INFINITE);
			dataPrepared = true;
			ReleaseMutex(mutex);
		}
	}
	thread2End = true;
	return 1;
}

void InitGLWin() {
	const PIXELFORMATDESCRIPTOR pfd={
			sizeof(PIXELFORMATDESCRIPTOR),1,
			PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
			PFD_TYPE_RGBA,32,
			0,0,0,0,0,0,
			0,0,0,
			0,0,0,0,
			24,0,0,
			PFD_MAIN_PLANE,0,0,0,0
	};
	hdc=GetDC(hWnd);
	int pixelFormat=ChoosePixelFormat(hdc,&pfd);
	SetPixelFormat(hdc,pixelFormat,&pfd);
	hrc=wglCreateContext(hdc);
	wglMakeCurrent(hdc,hrc);
}

void InitGL() {
	ShowCursor(false);
	app->init();
	InitMutex();
	CreateThreads();
}

void CreateThreads() {
	actThread = CreateThread(NULL, 0, ActThreadRun, NULL, 0, NULL);
	frameThread = CreateThread(NULL, 0, FrameThreadRun, NULL, 0, NULL);
	thread1End = false;
	thread2End = false;
}

void ReleaseThreads() {
	CloseHandle(frameThread);
	CloseHandle(actThread);
}

void InitMutex() {
	mutex = CreateMutex(NULL, FALSE, NULL);
}

void DeleteMutex() {
	ReleaseMutex(mutex);
	CloseHandle(mutex);
}

void CreateApplication() {
	app = new SimpleApplication();
}

void ReleaseApplication() {
	if (app) delete app;
	app = NULL;
}

int WINAPI WinMain(HINSTANCE hInst,HINSTANCE hPrevInstance,PSTR szCmdLine,int iCmdShow) {
	MSG msg;
	WNDCLASS wndClass;
	hInstance=hInst;

	wndClass.style=CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
	wndClass.lpfnWndProc=WndProc;
	wndClass.cbClsExtra=0;
	wndClass.cbWndExtra=0;
	wndClass.hInstance=hInstance;
	wndClass.hIcon=LoadIcon(NULL,IDI_APPLICATION);
	wndClass.hCursor=LoadCursor(NULL,IDC_ARROW);
	wndClass.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
	wndClass.lpszMenuName=NULL;
	wndClass.lpszClassName=szName;

	if(!RegisterClass(&wndClass)) {
		MessageBox(NULL,TEXT("Can not create!"),szName,MB_ICONERROR);
		return 0;
	}

	DWORD style=WS_OVERLAPPEDWINDOW;
	DWORD styleEX=WS_EX_APPWINDOW|WS_EX_WINDOWEDGE;

	CreateApplication();

	RECT winRect;
	winRect.left=(LONG)0;
	winRect.right=(LONG)app->windowWidth;
	winRect.top=(LONG)0;
	winRect.bottom=(LONG)app->windowHeight;
	int left=(GetSystemMetrics(SM_CXSCREEN)>>1)-(app->windowWidth>>1);
	int top=(GetSystemMetrics(SM_CYSCREEN)>>1)-(app->windowHeight>>1);

	AdjustWindowRectEx(&winRect,style,false,styleEX);
	hWnd=CreateWindowEx(styleEX,szName,TEXT("Tiny"),WS_CLIPSIBLINGS|WS_CLIPCHILDREN|style,
			left,top,(winRect.right-winRect.left),(winRect.bottom-winRect.top),
			NULL,NULL,hInstance,NULL);

	InitGLWin();
	ShowWindow(hWnd,iCmdShow);
	InitGL();
	ResizeWindow(app->windowWidth,app->windowHeight);
	SetForegroundWindow(hWnd);
	SetFocus(hWnd);
	UpdateWindow(hWnd);

	while(!app->willExit) {
		if(PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
			if(msg.message==WM_QUIT) {
				app->willExit=true;
			} else {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		} 
	}
	while (!thread1End || !thread2End)
		Sleep(0);
	KillWindow();
	return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) {
	static PAINTSTRUCT ps;
	switch(msg) {
		case WM_PAINT:
			BeginPaint(hWnd, &ps);
			if (!app->willExit)
				DrawWindow();
			SwapBuffers(hdc);
			EndPaint(hWnd, &ps);
			InvalidateRect(hWnd, NULL, FALSE);
			break;
		case WM_KEYDOWN:
			app->keyDown(wParam);
			break;
		case WM_KEYUP:
			app->keyUp(wParam);
			break;
		case WM_MOUSEMOVE:
			break;
		case WM_SIZE:
			ResizeWindow(LOWORD(lParam),HIWORD(lParam));
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd,msg,wParam,lParam);
	}

	return 0;
}
