#include "EditLexer.h"
#include "EditStyle.h"

// https://en.wikibooks.org/wiki/Windows_Programming/Resource_Script_Reference

static KEYWORDLIST Keywords_RC = {{
"defined __has_include "
// Resource
"ACCELERATORS BITMAP CURSOR DIALOG DIALOGEX FONT HTML ICON MENU MENUEX MESSAGETABLE POPUP PNG RCDATA REGISTRY RT_MANIFEST STRINGTABLE STYLE_XML TEXTINCLUDE TYPELIB VERSIONINFO "
// Control
"AUTO3STATE AUTOCHECKBOX AUTORADIOBUTTON CHECKBOX COMBOBOX CONTROL CTEXT DEFPUSHBUTTON EDITTEXT GROUPBOX LISTBOX LTEXT PUSHBOX PUSHBUTTON RADIOBUTTON RTEXT SCROLLBAR STATE3 "
// Statement
"CAPTION CHARACTERISTICS CLASS EXSTYLE LANGUAGE MENUITEM STYLE VERSION "
"BEGIN END BLOCK BUTTON SEPARATOR VALUE "
"VIRTKEY ALT SHIFT NOINVERT "
"PRELOAD LOADONCALL FIXED DISCARDABLE PURE IMPURE SHARED NONSHARED "

, // 1 Type keyword
NULL
, // 2 Preprocessor
"define undef ifdef ifndef if elif else endif include pragma error code_page "
"warning "

, // 3 Directive
NULL
, // 4 Attribute
NULL
, // 5 Class
NULL
, // 6 Interface
NULL
, // 7 Enumeration
NULL

, // 8 Constant
"RC_INVOKED __DATE__ __FILE__ __LINE__ __STDC__ __TIME__ __TIMESTAMP__ "

, NULL, NULL, NULL, NULL, NULL, NULL

, // 15 Code Snippet
"defined() code_page() __has_include() "
}};


EDITLEXER lexRC = { SCLEX_CPP, NP2LEX_RC, EDITLEXER_HOLE(L"Resource Script"), L"rc; rc2; rct; rh; dlg", &Keywords_RC,
{
	EDITSTYLE_DEFAULT,
	{ SCE_C_WORD, NP2STYLE_Keyword, EDITSTYLE_HOLE(L"Keyword"), L"fore:#0000FF" },
	{ SCE_C_PREPROCESSOR, NP2STYLE_Preprocessor, EDITSTYLE_HOLE(L"Preprocessor"), L"fore:#FF8000" },
	{ MULTI_STYLE(SCE_C_COMMENT, SCE_C_COMMENTLINE, SCE_C_COMMENTDOC, SCE_C_COMMENTLINEDOC), NP2STYLE_Comment, EDITSTYLE_HOLE(L"Comment"), L"fore:#608060" },
	{ MULTI_STYLE(SCE_C_STRING, SCE_C_CHARACTER, SCE_C_STRINGEOL, 0), NP2STYLE_String, EDITSTYLE_HOLE(L"String"), L"fore:#008000" },
	{ SCE_C_NUMBER, NP2STYLE_Number, EDITSTYLE_HOLE(L"Number"), L"fore:#FF0000" },
	{ SCE_C_OPERATOR, NP2STYLE_Operator, EDITSTYLE_HOLE(L"Operator"), L"fore:#B000B0" },
	EDITSTYLE_SENTINEL
}
};

