#include "stdafx.h"
#include "resource.h"
#include <windows.h>
#include <wchar.h>
#include <tchar.h>
#include <string>
#include <vector>    

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#pragma warning(disable: 4047)
HINSTANCE hInstance = (HINSTANCE)&__ImageBase;
#pragma warning(default: 4047)

#define EXPORTED_FUNCTION extern "C" _declspec(dllexport)
#define GWL_HWNDPARENT      (-8)

using namespace std;

typedef basic_string<WCHAR> tstring;

tstring widen(string str)
{
	const size_t wchar_count = str.size() + 1;

	vector<WCHAR> buf(wchar_count);

	return tstring{ buf.data(), (size_t)MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf.data(), wchar_count) };
}

string shorten(tstring str)
{
	int nbytes = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0, NULL, NULL);

	vector<char> buf((size_t)nbytes);

	return string{ buf.data(), (size_t)WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), buf.data(), nbytes, NULL, NULL) };
}

#define MONITOR_CENTER     0x0001        // center rect to monitor 
#define MONITOR_CLIP     0x0000        // clip rect to monitor 
#define MONITOR_WORKAREA 0x0002        // use monitor work area 
#define MONITOR_AREA     0x0000        // use monitor entire area 

// 
//  ClipOrCenterRectToMonitor 
// 
//  The most common problem apps have when running on a 
//  multimonitor system is that they "clip" or "pin" windows 
//  based on the SM_CXSCREEN and SM_CYSCREEN system metrics. 
//  Because of app compatibility reasons these system metrics 
//  return the size of the primary monitor. 
// 
//  This shows how you use the multi-monitor functions 
//  to do the same thing. 
// 
void ClipOrCenterRectToMonitor(LPRECT prc, UINT flags)
{
	HMONITOR hMonitor;
	MONITORINFO mi;
	RECT        rc;
	int         w = prc->right - prc->left;
	int         h = prc->bottom - prc->top;

	// 
	// get the nearest monitor to the passed rect. 
	// 
	hMonitor = MonitorFromRect(prc, MONITOR_DEFAULTTONEAREST);

	// 
	// get the work area or entire monitor rect. 
	// 
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);

	if (flags & MONITOR_WORKAREA)
		rc = mi.rcWork;
	else
		rc = mi.rcMonitor;

	// 
	// center or clip the passed rect to the monitor rect 
	// 
	if (flags & MONITOR_CENTER)
	{
		prc->left = rc.left + (rc.right - rc.left - w) / 2;
		prc->top = rc.top + (rc.bottom - rc.top - h) / 3;
		prc->right = prc->left + w;
		prc->bottom = prc->top + h;
	}
	else
	{
		prc->left = max(rc.left, min(rc.right - w, prc->left));
		prc->top = max(rc.top, min(rc.bottom - h, prc->top));
		prc->right = prc->left + w;
		prc->bottom = prc->top + h;
	}
}

void ClipOrCenterWindowToMonitor(HWND hwnd, UINT flags)
{
	RECT rc;
	GetWindowRect(hwnd, &rc);
	ClipOrCenterRectToMonitor(&rc, flags);
	SetWindowPos(hwnd, NULL, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

double display_get_dpi_x()
{
	HDC screen = GetDC(NULL);
	double hPixelsPerInch = GetDeviceCaps(screen, LOGPIXELSX);
	ReleaseDC(NULL, screen);

	return hPixelsPerInch;
}

double display_get_dpi_y()
{
	HDC screen = GetDC(NULL);
	double vPixelsPerInch = GetDeviceCaps(screen, LOGPIXELSY);
	ReleaseDC(NULL, screen);

	return vPixelsPerInch;
}

int ScaleToDpiPercentX(double PixelX)
{
	double result = PixelX * (display_get_dpi_x() / 96);
	return (int)result;
}

int ScaleToDpiPercentY(double PixelY)
{
	double result = PixelY * (display_get_dpi_y() / 96);
	return (int)result;
}

void ClientResize(HWND hWnd, int nWidth, int nHeight)
{
	RECT rcClient, rcWind;
	POINT ptDiff;
	GetClientRect(hWnd, &rcClient);
	GetWindowRect(hWnd, &rcWind);
	ptDiff.x = (rcWind.right - rcWind.left) - rcClient.right;
	ptDiff.y = (rcWind.bottom - rcWind.top) - rcClient.bottom;
	MoveWindow(hWnd, rcWind.left, rcWind.top, nWidth + ptDiff.x, nHeight + ptDiff.y, TRUE);
}

tstring tstrStr;
tstring tstrDef;
tstring tstrTextEntry;
string window_caption;
wchar_t *wstrResult;
double FontHeight;
int ENTRY_LENGTH;

double dialog_is_multiline = 0;
HWND dialog_get_owner = nullptr;
double dialog_owner_is_enabled = 1;
double dialog_has_caption = 1;
char *dialog_get_caption = "";
string dialog_get_button1 = "";
string dialog_get_button2 = "";
double dialog_is_embedded = 0;
double dialog_get_width = 320;
double dialog_get_height = 240;
double dialog_get_fontsize = 8;
double dialog_get_disableinput = 0;
double dialog_get_hiddeninput = 0;
double dialog_get_numbersonly = 0;
double dialog_get_cancel = 0;
double dialog_get_id = IDD_SINGLELINE;

LRESULT CALLBACK InputBoxHookProc(HWND hWndDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_INITDIALOG:
	{
		dialog_get_cancel = 1;
		RECT client;
		GetClientRect(dialog_get_owner, &client);
		if (ScaleToDpiPercentX((int)dialog_get_width) > client.right) dialog_get_width = client.right / (display_get_dpi_x() / 96);
		if (ScaleToDpiPercentY((int)dialog_get_height) > client.bottom) dialog_get_height = client.bottom / (display_get_dpi_x() / 96);
		HICON icon = (HICON)LoadImageW(NULL, L"icon.ico", IMAGE_ICON, 96, 96, LR_LOADFROMFILE | LR_SHARED);
		if (icon == NULL) icon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));
		SendMessageW(GetDlgItem(hWndDlg, IDC_STATIC), STM_SETICON, (WPARAM)icon, NULL);
		static HFONT s_hFont = NULL;
		TCHAR *fontName = "MS Shell Dlg 2";
		long nFontSize = (long)dialog_get_fontsize;
		HDC hdc = GetDC(hWndDlg);
		LOGFONT logFont = { 0 };
		logFont.lfHeight = -MulDiv(nFontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
		FontHeight = logFont.lfHeight;
		logFont.lfWeight = FW_REGULAR;
		_tcscpy_s(logFont.lfFaceName, fontName);
		s_hFont = CreateFontIndirect(&logFont);
		SendMessage(GetDlgItem(hWndDlg, IDC_EDIT), WM_SETFONT, (WPARAM)s_hFont, (LPARAM)MAKELONG(TRUE, 0));
		nFontSize = 8;
		logFont = { 0 };
		logFont.lfHeight = -MulDiv(nFontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
		logFont.lfWeight = FW_BOLD;
		_tcscpy_s(logFont.lfFaceName, fontName);
		s_hFont = CreateFontIndirect(&logFont);
		SendMessage(GetDlgItem(hWndDlg, IDC_PROMPT), WM_SETFONT, (WPARAM)s_hFont, (LPARAM)MAKELONG(TRUE, 0));
		ReleaseDC(hWndDlg, hdc);
		SetWindowLongPtr(hWndDlg, GWL_HWNDPARENT, (LONG_PTR)dialog_get_owner);
		if (dialog_has_caption == 0)
		{
			SetWindowLongPtr(hWndDlg, GWL_EXSTYLE, GetWindowLongPtr(hWndDlg, GWL_EXSTYLE) & ~WS_EX_DLGMODALFRAME);
			SetWindowLongPtr(hWndDlg, GWL_STYLE, GetWindowLongPtr(hWndDlg, GWL_STYLE) & ~WS_POPUP & ~WS_BORDER & ~WS_CAPTION);
		}
		else
		{
			SetWindowLongPtr(hWndDlg, GWL_EXSTYLE, GetWindowLongPtr(hWndDlg, GWL_EXSTYLE) | WS_EX_DLGMODALFRAME);
			SetWindowLongPtr(hWndDlg, GWL_STYLE, GetWindowLongPtr(hWndDlg, GWL_STYLE) | WS_BORDER | WS_CAPTION);
		}
		if (dialog_is_embedded == 0)
			SetParent(hWndDlg, GetDesktopWindow());
		else
		{
			SetParent(hWndDlg, dialog_get_owner);
			SetWindowLongPtr(hWndDlg, GWL_EXSTYLE, GetWindowLongPtr(hWndDlg, GWL_EXSTYLE) & ~WS_EX_DLGMODALFRAME);
			SetWindowLongPtr(hWndDlg, GWL_STYLE, GetWindowLongPtr(hWndDlg, GWL_STYLE) | WS_CHILD);
		}
		if (dialog_is_multiline == 0)
		{
			if (client.bottom > ScaleToDpiPercentY(90 - (FontHeight * 1.3)))
				ClientResize(hWndDlg, ScaleToDpiPercentX((int)dialog_get_width), ScaleToDpiPercentY(90 - (FontHeight * 1.3)));
			else
				ClientResize(hWndDlg, ScaleToDpiPercentX((int)dialog_get_width), client.bottom);
		}
		else
			ClientResize(hWndDlg, ScaleToDpiPercentX((int)dialog_get_width), ScaleToDpiPercentY((int)dialog_get_height));
		ClipOrCenterWindowToMonitor(hWndDlg, MONITOR_CENTER);
		if (dialog_get_disableinput == 0)
			PostMessage(GetDlgItem(hWndDlg, IDC_EDIT), EM_SETREADONLY, FALSE, 0);
		else
			PostMessage(GetDlgItem(hWndDlg, IDC_EDIT), EM_SETREADONLY, TRUE, 0);
		if (dialog_is_multiline == 0)
		{
			if (dialog_get_hiddeninput == 0)
				PostMessage(GetDlgItem(hWndDlg, IDC_EDIT), EM_SETPASSWORDCHAR, 0, 0);
			else
				PostMessage(GetDlgItem(hWndDlg, IDC_EDIT), EM_SETPASSWORDCHAR, '*', 0);
		}
		if (dialog_get_numbersonly == 0 || dialog_is_multiline == 1)
			SetWindowLongPtr(GetDlgItem(hWndDlg, IDC_EDIT), GWL_STYLE, GetWindowLongPtr(GetDlgItem(hWndDlg, IDC_EDIT), GWL_STYLE) & ~ES_NUMBER);
		else
			SetWindowLongPtr(GetDlgItem(hWndDlg, IDC_EDIT), GWL_STYLE, GetWindowLongPtr(GetDlgItem(hWndDlg, IDC_EDIT), GWL_STYLE) | ES_NUMBER);
		SetDlgItemTextW(hWndDlg, IDC_PROMPT, tstrStr.c_str());
		SetDlgItemTextW(hWndDlg, IDC_EDIT, tstrDef.c_str());
		if (dialog_get_button1 != "")
		{
			tstring tstrButton1 = widen(dialog_get_button1);
			SetDlgItemTextW(hWndDlg, IDOK, tstrButton1.c_str());
		}
		if (dialog_get_button1 != "")
		{
			tstring tstrButton2 = widen(dialog_get_button2);
			SetDlgItemTextW(hWndDlg, IDC_CANCEL, tstrButton2.c_str());
		}
		tstring tstr_empty = widen("");
		SetWindowTextW(hWndDlg, tstr_empty.c_str());
		window_caption = dialog_get_caption;
		string strWindowCaption = window_caption;
		tstring tstrWindowCaption = widen(strWindowCaption);
		SetWindowTextW(hWndDlg, tstrWindowCaption.c_str());
		if (!dialog_has_caption)
		{
			if (GetParent(hWndDlg) != GetDesktopWindow())
			{
				RECT childClient; RECT parentClient;
				GetClientRect(hWndDlg, &childClient);
				GetClientRect(GetParent(hWndDlg), &parentClient);

				MoveWindow(hWndDlg, (parentClient.right / 2) - (childClient.right / 2), 0, childClient.right, childClient.bottom, TRUE);
			}
		}
		SendDlgItemMessageW(hWndDlg, IDC_EDIT, EM_SETSEL, '*', 0);
		SendDlgItemMessageW(hWndDlg, IDC_EDIT, WM_SETFOCUS, 0, 0);
		return TRUE;
	}
	break;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			dialog_get_cancel = 0;
			ENTRY_LENGTH = GetWindowTextLengthW(GetDlgItem(hWndDlg, IDC_EDIT)) + 1;
			tstrTextEntry.resize(ENTRY_LENGTH);
			wstrResult = (wchar_t *)tstrTextEntry.c_str();
			GetDlgItemTextW(hWndDlg, IDC_EDIT, wstrResult, ENTRY_LENGTH);
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:
			SendMessage(hWndDlg, WM_CLOSE, 0, 0);
			return TRUE;
		case IDC_CANCEL:
			dialog_get_cancel = 1;
			tstrTextEntry = widen("");
			EndDialog(hWndDlg, 0);
			return TRUE;
		}
		break;

	case WM_CLOSE:
	{
		dialog_get_cancel = 1;
		tstrTextEntry = widen("");
		EndDialog(hWndDlg, 0);
		return TRUE;
	}

	case WM_SIZE:
	{
		RECT rectDialog;
		GetClientRect(hWndDlg, &rectDialog);
		RECT rectPrompt;
		HWND hwndPrompt = GetDlgItem(hWndDlg, IDC_PROMPT);
		GetWindowRect(hwndPrompt, &rectPrompt);
		RECT rectOK;
		HWND hwndOK = GetDlgItem(hWndDlg, IDOK);
		GetWindowRect(hwndOK, &rectOK);
		RECT rectCancel;
		HWND hwndCancel = GetDlgItem(hWndDlg, IDC_CANCEL);
		GetWindowRect(hwndCancel, &rectCancel);
		RECT rectEdit;
		HWND hwndEdit = GetDlgItem(hWndDlg, IDC_EDIT);
		GetWindowRect(hwndEdit, &rectEdit);
		if (dialog_is_multiline == 0)
		{
			MoveWindow(GetDlgItem(hWndDlg, IDC_STATIC), ScaleToDpiPercentX(10), ScaleToDpiPercentY(10), ScaleToDpiPercentX(64), ScaleToDpiPercentY(64), TRUE);
			MoveWindow(hwndPrompt, ScaleToDpiPercentX(84), ScaleToDpiPercentY(10), rectDialog.right - ScaleToDpiPercentX(95), ScaleToDpiPercentY(15), TRUE);
			MoveWindow(hwndOK, rectDialog.right - ScaleToDpiPercentX(220), rectDialog.bottom - ScaleToDpiPercentY(35), ScaleToDpiPercentX(100), ScaleToDpiPercentY(25), TRUE);
			MoveWindow(hwndCancel, rectDialog.right - ScaleToDpiPercentX(110), rectDialog.bottom - ScaleToDpiPercentY(35), ScaleToDpiPercentX(100), ScaleToDpiPercentY(25), TRUE);
			MoveWindow(hwndEdit, ScaleToDpiPercentX(84), ScaleToDpiPercentY(35), rectDialog.right - ScaleToDpiPercentX(95), rectDialog.bottom - ScaleToDpiPercentY(82), TRUE);
		}
		else
		{
			MoveWindow(GetDlgItem(hWndDlg, IDC_STATIC), ScaleToDpiPercentX(10), ScaleToDpiPercentY(10), ScaleToDpiPercentX(64), ScaleToDpiPercentY(64), TRUE);
			MoveWindow(hwndPrompt, ScaleToDpiPercentX(84), ScaleToDpiPercentY(10), rectDialog.right - ScaleToDpiPercentX(95), ScaleToDpiPercentY(15), TRUE);
			MoveWindow(hwndOK, rectDialog.right - ScaleToDpiPercentX(220), rectDialog.bottom - ScaleToDpiPercentY(35), ScaleToDpiPercentX(100), ScaleToDpiPercentY(25), TRUE);
			MoveWindow(hwndCancel, rectDialog.right - ScaleToDpiPercentX(110), rectDialog.bottom - ScaleToDpiPercentY(35), ScaleToDpiPercentX(100), ScaleToDpiPercentY(25), TRUE);
			MoveWindow(hwndEdit, ScaleToDpiPercentX(84), ScaleToDpiPercentY(35), rectDialog.right - ScaleToDpiPercentX(95), rectDialog.bottom - ScaleToDpiPercentY(85), TRUE);
		}
	}

	case WM_DPICHANGED:
	{
		RECT client;
		GetClientRect(dialog_get_owner, &client);
		if (dialog_is_multiline == 0)
		{
			if (client.bottom > ScaleToDpiPercentY(90 - (FontHeight * 1.3)))
				ClientResize(hWndDlg, ScaleToDpiPercentX((int)dialog_get_width), ScaleToDpiPercentY(90 - (FontHeight * 1.3)));
			else
				ClientResize(hWndDlg, ScaleToDpiPercentX((int)dialog_get_width), client.bottom);
		}
		else
			ClientResize(hWndDlg, ScaleToDpiPercentX((int)dialog_get_width), ScaleToDpiPercentY((int)dialog_get_height));
		RECT rectDialog;
		GetClientRect(hWndDlg, &rectDialog);
		RECT rectPrompt;
		HWND hwndPrompt = GetDlgItem(hWndDlg, IDC_PROMPT);
		GetWindowRect(hwndPrompt, &rectPrompt);
		RECT rectOK;
		HWND hwndOK = GetDlgItem(hWndDlg, IDOK);
		GetWindowRect(hwndOK, &rectOK);
		RECT rectCancel;
		HWND hwndCancel = GetDlgItem(hWndDlg, IDC_CANCEL);
		GetWindowRect(hwndCancel, &rectCancel);
		RECT rectEdit;
		HWND hwndEdit = GetDlgItem(hWndDlg, IDC_EDIT);
		GetWindowRect(hwndEdit, &rectEdit);
		if (dialog_is_multiline == 0)
		{
			MoveWindow(GetDlgItem(hWndDlg, IDC_STATIC), ScaleToDpiPercentX(10), ScaleToDpiPercentY(10), ScaleToDpiPercentX(64), ScaleToDpiPercentY(64), TRUE);
			MoveWindow(hwndPrompt, ScaleToDpiPercentX(84), ScaleToDpiPercentY(10), rectDialog.right - ScaleToDpiPercentX(95), ScaleToDpiPercentY(15), TRUE);
			MoveWindow(hwndOK, rectDialog.right - ScaleToDpiPercentX(220), rectDialog.bottom - ScaleToDpiPercentY(35), ScaleToDpiPercentX(100), ScaleToDpiPercentY(25), TRUE);
			MoveWindow(hwndCancel, rectDialog.right - ScaleToDpiPercentX(110), rectDialog.bottom - ScaleToDpiPercentY(35), ScaleToDpiPercentX(100), ScaleToDpiPercentY(25), TRUE);
			MoveWindow(hwndEdit, ScaleToDpiPercentX(84), ScaleToDpiPercentY(35), rectDialog.right - ScaleToDpiPercentX(95), rectDialog.bottom - ScaleToDpiPercentY(82), TRUE);
		}
		else
		{
			MoveWindow(GetDlgItem(hWndDlg, IDC_STATIC), ScaleToDpiPercentX(10), ScaleToDpiPercentY(10), ScaleToDpiPercentX(64), ScaleToDpiPercentY(64), TRUE);
			MoveWindow(hwndPrompt, ScaleToDpiPercentX(84), ScaleToDpiPercentY(10), rectDialog.right - ScaleToDpiPercentX(95), ScaleToDpiPercentY(15), TRUE);
			MoveWindow(hwndOK, rectDialog.right - ScaleToDpiPercentX(220), rectDialog.bottom - ScaleToDpiPercentY(35), ScaleToDpiPercentX(100), ScaleToDpiPercentY(25), TRUE);
			MoveWindow(hwndCancel, rectDialog.right - ScaleToDpiPercentX(110), rectDialog.bottom - ScaleToDpiPercentY(35), ScaleToDpiPercentX(100), ScaleToDpiPercentY(25), TRUE);
			MoveWindow(hwndEdit, ScaleToDpiPercentX(84), ScaleToDpiPercentY(35), rectDialog.right - ScaleToDpiPercentX(95), rectDialog.bottom - ScaleToDpiPercentY(85), TRUE);
		}
	}
	}

	return FALSE;
}

EXPORTED_FUNCTION double dialog_owner(HWND owner)
{
	dialog_get_owner = owner;

	return 0;
}

EXPORTED_FUNCTION double dialog_caption(double show, char *str)
{
	dialog_has_caption = show;
	dialog_is_embedded = !show;
	dialog_owner_is_enabled = !show;
	static string str_dialog_get_caption;
	tstring tstr_dialog_get_caption = widen(str);
	str_dialog_get_caption = shorten(tstr_dialog_get_caption);
	dialog_get_caption = (char *)str_dialog_get_caption.c_str();

	return 0;
}

EXPORTED_FUNCTION double dialog_buttons(char *btn1, char *btn2)
{
	dialog_get_button1 = btn1;
	dialog_get_button2 = btn2;

	return 0;
}

EXPORTED_FUNCTION double dialog_size(double width, double height)
{
	dialog_get_width = width;
	dialog_get_height = height;

	return 0;
}

EXPORTED_FUNCTION double dialog_fontsize(double fontsize)
{
	dialog_get_fontsize = fontsize * 0.75;

	return 0;
}

EXPORTED_FUNCTION double dialog_disableinput(double disableinput)
{
	dialog_get_disableinput = disableinput;

	return 0;
}

EXPORTED_FUNCTION double dialog_hiddeninput(double hiddeninput)
{
	dialog_get_hiddeninput = hiddeninput;

	return 0;
}

EXPORTED_FUNCTION double dialog_numbersonly(double numbersonly)
{
	dialog_get_numbersonly = numbersonly;

	return 0;
}

EXPORTED_FUNCTION double dialog_cancelled()
{
	return dialog_get_cancel;
}

EXPORTED_FUNCTION char *dialog_inputbox(char *str, char *def, double multiline)
{
	string strStr = str;
	string strDef = def;

	tstrStr = widen(strStr);
	tstrDef = widen(strDef);

	if (dialog_get_owner == nullptr)
		dialog_get_owner = GetAncestor(GetActiveWindow(), GA_ROOTOWNER);

	LONG_PTR cstyles = GetClassLongPtr(dialog_get_owner, GCL_STYLE);
	LONG_PTR wstyles = GetWindowLongPtr(dialog_get_owner, GWL_STYLE);

	SetClassLongPtr(dialog_get_owner, GCL_STYLE, cstyles | CS_PARENTDC);
	SetWindowLongPtr(dialog_get_owner, GWL_STYLE, wstyles | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

	if (dialog_owner_is_enabled == 0)
		SetWindowLongPtr(dialog_get_owner, GWL_STYLE, wstyles | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_DISABLED);

	if (dialog_has_caption == 0)
		SetWindowLongPtr(dialog_get_owner, GWL_STYLE, (wstyles | WS_CLIPCHILDREN | WS_CLIPSIBLINGS) & ~WS_SYSMENU & ~WS_SIZEBOX);

	RECT rc; GetClientRect(dialog_get_owner, &rc);
	SetWindowPos(dialog_get_owner, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
	ClientResize(dialog_get_owner, rc.right, rc.bottom);

	dialog_is_multiline = multiline;
	
	if (dialog_is_multiline == 0)
		dialog_get_id = IDD_SINGLELINE;
	else
		dialog_get_id = IDD_MULTILINE;

	DialogBoxW(hInstance, MAKEINTRESOURCEW(dialog_get_id), NULL, reinterpret_cast<DLGPROC>(InputBoxHookProc));

	SetWindowLongPtr(dialog_get_owner, GWL_STYLE, wstyles);
	SetClassLongPtr(dialog_get_owner, GWL_STYLE, cstyles);

	GetClientRect(dialog_get_owner, &rc);
	SetWindowPos(dialog_get_owner, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
	ClientResize(dialog_get_owner, rc.right, rc.bottom);

	static string strResult;
	strResult = shorten(tstrTextEntry);
	return (char *)strResult.c_str();
}