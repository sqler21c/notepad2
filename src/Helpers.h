/******************************************************************************
*
*
* Notepad2
*
* Helpers.h
*   Definitions for general helper functions and macros
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

#ifndef NOTEPAD2_HELPERS_H_
#define NOTEPAD2_HELPERS_H_
#include "compiler.h"

NP2_inline int min_i(int x, int y) {
	return (x < y) ? x : y;
}

NP2_inline int max_i(int x, int y) {
	return (x > y) ? x : y;
}

NP2_inline UINT min_u(UINT x, UINT y) {
	return (x < y) ? x : y;
}

NP2_inline UINT max_u(UINT x, UINT y) {
	return (x > y) ? x : y;
}

NP2_inline long max_l(long x, long y) {
	return (x > y) ? x : y;
}

NP2_inline int clamp_i(int x, int lower, int upper) {
	return (x < lower) ? lower : (x > upper) ? upper : x;
}

NP2_inline BOOL StrIsEmptyA(LPCSTR s) {
	return s == NULL || *s == '\0';
}

NP2_inline BOOL StrIsEmpty(LPCWSTR s) {
	return s == NULL || *s == L'\0';
}

NP2_inline BOOL StrNotEmptyA(LPCSTR s) {
	return s != NULL && *s != '\0';
}

NP2_inline BOOL StrNotEmpty(LPCWSTR s) {
	return s != NULL && *s != L'\0';
}

NP2_inline BOOL StrEqual(LPCWSTR s1, LPCWSTR s2) {
	return wcscmp(s1, s2) == 0;
}

NP2_inline BOOL StrCaseEqual(LPCWSTR s1, LPCWSTR s2) {
	return _wcsicmp(s1, s2) == 0;
}

NP2_inline BOOL StrNEqual(LPCWSTR s1, LPCWSTR s2, int cch) {
	return wcsncmp(s1, s2, cch) == 0;
}

NP2_inline BOOL StrNCaseEqual(LPCWSTR s1, LPCWSTR s2, int cch) {
	return _wcsnicmp(s1, s2, cch) == 0;
}

// str MUST NOT be NULL, can be empty
NP2_inline BOOL StrToFloat(LPCWSTR str, float *value) {
	LPWSTR end;
	*value = wcstof(str, &end);
	return str != end;
}

NP2_inline BOOL CRTStrToInt(LPCWSTR str, int *value) {
	LPWSTR end;
	*value = (int)wcstol(str, &end, 10);
	return str != end;
}

// str MUST NOT be NULL, can be empty
NP2_inline BOOL HexStrToInt(LPCWSTR str, int *value) {
	LPWSTR end;
	*value = (int)wcstol(str, &end, 16);
	return str != end;
}

int ParseCommaList(LPCWSTR str, int result[], int count);

typedef struct StopWatch {
	LARGE_INTEGER freq; // not changed after system boot
	LARGE_INTEGER begin;
	LARGE_INTEGER end;
} StopWatch;

#define StopWatch_Start(watch) do { \
		QueryPerformanceFrequency(&(watch).freq);	\
		QueryPerformanceCounter(&(watch).begin);	\
	} while (0)

#define StopWatch_Reset(watch) do { \
		(watch).begin.QuadPart = 0;	\
		(watch).end.QuadPart = 0;	\
	} while (0)

#define StopWatch_Restart(watch) do { \
		(watch).end.QuadPart = 0;					\
		QueryPerformanceCounter(&(watch).begin);	\
	} while (0)

#define StopWatch_Stop(watch) \
	QueryPerformanceCounter(&(watch).end)

NP2_inline double StopWatch_Get(const StopWatch *watch) {
	const LONGLONG diff = watch->end.QuadPart - watch->begin.QuadPart;
	const double freq = (double)(watch->freq.QuadPart);
	return (diff / freq) * 1000;
}

void StopWatch_Show(const StopWatch *watch, LPCWSTR msg);

#ifdef NDEBUG
#define DLog(msg)
#define DLogf(fmt, ...)
#else
#define DLog(msg)	OutputDebugStringA(msg)
void DLogf(const char *fmt, ...);
#endif

extern HINSTANCE g_hInstance;
extern HANDLE g_hDefaultHeap;
extern UINT16 g_uWinVer;
extern UINT g_uCurrentDPI;
extern UINT g_uCurrentPPI;
extern WCHAR szIniFile[MAX_PATH];

// MSDN: Operating System Version

#if 0
#define IsWin2KAndAbove()	(g_uWinVer >= 0x0500)
#define IsWinXPAndAbove()	(g_uWinVer >= 0x0501)
#else
#define IsWin2KAndAbove()	TRUE
#define IsWinXPAndAbove()	TRUE
#endif
#define IsVistaAndAbove()	(g_uWinVer >= 0x0600)
#define IsWin7AndAbove()	(g_uWinVer >= 0x0601)
#define IsWin8AndAbove()	(g_uWinVer >= 0x0602)
#define IsWin8p1AndAbove()	(g_uWinVer >= 0x0603)
#define IsWin10AndAbove()	(g_uWinVer >= 0x0A00)

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

// High DPI Reference
// https://msdn.microsoft.com/en-us/library/windows/desktop/hh447398(v=vs.85).aspx
#ifndef WM_DPICHANGED
#define WM_DPICHANGED	0x02E0
#endif
#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI		96
#endif

#define RoundToCurrentDPI(value)	((g_uCurrentDPI*(value) + USER_DEFAULT_SCREEN_DPI/2) / USER_DEFAULT_SCREEN_DPI)
#define ScaleFontSize(value)		MulDiv(g_uCurrentDPI, (value), g_uCurrentPPI)

// https://docs.microsoft.com/en-us/windows/desktop/Memory/comparing-memory-allocation-methods
// https://blogs.msdn.microsoft.com/oldnewthing/20120316-00/?p=8083/
#define NP2HeapAlloc(size)			HeapAlloc(g_hDefaultHeap, HEAP_ZERO_MEMORY, (size))
#define NP2HeapFree(hMem)			HeapFree(g_hDefaultHeap, 0, (hMem))
#define NP2HeapSize(hMem)			HeapSize(g_hDefaultHeap, 0, (hMem))

#define IniGetString(lpSection, lpName, lpDefault, lpReturnedStr, nSize) \
	GetPrivateProfileString(lpSection, lpName, lpDefault, lpReturnedStr, nSize, szIniFile)
#define IniGetInt(lpSection, lpName, nDefault) \
	GetPrivateProfileInt(lpSection, lpName, nDefault, szIniFile)
#define IniSetString(lpSection, lpName, lpString) \
	WritePrivateProfileString(lpSection, lpName, lpString, szIniFile)
#define IniDeleteSection(lpSection) \
	WritePrivateProfileSection(lpSection, NULL, szIniFile)
#define IniClearSection(lpSection) \
	WritePrivateProfileSection(lpSection, L"", szIniFile)

NP2_inline void IniSetInt(LPCWSTR lpSection, LPCWSTR lpName, int i) {
	WCHAR tch[16];
	wsprintf(tch, L"%i", i);
	IniSetString(lpSection, lpName, tch);
}

NP2_inline void IniSetBool(LPCWSTR lpSection, LPCWSTR lpName, BOOL b) {
	IniSetString(lpSection, lpName, (b ? L"1" : L"0"));
}

#define LoadIniSection(lpSection, lpBuf, cchBuf) \
	GetPrivateProfileSection(lpSection, lpBuf, cchBuf, szIniFile);
#define SaveIniSection(lpSection, lpBuf) \
	WritePrivateProfileSection(lpSection, lpBuf, szIniFile)

struct IniKeyValueNode;
typedef struct IniKeyValueNode {
	struct IniKeyValueNode *next;
	UINT hash;
	LPCWSTR key;
	LPCWSTR value;
} IniKeyValueNode;

// https://en.wikipedia.org/wiki/Sentinel_node
// https://en.wikipedia.org/wiki/Sentinel_value
#define IniSectionImplUseSentinelNode	1

typedef struct IniSection {
	int count;
	int capacity;
	IniKeyValueNode *head;
#if IniSectionImplUseSentinelNode
	IniKeyValueNode *sentinel;
#endif
	IniKeyValueNode *nodeList;
} IniSection;

NP2_inline void IniSectionInit(IniSection *section, int capacity) {
	section->count = 0;
	section->capacity = capacity;
	section->head = NULL;
#if IniSectionImplUseSentinelNode
	section->nodeList = (IniKeyValueNode *)NP2HeapAlloc((capacity + 1) * sizeof(IniKeyValueNode));
	section->sentinel = &section->nodeList[capacity];
#else
	section->nodeList = (IniKeyValueNode *)NP2HeapAlloc(capacity * sizeof(IniKeyValueNode));
#endif
}

NP2_inline void IniSectionFree(IniSection *section) {
	NP2HeapFree(section->nodeList);
}

NP2_inline void IniSectionClear(IniSection *section) {
	section->count = 0;
	section->head = NULL;
}

NP2_inline BOOL IniSectionIsEmpty(const IniSection *section) {
	return section->count == 0;
}

BOOL IniSectionParseArray(IniSection *section, LPWSTR lpCachedIniSection);
BOOL IniSectionParse(IniSection *section, LPWSTR lpCachedIniSection);
LPCWSTR IniSectionUnsafeGetValue(IniSection *section, LPCWSTR key, int keyLen);

NP2_inline LPCWSTR IniSectionGetValueImpl(IniSection *section, LPCWSTR key, int keyLen) {
	return section->count ? IniSectionUnsafeGetValue(section, key, keyLen) : NULL;
}

void IniSectionGetStringImpl(IniSection *section, LPCWSTR key, int keyLen, LPCWSTR lpDefault, LPWSTR lpReturnedString, int cchReturnedString);
int IniSectionGetIntImpl(IniSection *section, LPCWSTR key, int keyLen, int iDefault);
BOOL IniSectionGetBoolImpl(IniSection *section, LPCWSTR key, int keyLen, BOOL bDefault);


#define IniSectionGetValue(section, key) \
	IniSectionGetValueImpl(section, key, CSTRLEN(key))
#define IniSectionGetInt(section, key, iDefault) \
	IniSectionGetIntImpl(section, key, CSTRLEN(key), (iDefault))
#define IniSectionGetBool(section, key, bDefault) \
	IniSectionGetBoolImpl(section, key, CSTRLEN(key), (bDefault))
#define IniSectionGetString(section, key, lpDefault, lpReturnedString, cchReturnedString) \
	IniSectionGetStringImpl(section, key, CSTRLEN(key), (lpDefault), (lpReturnedString), (cchReturnedString))

NP2_inline LPCWSTR IniSectionGetValueEx(IniSection *section, LPCWSTR key) {
	return IniSectionGetValueImpl(section, key, -1);
}

NP2_inline int IniSectionGetIntEx(IniSection *section, LPCWSTR key, int iDefault) {
	return IniSectionGetIntImpl(section, key, -1, iDefault);
}

NP2_inline BOOL IniSectionGetBoolEx(IniSection *section, LPCWSTR key, BOOL bDefault) {
	return IniSectionGetBoolImpl(section, key, -1, bDefault);
}

NP2_inline void IniSectionGetStringEx(IniSection *section, LPCWSTR key, LPCWSTR lpDefault, LPWSTR lpReturnedString, int cchReturnedString) {
	IniSectionGetStringImpl(section, key, -1, lpDefault, lpReturnedString, cchReturnedString);
}

typedef struct IniSectionOnSave {
	LPWSTR next;
} IniSectionOnSave;

void IniSectionSetString(IniSectionOnSave *section, LPCWSTR key, LPCWSTR value);

NP2_inline void IniSectionSetInt(IniSectionOnSave *section, LPCWSTR key, int i) {
	WCHAR tch[16];
	wsprintf(tch, L"%i", i);
	IniSectionSetString(section, key, tch);
}

NP2_inline void IniSectionSetBool(IniSectionOnSave *section, LPCWSTR key, BOOL b) {
	IniSectionSetString(section, key, (b ? L"1" : L"0"));
}

NP2_inline void IniSectionSetStringEx(IniSectionOnSave *section, LPCWSTR key, LPCWSTR value, LPCWSTR lpDefault) {
	if (!StrCaseEqual(value, lpDefault)) {
		IniSectionSetString(section, key, value);
	}
}

NP2_inline void IniSectionSetIntEx(IniSectionOnSave *section, LPCWSTR key, int i, int iDefault) {
	if (i != iDefault) {
		IniSectionSetInt(section, key, i);
	}
}

NP2_inline void IniSectionSetBoolEx(IniSectionOnSave *section, LPCWSTR key, BOOL b, BOOL bDefault) {
	if (b != bDefault) {
		IniSectionSetString(section, key, (b ? L"1" : L"0"));
	}
}


extern HWND hwndEdit;
NP2_inline void BeginWaitCursor(void) {
	SendMessage(hwndEdit, SCI_SETCURSOR, (WPARAM)SC_CURSORWAIT, 0);
}

NP2_inline void EndWaitCursor(void) {
	POINT pt;
	SendMessage(hwndEdit, SCI_SETCURSOR, (WPARAM)SC_CURSORNORMAL, 0);
	GetCursorPos(&pt);
	SetCursorPos(pt.x, pt.y);
}

UINT GetCurrentPPI(HWND hwnd);
UINT GetCurrentDPI(HWND hwnd);
BOOL PrivateIsAppThemed(void);
HRESULT PrivateSetCurrentProcessExplicitAppUserModelID(PCWSTR AppID);
BOOL IsElevated(void);

//BOOL SetTheme(HWND hwnd, LPCWSTR lpszTheme)
//NP2_inline BOOL SetExplorerTheme(HWND hwnd) {
//	return SetTheme(hwnd, L"Explorer");
//}

HBITMAP ResizeImageForCurrentDPI(HBITMAP hbmp);
BOOL BitmapMergeAlpha(HBITMAP hbmp, COLORREF crDest);
BOOL BitmapAlphaBlend(HBITMAP hbmp, COLORREF crDest, BYTE alpha);
BOOL BitmapGrayScale(HBITMAP hbmp);
BOOL VerifyContrast(COLORREF cr1, COLORREF cr2);
BOOL IsFontAvailable(LPCWSTR lpszFontName);

void SetClipData(HWND hwnd, WCHAR *pszData);
BOOL SetWindowTitle(HWND hwnd, UINT uIDAppName, BOOL bIsElevated, UINT uIDUntitled,
					LPCWSTR lpszFile, int iFormat, BOOL bModified,
					UINT uIDReadOnly, BOOL bReadOnly, LPCWSTR lpszExcerpt);
void SetWindowTransparentMode(HWND hwnd, BOOL bTransparentMode);
void SetWindowLayoutRTL(HWND hwnd, BOOL bRTL);

void CenterDlgInParentEx(HWND hDlg, HWND hParent);
NP2_inline void CenterDlgInParent(HWND hDlg) {
	CenterDlgInParentEx(hDlg, GetParent(hDlg));
}
void SnapToDefaultButton(HWND hwndBox);

void GetDlgPos(HWND hDlg, LPINT xDlg, LPINT yDlg);
void SetDlgPos(HWND hDlg, int xDlg, int yDlg);
void ResizeDlg_Init(HWND hwnd, int cxFrame, int cyFrame, int nIdGrip);
void ResizeDlg_Destroy(HWND hwnd, int *cxFrame, int *cyFrame);
void ResizeDlg_Size(HWND hwnd, LPARAM lParam, int *cx, int *cy);
void ResizeDlg_GetMinMaxInfo(HWND hwnd, LPARAM lParam);
HDWP DeferCtlPos(HDWP hdwp, HWND hwndDlg, int nCtlId, int dx, int dy, UINT uFlags);
void MakeBitmapButton(HWND hwnd, int nCtlId, HINSTANCE hInstance, UINT uBmpId);
void MakeColorPickButton(HWND hwnd, int nCtlId, HINSTANCE hInstance, COLORREF crColor);
void DeleteBitmapButton(HWND hwnd, int nCtlId);

#define StatusSetSimple(hwnd, b)				SendMessage(hwnd, SB_SIMPLE, (b), 0)
#define StatusSetText(hwnd, nPart, lpszText)	SendMessage(hwnd, SB_SETTEXT, (nPart), (LPARAM)(lpszText))
BOOL StatusSetTextID(HWND hwnd, UINT nPart, UINT uID);
int  StatusCalcPaneWidth(HWND hwnd, LPCWSTR lpsz);

/**
 * we only have 25 commands in toolbar
 * max size = 25*(3 + 2) + 1 (each command with a separator)
 */
#define MAX_TOOLBAR_ITEM_COUNT_WITH_SEPARATOR	50
#define MAX_TOOLBAR_BUTTON_CONFIG_BUFFER_SIZE	160
int Toolbar_GetButtons(HWND hwnd, int cmdBase, LPWSTR lpszButtons, int cchButtons);
int Toolbar_SetButtons(HWND hwnd, LPCWSTR lpszButtons, LPCTBBUTTON ptbb, int ctbb);

LRESULT SendWMSize(HWND hwnd);

#define EnableCmd(hmenu, id, b)	EnableMenuItem(hmenu, id, (b)? (MF_BYCOMMAND | MF_ENABLED) : (MF_BYCOMMAND | MF_GRAYED))
#define CheckCmd(hmenu, id, b)	CheckMenuItem(hmenu, id, (b)? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED))

BOOL IsCmdEnabled(HWND hwnd, UINT uId);
#define IsButtonChecked(hwnd, uId)	(IsDlgButtonChecked(hwnd, (uId)) == BST_CHECKED)

#define GetString(id, pb, cb)	LoadString(g_hInstance, id, pb, cb)
#define StrEnd(pStart)			((pStart) + lstrlen(pStart))

/**
 * Variadic Macros
 * https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
 * https://docs.microsoft.com/en-us/cpp/preprocessor/variadic-macros?view=vs-2017
 */
#define FormatString(lpOutput, lpFormat, uIdFormat, ...) do {	\
		GetString((uIdFormat), (lpFormat), COUNTOF(lpFormat));	\
		wsprintf((lpOutput), (lpFormat), __VA_ARGS__);			\
	} while (0)

void PathRelativeToApp(LPCWSTR lpszSrc, LPWSTR lpszDest, int cchDest,
					   BOOL bSrcIsFile, BOOL bUnexpandEnv, BOOL bUnexpandMyDocs);
void PathAbsoluteFromApp(LPCWSTR lpszSrc, LPWSTR lpszDest, int cchDest, BOOL bExpandEnv);

BOOL PathIsLnkFile(LPCWSTR pszPath);
BOOL PathGetLnkPath(LPCWSTR pszLnkFile, LPWSTR pszResPath, int cchResPath);
BOOL PathIsLnkToDirectory(LPCWSTR pszPath, LPWSTR pszResPath, int cchResPath);
BOOL PathCreateDeskLnk(LPCWSTR pszDocument);
BOOL PathCreateFavLnk(LPCWSTR pszName, LPCWSTR pszTarget, LPCWSTR pszDir);

NP2_inline void TrimString(LPWSTR lpString) {
	StrTrim(lpString, L" ");
}

BOOL ExtractFirstArgument(LPCWSTR lpArgs, LPWSTR lpArg1, LPWSTR lpArg2);

void PrepareFilterStr(LPWSTR lpFilter);

void	StrTab2Space(LPWSTR lpsz);
void	PathFixBackslashes(LPWSTR lpsz);

void	ExpandEnvironmentStringsEx(LPWSTR lpSrc, DWORD dwSrc);
void	PathCanonicalizeEx(LPWSTR lpSrc);
DWORD	GetLongPathNameEx(LPWSTR lpszPath, DWORD cchBuffer);
DWORD_PTR SHGetFileInfo2(LPCWSTR pszPath, DWORD dwFileAttributes,
						 SHFILEINFO *psfi, UINT cbFileInfo, UINT uFlags);

void	FormatNumberStr(LPWSTR lpNumberStr);
BOOL	SetDlgItemIntEx(HWND hwnd, int nIdItem, UINT uValue);

UINT	GetDlgItemTextA2W(UINT uCP, HWND hDlg, int nIDDlgItem, LPSTR lpString, int nMaxCount);
UINT	SetDlgItemTextA2W(UINT uCP, HWND hDlg, int nIDDlgItem, LPSTR lpString);
LRESULT ComboBox_AddStringA2W(UINT uCP, HWND hwnd, LPCSTR lpString);

UINT CodePageFromCharSet(UINT uCharSet);

//==== MRU Functions ==========================================================
#define MRU_MAXITEMS	24
#define MRU_NOCASE		1
#define MRU_UTF8		2

// MRU_MAXITEMS * (MAX_PATH + 4)
#define MAX_INI_SECTION_SIZE_MRU	(8 * 1024)

typedef struct _mrulist {
	LPCWSTR	szRegKey;
	int		iFlags;
	int		iSize;
	LPWSTR pszItems[MRU_MAXITEMS];
} MRULIST,  *PMRULIST,  *LPMRULIST;

LPMRULIST MRU_Create(LPCWSTR pszRegKey, int iFlags, int iSize);
BOOL	MRU_Destroy(LPMRULIST pmru);
BOOL	MRU_Add(LPMRULIST pmru, LPCWSTR pszNew);
BOOL	MRU_AddFile(LPMRULIST pmru, LPCWSTR pszFile, BOOL bRelativePath, BOOL bUnexpandMyDocs);
BOOL	MRU_Delete(LPMRULIST pmru, int iIndex);
BOOL	MRU_DeleteFileFromStore(LPMRULIST pmru, LPCWSTR pszFile);
BOOL	MRU_Empty(LPMRULIST pmru);
int 	MRU_Enum(LPMRULIST pmru, int iIndex, LPWSTR pszItem, int cchItem);
NP2_inline int MRU_GetCount(LPMRULIST pmru) {
	return MRU_Enum(pmru, 0, NULL, 0);
}
BOOL	MRU_Load(LPMRULIST pmru);
BOOL	MRU_Save(LPMRULIST pmru);
BOOL	MRU_MergeSave(LPMRULIST pmru, BOOL bAddFiles, BOOL bRelativePath, BOOL bUnexpandMyDocs);

//==== Themed Dialogs =========================================================
#ifndef DLGTEMPLATEEX
#pragma pack(push,  1)
typedef struct {
	WORD	dlgVer;
	WORD	signature;
	DWORD	helpID;
	DWORD	exStyle;
	DWORD	style;
	WORD	cDlgItems;
	short	x;
	short	y;
	short	cx;
	short	cy;
} DLGTEMPLATEEX;
#pragma pack(pop)
#endif

BOOL	GetThemedDialogFont(LPWSTR lpFaceName, WORD *wSize);
DLGTEMPLATE *LoadThemedDialogTemplate(LPCTSTR lpDialogTemplateID, HINSTANCE hInstance);
#define ThemedDialogBox(hInstance, lpTemplate, hWndParent, lpDialogFunc) \
	ThemedDialogBoxParam(hInstance, lpTemplate, hWndParent, lpDialogFunc, 0)
INT_PTR ThemedDialogBoxParam(HINSTANCE hInstance, LPCTSTR lpTemplate, HWND hWndParent,
							 DLGPROC lpDialogFunc, LPARAM dwInitParam);
HWND	CreateThemedDialogParam(HINSTANCE hInstance, LPCTSTR lpTemplate, HWND hWndParent,
								DLGPROC lpDialogFunc, LPARAM dwInitParam);

//==== UnSlash Functions ======================================================
void TransformBackslashes(char *pszInput, BOOL bRegEx, UINT cpEdit);
BOOL AddBackslash(char *pszOut, const char *pszInput);

//==== MinimizeToTray Functions - see comments in Helpers.c ===================
BOOL GetDoAnimateMinimize(VOID);
VOID MinimizeWndToTray(HWND hwnd);
VOID RestoreWndFromTray(HWND hwnd);

#endif // NOTEPAD2_HELPERS_H_

// End of Helpers.h
