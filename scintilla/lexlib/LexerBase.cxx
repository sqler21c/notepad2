// Scintilla source code edit control
/** @file LexerBase.cxx
 ** A simple lexer with no state.
 **/
// Copyright 1998-2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <cstdlib>
#include <cassert>
#include <cstring>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "PropSetSimple.h"
#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "LexerModule.h"
#include "LexerBase.h"

using namespace Scintilla;

static const char styleSubable[] = { 0 };

LexerBase::LexerBase() {
	for (int wl = 0; wl < numWordLists; wl++) {
		keyWordLists[wl] = new WordList;
	}
	keyWordLists[numWordLists] = nullptr;
}

LexerBase::~LexerBase() {
	for (int wl = 0; wl < numWordLists; wl++) {
		delete keyWordLists[wl];
		keyWordLists[wl] = nullptr;
	}
	keyWordLists[numWordLists] = nullptr;
}

void SCI_METHOD LexerBase::Release() noexcept {
	delete this;
}

int SCI_METHOD LexerBase::Version() const noexcept {
	return lvRelease4;
}

const char * SCI_METHOD LexerBase::PropertyNames() const noexcept {
	return "";
}

int SCI_METHOD LexerBase::PropertyType(const char *) const noexcept {
	return SC_TYPE_BOOLEAN;
}

const char * SCI_METHOD LexerBase::DescribeProperty(const char *) const noexcept {
	return "";
}

Sci_Position SCI_METHOD LexerBase::PropertySet(const char *key, const char *val) {
	const char *valOld = props.Get(key);
	if (strcmp(val, valOld) != 0) {
		props.Set(key, val, strlen(key), strlen(val));
		return 0;
	} else {
		return -1;
	}
}

const char * SCI_METHOD LexerBase::DescribeWordListSets() const noexcept {
	return "";
}

Sci_Position SCI_METHOD LexerBase::WordListSet(int n, const char *wl) {
	if (n < numWordLists) {
		WordList wlNew;
		wlNew.Set(wl);
		if (*keyWordLists[n] != wlNew) {
			keyWordLists[n]->Set(wl);
			return 0;
		}
	}
	return -1;
}

void * SCI_METHOD LexerBase::PrivateCall(int, void *) noexcept {
	return nullptr;
}

int SCI_METHOD LexerBase::LineEndTypesSupported() const noexcept {
	return SC_LINE_END_TYPE_DEFAULT;
}

int SCI_METHOD LexerBase::AllocateSubStyles(int, int) noexcept {
	return -1;
}

int SCI_METHOD LexerBase::SubStylesStart(int) const noexcept {
	return -1;
}

int SCI_METHOD LexerBase::SubStylesLength(int) const noexcept {
	return 0;
}

int SCI_METHOD LexerBase::StyleFromSubStyle(int subStyle) const noexcept {
	return subStyle;
}

int SCI_METHOD LexerBase::PrimaryStyleFromStyle(int style) const noexcept {
	return style;
}

void SCI_METHOD LexerBase::FreeSubStyles() noexcept {
}

void SCI_METHOD LexerBase::SetIdentifiers(int, const char *) noexcept {
}

int SCI_METHOD LexerBase::DistanceToSecondaryStyles() const noexcept {
	return 0;
}

const char * SCI_METHOD LexerBase::GetSubStyleBases() const noexcept {
	return styleSubable;
}

int SCI_METHOD LexerBase::NamedStyles() const noexcept {
	return -1;
}

const char * SCI_METHOD LexerBase::NameOfStyle(int) const noexcept {
	return "";
}

const char * SCI_METHOD LexerBase::TagsOfStyle(int) const noexcept {
	return "";
}

const char * SCI_METHOD LexerBase::DescriptionOfStyle(int) const noexcept {
	return "";
}
