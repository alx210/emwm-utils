/*
 * Copyright (C) 2026 alx@fastestcode.org
 * This software is distributed under the terms of the X/MIT license.
 * See the included COPYING file for further information.
 */

/* Implements the workspace switcher widget */

#include <stdlib.h>
#include <stdio.h>
#include <Xm/XmP.h>
#include <Xm/PrimitiveP.h>
#include <Xm/DrawP.h>
#include "wswitchp.h"
#include "wswitch.h"


/* Local routines */
static void class_initialize(void);
static void initialize(Widget, Widget, ArgList, Cardinal*);
static void init_gcs(Widget w);
static void destroy(Widget);
static void realize(Widget, XtValueMask*, XSetWindowAttributes*);
static void expose(Widget, XEvent*, Region);
static void draw(Widget, Boolean);
static Boolean widget_display_rect(Widget, XRectangle*);
static void resize(Widget);
static Boolean set_values(Widget, Widget, Widget, ArgList, Cardinal*);
static XtGeometryResult query_geometry(Widget,
	XtWidgetGeometry*, XtWidgetGeometry*);
static void default_render_table(Widget, int, XrmValue*);
static void layout(Widget);
static short hit_test(Widget, Position, Position);
static void mouse_input(Widget, XEvent*, String*, Cardinal*);
static void select_workspace(Widget, XEvent*, String*, Cardinal*);

#define WARNING(w,s) XtAppWarning(XtWidgetToApplicationContext(w), s)

/* Widget resources */
#define RFO(fld) XtOffsetOf(struct switcher_rec, fld)
static XtResource resources[] = {
	{
		XmNrenderTable,
		XmCRenderTable,
		XmRRenderTable,
		sizeof(XmRenderTable),
		RFO(switcher.text_rt),
		XtRCallProc,
		(XtPointer)default_render_table
	},
	{
		XmNmarginWidth,
		XmCMarginWidth,
		XtRDimension,
		sizeof(Dimension),
		RFO(switcher.hmargin),
		XtRImmediate,
		(XtPointer)2
	},
	{
		XmNmarginHeight,
		XmCMarginHeight,
		XtRDimension,
		sizeof(Dimension),
		RFO(switcher.vmargin),
		XtRImmediate,
		(XtPointer)2
	},
	{
		NnumberOfWorkspaces,
		CNumberOfWorkspaces,
		XtRShort,
		sizeof(short),
		RFO(switcher.nbuttons),
		XtRImmediate,
		(XtPointer)0
	},
	{
		NactiveWorkspace,
		CActiveWorkspace,
		XtRShort,
		sizeof(short),
		RFO(switcher.iactive),
		XtRImmediate,
		(XtPointer)0
	},
	{
		XmNcolumns,
		XmCColumns,
		XtRShort,
		sizeof(short),
		RFO(switcher.ncols),
		XtRImmediate,
		(XtPointer) DEF_NCOLS
	},
	{
		XmNvalueChangedCallback,
		XmCValueChangedCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		RFO(switcher.ws_change_cb),
		XtRImmediate,
		NULL
	}
};
#undef RFO

static char translations[] = {
	"<Btn1Down>: PrimaryButton()\n"
	"<Btn2Down>: PrimaryButton()\n"
	"<Btn3Down>: PrimaryButton()\n"
	"<Key>1: SelectWorkspace(0)\n"
	"<Key>2: SelectWorkspace(1)\n"
	"<Key>3: SelectWorkspace(2)\n"
	"<Key>4: SelectWorkspace(3)\n"
	"<Key>5: SelectWorkspace(4)\n"
	"<Key>6: SelectWorkspace(5)\n"
	"<Key>7: SelectWorkspace(6)\n"
	"<Key>8: SelectWorkspace(7)\n"
	"<FocusIn>: FocusIn()\n"
	"<FocusOut>: FocusOut()\n"
};

static XtActionsRec actions[] = {
	{ "PrimaryButton", mouse_input },
	{ "SelectWorkspace", select_workspace },
};

/* Widget class declarations */
static XmPrimitiveClassExtRec primClassExtRec = {
	.next_extension = NULL,
	.record_type = NULLQUARK,
	.version = XmPrimitiveClassExtVersion,
	.record_size = sizeof(XmPrimitiveClassExtRec),
	.widget_baseline = NULL,
	.widget_display_rect = widget_display_rect,
	.widget_margins = NULL
};

static struct switcher_class_rec class_rec_def = {
	.core.superclass = (WidgetClass)&xmPrimitiveClassRec,
	.core.class_name = "WorkspaceSwitcher",
	.core.widget_size = sizeof(struct switcher_rec),
	.core.class_initialize = class_initialize,
	.core.class_part_initialize = NULL,
	.core.class_inited = False,
	.core.initialize = initialize,
	.core.initialize_hook = NULL,
	.core.realize = realize,
	.core.actions = actions,
	.core.num_actions = XtNumber(actions),
	.core.resources = resources,
	.core.num_resources = XtNumber(resources),
	.core.xrm_class = NULLQUARK,
	.core.compress_motion = True,
	.core.compress_exposure = XtExposeCompressMaximal,
	.core.compress_enterleave = True,
	.core.visible_interest = False,
	.core.destroy = destroy,
	.core.resize = resize,
	.core.expose = expose,
	.core.set_values = set_values,
	.core.set_values_hook = NULL,
	.core.set_values_almost = XtInheritSetValuesAlmost,
	.core.get_values_hook = NULL,
	.core.accept_focus = NULL,
	.core.version = XtVersion,
	.core.callback_private = NULL,
	.core.tm_table = translations,
	.core.query_geometry = query_geometry,
	.core.display_accelerator = NULL,
	.core.extension = NULL,

	.primitive.border_highlight = NULL,
	.primitive.border_unhighlight = NULL,
	.primitive.translations = XtInheritTranslations,
	.primitive.arm_and_activate = NULL,
	.primitive.syn_resources = NULL,
	.primitive.num_syn_resources = 0,
	.primitive.extension = (XtPointer) &primClassExtRec
};

WidgetClass switcherWidgetClass = (WidgetClass) &class_rec_def;

#define SWR_PART(w) (&((struct switcher_rec*)w)->switcher)
#define SWR_REC(w) ((struct switcher_rec*)w)
#define PRIM_PART(w) (&((XmPrimitiveRec*)w)->primitive)
#define CORE_WIDTH(w) (((struct switcher_rec*)w)->core.width)
#define CORE_HEIGHT(w) (((struct switcher_rec*)w)->core.height)

static void draw(Widget w, Boolean erase)
{
	Display *dpy = XtDisplay(w);
	Window wnd = XtWindow(w);
	XmPrimitivePart *prim = PRIM_PART(w);
	struct switcher_part *sw = SWR_PART(w);
	Dimension shadow = prim->shadow_thickness;
	int width = CORE_WIDTH(w);
	int height = CORE_HEIGHT(w);
	short i;
		
	if( (width < shadow * 2) || (height < shadow * 2) ) return;

	if(erase) {
		XFillRectangle(dpy, wnd, sw->bg_gc, 0, 0,
			CORE_WIDTH(w) - 1, CORE_HEIGHT(w) - 1);
	}

	XmeDrawShadows(dpy, wnd, prim->top_shadow_GC, prim->bottom_shadow_GC,
		0,  0, width, height, shadow, XmSHADOW_IN);
	
	if(!sw->buttons) {
		XmeDrawShadows(dpy, wnd, prim->top_shadow_GC, prim->bottom_shadow_GC,
			1,  1, width - 2, height - 2, shadow, XmSHADOW_OUT);
		return;
	}
	
	for(i = 0; i < sw->nbuttons; i++) {
		if(i == sw->iactive) {
			XFillRectangle(dpy, wnd, prim->highlight_GC,
					sw->buttons[i].x, sw->buttons[i].y,
					sw->buttons[i].width, sw->buttons[i].height);
		
			XmeDrawShadows(dpy, wnd, prim->bottom_shadow_GC,
				prim->bottom_shadow_GC,
				sw->buttons[i].x, sw->buttons[i].y,
				sw->buttons[i].width, sw->buttons[i].height, shadow,
				XmSHADOW_IN);
				
				if(sw->sfg_pixel != sw->fg_pixel)
					XSetForeground(dpy, sw->fg_gc, sw->sfg_pixel);
		} else {
			XmeDrawShadows(dpy, wnd, prim->top_shadow_GC,
				prim->bottom_shadow_GC,
				sw->buttons[i].x, sw->buttons[i].y,
				sw->buttons[i].width, sw->buttons[i].height, shadow,
				XmSHADOW_OUT);

				if(sw->sfg_pixel != sw->fg_pixel)
					XSetForeground(dpy, sw->fg_gc, sw->fg_pixel);
		}

		XmStringDraw(dpy, wnd, sw->text_rt, sw->buttons[i].label,
			sw->fg_gc, sw->buttons[i].x,
			sw->buttons[i].y + sw->vmargin + shadow,
			sw->buttons[i].width, XmALIGNMENT_CENTER,
			XmSTRING_DIRECTION_DEFAULT, NULL);
	}
}

/* Returns preferred widget dimensions */
static void get_pref_dimensions(Widget w, Dimension *width, Dimension *height)
{
	struct switcher_part *sw = SWR_PART(w);
	XmPrimitivePart *prim = PRIM_PART(w);
	Dimension shadow = prim->shadow_thickness;
	Dimension bw = shadow * 2 + sw->font_height + sw->hmargin * 2;
	Dimension bh = shadow * 2 + sw->font_height + sw->vmargin * 2;
	Dimension rows = (sw->nbuttons + (sw->ncols - 1)) / sw->ncols;
	
	if(!rows) rows = 1;
	
	if(width) *width = shadow * 2 + bw * sw->ncols;
	if(height) *height = shadow * 2 + bh * rows;
}

/* Computes switcher button positions */
static void layout(Widget w)
{
	XmPrimitivePart *prim = PRIM_PART(w);
	struct switcher_part *sw = SWR_PART(w);
	short c, r, i, rows;
	Dimension st = prim->shadow_thickness;
	int cw = (int)CORE_WIDTH(w) - st * 2;
	int ch = (int)CORE_HEIGHT(w) - st * 2;
	Dimension bw_min = st * 2 + sw->font_height + sw->hmargin * 2;
	Dimension bh_min = st * 2 + sw->font_height + sw->vmargin * 2;
	Dimension bpc = (sw->nbuttons > sw->ncols) ? sw->ncols : sw->nbuttons;
	
	if(!sw->nbuttons || (cw < bw_min) || (ch < bh_min)) return;
	
	rows = (sw->nbuttons + (bpc - 1)) / bpc;

	for(i = 0, r = 0; r < rows; r++) {
		Position cx = 0;
		Position cy = r * bh_min;
		Dimension bw = bw_min;
				
		if(bw_min * bpc < cw) bw = bw_min + ((cw - bw_min * bpc) / bpc);
		
		for(c = 0; c < bpc; c++, i++) {
			cx = bw * c;

			sw->buttons[i].x = cx + st;
			sw->buttons[i].y = cy + st;
			sw->buttons[i].width = bw;
			sw->buttons[i].height = bh_min;
		}
		/* compensate for odd button width by extending last edge */
		if(i && (cw % bw)) sw->buttons[i - 1].width += (cw % bw);
		
		bpc = ((sw->nbuttons - i) < sw->ncols) ? (sw->nbuttons - i) : sw->ncols;
	}
}

static short hit_test(Widget w, Position x, Position y)
{
	struct switcher_part *sw = SWR_PART(w);
	short i;
	
	for(i = 0; i < sw->nbuttons; i++) {
		if( (x > sw->buttons[i].x) &&
			(y > sw->buttons[i].y) &&
			(x < (sw->buttons[i].x + sw->buttons[i].width)) &&
			(y < (sw->buttons[i].y + sw->buttons[i].height)) ) return i;
	}
	return INONE;
}

/*
 * Intrinsic widget routines
 */
static void expose(Widget w, XEvent *evt, Region reg)
{
	draw(w, True);
}

static void resize(Widget w)
{
	layout(w);
}

static XtGeometryResult query_geometry(Widget w,
	XtWidgetGeometry *ig, XtWidgetGeometry *pg)
{
	Dimension pref_width;
	Dimension pref_height;

	get_pref_dimensions(w, &pref_width, &pref_height);

	pg->request_mode = CWWidth | CWHeight;
	pg->width = pref_width;
	pg->height = pref_height;
	
	return XmeReplyToQueryGeometry(w, ig, pg);
}

static Boolean widget_display_rect(Widget w, XRectangle *r)
{
	XmPrimitivePart *prim = PRIM_PART(w);
	int width, height;
	
	r->x = prim->shadow_thickness;
	r->y = prim->shadow_thickness;
	width = (int)CORE_WIDTH(w) - prim->shadow_thickness * 2;
	height = (int)CORE_HEIGHT(w) - prim->shadow_thickness * 2;
	if(width < 0 || height < 0) {
		width = 0;
		height = 0;
	}
	r->width = (Dimension)width;
	r->height = (Dimension)height;

	return True;
}

static void realize(Widget w, XtValueMask *mask, XSetWindowAttributes *att)
{
	(*switcherWidgetClass->core_class.superclass->core_class.realize)
		(w, mask, att);
}

static void initialize(Widget wreq, Widget wnew,
	ArgList init_args, Cardinal *ninit_args)
{
	struct switcher_part *p = SWR_PART(wnew);
	int height, ascent, descent;
	Dimension pref_width, pref_height;
	
	XmRenderTableGetDefaultFontExtents(
		p->text_rt,	&height, &ascent, &descent);

	p->font_height = height;

	if(p->nbuttons > 1) {
		short i;

		p->buttons = (struct button_rec*)
			XtMalloc(sizeof(struct button_rec) * p->nbuttons);

		for(i = 0; i < p->nbuttons; i++) {
			char sz[7];

			sprintf(sz, "%d", i + 1);
			p->buttons[i].label = XmStringCreateLocalized(sz);
		}
	} else {
		p->nbuttons = 0;
		p->buttons = NULL;
	}

	init_gcs(wnew);
	
	get_pref_dimensions(wnew, &pref_width, &pref_height);

	if(CORE_WIDTH(wreq) == 0)
		CORE_WIDTH(wnew) = pref_width;
	
	if(CORE_HEIGHT(wreq) == 0)
		CORE_HEIGHT(wnew) = pref_height;
	
	layout(wnew);
}

static void init_gcs(Widget w)
{
	struct switcher_rec *r = SWR_REC(w);
	XGCValues gcv;
	XtGCMask gc_mask;
	XColor xc = { 0 };

	r->switcher.fg_pixel = r->primitive.foreground;
	r->switcher.bg_pixel = r->core.background_pixel;
	
	/* Label GC */
	gcv.function = GXcopy;
	gcv.foreground = r->switcher.fg_pixel;
	gcv.line_style = LineOnOffDash;
	gcv.dashes = 1;
	gc_mask = GCForeground | GCFunction | GCLineStyle | GCDashList;

	/* XmStringDraw needs an allocated GC */
	r->switcher.fg_gc = XtAllocateGC(w, 0, gc_mask, &gcv,
		gc_mask | GCClipMask | GCFont, 0);

	/* Shareable background GC */
	gcv.foreground = r->core.background_pixel;
	r->switcher.bg_gc = XtGetGC(w, GCForeground, &gcv);

	xc.pixel = r->primitive.highlight_color;
	XQueryColor(XtDisplay(w), r->core.colormap, &xc);

	/* text color on highlighted background */
	xc.red /= 256;
	xc.green /= 256;
	xc.blue /= 256;

	r->switcher.sfg_pixel = 
		(((unsigned int)xc.red * xc.red + xc.green * xc.green +
			xc.blue * xc.blue) > FG_THRESHOLD) ?
		BlackPixelOfScreen(r->core.screen) :
		WhitePixelOfScreen(r->core.screen);

}

static Boolean set_values(Widget wcur, Widget wreq,
	Widget wset, ArgList args, Cardinal *nargs)
{
	struct switcher_rec *cur = (struct switcher_rec*) wcur;
	struct switcher_rec *set = (struct switcher_rec*) wset;
	
	
	if( (cur->primitive.foreground != set->primitive.foreground) ||
		(cur->core.background_pixel != set->core.background_pixel) ) {

		XtReleaseGC(wcur, set->switcher.fg_gc);
		XtReleaseGC(wcur, set->switcher.bg_gc);
		
		init_gcs(wset);
	}
	
	if(cur->switcher.nbuttons != set->switcher.nbuttons) {
		Dimension width;
		Dimension height;
		
		if(set->switcher.buttons) {
			XtFree((char*)set->switcher.buttons);
			set->switcher.buttons = NULL;
		}

		if(set->switcher.nbuttons > 1) {
			short i;

			set->switcher.buttons = (struct button_rec*)
				XtMalloc(sizeof(struct button_rec) *
					set->switcher.nbuttons);

			for(i = 0; i < set->switcher.nbuttons; i++) {
				char sz[7];

				sprintf(sz, "%d", i + 1);
				set->switcher.buttons[i].label =
					XmStringCreateLocalized(sz);
			}


		} else set->switcher.nbuttons = 0;

		get_pref_dimensions(wset, &width, &height);
		XtMakeResizeRequest(wset, width, height, &width, &height);
		XtMakeResizeRequest(wset, width, height, NULL, NULL);
		layout(wset);
	}
	if(cur->switcher.iactive != set->switcher.iactive) {
		if(set->switcher.iactive >= set->switcher.nbuttons) 
			set->switcher.iactive = 0;
	}

	return (XtIsRealized(wset) ? True : False);
}

static void class_initialize(void)
{
	/* nothing to do here yet */
}

static void destroy(Widget w)
{
	struct switcher_part *p = SWR_PART(w);
	
	if(p->buttons) {
		short i;
		
		for(i = 0; i < p->nbuttons; i++)
			XmStringFree(p->buttons[i].label);

		XtFree((char*)p->buttons);
	}

	XtReleaseGC(w, p->fg_gc);
	XtReleaseGC(w, p->bg_gc);
}


/*
 * Dynamic defaults
 */
static void default_render_table(Widget w, int offset, XrmValue *pv)
{
	static XmRenderTable rt;

	rt = XmeGetDefaultRenderTable(w, XmLABEL_RENDER_TABLE);

	pv->addr = (XPointer) &rt;
	pv->size = sizeof(XmRenderTable);
}

/*
 * Action callbacks
 */
static void mouse_input(Widget w, XEvent *evt,
	String *params, Cardinal *nparams)
{
	struct switcher_part *p = SWR_PART(w);
	short i;
	
	if(evt->type != ButtonPress && evt->type != ButtonRelease) {
		WARNING(w, "Wrong event type for the action PrimaryButton()");
		return;
	}
	
	i = hit_test(w, evt->xbutton.x, evt->xbutton.y);
	if(i != INONE && i != p->iactive) {
		
		p->iactive = i;
		draw(w, True);

		if(p->ws_change_cb) XtCallCallbackList(w,
			p->ws_change_cb, (XtPointer)&i);
	}
}

static void select_workspace(Widget w, XEvent *evt,
	String *params, Cardinal *nparams)
{
	struct switcher_part *p = SWR_PART(w);
	short iws;
	
	if(*nparams == 0) {
		WARNING(w, "Missing argument for the SelectWorkspace() action");
		return;
	}
	
	iws = (short)strtol(params[0], NULL, 10);

	if(p->nbuttons && (iws >= 0) &&
		(iws < p->nbuttons) && (iws != p->iactive)) {
		
		p->iactive = iws;
		draw(w, True);

		if(p->ws_change_cb) XtCallCallbackList(w,
			p->ws_change_cb, (XtPointer)&iws);
	}
}
