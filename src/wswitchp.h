/*
 * Copyright (C) 2026 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included COPYING file for further information.
 */

/* Workspace switcher widget private data structures */

#ifndef WSWITCHP_H
#define WSWITCHP_H
#include <Xm/PrimitiveP.h>

/* Default number of columns */
#define DEF_NCOLS 4

/* Background color magnitude at which text turns white */
#define FG_THRESHOLD 56000

#define INONE (-1)

struct button_rec {
	Position x;
	Position y;
	Dimension width;
	Dimension height;
	XmString label;
};

struct switcher_part {
	XmRenderTable text_rt;

	GC fg_gc;
	GC bg_gc;

	Pixel fg_pixel;
	Pixel bg_pixel;
	Pixel sfg_pixel;
	
	struct button_rec *buttons;
	short nbuttons;
	short iactive;
	short ncols;
	
	Dimension hmargin;
	Dimension vmargin;
	Dimension font_height;
	XtCallbackList ws_change_cb;
};

struct switcher_rec {
	CorePart core;
	XmPrimitivePart primitive;
	struct switcher_part switcher;
};

struct switcher_class_part {
	XtPointer extension;
};

struct switcher_class_rec {
	CoreClassPart core;
	XmPrimitiveClassPart primitive;
	struct switcher_class_part switcher;
};

/* libXm internals */
extern void _XmPrimitiveFocusIn(Widget, XEvent*, String*, Cardinal*);
extern void _XmPrimitiveFocusOut(Widget, XEvent*, String*, Cardinal*);
extern void XmRenderTableGetDefaultFontExtents(XmRenderTable,
	int *height, int *ascent, int *descent);

#endif /* WSWITCHP_H */
