// Scintilla source code edit control
/** @file Style.cxx
 ** Defines the font and colour style for a class of text.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <string.h>

#include "Platform.h"

#include "Scintilla.h"
#include "Style.h"

Style::Style() {
	aliasOfDefaultFont = true;
	Clear(Colour(0, 0, 0), Colour(0xff, 0xff, 0xff),
	      Platform::DefaultFontSize(), 0, SC_CHARSET_DEFAULT,
	      false, false, false, false, true);
}

Style::Style(const Style &source) {
	Clear(Colour(0, 0, 0), Colour(0xff, 0xff, 0xff),
	      0, 0, 0,
	      false, false, false, false, true);
	fore.desired = source.fore.desired;
	back.desired = source.back.desired;
	characterSet = source.characterSet;
	bold = source.bold;
	italic = source.italic;
	size = source.size;
	eolFilled = source.eolFilled;
	underline = source.underline;
	visible = source.visible;
}

Style::~Style() {
	if (aliasOfDefaultFont)
		font.SetID(0);
	else
		font.Release();
	aliasOfDefaultFont = false;
}

Style &Style::operator=(const Style &source) {
	if (this == &source)
		return * this;
	Clear(Colour(0, 0, 0), Colour(0xff, 0xff, 0xff),
	      0, 0, SC_CHARSET_DEFAULT,
	      false, false, false, false, true);
	fore.desired = source.fore.desired;
	back.desired = source.back.desired;
	characterSet = source.characterSet;
	bold = source.bold;
	italic = source.italic;
	size = source.size;
	eolFilled = source.eolFilled;
	underline = source.underline;
	visible = source.visible;
	return *this;
}

void Style::Clear(Colour fore_, Colour back_, int size_,
                  const char *fontName_, int characterSet_,
                  bool bold_, bool italic_, bool eolFilled_, bool underline_, bool visible_) {
	fore.desired = fore_;
	back.desired = back_;
	characterSet = characterSet_;
	bold = bold_;
	italic = italic_;
	size = size_;
	fontName = fontName_;
	eolFilled = eolFilled_;
	underline = underline_;
	visible = visible_;
	if (aliasOfDefaultFont)
		font.SetID(0);
	else
		font.Release();
	aliasOfDefaultFont = false;
}

bool Style::EquivalentFontTo(const Style *other) const {
	if (bold != other->bold ||
	        italic != other->italic ||
	        size != other->size ||
	        characterSet != other->characterSet)
		return false;
	if (fontName == other->fontName)
		return true;
	if (!fontName)
		return false;
	if (!other->fontName)
		return false;
	return strcmp(fontName, other->fontName) == 0;
}

void Style::Realise(Surface &surface, int zoomLevel, Style *defaultStyle) {
	sizeZoomed = size + zoomLevel;
	if (sizeZoomed <= 2)	// Hangs if sizeZoomed <= 1
		sizeZoomed = 2;

	if (aliasOfDefaultFont)
		font.SetID(0);
	else
		font.Release();
	int deviceHeight = surface.DeviceHeightFont(sizeZoomed);
	aliasOfDefaultFont = defaultStyle &&
	                     (EquivalentFontTo(defaultStyle) || !fontName);
	if (aliasOfDefaultFont) {
		font.SetID(defaultStyle->font.GetID());
	} else if (fontName) {
		font.Create(fontName, characterSet, deviceHeight, bold, italic);
	} else {
		font.SetID(0);
	}

	ascent = surface.Ascent(font);
	descent = surface.Descent(font);
	// Probably more typographically correct to include leading
	// but that means more complex drawing as leading must be erased
	//lineHeight = surface.ExternalLeading() + surface.Height();
	externalLeading = surface.ExternalLeading(font);
	lineHeight = surface.Height(font);
	aveCharWidth = surface.AverageCharWidth(font);
	spaceWidth = surface.WidthChar(font, ' ');
}
