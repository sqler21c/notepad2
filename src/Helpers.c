/******************************************************************************
*
*
* Notepad2
*
* Helpers.c
*   General helper functions
*   Parts taken from SciTE, (c) Neil Hodgson, http://www.scintilla.org
*   MinimizeToTray (c) 2000 Matthew Ellis
*
* See Readme.txt for more information about this source code.
* Please send me your comments to this work.
*
* See License.txt for details about distribution and modification.
*
*                                              (c) Florian Balmer 1996-2011
*                                                  florian.balmer@gmail.com
*                                               http://www.flos-freeware.ch
*
*
******************************************************************************/

#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <stdio.h>
#include "Scintilla.h"
#include "Helpers.h"
#include "resource.h"

void StopWatch_Show(const StopWatch *watch, LPCWSTR msg) {
	const double elapsed = StopWatch_Get(watch);
	WCHAR buf[128];
	swprintf(buf, COUNTOF(buf), L"%s: %.9f", msg, elapsed);
	MessageBox(NULL, buf, L"Notepad2", MB_OK);
}

#ifndef NDEBUG
void DLogf(const char *fmt, ...) {
	char buf[1024] = "";
	va_list va;
	va_start(va, fmt);
	wvsprintfA(buf, fmt, va);
	va_end(va);
	OutputDebugStringA(buf);
}
#endif

//=============================================================================
//
// Manipulation of (cached) ini file sections
//
BOOL IniSectionParseArray(IniSection *section, LPWSTR lpCachedIniSection) {
	IniSectionClear(section);
	if (StrIsEmpty(lpCachedIniSection)) {
		return FALSE;
	}

	const int capacity = section->capacity;
	LPWSTR p = lpCachedIniSection;
	int count = 0;

	do {
		IniKeyValueNode *node = &section->nodeList[count];
		LPWSTR v = StrChr(p, L'=');
		*v++ = L'\0';
		node->key = p;
		node->value = v;
		++count;
		p = StrEnd(v) + 1;
	} while (*p && count < capacity);

	section->count = count;
	return TRUE;
}

BOOL IniSectionParse(IniSection *section, LPWSTR lpCachedIniSection) {
	IniSectionClear(section);
	if (StrIsEmpty(lpCachedIniSection)) {
		return FALSE;
	}

	const int capacity = section->capacity;
	LPWSTR p = lpCachedIniSection;
	int count = 0;

	do {
		IniKeyValueNode *node = &section->nodeList[count];
		LPWSTR v = StrChr(p, L'=');
		*v++ = L'\0';
		const UINT keyLen = (UINT)(v - p - 1);
		node->hash = keyLen | (p[0] << 8) | (p[1] << 16);
		node->key = p;
		node->value = v;
		++count;
		p = StrEnd(v) + 1;
	} while (*p && count < capacity);

	section->count = count;
	section->head = &section->nodeList[0];
	--count;
#if IniSectionImplUseSentinelNode
	section->nodeList[count].next = section->sentinel;
#else
	section->nodeList[count].next = NULL;
#endif
	while (count != 0) {
		section->nodeList[count - 1].next = &section->nodeList[count];
		--count;
	}
	return TRUE;
}

LPCWSTR IniSectionUnsafeGetValue(IniSection *section, LPCWSTR key, int keyLen) {
	if (keyLen < 0) {
		keyLen = lstrlen(key);
	}

	const UINT hash = keyLen | (key[0] << 8) | (key[1] << 16);
	IniKeyValueNode *node = section->head;
	IniKeyValueNode *prev = NULL;
#if IniSectionImplUseSentinelNode
	section->sentinel->hash = hash;
	while (TRUE) {
		if (node->hash == hash) {
			if (node == section->sentinel) {
				return NULL;
			}
			if (StrEqual(node->key, key)) {
				// remove the node
				--section->count;
				if (prev == NULL) {
					section->head = node->next;
				} else {
					prev->next = node->next;
				}
				return node->value;
			}
		}
		prev = node;
		node = node->next;
	}
#else
	do {
		if (node->hash == hash && StrEqual(node->key, key)) {
			// remove the node
			--section->count;
			if (prev == NULL) {
				section->head = node->next;
			} else {
				prev->next = node->next;
			}
			return node->value;
		}
		prev = node;
		node = node->next;
	} while (node);
	return NULL;
#endif
}

void IniSectionGetStringImpl(IniSection *section, LPCWSTR key, int keyLen, LPCWSTR lpDefault, LPWSTR lpReturnedString, int cchReturnedString) {
	LPCWSTR value = IniSectionGetValueImpl(section, key, keyLen);
	// allow empty string value
	lstrcpyn(lpReturnedString, ((value == NULL) ? lpDefault : value), cchReturnedString);
}

int IniSectionGetIntImpl(IniSection *section, LPCWSTR key, int keyLen, int iDefault) {
	LPCWSTR value = IniSectionGetValueImpl(section, key, keyLen);
	if (value && CRTStrToInt(value, &keyLen)) {
		return keyLen;
	}
	return iDefault;
}

BOOL IniSectionGetBoolImpl(IniSection *section, LPCWSTR key, int keyLen, BOOL bDefault) {
	LPCWSTR value = IniSectionGetValueImpl(section, key, keyLen);
	if (value) {
		switch (*value) {
		case L'1':
			return TRUE;
		case L'0':
			return FALSE;
		}
	}
	return bDefault;
}

void IniSectionSetString(IniSectionOnSave *section, LPCWSTR key, LPCWSTR value) {
	LPWSTR p = section->next;
	lstrcpy(p, key);
	lstrcat(p, L"=");
	lstrcat(p, value);
	p = StrEnd(p) + 1;
	*p = L'\0';
	section->next = p;
}

int ParseCommaList(LPCWSTR str, int result[], int count) {
	if (StrIsEmpty(str)) {
		return 0;
	}

	int index = 0;
	while (index < count) {
		LPWSTR end;
		result[index] = (int)wcstol(str, &end, 10);
		if (str == end) {
			break;
		}

		++index;
		if (*end == L',') {
			++end;
		}
		str = end;
	}
	return index;
}

UINT GetCurrentPPI(HWND hwnd) {
	HDC hDC = GetDC(hwnd);
	UINT ppi = GetDeviceCaps(hDC, LOGPIXELSY);
	ReleaseDC(hwnd, hDC);
	ppi = max_u(ppi, USER_DEFAULT_SCREEN_DPI);
	return ppi;
}

UINT GetCurrentDPI(HWND hwnd) {
	UINT dpi = 0;
	if (IsWin10AndAbove()) {
		FARPROC pfnGetDpiForWindow = GetProcAddress(GetModuleHandle(L"user32.dll"), "GetDpiForWindow");
		if (pfnGetDpiForWindow) {
			dpi = (UINT)pfnGetDpiForWindow(hwnd);
		}
	}

	if (dpi == 0 && IsWin8p1AndAbove()) {
		HMODULE hShcore = LoadLibraryEx(L"shcore.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
		if (hShcore) {
			FARPROC pfnGetDpiForMonitor = GetProcAddress(hShcore, "GetDpiForMonitor");
			if (pfnGetDpiForMonitor) {
				HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
				UINT dpiX = 0, dpiY = 0;
				if (pfnGetDpiForMonitor(hMonitor, 0 /* MDT_EFFECTIVE_DPI */, &dpiX, &dpiY) == S_OK) {
					dpi = dpiX;
				}
			}
			FreeLibrary(hShcore);
		}
	}

	if (dpi == 0) {
		HDC hDC = GetDC(hwnd);
		dpi = GetDeviceCaps(hDC, LOGPIXELSY);
		ReleaseDC(hwnd, hDC);
	}

	dpi = max_u(dpi, USER_DEFAULT_SCREEN_DPI);
	return dpi;
}

HBITMAP ResizeImageForCurrentDPI(HBITMAP hbmp) {
	BITMAP bmp;
	if (g_uCurrentDPI > USER_DEFAULT_SCREEN_DPI && GetObject(hbmp, sizeof(BITMAP), &bmp)) {
		const int width = RoundToCurrentDPI(bmp.bmWidth);
		const int height = RoundToCurrentDPI(bmp.bmHeight);
		HBITMAP hCopy = CopyImage(hbmp, IMAGE_BITMAP, width, height, LR_COPYRETURNORG | LR_COPYDELETEORG);
		if (hCopy != NULL) {
			hbmp = hCopy;
		}
	}

	return hbmp;
}

//=============================================================================
//
// PrivateIsAppThemed()
//
extern HMODULE hModUxTheme;
BOOL PrivateIsAppThemed(void) {
	BOOL bIsAppThemed = FALSE;

	if (hModUxTheme) {
		FARPROC pfnIsAppThemed = GetProcAddress(hModUxTheme, "IsAppThemed");
		if (pfnIsAppThemed) {
			bIsAppThemed = (BOOL)pfnIsAppThemed();
		}
	}
	return bIsAppThemed;
}

//=============================================================================
//
// PrivateSetCurrentProcessExplicitAppUserModelID()
//
HRESULT PrivateSetCurrentProcessExplicitAppUserModelID(PCWSTR AppID) {
	FARPROC pfnSetCurrentProcessExplicitAppUserModelID;

	if (StrIsEmpty(AppID)) {
		return S_OK;
	}

	if (StrCaseEqual(AppID, L"(default)")) {
		return S_OK;
	}

	// since Windows 7
	pfnSetCurrentProcessExplicitAppUserModelID =
		GetProcAddress(GetModuleHandle(L"shell32.dll"), "SetCurrentProcessExplicitAppUserModelID");

	if (pfnSetCurrentProcessExplicitAppUserModelID) {
		return (HRESULT)pfnSetCurrentProcessExplicitAppUserModelID(AppID);
	}
	return S_OK;
}

//=============================================================================
//
// IsElevated()
//
BOOL IsElevated(void) {
	if (!IsVistaAndAbove()) {
		return FALSE;
	}

	BOOL bIsElevated = FALSE;
	HANDLE hToken = NULL;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		struct {
			DWORD TokenIsElevated;
		} /*TOKEN_ELEVATION*/te;
		DWORD dwReturnLength = 0;

		if (GetTokenInformation(hToken, /*TokenElevation*/20, &te, sizeof(te), &dwReturnLength)) {
			if (dwReturnLength == sizeof(te)) {
				bIsElevated = te.TokenIsElevated;
			}
		}
		CloseHandle(hToken);
	}
	return bIsElevated;
}

//=============================================================================
//
//  SetTheme()
//
//BOOL SetTheme(HWND hwnd, LPCWSTR lpszTheme) {
//	if (hModUxTheme) {
//		FARPROC pfnSetWindowTheme = GetProcAddress(hModUxTheme, "SetWindowTheme");
//
//		if (pfnSetWindowTheme) {
//			return (S_OK == pfnSetWindowTheme(hwnd, lpszTheme, NULL));
//		}
//	}
//	return FALSE;
//}

//=============================================================================
//
// BitmapMergeAlpha()
// Merge alpha channel into color channel
//
BOOL BitmapMergeAlpha(HBITMAP hbmp, COLORREF crDest) {
	BITMAP bmp;
	if (GetObject(hbmp, sizeof(BITMAP), &bmp)) {
		if (bmp.bmBitsPixel == 32) {
			RGBQUAD *prgba = bmp.bmBits;

			for (int y = 0; y < bmp.bmHeight; y++) {
				for (int x = 0; x < bmp.bmWidth; x++) {
					const BYTE alpha = prgba[x].rgbReserved;
					prgba[x].rgbRed = ((prgba[x].rgbRed * alpha) + (GetRValue(crDest) * (255 - alpha))) >> 8;
					prgba[x].rgbGreen = ((prgba[x].rgbGreen * alpha) + (GetGValue(crDest) * (255 - alpha))) >> 8;
					prgba[x].rgbBlue = ((prgba[x].rgbBlue * alpha) + (GetBValue(crDest) * (255 - alpha))) >> 8;
					prgba[x].rgbReserved = 0xFF;
				}
				prgba = (RGBQUAD *)((LPBYTE)prgba + bmp.bmWidthBytes);
			}
			return TRUE;
		}
	}
	return FALSE;
}

//=============================================================================
//
// BitmapAlphaBlend()
// Perform alpha blending to color channel only
//
BOOL BitmapAlphaBlend(HBITMAP hbmp, COLORREF crDest, BYTE alpha) {
	BITMAP bmp;
	if (GetObject(hbmp, sizeof(BITMAP), &bmp)) {
		if (bmp.bmBitsPixel == 32) {
			RGBQUAD *prgba = bmp.bmBits;

			for (int y = 0; y < bmp.bmHeight; y++) {
				for (int x = 0; x < bmp.bmWidth; x++) {
					prgba[x].rgbRed = ((prgba[x].rgbRed * alpha) + (GetRValue(crDest) * (255 - alpha))) >> 8;
					prgba[x].rgbGreen = ((prgba[x].rgbGreen * alpha) + (GetGValue(crDest) * (255 - alpha))) >> 8;
					prgba[x].rgbBlue = ((prgba[x].rgbBlue * alpha) + (GetBValue(crDest) * (255 - alpha))) >> 8;
				}
				prgba = (RGBQUAD *)((LPBYTE)prgba + bmp.bmWidthBytes);
			}
			return TRUE;
		}
	}
	return FALSE;
}

//=============================================================================
//
// BitmapGrayScale()
// Gray scale color channel only
//
BOOL BitmapGrayScale(HBITMAP hbmp) {
	BITMAP bmp;
	if (GetObject(hbmp, sizeof(BITMAP), &bmp)) {
		if (bmp.bmBitsPixel == 32) {
			RGBQUAD *prgba = bmp.bmBits;

			for (int y = 0; y < bmp.bmHeight; y++) {
				for (int x = 0; x < bmp.bmWidth; x++) {
					prgba[x].rgbRed = prgba[x].rgbGreen = prgba[x].rgbBlue =
							(((BYTE)((prgba[x].rgbRed * 38 + prgba[x].rgbGreen * 75 + prgba[x].rgbBlue * 15) >> 7) * 0x80) + (0xD0 * (255 - 0x80))) >> 8;
				}
				prgba = (RGBQUAD *)((LPBYTE)prgba + bmp.bmWidthBytes);
			}
			return TRUE;
		}
	}
	return FALSE;
}

//=============================================================================
//
// VerifyContrast()
// Check if two colors can be distinguished
//
BOOL VerifyContrast(COLORREF cr1, COLORREF cr2) {
	const BYTE r1 = GetRValue(cr1);
	const BYTE g1 = GetGValue(cr1);
	const BYTE b1 = GetBValue(cr1);
	const BYTE r2 = GetRValue(cr2);
	const BYTE g2 = GetGValue(cr2);
	const BYTE b2 = GetBValue(cr2);

	return	((abs((3 * r1 + 5 * g1 + 1 * b1) - (3 * r2 + 6 * g2 + 1 * b2))) >= 400) ||
			((abs(r1 - r2) + abs(b1 - b2) + abs(g1 - g2)) >= 400);
}

//=============================================================================
//
// IsFontAvailable()
// Test if a certain font is installed on the system
//
int CALLBACK EnumFontsProc(CONST LOGFONT *plf, CONST TEXTMETRIC *ptm, DWORD fontType, LPARAM lParam) {
	UNREFERENCED_PARAMETER(plf);
	UNREFERENCED_PARAMETER(ptm);
	UNREFERENCED_PARAMETER(fontType);

	*((PBOOL)lParam) = TRUE;
	return FALSE;
}

BOOL IsFontAvailable(LPCWSTR lpszFontName) {
	BOOL fFound = FALSE;

	HDC hDC = GetDC(NULL);
	EnumFonts(hDC, lpszFontName, EnumFontsProc, (LPARAM)&fFound);
	ReleaseDC(NULL, hDC);

	return fFound;
}

//=============================================================================
//
// SetClipData()
//
void SetClipData(HWND hwnd, WCHAR *pszData) {
	if (OpenClipboard(hwnd)) {
		EmptyClipboard();
		HANDLE hData = GlobalAlloc(GHND, sizeof(WCHAR) * (lstrlen(pszData) + 1));
		WCHAR *pData = GlobalLock(hData);
		lstrcpyn(pData, pszData, (int)GlobalSize(hData) / sizeof(WCHAR));
		GlobalUnlock(hData);
		SetClipboardData(CF_UNICODETEXT, hData);
		CloseClipboard();
	}
}

//=============================================================================
//
// SetWindowTitle()
//
BOOL bFreezeAppTitle = FALSE;

BOOL SetWindowTitle(HWND hwnd, UINT uIDAppName, BOOL bIsElevated, UINT uIDUntitled,
					LPCWSTR lpszFile, int iFormat, BOOL bModified,
					UINT uIDReadOnly, BOOL bReadOnly, LPCWSTR lpszExcerpt) {

	static WCHAR szCachedFile[MAX_PATH] = L"";
	static WCHAR szCachedDisplayName[MAX_PATH] = L"";
	static const WCHAR *pszSep = L" - ";
	static const WCHAR *pszMod = L"* ";

	if (bFreezeAppTitle) {
		return FALSE;
	}

	WCHAR szUntitled[128];
	WCHAR szAppName[128];
	GetString(uIDAppName, szAppName, COUNTOF(szAppName));
	GetString(uIDUntitled, szUntitled, COUNTOF(szUntitled));

	if (bIsElevated) {
		WCHAR szElevatedAppName[128];
		WCHAR fmt[64];
		FormatString(szElevatedAppName, fmt, IDS_APPTITLE_ELEVATED, szAppName);
		lstrcpyn(szAppName, szElevatedAppName, COUNTOF(szAppName));
	}

	WCHAR szTitle[512];
	lstrcpy(szTitle, (bModified ? pszMod : L""));

	if (StrNotEmpty(lpszExcerpt)) {
		WCHAR szExcerptQuote[256];
		WCHAR szExcerptFmt[32];
		FormatString(szExcerptQuote, szExcerptFmt, IDS_TITLEEXCERPT, lpszExcerpt);
		lstrcat(szTitle, szExcerptQuote);
	} else if (StrNotEmpty(lpszFile)) {
		if (iFormat < 2 && !PathIsRoot(lpszFile)) {
			if (!StrCaseEqual(szCachedFile, lpszFile)) {
				SHFILEINFO shfi;
				lstrcpy(szCachedFile, lpszFile);
				if (SHGetFileInfo2(lpszFile, 0, &shfi, sizeof(SHFILEINFO), SHGFI_DISPLAYNAME)) {
					lstrcpy(szCachedDisplayName, shfi.szDisplayName);
				} else {
					lstrcpy(szCachedDisplayName, PathFindFileName(lpszFile));
				}
			}
			lstrcat(szTitle, szCachedDisplayName);
			if (iFormat == 1) {
				WCHAR tchPath[MAX_PATH];
				lstrcpyn(tchPath, lpszFile, COUNTOF(tchPath));
				PathRemoveFileSpec(tchPath);
				lstrcat(szTitle, L" [");
				lstrcat(szTitle, tchPath);
				lstrcat(szTitle, L"]");
			}
		} else {
			lstrcat(szTitle, lpszFile);
		}
	} else {
		lstrcpy(szCachedFile, L"");
		lstrcpy(szCachedDisplayName, L"");
		lstrcat(szTitle, szUntitled);
	}

	if (bReadOnly) {
		WCHAR szReadOnly[32];
		GetString(uIDReadOnly, szReadOnly, COUNTOF(szReadOnly));
		lstrcat(szTitle, L" ");
		lstrcat(szTitle, szReadOnly);
	}

	lstrcat(szTitle, pszSep);
	lstrcat(szTitle, szAppName);

#if 0
	wsprintf(szAppName, L"; dpi=%u, %u", g_uCurrentDPI, g_uCurrentPPI);
	lstrcat(szTitle, szAppName);
#endif

	return SetWindowText(hwnd, szTitle);
}

//=============================================================================
//
// SetWindowTransparentMode()
//
extern int iOpacityLevel;
void SetWindowTransparentMode(HWND hwnd, BOOL bTransparentMode) {
	const LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	if (bTransparentMode) {
		if (IsWin2KAndAbove()) {
			SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
			const BYTE bAlpha = (BYTE)(iOpacityLevel * 255 / 100);
			SetLayeredWindowAttributes(hwnd, 0, bAlpha, LWA_ALPHA);
		}
	} else {
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
	}
}

void SetWindowLayoutRTL(HWND hwnd, BOOL bRTL) {
	const LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	if (bRTL) {
		SetWindowLongPtr(hwnd, GWL_EXSTYLE,  exStyle | WS_EX_LAYOUTRTL);
	} else {
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYOUTRTL);
	}
}

//=============================================================================
//
// CenterDlgInParentEx()
//
void CenterDlgInParentEx(HWND hDlg, HWND hParent) {
	RECT rcDlg;
	RECT rcParent;

	GetWindowRect(hDlg, &rcDlg);
	GetWindowRect(hParent, &rcParent);

	HMONITOR hMonitor = MonitorFromRect(&rcParent, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);

	const int xMin = mi.rcWork.left;
	const int yMin = mi.rcWork.top;

	const int xMax = (mi.rcWork.right) - (rcDlg.right - rcDlg.left);
	const int yMax = (mi.rcWork.bottom) - (rcDlg.bottom - rcDlg.top);

	int x, y;

	if ((rcParent.right - rcParent.left) - (rcDlg.right - rcDlg.left) > 20) {
		x = rcParent.left + (((rcParent.right - rcParent.left) - (rcDlg.right - rcDlg.left)) / 2);
	} else {
		x = rcParent.left + 70;
	}

	if ((rcParent.bottom - rcParent.top) - (rcDlg.bottom - rcDlg.top) > 20) {
		y = rcParent.top + (((rcParent.bottom - rcParent.top) - (rcDlg.bottom - rcDlg.top)) / 2);
	} else {
		y = rcParent.top + 60;
	}

	SetWindowPos(hDlg, NULL, clamp_i(x, xMin, xMax), clamp_i(y, yMin, yMax), 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

// Why doesn’t the "Automatically move pointer to the default button in a dialog box"
// work for nonstandard dialog boxes, and how do I add it to my own nonstandard dialog boxes?
// https://blogs.msdn.microsoft.com/oldnewthing/20130826-00/?p=3413/
void SnapToDefaultButton(HWND hwndBox) {
	BOOL fSnapToDefButton = FALSE;
	if (SystemParametersInfo(SPI_GETSNAPTODEFBUTTON, 0, &fSnapToDefButton, 0) && fSnapToDefButton) {
		// get child window at the top of the Z order.
		// for all our MessageBoxs it's the OK or YES button or NULL.
		HWND btn = GetWindow(hwndBox, GW_CHILD);
		if (btn != NULL) {
			WCHAR className[8] = L"";
			GetClassName(btn, className, COUNTOF(className));
			if (StrCaseEqual(className, L"Button")) {
				RECT rect;
				GetWindowRect(btn, &rect);
				const int x = rect.left + (rect.right - rect.left) / 2;
				const int y = rect.top + (rect.bottom - rect.top) / 2;
				SetCursorPos(x, y);
			}
		}
	}
}

//=============================================================================
//
// GetDlgPos()
//
void GetDlgPos(HWND hDlg, LPINT xDlg, LPINT yDlg) {
	RECT rcDlg;
	GetWindowRect(hDlg, &rcDlg);

	HWND hParent = GetParent(hDlg);
	RECT rcParent;
	GetWindowRect(hParent, &rcParent);

	// return positions relative to parent window
	*xDlg = rcDlg.left - rcParent.left;
	*yDlg = rcDlg.top - rcParent.top;
}

//=============================================================================
//
// SetDlgPos()
//
void SetDlgPos(HWND hDlg, int xDlg, int yDlg) {
	RECT rcDlg;
	GetWindowRect(hDlg, &rcDlg);

	HWND hParent = GetParent(hDlg);
	RECT rcParent;
	GetWindowRect(hParent, &rcParent);

	HMONITOR hMonitor = MonitorFromRect(&rcParent, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);

	const int xMin = mi.rcWork.left;
	const int yMin = mi.rcWork.top;

	const int xMax = (mi.rcWork.right) - (rcDlg.right - rcDlg.left);
	const int yMax = (mi.rcWork.bottom) - (rcDlg.bottom - rcDlg.top);

	// desired positions relative to parent window
	const int x = rcParent.left + xDlg;
	const int y = rcParent.top + yDlg;

	SetWindowPos(hDlg, NULL, clamp_i(x, xMin, xMax), clamp_i(y, yMin, yMax), 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

//=============================================================================
//
// Resize Dialog Helpers()
//
typedef struct _resizedlg {
	int cxClient;
	int cyClient;
	int cxFrame;
	int cyFrame;
	int mmiPtMinX;
	int mmiPtMinY;
} RESIZEDLG, *PRESIZEDLG;

void ResizeDlg_Init(HWND hwnd, int cxFrame, int cyFrame, int nIdGrip) {
	RESIZEDLG *pm = NP2HeapAlloc(sizeof(RESIZEDLG));

	RECT rc;
	GetClientRect(hwnd, &rc);
	pm->cxClient = rc.right - rc.left;
	pm->cyClient = rc.bottom - rc.top;

	pm->cxFrame = cxFrame;
	pm->cyFrame = cyFrame;

	AdjustWindowRectEx(&rc, GetWindowLong(hwnd, GWL_STYLE) | WS_THICKFRAME, FALSE, 0);
	pm->mmiPtMinX = rc.right - rc.left;
	pm->mmiPtMinY = rc.bottom - rc.top;

	if (pm->cxFrame < (rc.right - rc.left)) {
		pm->cxFrame = rc.right - rc.left;
	}
	if (pm->cyFrame < (rc.bottom - rc.top)) {
		pm->cyFrame = rc.bottom - rc.top;
	}

	SetProp(hwnd, L"ResizeDlg", (HANDLE)pm);

	SetWindowPos(hwnd, NULL, rc.left, rc.top, pm->cxFrame, pm->cyFrame, SWP_NOZORDER);

	SetWindowLongPtr(hwnd, GWL_STYLE, GetWindowLongPtr(hwnd, GWL_STYLE) | WS_THICKFRAME);
	SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

	WCHAR wch[64];
	GetMenuString(GetSystemMenu(GetParent(hwnd), FALSE), SC_SIZE, wch, COUNTOF(wch), MF_BYCOMMAND);
	InsertMenu(GetSystemMenu(hwnd, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_STRING | MF_ENABLED, SC_SIZE, wch);
	InsertMenu(GetSystemMenu(hwnd, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_SEPARATOR, 0, NULL);

	SetWindowLongPtr(GetDlgItem(hwnd, nIdGrip), GWL_STYLE,
					 GetWindowLongPtr(GetDlgItem(hwnd, nIdGrip), GWL_STYLE) | SBS_SIZEGRIP | WS_CLIPSIBLINGS);
	const int cGrip = GetSystemMetrics(SM_CXHTHUMB);
	SetWindowPos(GetDlgItem(hwnd, nIdGrip), NULL, pm->cxClient - cGrip, pm->cyClient - cGrip, cGrip, cGrip, SWP_NOZORDER);
}

void ResizeDlg_Destroy(HWND hwnd, int *cxFrame, int *cyFrame) {
	PRESIZEDLG pm = GetProp(hwnd, L"ResizeDlg");

	RECT rc;
	GetWindowRect(hwnd, &rc);
	*cxFrame = rc.right - rc.left;
	*cyFrame = rc.bottom - rc.top;

	RemoveProp(hwnd, L"ResizeDlg");
	NP2HeapFree(pm);
}

void ResizeDlg_Size(HWND hwnd, LPARAM lParam, int *cx, int *cy) {
	PRESIZEDLG pm = GetProp(hwnd, L"ResizeDlg");

	*cx = LOWORD(lParam) - pm->cxClient;
	*cy = HIWORD(lParam) - pm->cyClient;
	pm->cxClient = LOWORD(lParam);
	pm->cyClient = HIWORD(lParam);
}

void ResizeDlg_GetMinMaxInfo(HWND hwnd, LPARAM lParam) {
	PRESIZEDLG pm = GetProp(hwnd, L"ResizeDlg");

	LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
	lpmmi->ptMinTrackSize.x = pm->mmiPtMinX;
	lpmmi->ptMinTrackSize.y = pm->mmiPtMinY;
}

HDWP DeferCtlPos(HDWP hdwp, HWND hwndDlg, int nCtlId, int dx, int dy, UINT uFlags) {
	HWND hwndCtl = GetDlgItem(hwndDlg, nCtlId);
	RECT rc;
	GetWindowRect(hwndCtl, &rc);
	MapWindowPoints(NULL, hwndDlg, (LPPOINT)&rc, 2);
	if (uFlags & SWP_NOSIZE) {
		return DeferWindowPos(hdwp, hwndCtl, NULL, rc.left + dx, rc.top + dy, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	}
	return DeferWindowPos(hdwp, hwndCtl, NULL, 0, 0, rc.right - rc.left + dx, rc.bottom - rc.top + dy, SWP_NOZORDER | SWP_NOMOVE);
}

//=============================================================================
//
// MakeBitmapButton()
//
void MakeBitmapButton(HWND hwnd, int nCtlId, HINSTANCE hInstance, UINT uBmpId) {
	HWND hwndCtl = GetDlgItem(hwnd, nCtlId);
	HBITMAP hBmp = LoadImage(hInstance, MAKEINTRESOURCE(uBmpId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	hBmp = ResizeImageForCurrentDPI(hBmp);
	BITMAP bmp;
	GetObject(hBmp, sizeof(BITMAP), &bmp);
	BUTTON_IMAGELIST bi;
	bi.himl = ImageList_Create(bmp.bmWidth, bmp.bmHeight, ILC_COLOR32 | ILC_MASK, 1, 0);
	ImageList_AddMasked(bi.himl, hBmp, CLR_DEFAULT);
	DeleteObject(hBmp);
	SetRect(&bi.margin, 0, 0, 0, 0);
	bi.uAlign = BUTTON_IMAGELIST_ALIGN_CENTER;
	SendMessage(hwndCtl, BCM_SETIMAGELIST, 0, (LPARAM)&bi);
}

//=============================================================================
//
// MakeColorPickButton()
//
void MakeColorPickButton(HWND hwnd, int nCtlId, HINSTANCE hInstance, COLORREF crColor) {
	HWND hwndCtl = GetDlgItem(hwnd, nCtlId);
	BUTTON_IMAGELIST bi;
	HIMAGELIST himlOld = NULL;
	COLORMAP colormap[2];

	if (SendMessage(hwndCtl, BCM_GETIMAGELIST, 0, (LPARAM)&bi)) {
		himlOld = bi.himl;
	}

	if (IsWindowEnabled(hwndCtl) && crColor != (COLORREF)(-1)) {
		colormap[0].from = RGB(0x00, 0x00, 0x00);
		colormap[0].to	 = GetSysColor(COLOR_3DSHADOW);
	} else {
		colormap[0].from = RGB(0x00, 0x00, 0x00);
		colormap[0].to	 = RGB(0xFF, 0xFF, 0xFF);
	}

	if (IsWindowEnabled(hwndCtl) && crColor != (COLORREF)(-1)) {
		if (crColor == RGB(0xFF, 0xFF, 0xFF)) {
			crColor = RGB(0xFF, 0xFF, 0xFE);
		}

		colormap[1].from = RGB(0xFF, 0xFF, 0xFF);
		colormap[1].to	 = crColor;
	} else {
		colormap[1].from = RGB(0xFF, 0xFF, 0xFF);
		colormap[1].to	 = RGB(0xFF, 0xFF, 0xFF);
	}

	HBITMAP hBmp = CreateMappedBitmap(hInstance, IDB_PICK, 0, colormap, 2);

	bi.himl = ImageList_Create(10, 10, ILC_COLORDDB | ILC_MASK, 1, 0);
	ImageList_AddMasked(bi.himl, hBmp, RGB(0xFF, 0xFF, 0xFF));
	DeleteObject(hBmp);

	SetRect(&bi.margin, 0, 0, 4, 0);
	bi.uAlign = BUTTON_IMAGELIST_ALIGN_RIGHT;

	SendMessage(hwndCtl, BCM_SETIMAGELIST, 0, (LPARAM)&bi);
	InvalidateRect(hwndCtl, NULL, TRUE);

	if (himlOld) {
		ImageList_Destroy(himlOld);
	}
}

//=============================================================================
//
// DeleteBitmapButton()
//
void DeleteBitmapButton(HWND hwnd, int nCtlId) {
	HWND hwndCtl = GetDlgItem(hwnd, nCtlId);
	BUTTON_IMAGELIST bi;
	if (SendMessage(hwndCtl, BCM_GETIMAGELIST, 0, (LPARAM)&bi)) {
		ImageList_Destroy(bi.himl);
	}
}

//=============================================================================
//
// SendWMSize()
//
LRESULT SendWMSize(HWND hwnd) {
	RECT rc;
	GetClientRect(hwnd, &rc);
	return SendMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
}

//=============================================================================
//
// StatusSetTextID()
//
BOOL StatusSetTextID(HWND hwnd, UINT nPart, UINT uID) {
	WCHAR szText[256];
	GetString(uID, szText, COUNTOF(szText));
	return (BOOL)SendMessage(hwnd, SB_SETTEXT, nPart, (LPARAM)szText);
}

//=============================================================================
//
// StatusCalcPaneWidth()
//
int StatusCalcPaneWidth(HWND hwnd, LPCWSTR lpsz) {
	HDC hdc = GetDC(hwnd);
	HFONT hfont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
	HFONT hfold = SelectObject(hdc, hfont);
	const int mmode = SetMapMode(hdc, MM_TEXT);

	SIZE size;
	GetTextExtentPoint32(hdc, lpsz, lstrlen(lpsz), &size);

	SetMapMode(hdc, mmode);
	SelectObject(hdc, hfold);
	ReleaseDC(hwnd, hdc);

	return size.cx + 9;
}

//=============================================================================
//
// Toolbar_Get/SetButtons()
//
int Toolbar_GetButtons(HWND hwnd, int cmdBase, LPWSTR lpszButtons, int cchButtons) {
	const int count = min_i(MAX_TOOLBAR_ITEM_COUNT_WITH_SEPARATOR, (int)SendMessage(hwnd, TB_BUTTONCOUNT, 0, 0));
	const int maxCch = cchButtons - 3; // two digits, one space and NULL
	int len = 0;

	for (int i = 0; i < count && len < maxCch; i++) {
		TBBUTTON tbb;
		SendMessage(hwnd, TB_GETBUTTON, i, (LPARAM)&tbb);
		const int iCmd = (tbb.idCommand == 0) ? 0 : tbb.idCommand - cmdBase + 1;
		len += wsprintf(lpszButtons + len, L"%i ", iCmd);
	}

	lpszButtons[len--] = L'\0';
	if (len >= 0) {
		lpszButtons[len] = L'\0';
	}
	return count;
}

int Toolbar_SetButtons(HWND hwnd, LPCWSTR lpszButtons, LPCTBBUTTON ptbb, int ctbb) {
	int count = (int)SendMessage(hwnd, TB_BUTTONCOUNT, 0, 0);
	if (StrIsEmpty(lpszButtons)) {
		return count;
	}

	while (count) {
		SendMessage(hwnd, TB_DELETEBUTTON, 0, 0);
		--count;
	}

	LPCWSTR p = lpszButtons;
	--ctbb;
	while (TRUE) {
		LPWSTR end;
		int iCmd = (int)wcstol(p, &end, 10);
		if (p != end) {
			iCmd = clamp_i(iCmd, 0, ctbb);
			SendMessage(hwnd, TB_ADDBUTTONS, 1, (LPARAM)&ptbb[iCmd]);
			p = end;
			++count;
			//if (count == MAX_TOOLBAR_ITEM_COUNT_WITH_SEPARATOR) {
			//	break;
			//}
		} else {
			break;
		}
	}

	return count;
}

//=============================================================================
//
// IsCmdEnabled()
//
BOOL IsCmdEnabled(HWND hwnd, UINT uId) {
	HMENU hmenu = GetMenu(hwnd);
	SendMessage(hwnd, WM_INITMENU, (WPARAM)hmenu, 0);
	const UINT ustate = GetMenuState(hmenu, uId, MF_BYCOMMAND);

	if (ustate == 0xFFFFFFFF) {
		return TRUE;
	}
	return !(ustate & (MF_GRAYED | MF_DISABLED));
}

//=============================================================================
//
// PathRelativeToApp()
//
void PathRelativeToApp(LPCWSTR lpszSrc, LPWSTR lpszDest, int cchDest,
					   BOOL bSrcIsFile, BOOL bUnexpandEnv, BOOL bUnexpandMyDocs) {
	WCHAR wchAppPath[MAX_PATH];
	WCHAR wchWinDir[MAX_PATH];
	WCHAR wchUserFiles[MAX_PATH];
	WCHAR wchPath[MAX_PATH];
	DWORD dwAttrTo = bSrcIsFile ? 0 : FILE_ATTRIBUTE_DIRECTORY;

	GetModuleFileName(NULL, wchAppPath, COUNTOF(wchAppPath));
	PathRemoveFileSpec(wchAppPath);
	GetWindowsDirectory(wchWinDir, COUNTOF(wchWinDir));
	SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, wchUserFiles);

	if (bUnexpandMyDocs &&
			!PathIsRelative(lpszSrc) &&
			!PathIsPrefix(wchUserFiles, wchAppPath) &&
			PathIsPrefix(wchUserFiles, lpszSrc) &&
			PathRelativePathTo(wchPath, wchUserFiles, FILE_ATTRIBUTE_DIRECTORY, lpszSrc, dwAttrTo)) {
		lstrcpy(wchUserFiles, L"%CSIDL:MYDOCUMENTS%");
		PathAppend(wchUserFiles, wchPath);
		lstrcpy(wchPath, wchUserFiles);
	} else if (PathIsRelative(lpszSrc) || PathCommonPrefix(wchAppPath, wchWinDir, NULL)) {
		lstrcpyn(wchPath, lpszSrc, COUNTOF(wchPath));
	} else {
		if (!PathRelativePathTo(wchPath, wchAppPath, FILE_ATTRIBUTE_DIRECTORY, lpszSrc, dwAttrTo)) {
			lstrcpyn(wchPath, lpszSrc, COUNTOF(wchPath));
		}
	}

	WCHAR wchResult[MAX_PATH];
	if (bUnexpandEnv) {
		if (!PathUnExpandEnvStrings(wchPath, wchResult, COUNTOF(wchResult))) {
			lstrcpyn(wchResult, wchPath, COUNTOF(wchResult));
		}
	} else {
		lstrcpyn(wchResult, wchPath, COUNTOF(wchResult));
	}

	lstrcpyn(lpszDest, wchResult, (cchDest == 0) ? MAX_PATH : cchDest);
}

//=============================================================================
//
// PathAbsoluteFromApp()
//
void PathAbsoluteFromApp(LPCWSTR lpszSrc, LPWSTR lpszDest, int cchDest, BOOL bExpandEnv) {
	WCHAR wchPath[MAX_PATH];

	if (StrNEqual(lpszSrc, L"%CSIDL:MYDOCUMENTS%", CSTRLEN("%CSIDL:MYDOCUMENTS%"))) {
		SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, wchPath);
		PathAppend(wchPath, lpszSrc + CSTRLEN("%CSIDL:MYDOCUMENTS%"));
	} else {
		lstrcpyn(wchPath, lpszSrc, COUNTOF(wchPath));
	}

	if (bExpandEnv) {
		ExpandEnvironmentStringsEx(wchPath, COUNTOF(wchPath));
	}

	WCHAR wchResult[MAX_PATH];
	if (PathIsRelative(wchPath)) {
		GetModuleFileName(NULL, wchResult, COUNTOF(wchResult));
		PathRemoveFileSpec(wchResult);
		PathAppend(wchResult, wchPath);
	} else {
		lstrcpyn(wchResult, wchPath, COUNTOF(wchResult));
	}

	PathCanonicalizeEx(wchResult);
	if (PathGetDriveNumber(wchResult) != -1) {
		CharUpperBuff(wchResult, 1);
	}

	lstrcpyn(lpszDest, wchResult, (cchDest == 0) ? MAX_PATH : cchDest);
}

///////////////////////////////////////////////////////////////////////////////
//
//
// Name: PathIsLnkFile()
//
// Purpose: Determine wheter pszPath is a Windows Shell Link File by
// comparing the filename extension with L".lnk"
//
// Manipulates:
//
BOOL PathIsLnkFile(LPCWSTR pszPath) {
	if (!pszPath || !*pszPath) {
		return FALSE;
	}

	/*
	LPCWSTR pszExt = StrRChr(pszPath, NULL, L'.');
	if (!pszExt) {
		return FALSE;
	}
	if (StrCaseEqual(pszExt, L".lnk")) {
		return TRUE;
	}
	return FALSE;

	if (StrCaseEqual(PathFindExtension(pszPath), L".lnk")) {
		return TRUE;
	}
	return FALSE;
	*/

	if (!StrCaseEqual(PathFindExtension(pszPath), L".lnk")) {
		return FALSE;
	}

	WCHAR tchResPath[MAX_PATH];
	return PathGetLnkPath(pszPath, tchResPath, COUNTOF(tchResPath));
}

///////////////////////////////////////////////////////////////////////////////
//
//
// Name: PathGetLnkPath()
//
// Purpose: Try to get the path to which a lnk-file is linked
//
//
// Manipulates: pszResPath
//
BOOL PathGetLnkPath(LPCWSTR pszLnkFile, LPWSTR pszResPath, int cchResPath) {
	IShellLink *psl;
	BOOL bSucceeded = FALSE;

	if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLink, (LPVOID *)(&psl)))) {
		IPersistFile *ppf;

		if (SUCCEEDED(psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void **)(&ppf)))) {
			WCHAR wsz[MAX_PATH];

			/*MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, pszLnkFile, -1, wsz, MAX_PATH);*/
			lstrcpy(wsz, pszLnkFile);

			if (SUCCEEDED(ppf->lpVtbl->Load(ppf, wsz, STGM_READ))) {
				WIN32_FIND_DATA fd;
				if (NOERROR == psl->lpVtbl->GetPath(psl, pszResPath, cchResPath, &fd, 0)) {
					// This additional check seems reasonable
					bSucceeded = StrNotEmpty(pszResPath);
				}
			}
			ppf->lpVtbl->Release(ppf);
		}
		psl->lpVtbl->Release(psl);
	}

	if (bSucceeded) {
		ExpandEnvironmentStringsEx(pszResPath, cchResPath);
		PathCanonicalizeEx(pszResPath);
	}

	return bSucceeded;
}

///////////////////////////////////////////////////////////////////////////////
//
//
// Name: PathIsLnkToDirectory()
//
// Purpose: Determine wheter pszPath is a Windows Shell Link File which
// refers to a directory
//
// Manipulates: pszResPath
//
BOOL PathIsLnkToDirectory(LPCWSTR pszPath, LPWSTR pszResPath, int cchResPath) {
	if (PathIsLnkFile(pszPath)) {
		WCHAR tchResPath[MAX_PATH];
		if (PathGetLnkPath(pszPath, tchResPath, COUNTOF(tchResPath))) {
			if (PathIsDirectory(tchResPath)) {
				lstrcpyn(pszResPath, tchResPath, cchResPath);
				return TRUE;
			}
		}
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
//
//
// Name: PathCreateDeskLnk()
//
// Purpose: Modified to create a desktop link to Notepad2
//
// Manipulates:
//
BOOL PathCreateDeskLnk(LPCWSTR pszDocument) {
	if (StrIsEmpty(pszDocument)) {
		return TRUE;
	}

	// init strings
	WCHAR tchExeFile[MAX_PATH];
	GetModuleFileName(NULL, tchExeFile, COUNTOF(tchExeFile));

	WCHAR tchDocTemp[MAX_PATH];
	lstrcpy(tchDocTemp, pszDocument);
	PathQuoteSpaces(tchDocTemp);

	WCHAR tchArguments[MAX_PATH + 16];
	lstrcpy(tchArguments, L"-n ");
	lstrcat(tchArguments, tchDocTemp);

	WCHAR tchLinkDir[MAX_PATH];
	SHGetSpecialFolderPath(NULL, tchLinkDir, CSIDL_DESKTOPDIRECTORY, TRUE);

	WCHAR tchDescription[64];
	GetString(IDS_LINKDESCRIPTION, tchDescription, COUNTOF(tchDescription));

	// Try to construct a valid filename...
	BOOL fMustCopy;
	WCHAR tchLnkFileName[MAX_PATH];
	if (!SHGetNewLinkInfo(pszDocument, tchLinkDir, tchLnkFileName, &fMustCopy, SHGNLI_PREFIXNAME)) {
		return FALSE;
	}

	IShellLink *psl;
	BOOL bSucceeded = FALSE;
	if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLink, (LPVOID *)(&psl)))) {
		IPersistFile *ppf;

		if (SUCCEEDED(psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void **)(&ppf)))) {
			WCHAR wsz[MAX_PATH];

			/*MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, tchLnkFileName, -1, wsz, MAX_PATH);*/
			lstrcpy(wsz, tchLnkFileName);

			psl->lpVtbl->SetPath(psl, tchExeFile);
			psl->lpVtbl->SetArguments(psl, tchArguments);
			psl->lpVtbl->SetDescription(psl, tchDescription);

			if (SUCCEEDED(ppf->lpVtbl->Save(ppf, wsz, TRUE))) {
				bSucceeded = TRUE;
			}

			ppf->lpVtbl->Release(ppf);
		}
		psl->lpVtbl->Release(psl);
	}

	return bSucceeded;
}

///////////////////////////////////////////////////////////////////////////////
//
//
// Name: PathCreateFavLnk()
//
// Purpose: Modified to create a Notepad2 favorites link
//
// Manipulates:
//
BOOL PathCreateFavLnk(LPCWSTR pszName, LPCWSTR pszTarget, LPCWSTR pszDir) {
	if (StrIsEmpty(pszName)) {
		return TRUE;
	}

	WCHAR tchLnkFileName[MAX_PATH];
	lstrcpy(tchLnkFileName, pszDir);
	PathAppend(tchLnkFileName, pszName);
	lstrcat(tchLnkFileName, L".lnk");

	if (PathFileExists(tchLnkFileName)) {
		return FALSE;
	}

	IShellLink *psl;
	BOOL bSucceeded = FALSE;
	if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLink, (LPVOID *)(&psl)))) {
		IPersistFile *ppf;

		if (SUCCEEDED(psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void **)(&ppf)))) {
			WCHAR wsz[MAX_PATH];

			/*MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, tchLnkFileName, -1, wsz, MAX_PATH);*/
			lstrcpy(wsz, tchLnkFileName);

			psl->lpVtbl->SetPath(psl, pszTarget);

			if (SUCCEEDED(ppf->lpVtbl->Save(ppf, wsz, TRUE))) {
				bSucceeded = TRUE;
			}

			ppf->lpVtbl->Release(ppf);
		}
		psl->lpVtbl->Release(psl);
	}
	return bSucceeded;
}

//=============================================================================
//
// ExtractFirstArgument()
//
BOOL ExtractFirstArgument(LPCWSTR lpArgs, LPWSTR lpArg1, LPWSTR lpArg2) {
	BOOL bQuoted = FALSE;

	lstrcpy(lpArg1, lpArgs);
	if (lpArg2) {
		*lpArg2 = L'\0';
	}

	TrimString(lpArg1);
	if (!*lpArg1) {
		return FALSE;
	}

	if (*lpArg1 == L'\"') {
		*lpArg1 = L' ';
		TrimString(lpArg1);
		bQuoted = TRUE;
	}

	LPWSTR psz = StrChr(lpArg1, (bQuoted ? L'\"' : L' '));
	if (psz) {
		*psz = L'\0';
		if (lpArg2) {
			lstrcpy(lpArg2, psz + 1);
		}
	}

	TrimString(lpArg1);

	if (lpArg2) {
		TrimString(lpArg2);
	}

	return TRUE;
}

//=============================================================================
//
// PrepareFilterStr()
//
void PrepareFilterStr(LPWSTR lpFilter) {
	LPWSTR psz = StrEnd(lpFilter);
	while (psz != lpFilter) {
		if (*(psz = CharPrev(lpFilter, psz)) == L'\n') {
			*psz = L'\0';
		}
	}
}

//=============================================================================
//
// StrTab2Space() - in place conversion
//
void StrTab2Space(LPWSTR lpsz) {
	WCHAR *c = lpsz;
	while ((c = StrChr(lpsz, L'\t')) != NULL) {
		*c = L' ';
	}
}

//=============================================================================
//
// PathFixBackslashes() - in place conversion
//
void PathFixBackslashes(LPWSTR lpsz) {
	WCHAR *c = lpsz;
	while ((c = StrChr(c, L'/')) != NULL) {
		if (*CharPrev(lpsz, c) == L':' && *CharNext(c) == L'/') {
			c += 2;
		} else {
			*c = L'\\';
		}
	}
}

//=============================================================================
//
// ExpandEnvironmentStringsEx()
//
// Adjusted for Windows 95
//
void ExpandEnvironmentStringsEx(LPWSTR lpSrc, DWORD dwSrc) {
	WCHAR szBuf[312];

	if (ExpandEnvironmentStrings(lpSrc, szBuf, COUNTOF(szBuf))) {
		lstrcpyn(lpSrc, szBuf, dwSrc);
	}
}

//=============================================================================
//
// PathCanonicalizeEx()
//
//
void PathCanonicalizeEx(LPWSTR lpSrc) {
	WCHAR szDst[MAX_PATH];

	if (PathCanonicalize(szDst, lpSrc)) {
		lstrcpy(lpSrc, szDst);
	}
}

//=============================================================================
//
// GetLongPathNameEx()
//
//
DWORD GetLongPathNameEx(LPWSTR lpszPath, DWORD cchBuffer) {
	const DWORD dwRet = GetLongPathName(lpszPath, lpszPath, cchBuffer);
	if (dwRet) {
		if (PathGetDriveNumber(lpszPath) != -1) {
			CharUpperBuff(lpszPath, 1);
		}
		return dwRet;
	}
	return 0;
}

//=============================================================================
//
//	SHGetFileInfo2()
//
//	Return a default name when the file has been removed, and always append
//	a filename extension
//
DWORD_PTR SHGetFileInfo2(LPCWSTR pszPath, DWORD dwFileAttributes, SHFILEINFO *psfi, UINT cbFileInfo, UINT uFlags) {
	if (PathFileExists(pszPath)) {
		const DWORD_PTR dw = SHGetFileInfo(pszPath, dwFileAttributes, psfi, cbFileInfo, uFlags);
		if (lstrlen(psfi->szDisplayName) < lstrlen(PathFindFileName(pszPath))) {
			StrCatBuff(psfi->szDisplayName, PathFindExtension(pszPath), COUNTOF(psfi->szDisplayName));
		}
		return dw;
	}
	{
		const DWORD_PTR dw = SHGetFileInfo(pszPath, FILE_ATTRIBUTE_NORMAL, psfi, cbFileInfo, uFlags | SHGFI_USEFILEATTRIBUTES);
		if (lstrlen(psfi->szDisplayName) < lstrlen(PathFindFileName(pszPath))) {
			StrCatBuff(psfi->szDisplayName, PathFindExtension(pszPath), COUNTOF(psfi->szDisplayName));
		}
		return dw;
	}
}

//=============================================================================
//
// FormatNumberStr()
//
void FormatNumberStr(LPWSTR lpNumberStr) {
	const int i = lstrlen(lpNumberStr);
	if (i <= 3) {
		return;
	}

	// https://docs.microsoft.com/en-us/windows/desktop/Intl/locale-sthousand
	// https://docs.microsoft.com/en-us/windows/desktop/Intl/locale-sgrouping
	WCHAR szSep[4];
	const WCHAR sep = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, szSep, COUNTOF(szSep))? szSep[0] : L',';

	WCHAR *c = lpNumberStr + i;
	WCHAR *end = c;
#if 0
	i = 0;
	while ((c = CharPrev(lpNumberStr, c)) != lpNumberStr) {
		if (++i == 3) {
			i = 0;
			MoveMemory(c + 1, c, sizeof(WCHAR) * (end - c + 1));
			*c = sep;
			++end;
		}
	}
#endif
	lpNumberStr += 3;
	do {
		c -= 3;
		MoveMemory(c + 1, c, sizeof(WCHAR) * (end - c + 1));
		*c = sep;
		++end;
	} while (c > lpNumberStr);
}

//=============================================================================
//
// SetDlgItemIntEx()
//
BOOL SetDlgItemIntEx(HWND hwnd, int nIdItem, UINT uValue) {
	WCHAR szBuf[32];

	wsprintf(szBuf, L"%u", uValue);
	FormatNumberStr(szBuf);

	return SetDlgItemText(hwnd, nIdItem, szBuf);
}

//=============================================================================
//
// A2W: Convert Dialog Item Text form Unicode to UTF-8 and vice versa
//
UINT GetDlgItemTextA2W(UINT uCP, HWND hDlg, int nIDDlgItem, LPSTR lpString, int nMaxCount) {
	WCHAR wsz[1024] = L"";
	const UINT uRet = GetDlgItemText(hDlg, nIDDlgItem, wsz, COUNTOF(wsz));
	ZeroMemory(lpString, nMaxCount);
	WideCharToMultiByte(uCP, 0, wsz, -1, lpString, nMaxCount - 2, NULL, NULL);
	return uRet;
}

UINT SetDlgItemTextA2W(UINT uCP, HWND hDlg, int nIDDlgItem, LPSTR lpString) {
	WCHAR wsz[1024] = L"";
	MultiByteToWideChar(uCP, 0, lpString, -1, wsz, COUNTOF(wsz));
	return SetDlgItemText(hDlg, nIDDlgItem, wsz);
}

LRESULT ComboBox_AddStringA2W(UINT uCP, HWND hwnd, LPCSTR lpString) {
	WCHAR wsz[1024] = L"";
	MultiByteToWideChar(uCP, 0, lpString, -1, wsz, COUNTOF(wsz));
	return SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)wsz);
}

//=============================================================================
//
// CodePageFromCharSet()
//
UINT CodePageFromCharSet(UINT uCharSet) {
	CHARSETINFO ci;
	if (TranslateCharsetInfo((DWORD *)(UINT_PTR)uCharSet, &ci, TCI_SRCCHARSET)) {
		return ci.ciACP;
	}
	return GetACP();
}

//=============================================================================
//
// MRU functions
//
LPMRULIST MRU_Create(LPCWSTR pszRegKey, int iFlags, int iSize) {
	LPMRULIST pmru = NP2HeapAlloc(sizeof(MRULIST));
	pmru->szRegKey = pszRegKey;
	pmru->iFlags = iFlags;
	pmru->iSize = min_i(iSize, MRU_MAXITEMS);
	return pmru;
}

BOOL MRU_Destroy(LPMRULIST pmru) {
	for (int i = 0; i < pmru->iSize; i++) {
		if (pmru->pszItems[i]) {
			LocalFree(pmru->pszItems[i]);
		}
	}

	ZeroMemory(pmru, sizeof(MRULIST));
	NP2HeapFree(pmru);
	return TRUE;
}

int MRU_Compare(LPMRULIST pmru, LPCWSTR psz1, LPCWSTR psz2) {
	return (pmru->iFlags & MRU_NOCASE) ? StrCmpI(psz1, psz2) : StrCmp(psz1, psz2);
}

BOOL MRU_Add(LPMRULIST pmru, LPCWSTR pszNew) {
	int i;
	for (i = 0; i < pmru->iSize; i++) {
		if (MRU_Compare(pmru, pmru->pszItems[i], pszNew) == 0) {
			LocalFree(pmru->pszItems[i]);
			break;
		}
	}
	i = min_i(i, pmru->iSize - 1);
	for (; i > 0; i--) {
		pmru->pszItems[i] = pmru->pszItems[i - 1];
	}
	pmru->pszItems[0] = StrDup(pszNew);
	return TRUE;
}

BOOL MRU_AddFile(LPMRULIST pmru, LPCWSTR pszFile, BOOL bRelativePath, BOOL bUnexpandMyDocs) {
	int i;
	for (i = 0; i < pmru->iSize; i++) {
		if (pmru->pszItems[i] == NULL) {
			break;
		}
		if (StrCaseEqual(pmru->pszItems[i], pszFile)) {
			LocalFree(pmru->pszItems[i]);
			break;
		}
		{
			WCHAR wchItem[MAX_PATH];
			PathAbsoluteFromApp(pmru->pszItems[i], wchItem, COUNTOF(wchItem), TRUE);
			if (StrCaseEqual(wchItem, pszFile)) {
				LocalFree(pmru->pszItems[i]);
				break;
			}
		}
	}
	i = min_i(i, pmru->iSize - 1);
	for (; i > 0; i--) {
		pmru->pszItems[i] = pmru->pszItems[i - 1];
	}

	if (bRelativePath) {
		WCHAR wchFile[MAX_PATH];
		PathRelativeToApp(pszFile, wchFile, COUNTOF(wchFile), TRUE, TRUE, bUnexpandMyDocs);
		pmru->pszItems[0] = StrDup(wchFile);
	} else {
		pmru->pszItems[0] = StrDup(pszFile);
	}

	/* notepad2-mod custom code start */
	// Needed to make Win7 jump lists work when NP2 is not explicitly associated
	if (IsWin7AndAbove()) {
		SHAddToRecentDocs(SHARD_PATHW, pszFile);
	}
	/* notepad2-mod custom code end */

	return TRUE;
}

BOOL MRU_Delete(LPMRULIST pmru, int iIndex) {
	if (iIndex < 0 || iIndex > pmru->iSize - 1) {
		return 0;
	}
	if (pmru->pszItems[iIndex]) {
		LocalFree(pmru->pszItems[iIndex]);
	}
	for (int i = iIndex; i < pmru->iSize - 1; i++) {
		pmru->pszItems[i] = pmru->pszItems[i + 1];
		pmru->pszItems[i + 1] = NULL;
	}
	return TRUE;
}

BOOL MRU_DeleteFileFromStore(LPMRULIST pmru, LPCWSTR pszFile) {
	int i = 0;
	WCHAR wchItem[MAX_PATH];

	LPMRULIST pmruStore = MRU_Create(pmru->szRegKey, pmru->iFlags, pmru->iSize);
	MRU_Load(pmruStore);

	while (MRU_Enum(pmruStore, i, wchItem, COUNTOF(wchItem)) != -1) {
		PathAbsoluteFromApp(wchItem, wchItem, COUNTOF(wchItem), TRUE);
		if (StrCaseEqual(wchItem, pszFile)) {
			MRU_Delete(pmruStore, i);
		} else {
			i++;
		}
	}

	MRU_Save(pmruStore);
	MRU_Destroy(pmruStore);
	return 1;
}

BOOL MRU_Empty(LPMRULIST pmru) {
	for (int i = 0; i < pmru->iSize; i++) {
		if (pmru->pszItems[i]) {
			LocalFree(pmru->pszItems[i]);
			pmru->pszItems[i] = NULL;
		}
	}
	return TRUE;
}

int MRU_Enum(LPMRULIST pmru, int iIndex, LPWSTR pszItem, int cchItem) {
	if (pszItem == NULL || cchItem == 0) {
		int i = 0;
		while (i < pmru->iSize && pmru->pszItems[i]) {
			i++;
		}
		return i;
	}

	if (iIndex < 0 || iIndex > pmru->iSize - 1 || !pmru->pszItems[iIndex]) {
		return -1;
	}
	lstrcpyn(pszItem, pmru->pszItems[iIndex], cchItem);
	return TRUE;
}

BOOL MRU_Load(LPMRULIST pmru) {
	IniSection section;
	WCHAR *pIniSectionBuf = NP2HeapAlloc(sizeof(WCHAR) * MAX_INI_SECTION_SIZE_MRU);
	const int cchIniSection = (int)NP2HeapSize(pIniSectionBuf) / sizeof(WCHAR);
	IniSection *pIniSection = &section;

	MRU_Empty(pmru);
	IniSectionInit(pIniSection, MRU_MAXITEMS);

	LoadIniSection(pmru->szRegKey, pIniSectionBuf, cchIniSection);
	IniSectionParseArray(pIniSection, pIniSectionBuf);
	const int count = pIniSection->count;
	const int size = pmru->iSize;

	for (int i = 0, n = 0; i < count && n < size; i++) {
		const IniKeyValueNode *node = &pIniSection->nodeList[i];
		LPCWSTR tchItem = node->value;
		if (StrNotEmpty(tchItem)) {
			/*if (pmru->iFlags & MRU_UTF8) {
				WCHAR wchItem[1024];
				int cbw = MultiByteToWideChar(CP_UTF7, 0, tchItem, -1, wchItem, COUNTOF(wchItem));
				WideCharToMultiByte(CP_UTF8, 0, wchItem, cbw, tchItem, COUNTOF(tchItem), NULL, NULL);
				pmru->pszItems[n++] = StrDup(tchItem);
			}
			else*/
			pmru->pszItems[n++] = StrDup(tchItem);
		}
	}

	IniSectionFree(pIniSection);
	NP2HeapFree(pIniSectionBuf);
	return TRUE;
}

BOOL MRU_Save(LPMRULIST pmru) {
	if (MRU_GetCount(pmru) == 0) {
		IniClearSection(pmru->szRegKey);
		return TRUE;
	}

	WCHAR tchName[16];
	IniSectionOnSave section;
	WCHAR *pIniSectionBuf = NP2HeapAlloc(sizeof(WCHAR) * MAX_INI_SECTION_SIZE_MRU);
	IniSectionOnSave *pIniSection = &section;
	pIniSection->next = pIniSectionBuf;

	for (int i = 0; i < pmru->iSize; i++) {
		if (StrNotEmpty(pmru->pszItems[i])) {
			wsprintf(tchName, L"%02i", i + 1);
			/*if (pmru->iFlags & MRU_UTF8) {
				WCHAR	 tchItem[1024];
				WCHAR wchItem[1024];
				int cbw = MultiByteToWideChar(CP_UTF8, 0, pmru->pszItems[i], -1, wchItem, COUNTOF(wchItem));
				WideCharToMultiByte(CP_UTF7, 0, wchItem, cbw, tchItem, COUNTOF(tchItem), NULL, NULL);
				IniSectionSetString(pIniSection, tchName, tchItem);
			}
			else*/
			IniSectionSetString(pIniSection, tchName, pmru->pszItems[i]);
		}
	}

	SaveIniSection(pmru->szRegKey, pIniSectionBuf);
	NP2HeapFree(pIniSectionBuf);
	return TRUE;
}

BOOL MRU_MergeSave(LPMRULIST pmru, BOOL bAddFiles, BOOL bRelativePath, BOOL bUnexpandMyDocs) {
	LPMRULIST pmruBase = MRU_Create(pmru->szRegKey, pmru->iFlags, pmru->iSize);
	MRU_Load(pmruBase);

	if (bAddFiles) {
		for (int i = pmru->iSize - 1; i >= 0; i--) {
			if (pmru->pszItems[i]) {
				WCHAR wchItem[MAX_PATH];
				PathAbsoluteFromApp(pmru->pszItems[i], wchItem, COUNTOF(wchItem), TRUE);
				MRU_AddFile(pmruBase, wchItem, bRelativePath, bUnexpandMyDocs);
			}
		}
	} else {
		for (int i = pmru->iSize - 1; i >= 0; i--) {
			if (pmru->pszItems[i]) {
				MRU_Add(pmruBase, pmru->pszItems[i]);
			}
		}
	}

	MRU_Save(pmruBase);
	MRU_Destroy(pmruBase);
	return TRUE;
}

/*

 Themed Dialogs
 Modify dialog templates to use current theme font
 Based on code of MFC helper class CDialogTemplate

*/
BOOL GetThemedDialogFont(LPWSTR lpFaceName, WORD *wSize) {
	BOOL bSucceed = FALSE;

	HDC hDC = GetDC(NULL);
	const int iLogPixelsY = GetDeviceCaps(hDC, LOGPIXELSY);
	ReleaseDC(NULL, hDC);

	if (hModUxTheme) {
		if ((BOOL)(GetProcAddress(hModUxTheme, "IsAppThemed"))()) {
			HTHEME hTheme = (HTHEME)(INT_PTR)(GetProcAddress(hModUxTheme, "OpenThemeData"))(NULL, L"WINDOWSTYLE;WINDOW");
			if (hTheme) {
				LOGFONT lf;
				if (S_OK == (HRESULT)(GetProcAddress(hModUxTheme, "GetThemeSysFont"))(hTheme, /*TMT_MSGBOXFONT*/805, &lf)) {
					if (lf.lfHeight < 0) {
						lf.lfHeight = -lf.lfHeight;
					}
					*wSize = (WORD)MulDiv(lf.lfHeight, 72, iLogPixelsY);
					if (*wSize == 0) {
						*wSize = 8;
					}
					lstrcpyn(lpFaceName, lf.lfFaceName, LF_FACESIZE);
					bSucceed = TRUE;
				}
				(GetProcAddress(hModUxTheme, "CloseThemeData"))(hTheme);
			}
		}
	}

	/*
	if (!bSucceed) {
		NONCLIENTMETRICS ncm;
		ncm.cbSize = sizeof(NONCLIENTMETRICS);
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
		if (ncm.lfMessageFont.lfHeight < 0)
			ncm.lfMessageFont.lfHeight = -ncm.lfMessageFont.lfHeight;
		*wSize = (WORD)MulDiv(ncm.lfMessageFont.lfHeight, 72, iLogPixelsY);
		if (*wSize == 0)
			*wSize = 8;
		lstrcpyn(lpFaceName, ncm.lfMessageFont.lfFaceName, LF_FACESIZE);
	}*/

	return bSucceed;
}

static inline BOOL DialogTemplate_IsDialogEx(const DLGTEMPLATE *pTemplate) {
	return ((DLGTEMPLATEEX *)pTemplate)->signature == 0xFFFF;
}

static inline BOOL DialogTemplate_HasFont(const DLGTEMPLATE *pTemplate) {
	return (DS_SETFONT & (DialogTemplate_IsDialogEx(pTemplate) ? ((DLGTEMPLATEEX *)pTemplate)->style : pTemplate->style));
}

static inline int DialogTemplate_FontAttrSize(BOOL bDialogEx) {
	return (int)sizeof(WORD) * (bDialogEx ? 3 : 1);
}

static inline BYTE *DialogTemplate_GetFontSizeField(const DLGTEMPLATE *pTemplate) {
	const BOOL bDialogEx = DialogTemplate_IsDialogEx(pTemplate);
	WORD *pw;

	if (bDialogEx) {
		pw = (WORD *)((DLGTEMPLATEEX *)pTemplate + 1);
	} else {
		pw = (WORD *)(pTemplate + 1);
	}

	if (*pw == (WORD) - 1) {
		pw += 2;
	} else {
		while (*pw++){}
	}

	if (*pw == (WORD) - 1) {
		pw += 2;
	} else {
		while (*pw++){}
	}

	while (*pw++){}

	return (BYTE *)pw;
}

DLGTEMPLATE *LoadThemedDialogTemplate(LPCTSTR lpDialogTemplateID, HINSTANCE hInstance) {
	HRSRC hRsrc = FindResource(hInstance, lpDialogTemplateID, RT_DIALOG);
	if (hRsrc == NULL) {
		return NULL;
	}

	HGLOBAL hRsrcMem = LoadResource(hInstance, hRsrc);
	DLGTEMPLATE *pRsrcMem = (DLGTEMPLATE *)LockResource(hRsrcMem);
	const UINT dwTemplateSize = (UINT)SizeofResource(hInstance, hRsrc);

	DLGTEMPLATE *pTemplate = dwTemplateSize ? NP2HeapAlloc(dwTemplateSize + LF_FACESIZE * 2) : NULL;
	if (pTemplate == NULL) {
		UnlockResource(hRsrcMem);
		FreeResource(hRsrcMem);
		return NULL;
	}

	CopyMemory((BYTE *)pTemplate, pRsrcMem, (size_t)dwTemplateSize);
	UnlockResource(hRsrcMem);
	FreeResource(hRsrcMem);

	WCHAR wchFaceName[LF_FACESIZE];
	WORD wFontSize;
	if (!GetThemedDialogFont(wchFaceName, &wFontSize)) {
		return pTemplate;
	}

	const BOOL bDialogEx = DialogTemplate_IsDialogEx(pTemplate);
	const BOOL bHasFont = DialogTemplate_HasFont(pTemplate);
	const int cbFontAttr = DialogTemplate_FontAttrSize(bDialogEx);

	if (bDialogEx) {
		((DLGTEMPLATEEX *)pTemplate)->style |= DS_SHELLFONT;
	} else {
		pTemplate->style |= DS_SHELLFONT;
	}

	const int cbNew = cbFontAttr + ((lstrlen(wchFaceName) + 1) * sizeof(WCHAR));
	const BYTE *pbNew = (BYTE *)wchFaceName;

	BYTE *pb = DialogTemplate_GetFontSizeField(pTemplate);
	const int cbOld = (int)(bHasFont ? cbFontAttr + 2 * (lstrlen((WCHAR *)(pb + cbFontAttr)) + 1) : 0);

	const BYTE *pOldControls = (BYTE *)(((DWORD_PTR)pb + cbOld + 3) & ~(DWORD_PTR)3);
	BYTE *pNewControls = (BYTE *)(((DWORD_PTR)pb + cbNew + 3) & ~(DWORD_PTR)3);

	const WORD nCtrl = bDialogEx ? (WORD)((DLGTEMPLATEEX *)pTemplate)->cDlgItems : (WORD)pTemplate->cdit;
	if (cbNew != cbOld && nCtrl > 0) {
		MoveMemory(pNewControls, pOldControls, (size_t)(dwTemplateSize - (pOldControls - (BYTE *)pTemplate)));
	}

	*(WORD *)pb = wFontSize;
	MoveMemory(pb + cbFontAttr, pbNew, (size_t)(cbNew - cbFontAttr));

	return pTemplate;
}

INT_PTR ThemedDialogBoxParam(HINSTANCE hInstance, LPCTSTR lpTemplate,
							 HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
	DLGTEMPLATE *pDlgTemplate = LoadThemedDialogTemplate(lpTemplate, hInstance);
	const INT_PTR ret = DialogBoxIndirectParam(hInstance, pDlgTemplate, hWndParent, lpDialogFunc, dwInitParam);
	if (pDlgTemplate) {
		NP2HeapFree(pDlgTemplate);
	}

	return ret;
}

HWND CreateThemedDialogParam(HINSTANCE hInstance, LPCTSTR lpTemplate,
							 HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
	DLGTEMPLATE *pDlgTemplate = LoadThemedDialogTemplate(lpTemplate, hInstance);
	HWND hwnd = CreateDialogIndirectParam(hInstance, pDlgTemplate, hWndParent, lpDialogFunc, dwInitParam);
	if (pDlgTemplate) {
		NP2HeapFree(pDlgTemplate);
	}

	return hwnd;
}

/******************************************************************************
*
* UnSlash functions
* Mostly taken from SciTE, (c) Neil Hodgson, http://www.scintilla.org
*
*/

/**
 * Is the character an octal digit?
 */
static inline BOOL IsOctalDigit(char ch) {
	return ch >= '0' && ch <= '7';
}

/**
 * If the character is an hexa digit, get its value.
 */
static inline int GetHexDigit(char ch) {
	if (ch >= '0' && ch <= '9') {
		return ch - '0';
	}
	if (ch >= 'A' && ch <= 'F') {
		return ch - 'A' + 10;
	}
	if (ch >= 'a' && ch <= 'f') {
		return ch - 'a' + 10;
	}
	return -1;
}

/**
 * Convert C style \a, \b, \f, \n, \r, \t, \v, \xhh and \uhhhh into their indicated characters.
 */
unsigned int UnSlash(char *s, UINT cpEdit) {
	char *sStart = s;
	char *o = s;

	while (*s) {
		if (*s == '\\') {
			s++;
			if (*s == 'a') {
				*o = '\a';
			} else if (*s == 'b') {
				*o = '\b';
			} else if (*s == 'f') {
				*o = '\f';
			} else if (*s == 'n') {
				*o = '\n';
			} else if (*s == 'r') {
				*o = '\r';
			} else if (*s == 't') {
				*o = '\t';
			} else if (*s == 'v') {
				*o = '\v';
			} else if (*s == 'x' || *s == 'u') {
				const BOOL bShort = (*s == 'x');
				char ch[8];
				const char *pch = ch;
				WCHAR val[2] = L"";
				int hex;
				val[0] = 0;
				hex = GetHexDigit(*(s + 1));
				if (hex >= 0) {
					int value = hex;
					s++;
					hex = GetHexDigit(*(s + 1));
					if (hex >= 0) {
						s++;
						value = (value << 4) | hex;
						if (!bShort) {
							hex = GetHexDigit(*(s + 1));
							if (hex >= 0) {
								s++;
								value = (value << 4) | hex;
								hex = GetHexDigit(*(s + 1));
								if (hex >= 0) {
									s++;
									value = (value << 4) | hex;
								}
							}
						}
					}
					if (value) {
						val[0] = (WCHAR)value;
						val[1] = 0;
						WideCharToMultiByte(cpEdit, 0, val, -1, ch, COUNTOF(ch), NULL, NULL);
						*o = *pch++;
						while (*pch) {
							*++o = *pch++;
						}
					} else {
						o--;
					}
				} else {
					o--;
				}
			} else {
				*o = *s;
			}
		} else {
			*o = *s;
		}
		o++;
		if (*s) {
			s++;
		}
	}
	*o = '\0';
	return (unsigned int)(o - sStart);
}

/**
 * Convert C style \0oo into their indicated characters.
 * This is used to get control characters into the regular expresion engine.
 */
unsigned int UnSlashLowOctal(char *s) {
	const char *sStart = s;
	char *o = s;
	while (*s) {
		if ((s[0] == '\\') && (s[1] == '0') && IsOctalDigit(s[2]) && IsOctalDigit(s[3])) {
			*o = (char)(8 * (s[2] - '0') + (s[3] - '0'));
			s += 3;
		} else {
			*o = *s;
		}
		o++;
		if (*s) {
			s++;
		}
	}
	*o = '\0';
	return (unsigned int)(o - sStart);
}

void TransformBackslashes(char *pszInput, BOOL bRegEx, UINT cpEdit) {
	if (bRegEx) {
		UnSlashLowOctal(pszInput);
	} else {
		UnSlash(pszInput, cpEdit);
	}
}

BOOL AddBackslash(char *pszOut, const char *pszInput) {
	BOOL hasEscapeChar = FALSE;
	BOOL hasSlash = FALSE;
	char *lpszEsc = pszOut;
	const char *lpsz = pszInput;
	while (*lpsz) {
		switch (*lpsz) {
		case '\n':
			*lpszEsc++ = '\\';
			*lpszEsc++ = 'n';
			hasEscapeChar = TRUE;
			break;
		case '\r':
			*lpszEsc++ = '\\';
			*lpszEsc++ = 'r';
			hasEscapeChar = TRUE;
			break;
		case '\t':
			*lpszEsc++ = '\\';
			*lpszEsc++ = 't';
			hasEscapeChar = TRUE;
			break;
		case '\\':
			*lpszEsc++ = '\\';
			*lpszEsc++ = '\\';
			hasSlash = TRUE;
			break;
		case '\f':
			*lpszEsc++ = '\\';
			*lpszEsc++ = 'f';
			hasEscapeChar = TRUE;
			break;
		case '\v':
			*lpszEsc++ = '\\';
			*lpszEsc++ = 'v';
			hasEscapeChar = TRUE;
			break;
		case '\a':
			*lpszEsc++ = '\\';
			*lpszEsc++ = 'b';
			hasEscapeChar = TRUE;
			break;
		case '\b':
			*lpszEsc++ = '\\';
			*lpszEsc++ = 'a';
			hasEscapeChar = TRUE;
			break;
		default:
			*lpszEsc++ = *lpsz;
			break;
		}
		lpsz++;
	}

	if (hasSlash && !hasEscapeChar) {
		strcpy(pszOut, pszInput);
	}
	return hasEscapeChar;
}

/*

 MinimizeToTray - Copyright 2000 Matthew Ellis <m.t.ellis@bigfoot.com>

 Changes made by flo:
 - Commented out: #include "stdafx.h"
 - Moved variable declaration: APPBARDATA appBarData;

*/

// MinimizeToTray
//
// A couple of routines to show how to make it produce a custom caption
// animation to make it look like we are minimizing to and maximizing
// from the system tray
//
// These routines are public domain, but it would be nice if you dropped
// me a line if you use them!
//
// 1.0 29.06.2000 Initial version
// 1.1 01.07.2000 The window retains it's place in the Z-order of windows
//    when minimized/hidden. This means that when restored/shown, it doen't
//    always appear as the foreground window unless we call SetForegroundWindow
//
// Copyright 2000 Matthew Ellis <m.t.ellis@bigfoot.com>
/*#include "stdafx.h"*/

// Odd. VC++6 winuser.h has IDANI_CAPTION defined (as well as IDANI_OPEN and
// IDANI_CLOSE), but the Platform SDK only has IDANI_OPEN...

// I don't know what IDANI_OPEN or IDANI_CLOSE do. Trying them in this code
// produces nothing. Perhaps they were intended for window opening and closing
// like the MAC provides...
#ifndef IDANI_OPEN
#define IDANI_OPEN 1
#endif
#ifndef IDANI_CLOSE
#define IDANI_CLOSE 2
#endif
#ifndef IDANI_CAPTION
#define IDANI_CAPTION 3
#endif

#define DEFAULT_RECT_WIDTH 150
#define DEFAULT_RECT_HEIGHT 30

// Returns the rect of where we think the system tray is. This will work for
// all current versions of the shell. If explorer isn't running, we try our
// best to work with a 3rd party shell. If we still can't find anything, we
// return a rect in the lower right hand corner of the screen
static VOID GetTrayWndRect(LPRECT lpTrayRect) {
	// First, we'll use a quick hack method. We know that the taskbar is a window
	// of class Shell_TrayWnd, and the status tray is a child of this of class
	// TrayNotifyWnd. This provides us a window rect to minimize to. Note, however,
	// that this is not guaranteed to work on future versions of the shell. If we
	// use this method, make sure we have a backup!
	HWND hShellTrayWnd = FindWindowEx(NULL, NULL, TEXT("Shell_TrayWnd"), NULL);
	if (hShellTrayWnd) {
		HWND hTrayNotifyWnd = FindWindowEx(hShellTrayWnd, NULL, TEXT("TrayNotifyWnd"), NULL);
		if (hTrayNotifyWnd) {
			GetWindowRect(hTrayNotifyWnd, lpTrayRect);
			return;
		}
	}

	// OK, we failed to get the rect from the quick hack. Either explorer isn't
	// running or it's a new version of the shell with the window class names
	// changed (how dare Microsoft change these undocumented class names!) So, we
	// try to find out what side of the screen the taskbar is connected to. We
	// know that the system tray is either on the right or the bottom of the
	// taskbar, so we can make a good guess at where to minimize to
	APPBARDATA appBarData;
	appBarData.cbSize = sizeof(appBarData);
	if (SHAppBarMessage(ABM_GETTASKBARPOS, &appBarData)) {
		// We know the edge the taskbar is connected to, so guess the rect of the
		// system tray. Use various fudge factor to make it look good
		switch (appBarData.uEdge) {
		case ABE_LEFT:
		case ABE_RIGHT:
			// We want to minimize to the bottom of the taskbar
			lpTrayRect->top = appBarData.rc.bottom - 100;
			lpTrayRect->bottom = appBarData.rc.bottom - 16;
			lpTrayRect->left = appBarData.rc.left;
			lpTrayRect->right = appBarData.rc.right;
			break;

		case ABE_TOP:
		case ABE_BOTTOM:
			// We want to minimize to the right of the taskbar
			lpTrayRect->top = appBarData.rc.top;
			lpTrayRect->bottom = appBarData.rc.bottom;
			lpTrayRect->left = appBarData.rc.right - 100;
			lpTrayRect->right = appBarData.rc.right - 16;
			break;
		}

		return;
	}

	// Blimey, we really aren't in luck. It's possible that a third party shell
	// is running instead of explorer. This shell might provide support for the
	// system tray, by providing a Shell_TrayWnd window (which receives the
	// messages for the icons) So, look for a Shell_TrayWnd window and work out
	// the rect from that. Remember that explorer's taskbar is the Shell_TrayWnd,
	// and stretches either the width or the height of the screen. We can't rely
	// on the 3rd party shell's Shell_TrayWnd doing the same, in fact, we can't
	// rely on it being any size. The best we can do is just blindly use the
	// window rect, perhaps limiting the width and height to, say 150 square.
	// Note that if the 3rd party shell supports the same configuraion as
	// explorer (the icons hosted in NotifyTrayWnd, which is a child window of
	// Shell_TrayWnd), we would already have caught it above
	hShellTrayWnd = FindWindowEx(NULL, NULL, TEXT("Shell_TrayWnd"), NULL);
	if (hShellTrayWnd) {
		GetWindowRect(hShellTrayWnd, lpTrayRect);
		if (lpTrayRect->right - lpTrayRect->left > DEFAULT_RECT_WIDTH) {
			lpTrayRect->left = lpTrayRect->right - DEFAULT_RECT_WIDTH;
		}
		if (lpTrayRect->bottom - lpTrayRect->top > DEFAULT_RECT_HEIGHT) {
			lpTrayRect->top = lpTrayRect->bottom - DEFAULT_RECT_HEIGHT;
		}

		return;
	}

	// OK. Haven't found a thing. Provide a default rect based on the current work
	// area
	SystemParametersInfo(SPI_GETWORKAREA, 0, lpTrayRect, 0);
	lpTrayRect->left = lpTrayRect->right - DEFAULT_RECT_WIDTH;
	lpTrayRect->top = lpTrayRect->bottom - DEFAULT_RECT_HEIGHT;
}

// Check to see if the animation has been disabled
/*static */BOOL GetDoAnimateMinimize(VOID) {
	ANIMATIONINFO ai;

	ai.cbSize = sizeof(ai);
	SystemParametersInfo(SPI_GETANIMATION, sizeof(ai), &ai, 0);

	return ai.iMinAnimate != 0;
}

VOID MinimizeWndToTray(HWND hwnd) {
	if (GetDoAnimateMinimize()) {
		RECT rcFrom, rcTo;

		// Get the rect of the window. It is safe to use the rect of the whole
		// window - DrawAnimatedRects will only draw the caption
		GetWindowRect(hwnd, &rcFrom);
		GetTrayWndRect(&rcTo);

		// Get the system to draw our animation for us
		DrawAnimatedRects(hwnd, IDANI_CAPTION, &rcFrom, &rcTo);
	}

	// Add the tray icon. If we add it before the call to DrawAnimatedRects,
	// the taskbar gets erased, but doesn't get redrawn until DAR finishes.
	// This looks untidy, so call the functions in this order

	// Hide the window
	ShowWindow(hwnd, SW_HIDE);
}

VOID RestoreWndFromTray(HWND hwnd) {
	if (GetDoAnimateMinimize()) {
		// Get the rect of the tray and the window. Note that the window rect
		// is still valid even though the window is hidden
		RECT rcFrom, rcTo;
		GetTrayWndRect(&rcFrom);
		GetWindowRect(hwnd, &rcTo);

		// Get the system to draw our animation for us
		DrawAnimatedRects(hwnd, IDANI_CAPTION, &rcFrom, &rcTo);
	}

	// Show the window, and make sure we're the foreground window
	ShowWindow(hwnd, SW_SHOW);
	SetActiveWindow(hwnd);
	SetForegroundWindow(hwnd);

	// Remove the tray icon. As described above, remove the icon after the
	// call to DrawAnimatedRects, or the taskbar will not refresh itself
	// properly until DAR finished
}

// End of Helpers.c
