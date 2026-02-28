/*
 * Copyright (C) 2018-2026 alx@fastestcode.org
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Toolbox initialization and GUI routines
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/PushBG.h>
#include <Xm/CascadeBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/Frame.h>
#include <Xm/LabelG.h>
#include <Xm/SelectioB.h>
#include <Xm/TextF.h>
#include <Xm/MessageB.h>
#include <Xm/MwmUtil.h>
#include <X11/cursorfont.h>
#include <errno.h>
#include "tbparse.h"
#include "common.h"
#include "smglobal.h"
#include "wswitch.h"

/* Forward declarations */
static char* find_rc_file(void);
static Boolean construct_menu(void);
static void create_utility_widgets(Widget);
static void set_icon(Widget);
static Boolean setup_hotkeys(void);
static int xgrabkey_err_handler(Display*,XErrorEvent*);
static void handle_root_event(XEvent*);
void raise_and_focus(Widget w);
static void time_update_cb(XtPointer,XtIntervalId*);
static int exec_command(const char*);
static void report_exec_error(const char*,const char*,int);
static void report_rcfile_error(const char*,const char*);
static Boolean message_dialog(Boolean,const char*);
static void exec_cb(Widget,XtPointer,XtPointer);
static void exec_dialog_cb(Widget,XtPointer,XtPointer);
static void menu_command_cb(Widget,XtPointer,XtPointer);
static void message_dialog_cb(Widget,XtPointer,XtPointer);
static void sigchld_handler(int);
static void sigusr_handler(int);
static void xt_sigusr1_handler(XtPointer,XtSignalId*);
static void suspend_cb(Widget,XtPointer,XtPointer);
static void logout_cb(Widget,XtPointer,XtPointer);
static void lock_cb(Widget,XtPointer,XtPointer);
static Boolean send_xmsm_cmd(const char *command);
static int local_x_err_handler(Display*,XErrorEvent*);
static Boolean get_xmsm_config(unsigned long*);
static void set_ws_presence(Widget);
static Boolean get_ws_info(unsigned short*, unsigned short*);
static void ws_change_cb(Widget,XtPointer,XtPointer);

struct tb_resources {
	char *title;
	Boolean show_date_time;
	char *date_time_fmt;
	char *rc_file;
	char *hotkey;
	Boolean horizontal;
	Boolean separators;
	Boolean switcher;
	Boolean occupy_all;
} app_res;

#define RES_FIELD(f) XtOffsetOf(struct tb_resources,f)
static XtResource xrdb_resources[]={
	{ "title","Title",XmRString,sizeof(String),
		RES_FIELD(title),XmRImmediate,(XtPointer)NULL
	},
	{ "dateTimeDisplay","DateTimeDisplay",XmRBoolean,sizeof(Boolean),
		RES_FIELD(show_date_time),XmRImmediate,(XtPointer)True
	},
	{ "dateTimeFormat","DateTimeFormat",XmRString,sizeof(String),
		RES_FIELD(date_time_fmt),XmRImmediate,(XtPointer)"%m/%d %l:%M %p"
	},
	{ "rcFile","RcFile",XmRString,sizeof(String),
		RES_FIELD(rc_file),XmRImmediate,(XtPointer)NULL
	},
	{ "hotkey","Hotkey",XmRString,sizeof(String),
		RES_FIELD(hotkey),XmRImmediate,(XtPointer)NULL
	},
	{ "horizontal","Horizontal",XmRBoolean,sizeof(Boolean),
		RES_FIELD(horizontal),XmRImmediate,(XtPointer)False
	},
	{ "separators","Separators",XmRBoolean,sizeof(Boolean),
		RES_FIELD(separators),XmRImmediate,(XtPointer)True
	},
	{ "workspaceSwitcher","WorkspaceSwitcher",XmRBoolean,sizeof(Boolean),
		RES_FIELD(switcher),XmRImmediate,(XtPointer)True
	},
	{ "occupyAllWorkspaces","OccupyAllWorkspaces",XmRBoolean,sizeof(Boolean),
		RES_FIELD(occupy_all),XmRImmediate,(XtPointer)True
	}

};
#undef RES_FIELD

static XrmOptionDescRec xrdb_options[]={
	{"-title","title",XrmoptionSepArg,(caddr_t)NULL},
	{"-rcfile","rcFile",XrmoptionSepArg,(caddr_t)NULL},
	{"-hotkey","hotkey",XrmoptionSepArg,(caddr_t)NULL},
	{"-horizontal", "horizontal", XrmoptionNoArg, (caddr_t)"True"},
	{"+horizontal", "horizontal", XrmoptionNoArg, (caddr_t)"False"}
};

static String fallback_res[]={
	"XmToolbox.x: 8",
	"XmToolbox.y: 28",
	"XmToolbox.mwmDecorations: 58",
	"*mainFrame.shadowThickness: 1",
	NULL
};

#define APP_TITLE "Toolbox"
#define APP_NAME "xmtoolbox"
#define RC_NAME	"toolboxrc"

/* MWM workspace constants (from WmGlobal.h) */
#define _XA_MWM_WORKSPACE_PRESENCE "_MWM_WORKSPACE_PRESENCE"
#define _XA_MWM_WORKSPACE_ALL "all"

/* EWMH virtual desktop properties */
#define _NET_NUMBER_OF_DESKTOPS "_NET_NUMBER_OF_DESKTOPS"
#define _NET_CURRENT_DESKTOP "_NET_CURRENT_DESKTOP"

XtAppContext app_context;
Widget wshell = None;

static Atom xa_ndesks = None;
static Atom xa_cdesk = None;

static Atom xa_xmsm_mgr = None;
static Atom xa_xmsm_pid = None;
static Atom xa_xmsm_cmd = None;
static Atom xa_xmsm_cfg = None;
static int (*def_x_err_handler)(Display*,XErrorEvent*) = NULL;
static const char xmsm_cmd_err[] =
	"Cannot retrieve session manager PID.\nxmsm not running?";

static Widget wmain = None;
static Widget wdtlabel = None;
static Widget wdtframe = None;
static Widget wmenu = None;
static Widget wswitch = None;
static Widget wgadsep = None;
static Widget wgadrc = None;
static String rc_file_path = NULL;
static XtSignalId xt_sigusr1;
static KeyCode hotkey_code = 0;
unsigned int hotkey_mods = 0;
unsigned long xmsm_cfg = 0;
static Boolean sm_reqstat;


int main(int argc, char **argv)
{
	Window root_window;
	Widget wframe;
	int root_event_mask = PropertyChangeMask;
	
	rsignal(SIGUSR1, sigusr_handler);
	rsignal(SIGUSR2, sigusr_handler);
	rsignal(SIGCHLD, sigchld_handler);

	XtSetLanguageProc(NULL,NULL,NULL);
	XtToolkitInitialize();
	
	wshell = XtVaAppInitialize(&app_context, "XmToolbox",
		xrdb_options, XtNumber(xrdb_options), &argc,argv, fallback_res,
		XmNiconName, APP_TITLE, XmNallowShellResize, True,
		XmNmwmFunctions, MWM_FUNC_MOVE|MWM_FUNC_MINIMIZE,
		XmNmappedWhenManaged, False, NULL);

	if(argc > 1) {
		int i;
		
		for(i = 1; i < argc; i++) {
			if(!strcmp("-version", argv[i])) {
				print_version(APP_NAME);
				XtDestroyApplicationContext(app_context);
				return 0;
			}
		}
	}
	
	XtGetApplicationResources(wshell,&app_res,xrdb_resources,
		XtNumber(xrdb_resources),NULL,0);
	
	/* XMSM IPC atoms */
	xa_xmsm_mgr = XInternAtom(XtDisplay(wshell), XMSM_ATOM_NAME, True);
	xa_xmsm_pid = XInternAtom(XtDisplay(wshell), XMSM_PID_ATOM_NAME, True);
	xa_xmsm_cmd = XInternAtom(XtDisplay(wshell), XMSM_CMD_ATOM_NAME, True);
	xa_xmsm_cfg = XInternAtom(XtDisplay(wshell), XMSM_CFG_ATOM_NAME, True);
	
	/* EWMH virtual desktop atoms */
	xa_ndesks = XInternAtom(XtDisplay(wshell), _NET_NUMBER_OF_DESKTOPS, False);
	xa_cdesk = XInternAtom(XtDisplay(wshell), _NET_CURRENT_DESKTOP, False);


	if(!get_xmsm_config(&xmsm_cfg)) 
		message_dialog(False, xmsm_cmd_err);

	if(!app_res.title){
		char *title;
		char *login;
		char host[256]="localhost";

		if( (login = get_login()) ) {
			gethostname(host,255);

			title=malloc(strlen(login)+strlen(host)+2);
			if(!title){
				perror("malloc");
				return EXIT_FAILURE;
			}
			sprintf(title,"%s@%s",login,host);
			XtVaSetValues(wshell,XmNtitle,title,NULL);
			free(title);
		}
	}

	wframe = XmVaCreateManagedFrame(wshell,"mainFrame",
		XmNshadowType, XmSHADOW_OUT, NULL);
	
	wmain = XmVaCreateManagedRowColumn(wframe, "main",
		XmNmarginWidth, 0,
		XmNmarginHeight, 0,
		XmNspacing, 0,
		XmNorientation, (app_res.horizontal ? XmHORIZONTAL:XmVERTICAL),
		NULL);

	rc_file_path=(app_res.rc_file)?app_res.rc_file:find_rc_file();
	
	if(rc_file_path){
		if(access(rc_file_path, R_OK) == -1){
			message_dialog(False, "Cannot access RC file. Exiting!");
			perror(rc_file_path);
			return EXIT_FAILURE;
		}
		if(!construct_menu()) return EXIT_FAILURE;
	}else{
		message_dialog(False, "RC file not found, nor specified. Exiting!");
		fprintf(stderr,"%s not found, nor specified.\n",RC_NAME);
		return EXIT_FAILURE;
	}

	create_utility_widgets(wmain);
	
	root_window = XDefaultRootWindow(XtDisplay(wshell));
	xt_sigusr1 = XtAppAddSignal(app_context,xt_sigusr1_handler,NULL);
	
	XtRealizeWidget(wshell);
	if(app_res.occupy_all) set_ws_presence(wshell);
	set_icon(wshell);

	if(setup_hotkeys())
		root_event_mask |= KeyPressMask;

	XtMapWidget(wshell);

	if(XtIsManaged(wswitch))
		XmProcessTraversal(wswitch, XmTRAVERSE_CURRENT);

	XSelectInput(XtDisplay(wshell), root_window, root_event_mask);

	for(;;) {
		XEvent evt;
		XtAppNextEvent(app_context,&evt);
		if(evt.xany.window == root_window)
			handle_root_event(&evt);
		else
			XtDispatchEvent(&evt);
	}
	
	return 0;
}

static void set_icon(Widget wshell)
{
	Pixmap image;
	Pixmap mask;
	Window root;
	Display *dpy = XtDisplay(wshell);
	int depth, screen;
	Screen *pscreen;
	Colormap cmap;
	XColor bg_color;
	XColor fg_color;
	XColor tmp;
	
	#include "xbm/toolbox.xbm"
	#include "xbm/toolbox_m.xbm"
	
	pscreen = XDefaultScreenOfDisplay(dpy);
	screen = XScreenNumberOfScreen(pscreen);
	root = RootWindowOfScreen(pscreen);
	depth = DefaultDepth(dpy, screen);
	cmap = DefaultColormap(dpy, screen);
	
	fg_color.pixel = BlackPixel(dpy, screen);
	bg_color.pixel = WhitePixel(dpy, screen);
	
	XAllocNamedColor(dpy, cmap, "LightBlue", &bg_color, &tmp);
	
	image = XCreatePixmapFromBitmapData(dpy, root,
		(char*)toolbox_xbm_bits,
		toolbox_xbm_width, toolbox_xbm_height,
		fg_color.pixel, bg_color.pixel, depth);

	mask = XCreatePixmapFromBitmapData(dpy, root,
		(char*)toolbox_m_xbm_bits,
		toolbox_m_xbm_width,
		toolbox_m_xbm_height, 1, 0, 1);
	
	XtVaSetValues(wshell, XmNiconPixmap, image, XmNiconMask, mask, NULL);
}

/* Sets shell's workspace presence to all */
static void set_ws_presence(Widget wshell)
{
	Display *dpy = XtDisplay(wshell);
	Atom wps_atom = XInternAtom(dpy, _XA_MWM_WORKSPACE_PRESENCE, False);
	Atom all_atom = XInternAtom(dpy, _XA_MWM_WORKSPACE_ALL, False);
	
	XChangeProperty(dpy, XtWindow(wshell), wps_atom,
		wps_atom, 32, PropModeReplace, (unsigned char*)&all_atom, 1);
}

/*
 * Parse the specified hotkey combination, grab the key and attach a callback.
 */
static Boolean setup_hotkeys(void)
{
	Window root_window;
	char *buf;
	char *token;
	KeySym key_sym = NoSymbol;
	static int (*def_x_err_handler)(Display*, XErrorEvent*) = NULL;
	
	if(!app_res.hotkey || !strcasecmp(app_res.hotkey, "none")) return False;

	hotkey_code = 0;
	hotkey_mods = 0;

	buf=strdup(app_res.hotkey);	
	token=strtok(buf," \t+");
	if(token){
		while(token){
			if(!strcasecmp(token, "alt")){
				hotkey_mods |= Mod1Mask;
			}else if(!strcasecmp(token, "ctrl") ||
				!strcasecmp(token, "control")){
				hotkey_mods |= ControlMask;
			}else if(!strcasecmp(token, "shift")){
				hotkey_mods |= ShiftMask;
			}else{
				key_sym = XStringToKeysym(token);
				break;
			}
			token = strtok(NULL," \t+");
		}
	}else{
		key_sym = XStringToKeysym(buf);
	}
	free(buf);

	if(key_sym == NoSymbol){
		fputs("Invalid hotkey specification\n", stderr);
		return False;
	}
	hotkey_code = XKeysymToKeycode(XtDisplay(wshell), key_sym);
	
	root_window = XDefaultRootWindow(XtDisplay(wshell));
	
	XSync(XtDisplay(wshell), False);
	def_x_err_handler = XSetErrorHandler(xgrabkey_err_handler);
	
	XGrabKey(XtDisplay(wshell), hotkey_code, hotkey_mods,
		root_window, False, GrabModeAsync, GrabModeAsync);
	XGrabKey(XtDisplay(wshell), hotkey_code, hotkey_mods | Mod2Mask,
		root_window, False, GrabModeAsync, GrabModeAsync);
	XGrabKey(XtDisplay(wshell), hotkey_code, hotkey_mods | LockMask,
		root_window, False, GrabModeAsync, GrabModeAsync);
	XGrabKey(XtDisplay(wshell), hotkey_code, hotkey_mods | LockMask | Mod2Mask,
		root_window, False, GrabModeAsync, GrabModeAsync);	
	
	XSync(XtDisplay(wshell), False);
	XSetErrorHandler(def_x_err_handler);
	
	return True;		
}

/*
 * Temporarily set in setup_hotkeys to catch BadAccess errors produced
 * by XGrabKey when an already grabbed key is specified.
 */
static int xgrabkey_err_handler(Display *dpy, XErrorEvent *evt)
{
	if(evt->error_code == BadAccess){
		fputs("Cannot setup hotkey. "
			"Specified key code is used by another application.\n",stderr);
		return 0;
	}
	exit(EXIT_FAILURE); /* shouldn't normally happen */
}

/*
 * Called whenever the hotkey handler widget receives a KeyPress event
 */
static void handle_root_event(XEvent *evt)
{
	
	if(evt->type == KeyRelease) {
		XKeyEvent *e = (XKeyEvent*)evt;
	
		if(e->keycode == hotkey_code &&
			((e->state & hotkey_mods) || !hotkey_mods))
				raise_and_focus(wshell);

	} else if(evt->type == PropertyNotify) {
		XPropertyEvent *e = (XPropertyEvent*)evt;

		if((e->atom == xa_cdesk || e->atom == xa_ndesks) && app_res.switcher) {
			unsigned short nws, iws;
			if(get_ws_info(&nws, &iws)) {
				
				if(e->atom == xa_ndesks) {
					Arg args[4];
					int n = 0;

					XtSetArg(args[n], NnumberOfWorkspaces, nws); n++;
					XtSetArg(args[n], NactiveWorkspace, iws); n++;
					if(app_res.horizontal) {
						XtSetArg(args[n], XmNcolumns, nws);
						n++;
					}

					if(nws > 1) {
						XtManageChild(wswitch);
						XtManageChild(wgadrc);
						if(app_res.separators) XtManageChild(wgadsep);
						XmProcessTraversal(wswitch, XmTRAVERSE_CURRENT);
					} else {
						XtUnmanageChild(wswitch);
						if(!app_res.show_date_time) {
							XtUnmanageChild(wgadrc);
							XtUnmanageChild(wgadsep);
						}
					}
					XtSetValues(wswitch, args, n);
				} else if(e->atom == xa_cdesk) {
					SwitcherSetActiveWorkspace(wswitch, iws);
				}
			} else {
				fputs("Failed to retrieve workspace information.\n", stderr);
				XtUnmanageChild(wswitch);
				if(!app_res.show_date_time) {
					XtUnmanageChild(wgadrc);
					XtUnmanageChild(wgadsep);
				}
			}
		}
	}
}

/* 
 * Raise and focus the given shell widget.
 */
void raise_and_focus(Widget w)
{
	static Atom XaNET_ACTIVE_WINDOW=None;
	static Atom XaWM_STATE=None;
	static Atom XaWM_CHANGE_STATE=None;
	Atom ret_type;
	int ret_fmt;
	unsigned long ret_items;
	unsigned long ret_bytes;
	uint32_t *state=NULL;
	XClientMessageEvent evt;
	Display *dpy = XtDisplay(wshell);

	if(XaWM_STATE==None){
		XaWM_STATE=XInternAtom(dpy,"WM_STATE",True);
		XaWM_CHANGE_STATE=XInternAtom(dpy,"WM_CHANGE_STATE",True);
		XaNET_ACTIVE_WINDOW=XInternAtom(dpy,"_NET_ACTIVE_WINDOW",True);
	}

	if(XaWM_STATE==None) return;

	if(XGetWindowProperty(dpy,XtWindow(w),XaWM_STATE,0,1,
		False,XaWM_STATE,&ret_type,&ret_fmt,&ret_items,
		&ret_bytes,(unsigned char**)&state)!=Success) return;
	if(ret_type==XaWM_STATE && ret_fmt && *state==IconicState){
		evt.type=ClientMessage;
		evt.send_event=True;
		evt.message_type=XaWM_CHANGE_STATE;
		evt.display=dpy;
		evt.window=XtWindow(w);
		evt.format=32;
		evt.data.l[0]=NormalState;
		XSendEvent(dpy,XDefaultRootWindow(dpy),True,
			SubstructureNotifyMask|SubstructureRedirectMask,
			(XEvent*)&evt);
	}else{
		if(XaNET_ACTIVE_WINDOW){
			evt.type=ClientMessage,
			evt.send_event=True;
			evt.serial=0;
			evt.display=dpy;
			evt.window=XtWindow(w);
			evt.message_type=XaNET_ACTIVE_WINDOW;
			evt.format=32;

			XSendEvent(dpy,XDefaultRootWindow(dpy),False,
				SubstructureNotifyMask|SubstructureRedirectMask,(XEvent*)&evt);
		}else{
			XRaiseWindow(dpy,XtWindow(w));
			XSync(dpy,False);
			XSetInputFocus(dpy,XtWindow(w),RevertToParent,CurrentTime);
		}
	}
	XFree((char*)state);
}

/*
 * Build menu structure from the rc file
 */
static Boolean construct_menu(void)
{
	Widget *wlevel;
	unsigned int nlevels=1;
	struct tb_entry *entries, *cur;
	int err;

	if((err=tb_parse_config(rc_file_path,&entries))){
		report_rcfile_error(rc_file_path,
			tb_parser_error_string()?tb_parser_error_string():strerror(err));
		return False;
	}

	if(!entries){
		report_rcfile_error(rc_file_path,
			"File doesn't seem to contain any entries.");
		return False;
	}
	
	cur=entries;
	
	while(cur){
		nlevels=(cur->level > nlevels)?cur->level:nlevels;
		cur=cur->next;
	}
	
	if(wmenu){
		XtUnmanageChild(wmenu);
		XtDestroyWidget(wmenu);
	}

	wmenu=XmVaCreateManagedRowColumn(wmain,"menu",
		XmNshadowThickness, 0,
		XmNspacing, 1,
		XmNmarginWidth, 0,
		XmNorientation, (app_res.horizontal ? XmHORIZONTAL:XmVERTICAL),
		XmNrowColumnType, XmMENU_BAR,
		XmNpacking, (app_res.horizontal ? XmPACK_TIGHT:XmPACK_COLUMN),
		XmNpositionIndex, 0,
		NULL);

	#ifdef DEBUG_MENU
	printf("Max %d cascade levels\n",nlevels);
	#endif

	wlevel=calloc(nlevels+1,sizeof(Widget));
	if(!wlevel){
		perror("malloc");
		return False;
	}
	
	cur=entries;
	wlevel[0]=wmenu;
	
	while(cur){
		Widget w;
		XmString title;
		Arg args[10];
		int n;

		if(cur->type == TBE_CASCADE && cur->next){
			Widget new_pulldown, new_cascade;
			
			#ifdef DEBUG_MENU
			printf("Adding Cascade: %s; Level: %d\n",cur->title,cur->level);
			#endif
			new_pulldown=XmCreatePulldownMenu(
				wlevel[cur->level],"commandPulldown",NULL,0);
			
			title=XmStringCreateLocalized(cur->title);
			n=0;
			XtSetArg(args[n],XmNlabelString,title); n++;
			XtSetArg(args[n],XmNmnemonic,(KeySym)cur->mnemonic); n++;
			XtSetArg(args[n],XmNsubMenuId,new_pulldown); n++;
			new_cascade=XmCreateCascadeButtonGadget(
				wlevel[cur->level],"cascadeButton",args,n);
			XmStringFree(title);
		
			wlevel[cur->next->level]=new_pulldown;
						
			XtManageChild(new_cascade);
		
		}else if(cur->type == TBE_COMMAND){
			XtCallbackRec push_callback[]={
				{(XtCallbackProc)menu_command_cb,(XtPointer)cur->command},
				{(XtCallbackProc)NULL,(XtPointer)NULL}
			};
			#ifdef DEBUG_MENU
			printf("Adding Command: %s; Level: %d\n",cur->title,cur->level);
			#endif
			title=XmStringCreateLocalized(cur->title);
			n=0;
			XtSetArg(args[n],XmNlabelString,title); n++;
			if(cur->mnemonic){
				XtSetArg(args[n],XmNmnemonic,(KeySym)cur->mnemonic);
				n++;
			}
			XtSetArg(args[n],XmNactivateCallback,push_callback); n++;
			w=XmCreatePushButtonGadget(wlevel[cur->level],
				"menuButton",args,n);
			XmStringFree(title);
			XtManageChild(w);
		}else if(cur->type == TBE_SEPARATOR){
			w=XmCreateSeparatorGadget(wlevel[cur->level],"separator",NULL,0);
			XtManageChild(w);
		}
		cur=cur->next;
	}

	free(wlevel);
	
	return True;
}

static void report_rcfile_error(const char *rc_file, const char *err_desc)
{
	char *buffer;
	char err_msg[]="Error while parsing RC file:";
	size_t msg_len;

	msg_len=strlen(err_msg)+strlen(err_desc)+strlen(rc_file)+10;
	buffer=malloc(msg_len);
	if(!buffer){
		perror("malloc");
		return;
	}

	sprintf(buffer,"%s %s\n%s.",err_msg,rc_file,err_desc);
	message_dialog(False,buffer);
	free(buffer);
}

static void create_utility_widgets(Widget wparent)
{
	XtCallbackRec cbr[2] = { NULL };
	Widget wmenu;
	Widget wpulldown;
	Widget wcascade;
	Widget w;
	XmString title;
	unsigned short nws = 0;
	unsigned short iws = 0;
	Arg args[10];
	int n;
	
	XtSetArg(args[0], XmNorientation,
		(app_res.horizontal ? XmVERTICAL:XmHORIZONTAL));
	w = XmCreateSeparatorGadget(wparent, "separator", args, 1);
	if(app_res.separators) XtManageChild(w);
	
	/* 'Session' menu */
	wmenu = XmVaCreateManagedRowColumn(wparent, "menu",
		XmNshadowThickness, 0,
		XmNspacing, 1,
		XmNmarginWidth, 0,
		XmNorientation, (app_res.horizontal ? XmHORIZONTAL:XmVERTICAL),
		XmNrowColumnType, XmMENU_BAR, NULL);
	
	wpulldown=XmCreatePulldownMenu(wmenu,"sessionPulldown",NULL,0);
			
	title = XmStringCreateLocalized("Session");
	n=0;
	XtSetArg(args[n], XmNlabelString, title); n++;
	XtSetArg(args[n], XmNmnemonic, (KeySym)'S'); n++;
	XtSetArg(args[n], XmNsubMenuId, wpulldown); n++;
	wcascade = XmCreateCascadeButtonGadget(wmenu, "cascadeButton", args, n);
	XmStringFree(title);
	XtManageChild(wcascade);
		
	n = 0;
	cbr[0].callback = exec_cb;
	title=XmStringCreateLocalized("Execute...");
	XtSetArg(args[n], XmNlabelString, title); n++;
	XtSetArg(args[n], XmNmnemonic, (KeySym)'E'); n++;
	XtSetArg(args[n], XmNactivateCallback, cbr); n++;
	w = XmCreatePushButtonGadget(wpulldown, "execMenuButton", args, n);
	XmStringFree(title);
	XtManageChild(w);

	w = XmCreateSeparatorGadget(wpulldown,"separator",NULL,0);
	XtManageChild(w);

	if(xmsm_cfg & XMSM_CFG_LOCK) {
		n = 0;
		cbr[0].callback = lock_cb;
		title = XmStringCreateLocalized("Lock");
		XtSetArg(args[n], XmNlabelString, title); n++;
		XtSetArg(args[n], XmNmnemonic, (KeySym)'L'); n++;
		XtSetArg(args[n], XmNactivateCallback, cbr); n++;
		w = XmCreatePushButtonGadget(wpulldown,"lockMenuButton", args, n);
		XmStringFree(title);
		XtManageChild(w);
	}

	n = 0;
	cbr[0].callback = logout_cb;
	title=XmStringCreateLocalized("Logout...");
	XtSetArg(args[n], XmNlabelString,title); n++;
	XtSetArg(args[n], XmNmnemonic,(KeySym)'o'); n++;
	XtSetArg(args[n], XmNactivateCallback, cbr); n++;
	w = XmCreatePushButtonGadget(wpulldown,"logoutMenuButton",args,n);
	XmStringFree(title);
	XtManageChild(w);

	if(xmsm_cfg & XMSM_CFG_SUSPEND) {
		n = 0;
		cbr[0].callback = suspend_cb;
		title = XmStringCreateLocalized("Suspend");
		XtSetArg(args[n], XmNlabelString,title); n++;
		XtSetArg(args[n], XmNmnemonic,(KeySym)'S'); n++;
		XtSetArg(args[n], XmNactivateCallback, cbr); n++;
		w = XmCreatePushButtonGadget(wpulldown, "suspendMenuButton", args, n);
		XmStringFree(title);
		XtManageChild(w);
	}

	XtSetArg(args[0], XmNorientation,
		(app_res.horizontal ? XmVERTICAL:XmHORIZONTAL));
	wgadsep = XmCreateSeparatorGadget(wparent, "separator", args, 1);

	/* Time and workspace switcher RC */
	wgadrc = XmVaCreateRowColumn(wparent, "gadgets",
		XmNmarginWidth, 3,
		XmNmarginHeight, 3,
		XmNspacing, 3,
		XmNorientation, (app_res.horizontal ? XmHORIZONTAL:XmVERTICAL),
		NULL);

	/* The workspace switcher */
	n = 0;
	if(get_ws_info(&nws, &iws)) {
		XtSetArg(args[n], NnumberOfWorkspaces, nws); n++;
		XtSetArg(args[n], NactiveWorkspace, iws); n++;
	}
	cbr[0].callback = ws_change_cb;
	XtSetArg(args[n], XmNvalueChangedCallback, &cbr); n++;
	wswitch = CreateSwitcher(wgadrc, "workspaceSwitcher", args, n);
	if(app_res.switcher && (nws > 1)) {
		XtManageChild(wswitch);
		if(app_res.separators) XtManageChild(wgadsep);
	}

	/* The time-date display */
	n = 0;
	XtSetArg(args[n], XmNshadowType, XmSHADOW_IN); n++;
	XtSetArg(args[n], XmNshadowThickness, 1); n++;
	XtSetArg(args[n], XmNmarginWidth, 2); n++;
	XtSetArg(args[n], XmNmarginHeight, 2); n++;
	wdtframe = XmCreateFrame(wgadrc, "dateTimeFrame", args, n);

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	wdtlabel = XmCreateLabelGadget(wdtframe, "dateTime", args, n);
	if(app_res.show_date_time){
		XtManageChild(wdtlabel);
		XtManageChild(wdtframe);
		if(app_res.separators) XtManageChild(wgadsep);
		time_update_cb(NULL,NULL);
	}
	if(XtIsManaged(wswitch) || app_res.show_date_time)
		XtManageChild(wgadrc);
}

/*
 * Search home and system directories for the RC file.
 * Returns a malloc()ed full path to the RC file on success,
 * or NULL otherwise.
 */
#ifndef RCDIR
#define RCDIR "/usr/local/etc/X11"
#endif

static char* find_rc_file(void)
{
	size_t len;
	char *home=NULL;
	char *lang=NULL;
	char *path;
	int i;
	char *sys_paths[32]={
		RCDIR,
		"/etc/X11",
		"/usr/lib/X11",
		"/usr/local/lib/X11",
		NULL
	};
	
	home = getenv("HOME");
	lang = getenv("LANG");
	
	if(!home){
		fprintf(stderr,"HOME is not set!\n");
		return NULL;
	}

	len = 36 + strlen(home) + strlen(RC_NAME);
	if(lang) len += strlen(lang);

	path = malloc(len);
	if(!path){
		perror("malloc");
		return NULL;
	}

	snprintf(path, len, "%s/.%s", home, RC_NAME);
	if(!access(path,R_OK)) return path;

	for(i = 0; sys_paths[i] != NULL; i++){
		if(lang){
			snprintf(path, len, "%s/%s/%s", sys_paths[i], lang, RC_NAME);
			if(!access(path,R_OK)) return path;
		}
		snprintf(path, len, "%s/%s", sys_paths[i], RC_NAME);
		if(!access(path,R_OK)) return path;
	}

	free(path);
	return NULL;
}

/*
 * Time display update interval function
 */
static void time_update_cb(XtPointer client_data, XtIntervalId *id)
{
	char time_str[256];
	time_t secs;
	struct tm *the_time;
	XmString xm_str;
	
	time(&secs);
	the_time=localtime(&secs);
	strftime(time_str,255,app_res.date_time_fmt,the_time);
	xm_str=XmStringCreateLocalized(time_str);
	
	XtVaSetValues(wdtlabel, XmNlabelString, xm_str, NULL);
	XmStringFree(xm_str);
	
	XtAppAddTimeOut(app_context,
		60000-(the_time->tm_sec*1000),time_update_cb,NULL);
}

/*
 * SIGUSR1 RC file reload request handler
 */
static void xt_sigusr1_handler(XtPointer client_data, XtSignalId *id)
{
	/* on parse error, the previous configuration remains active
	 * and the user is informed about */
	construct_menu();
}

/*
 * Fetches a string from an input dialog and runs it as a command
 */
static void exec_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	static Widget wdlg = None;
	static Widget wtext = None;
	Arg args[5];
	int n = 0;

	if(wdlg == None){
		XmString xm_title;
		XmString xm_prompt;
		XtCallbackRec callback[]={
			{(XtCallbackProc)exec_dialog_cb, (XtPointer) NULL},
			{(XtCallbackProc)NULL, (XtPointer)NULL}
		};
		/* Reset text field's Home/End translations to defaults, since the
		 * selection box widget overrides them to control the list above,
		 * which is rather unexpected and not very useful either */
		char alt_tt_src[] = 
			":s <Key>osfEndLine: end-of-line(extend)\n"
			":s <Key>osfBeginLine: beginning-of-line(extend)\n"
			":<Key>osfEndLine: end-of-line()\n"
			":<Key>osfBeginLine: beginning-of-line()\n";
		XtTranslations alt_tt = NULL;

		n = 0;
		xm_title = XmStringCreateLocalized(APP_TITLE);
		xm_prompt = XmStringCreateLocalized("Specify a command");
		XtSetArg(args[n], XmNdialogTitle, xm_title); n++;
		XtSetArg(args[n], XmNokCallback, callback); n++;
		XtSetArg(args[n], XmNcancelCallback, callback); n++;
		XtSetArg(args[n], XmNselectionLabelString, xm_prompt); n++;

		wdlg = XmCreatePromptDialog(wshell, "promptDialog", args, n);
		XmStringFree(xm_title);
		XmStringFree(xm_prompt);

		wtext = XmSelectionBoxGetChild(wdlg, XmDIALOG_TEXT);
		alt_tt = XtParseTranslationTable(alt_tt_src);
		if(alt_tt) XtOverrideTranslations(wtext, alt_tt);

		XtUnmanageChild(XmSelectionBoxGetChild(wdlg, XmDIALOG_HELP_BUTTON));
	} else {
		char *text;
		size_t len;
		
		text = XmTextFieldGetString(wtext);
		if( (len = strlen(text)) ) {
			XmTextFieldSetSelection(wtext, 0, len,
				XtLastTimestampProcessed(XtDisplay(wtext)));
		}
		XtFree(text);
	}
	XtManageChild(wdlg);
}

/*
 * exec_cb dialog callback
 */
static void exec_dialog_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	char *command;
	char *exp_cmd;
	int errval;
	XmSelectionBoxCallbackStruct *cbs=
		(XmSelectionBoxCallbackStruct*)call_data;

	if(cbs->reason == XmCR_CANCEL) return;

	command = (char*)XmStringUnparse(cbs->value, NULL, 0,
			XmCHARSET_TEXT, NULL, 0, XmOUTPUT_ALL);
	if(!command) return;

	if(!strlen(command)) {
		XtFree(command);
		return;
	}

	errval = expand_env_vars(command, &exp_cmd);
	XtFree(command);

	if(errval) {
		report_exec_error("Failed to parse command string", command, errval);
		return;
	}

	if((errval = exec_command(exp_cmd)))
		report_exec_error("Error executing command", exp_cmd, errval);
		
	free(exp_cmd);
}

/*
 * Displays a blocking message dialog. If 'confirm' is True, the dialog will
 * have OK+Cancel buttons, OK only otherwise. Returns True if OK was chosen.
 */
static Boolean message_dialog(Boolean confirm, const char *message_str)
{
	Widget wdlg;
	XmString xm_message_str;
	Arg args[8];
	int n = 0;
	int result = (-1);
	XmString xm_title;
	XtCallbackRec callback[]={
		{(XtCallbackProc)message_dialog_cb,(XtPointer)&result},
		{(XtCallbackProc)NULL,(XtPointer)NULL}
	};

	xm_message_str=XmStringCreateLocalized((char*)message_str);
	xm_title=XmStringCreateLocalized(APP_TITLE);

	XtSetArg(args[n], XmNdialogTitle, xm_title); n++;
	XtSetArg(args[n], XmNokCallback, callback); n++;
	XtSetArg(args[n], XmNcancelCallback, callback); n++;
	XtSetArg(args[n], XmNmessageString,xm_message_str); n++;
	XtSetArg(args[n], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); n++;

	wdlg = XmCreateMessageDialog(wshell, "messageDialog", args, n);
	
	n = 0;
	XtSetArg(args[n], XmNdialogType,
		confirm ? XmDIALOG_QUESTION : XmDIALOG_INFORMATION); n++;
	XtSetArg(args[n], XmNdefaultButtonType,
		confirm ? XmDIALOG_CANCEL_BUTTON : XmDIALOG_OK_BUTTON); n++;
	
	XtSetValues(wdlg, args, n);

	XmStringFree(xm_title);
	XmStringFree(xm_message_str);

	if(!confirm) XtUnmanageChild(
		XmMessageBoxGetChild(wdlg, XmDIALOG_CANCEL_BUTTON));
	XtUnmanageChild(XmMessageBoxGetChild(wdlg, XmDIALOG_HELP_BUTTON));

	XtManageChild(wdlg);

	while(XtIsManaged(wdlg) && result == (-1))
		XtAppProcessEvent(app_context, XtIMXEvent);
	
	if(result == (-1)) result = 0;
	
	XtDestroyWidget(wdlg);
	XSync(XtDisplay(wdlg), False);
	XmUpdateDisplay(wshell);
	
	return (Boolean)result;
}

/*
 * message_dialog dialog callback
 */
static void message_dialog_cb(Widget w, XtPointer client_data,
	XtPointer call_data)
{
	XmSelectionBoxCallbackStruct *cbs=
		(XmSelectionBoxCallbackStruct*)call_data;
	char *result=(Boolean*)client_data;

	if(cbs->reason==XmCR_OK)
		*result=1;
	else
		*result=0;
}

static int exec_command(const char *cmd_spec)
{
	pid_t pid;
	char *str;
	char *p,*t;
	char pc=0;
	int done=0;
	char **argv=NULL;
	size_t argv_size=0;
	unsigned int argc=0;
	volatile int errval=0;
	
	str = strdup(cmd_spec);

	p=str;
	t=NULL;
	
	/* split the command string into separate arguments */
	while(!done){
		if(!t){
			while(*p && isblank(*p)) p++;
			if(*p == '\0') break;
			t = p;
		}
		
		if(*p == '\"' || *p == '\''){
			if(pc == '\\'){
				/* literal " or ' */
				memmove(p - 1, p, strlen(p) + 1);
			}else{
				/* quotation marks, remove them ignoring blanks within */
				memmove(p, p + 1, strlen(p));
				while(*p != '\"' && *p != '\''){
					if(*p == '\0'){
						if(argv) free(argv);
						return EINVAL;
					}
					p++;
				}
				memmove(p, p + 1, strlen(p));
			}
		}
		if(isblank(*p) || *p == '\0'){
			if(*p == '\0') done = 1;
			if(argv_size < argc+1){
				char **new_ptr;
				new_ptr = realloc(argv, (argv_size += 64) * sizeof(char*));
				if(!new_ptr){
					free(str);
					if(argv) free(argv);
					return ENOMEM;
				}
				argv=new_ptr;
			}
			*p = '\0';
			argv[argc] = t;
			
			#ifdef DEBUG_EXEC
			printf("argv[%d]: %s\n",argc,argv[argc]);
			#endif
			
			t=NULL;
			argc++;
		}
		pc = *p;
		p++;
	}
	
	if(!argc) return EINVAL;
	argv[argc]=NULL;
	
	pid=vfork();
	if(pid == 0){
		setsid();
		
		if(execvp(argv[0],argv) == (-1))
			errval=errno;
		_exit(0);
	}else if(pid == -1){
		errval=errno;
	}
	
	free(str);
	free(argv);
	return errval;
}

/*
 * Display a message dialog containing the failed command name and
 * the system error string.
 */
static void report_exec_error(const char *err_msg,
	const char *command, int errno_value)
{
	char *errno_str=strerror(errno_value);
	char *buffer;

	buffer=malloc(strlen(err_msg)+strlen(command)+strlen(errno_str)+10);
	if(!err_msg){
		perror("malloc");
		return;
	}		
	sprintf(buffer,"%s \'%s\'.\n%s.",err_msg,command,errno_str);
	message_dialog(False,buffer);
	free(buffer);
}

static void menu_command_cb(Widget w,
	XtPointer client_data, XtPointer call_data)
{
	int errval;
	char *cmd = (char*) client_data;
	char *exp_cmd;
	
	errval = expand_env_vars(cmd, &exp_cmd);
	if(errval) {
		report_exec_error("Failed to parse command string", cmd, errval);
		return;
	}

	if((errval = exec_command(exp_cmd)))
		report_exec_error("Error executing command", exp_cmd, errval);
	
	free(exp_cmd);
}

/*
 * Sends a command message to XmSm. Returns True on success.
 */
static Boolean send_xmsm_cmd(const char *command)
{
	Display *dpy = XtDisplay(wshell);
	Window root = DefaultRootWindow(dpy);
	Window shell;
	Atom ret_type;
	int ret_format;
	unsigned long ret_items;
	unsigned long left_items;
	unsigned char *prop_data;
	
	if(xa_xmsm_mgr == None || xa_xmsm_pid == None) return False;
	
	XGetWindowProperty(dpy, root, xa_xmsm_mgr, 0, sizeof(Window),
		False, XA_WINDOW, &ret_type, &ret_format, &ret_items,
		&left_items, &prop_data);
	
	if(ret_type == XA_WINDOW){
		shell = *((Window*)prop_data);
		XFree(prop_data);

		def_x_err_handler = XSetErrorHandler(local_x_err_handler);
		sm_reqstat = True;
		
		XGetWindowProperty(dpy, shell, xa_xmsm_pid, 0, sizeof(pid_t),
			False, XA_INTEGER, &ret_type, &ret_format,
			&ret_items, &left_items, &prop_data);
		if(ret_items) XFree(prop_data);

		if(ret_type == XA_INTEGER) {
			XClientMessageEvent evt = {
				.type = ClientMessage,
				.serial = 0,
				.send_event = True,
				.display = dpy,
				.window = None,
				.message_type = xa_xmsm_cmd,
				.format = 8
			};

			memcpy(evt.data.b, command, strlen(command) + 1);
			XSendEvent(dpy, shell, True, 0, (XEvent*)&evt);

			XSync(dpy, False);
			XSetErrorHandler(def_x_err_handler);
			return sm_reqstat;
		}
		XSync(dpy, False);
		XSetErrorHandler(def_x_err_handler);
	}
	return False;
}

/*
 * Retrieves xmsm configuration state. Returns True on success.
 */
static Boolean get_xmsm_config(unsigned long *flags)
{
	Display *dpy = XtDisplay(wshell);
	Window root = DefaultRootWindow(dpy);
	Atom ret_type;
	int ret_format;
	unsigned long ret_items;
	unsigned long left_items;
	unsigned char *prop_data;
	
	if(xa_xmsm_cfg == None) return False;
	
	XGetWindowProperty(dpy, root, xa_xmsm_cfg, 0, sizeof(unsigned long),
			False, XA_INTEGER, &ret_type, &ret_format, &ret_items,
			&left_items, &prop_data);
	if(ret_items) {
		*flags = *prop_data;
		XFree(prop_data);
		return True;
	}
	return False;
}

/*
 * This is temporarily set in send_xmsm_pid just to catch BadWindow errors
 * originating from a window handle stored on root in MGR_ATOM_NAME by
 * xmsm process that is no longer active
 */
static int local_x_err_handler(Display *dpy, XErrorEvent *evt)
{
	if(evt->error_code == BadWindow) {
		sm_reqstat = False;
		return 0;
	}
	return def_x_err_handler(dpy,evt);
}

/* Retrieves EWMH workspace info properties from the root window */
static Boolean get_ws_info(unsigned short *ws_count, unsigned short *iactive)
{
	Boolean success = True;
	Display *dpy = XtDisplay(wshell);
	Window root = DefaultRootWindow(dpy);

	Atom ret_type;
	int ret_format;
	unsigned long ret_items;
	unsigned long left_items;
	unsigned char *prop_data;
	
	XGetWindowProperty(dpy, root, xa_ndesks, 0, sizeof(unsigned long),
			False, XA_CARDINAL, &ret_type, &ret_format, &ret_items,
			&left_items, &prop_data);
	if(ret_items) {
		*ws_count = (unsigned short)*prop_data;
		XFree(prop_data);
	} else {
		ws_count = 0;
		success = False;
	}

	XGetWindowProperty(dpy, root, xa_cdesk, 0, sizeof(unsigned long),
			False, XA_CARDINAL, &ret_type, &ret_format, &ret_items,
			&left_items, &prop_data);

	if(ret_items) {
		*iactive = (unsigned short)*prop_data;
		XFree(prop_data);
	} else {
		*iactive = 0;
		success = False;
	}

	return success;
}

static void ws_change_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	Display *dpy = XtDisplay(wshell);
	Window root_wnd = XDefaultRootWindow(XtDisplay(wshell));
	short *index = (short*)call_data;

	XClientMessageEvent evt = {
		.type = ClientMessage,
		.display = dpy,
		.window = root_wnd,
		.message_type = xa_cdesk,
		.format = 32
	};

	evt.data.l[0] = (long)*index;
	evt.data.l[1] = CurrentTime;
	XSendEvent(dpy, root_wnd, False,
		SubstructureRedirectMask | SubstructureNotifyMask, (XEvent*)&evt);

}

static void suspend_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	if(!send_xmsm_cmd(XMSM_SUSPEND_CMD)){
		message_dialog(False, xmsm_cmd_err);
	}
}

static void lock_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	if(!send_xmsm_cmd(XMSM_LOCK_CMD)){
		message_dialog(False, xmsm_cmd_err);
	}
}

static void logout_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	if(!send_xmsm_cmd(XMSM_LOGOUT_CMD)){
		message_dialog(False, xmsm_cmd_err);
	}
}

static void sigchld_handler(int sig)
{
	int status;
	waitpid(-1, &status, WNOHANG);
}

static void sigusr_handler(int sig)
{
	if(sig == SIGUSR1) XtNoticeSignal(xt_sigusr1);
}
