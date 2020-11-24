/*
 * Copyright (C) 2018 alx@fastestcode.org
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
 * A simple session manager for use with MWM and xmtoolbox
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/TextF.h>
#include <Xm/IconG.h>
#include <Xm/Frame.h>
#include <Xm/DrawingA.h>
#include <Xm/LabelG.h>
#include <Xm/DialogS.h>
#include <Xm/SeparatoG.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleBG.h>
#include <Xm/MessageB.h>
#include <Xm/MwmUtil.h>
#include <X11/IntrinsicP.h>
#include <X11/XKBlib.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/scrnsaver.h>
#include <errno.h>
#include <assert.h>
#ifdef __linux__
#include <crypt.h>
#include <shadow.h>
#endif
#include "smglobal.h"

/* Local prototypes */
static Boolean set_privileges(Boolean);
static void init_session(void);
static void sigchld_handler(int);
static void sigterm_handler(int);
static void sigusr_handler(int);
static void create_locking_widgets(void);
static void get_screen_size(Dimension*,Dimension*,Position*,Position*);
static void lock_screen(void);
static void unlock_screen(void);
static void show_unlock_widget(void);
static void show_covers(Boolean);
static void reset_unlock_timer(void);
static void set_unlock_message(const char*);
static void unlock_widget_timeout_cb(XtPointer,XtIntervalId*);
static void blank_delay_timeout_cb(XtPointer,XtIntervalId*);
static void lock_timeout_cb(XtPointer,XtIntervalId*);
static void passwd_modify_cb(Widget,XtPointer,XtPointer);
static void passwd_enter_cb(Widget,XtPointer,XtPointer);
static void exit_dialog_cb(Widget,XtPointer,XtPointer);
static void covers_up_cb(Widget,XtPointer,XEvent*,Boolean*);
static void register_screen_saver(void);
static int launch_process(const char*);
static void process_sessionetc(void);
static void set_root_background(void);
static void set_numlock_state(void);
static int local_x_err_handler(Display*,XErrorEvent*);
static void xt_sigterm_handler(XtPointer,XtSignalId*);
static void xt_sigusr1_handler(XtPointer,XtSignalId*);

/* Application resources */
struct session_res {
	Boolean enable_locking;
	Boolean blank_on_lock;
	char *numlock_state;
	char *wkspace_bg_image;
	Pixel wkspace_bg_pixel;
	char *lock_bg_image;
	Pixel lock_bg_pixel;
	char *window_manager;
	char *launcher;
	unsigned int lock_timeout;
	unsigned int unlock_scr_timeout;
	unsigned int blank_timeout;
	unsigned int prim_xinerama_screen;
	Boolean show_shutdown;
	Boolean show_reboot;
} app_res;

#ifndef PREFIX
/* Used to construct full paths to mwm and xmtoolbox */
#define PREFIX "/usr/local"
#endif

#define RES_FIELD(f) XtOffsetOf(struct session_res,f)
XtResource xrdb_resources[]={
	{ "enableLocking","EnableLocking",XmRString,sizeof(Boolean),
		RES_FIELD(enable_locking),XmRImmediate,(XtPointer)True
	},
	{ "blankOnLock","BlankOnLock",XmRString,sizeof(Boolean),
		RES_FIELD(blank_on_lock),XmRImmediate,(XtPointer)True
	},
	{ "numLockState","NumLockState",XmRString,sizeof(String),
		RES_FIELD(numlock_state),XmRImmediate,(XtPointer)"on"
	},
	{ "workspaceBackgroundImage","WorkspaceBackgroundImage",
		XmRString,sizeof(String),
		RES_FIELD(wkspace_bg_image),XmRImmediate,(XtPointer)NULL
	},
	{ "workspaceBackgroundColor","WorkspaceBackgroundColor",
		XmRPixel,sizeof(Pixel),RES_FIELD(wkspace_bg_pixel),
		XmRString, (XtPointer)"#4C719E"
	},
	{ "lockBackgroundImage","LockBackgroundImage",
		XmRString,sizeof(String),
		RES_FIELD(lock_bg_image),XmRImmediate,(XtPointer)NULL
	},
	{ "lockBackgroundColor","LockBackgroundColor",
		XmRPixel,sizeof(Pixel),RES_FIELD(lock_bg_pixel),
		XmRString, (XtPointer)"#4C719E"
	},
	{ "windowManager","WindowManager",XmRString,sizeof(String),
		RES_FIELD(window_manager),XmRImmediate,(XtPointer)PREFIX"/bin/emwm"
	},
	{ "launcher","Launcher",XmRString,sizeof(String),
		RES_FIELD(launcher),XmRImmediate,(XtPointer)PREFIX"/bin/xmtoolbox"
	},
	{ "blankTimeout","BlankTimeout",XmRInt,sizeof(unsigned int),
		RES_FIELD(blank_timeout),XmRImmediate,(XtPointer)400
	},
	{ "lockTimeout","LockTimeout",XmRInt,sizeof(unsigned int),
		RES_FIELD(lock_timeout),XmRImmediate,(XtPointer)600
	},
	{ "unlockScreenTimeout","UnlockScreenTimeout",XmRInt,sizeof(unsigned int),
		RES_FIELD(unlock_scr_timeout),XmRImmediate,(XtPointer)8
	},
	{ "primaryXineramaScreen","PrimaryXineramaScreen",XmRInt,
		sizeof(unsigned int),RES_FIELD(prim_xinerama_screen),
		XmRImmediate,(XtPointer)0
	},
	{ "showShutdown","ShowShutdown",XmRBoolean,
		sizeof(unsigned int),RES_FIELD(show_shutdown),
		XmRImmediate,(XtPointer)True
	},
	{ "showReboot","ShowReboot",XmRBoolean,
		sizeof(unsigned int),RES_FIELD(show_reboot),
		XmRImmediate,(XtPointer)True
	}
};
#undef RES_FIELD

#ifndef SHUTDOWN_CMD
#define SHUTDOWN_CMD "/sbin/poweroff"
#endif
#ifndef REBOOT_CMD
#define REBOOT_CMD "/sbin/reboot"
#endif

#define MSG_NOACCESS "Password Incorrect"
#define APP_TITLE "XmSm"
#define APP_NAME "xmsm"
#define DEF_MAX_PASSWD 255

#define log_msg(fmt,...) fprintf(stderr,"[XMSM] "fmt,##__VA_ARGS__)

char *bin_name = NULL;
static Atom xa_mgr;
static Atom xa_pid;
XtAppContext app_context;
Widget wshell;
static Widget *wcovers;
static Widget wunlock;
static Widget wpasswd;
static Widget wmessage;
static XtIntervalId unlock_widget_timer=None;
static XtIntervalId lock_timer=None;
static XtSignalId xt_sigterm;
static XtSignalId xt_sigusr;
static Boolean scr_locked = False;
static Boolean covers_up = False;
static Boolean unlock_up = False;
static Boolean ptr_grabbed = False;
static Boolean kbd_grabbed = False;
static XHostAddress *acl_hosts = NULL;
static int num_acl_hosts;
static Bool acl_state;
static int xss_event_base = 0;
static int xss_error_base = 0;
static int (*def_x_err_handler)(Display*,XErrorEvent*)=NULL;


int main(int argc, char **argv)
{
	int rv;
	char *home;
	
	bin_name = argv[0];

	set_privileges(False);
		
	home=getenv("HOME");
	if(!home){
		log_msg("HOME is not set.\n");
		return EXIT_FAILURE;
	}

	if(chdir(home) == (-1)){
		perror("chdir");
	}

	signal(SIGCHLD,sigchld_handler);
	signal(SIGTERM,sigterm_handler);
	signal(SIGUSR1,sigusr_handler);
	signal(SIGUSR2,sigusr_handler);
	
	XtSetLanguageProc(NULL,NULL,NULL);
	XtToolkitInitialize();

	wshell=XtVaAppInitialize(&app_context,
		APP_TITLE,NULL,0,
		&argc,argv,NULL,
		XmNiconName,APP_TITLE,
		XmNmwmFunctions,0,
		XmNmwmDecorations,0,
		XmNmappedWhenManaged,False,
		XmNoverrideRedirect,True,NULL);

	XtGetApplicationResources(wshell,&app_res,xrdb_resources,
		XtNumber(xrdb_resources),NULL,0);

	xa_mgr = XInternAtom(XtDisplay(wshell),XMSM_ATOM_NAME,False);
	xa_pid = XInternAtom(XtDisplay(wshell),XMSM_PID_ATOM_NAME,False);
	
	/* init_session requires an X window */
	XtRealizeWidget(wshell);

	init_session();
	set_root_background();
	set_numlock_state();

	if(app_res.enable_locking) {
		register_screen_saver();
		create_locking_widgets();
	}

	xt_sigterm = XtAppAddSignal(app_context,xt_sigterm_handler,NULL);
	xt_sigusr = XtAppAddSignal(app_context,xt_sigusr1_handler,NULL);
	
	rv = launch_process(app_res.window_manager);
	if(rv){
		log_msg("Failed to exec the window manager (%s): %s\n",
			app_res.window_manager,strerror(rv));
		return EXIT_FAILURE;
	}
	
	if(app_res.launcher){
		rv = launch_process(app_res.launcher);
		if(rv){
			log_msg("Failed to exec the launcher (%s): %s\n",
				app_res.launcher,strerror(rv));
			return EXIT_FAILURE;
		}
	}
	process_sessionetc();

	for(;;) {
		XEvent evt;
		
		XtAppNextEvent(app_context,&evt);
		
		if(evt.type == KeyPress || evt.type == ButtonPress ||
			evt.type == MotionNotify){
			if(unlock_up){
				reset_unlock_timer();
			}else if(scr_locked){
				show_unlock_widget();
				/* discard this event, since its purpose was to
				 * map the 'unlock' widget */
				continue;
			}
		}else if(evt.type == xss_event_base){
			XScreenSaverNotifyEvent *xsse=(XScreenSaverNotifyEvent*)&evt;
			
			if(!scr_locked && lock_timer == None &&
				xsse->state == ScreenSaverOn &&	app_res.lock_timeout){
				lock_timer = XtAppAddTimeOut(app_context,
					app_res.lock_timeout,lock_timeout_cb,NULL);
			}else if(xsse->state == ScreenSaverOff){
				if(lock_timer) XtRemoveTimeOut(lock_timer);
				lock_timer = None;
			}
		}
		XtDispatchEvent(&evt);
	}

	return 0;
}

/*
 * Puts up covers and the unlock widget, removes ACL.
 */
static void lock_screen(void)
{
	Boolean can_auth = False;
	char *login;
	struct passwd *passwd;
	
	/* make sure we can authenticate before locking */
	login = getlogin();
	
	set_privileges(True);
	
	passwd = getpwnam(login);
	if(passwd && passwd->pw_passwd[0] != '*') can_auth = True;
	
	#ifdef __linux__
	if(passwd && passwd->pw_passwd[0] == 'x'){
		struct spwd *spwd = getspnam(login);
		if(spwd) can_auth = True;	
	}
	#endif /* __linux __ */
	
	set_privileges(False);

	if(!can_auth){
		log_msg("Cannot authenticate. Screen locking disabled!\n");
		app_res.enable_locking = False;
		return;
	}
	
	show_covers(True);
	scr_locked = True;
	if(!unlock_up) show_unlock_widget();
	
	acl_hosts = XListHosts(XtDisplay(wshell),&num_acl_hosts,&acl_state);
	if(acl_hosts) XRemoveHosts(XtDisplay(wshell),acl_hosts,num_acl_hosts);
	
	XFlush(XtDisplay(wshell));
}

/*
 * Unmaps cover windows, ungrabs input and restores ACL
 */
static void unlock_screen(void)
{
	assert(covers_up);

	if(ptr_grabbed){
		XtUngrabPointer(wcovers[0],CurrentTime);
		ptr_grabbed = False;
	}
	if(kbd_grabbed){
		XtUngrabKeyboard(wcovers[0],CurrentTime);
		kbd_grabbed = False;
	}

	show_covers(False);
	unlock_up = False;
	
	if(unlock_widget_timer){
		XtRemoveTimeOut(unlock_widget_timer);
		unlock_widget_timer = None;
	}
	
	if(acl_hosts){
		XAddHosts(XtDisplay(wshell),acl_hosts,num_acl_hosts);
		XFree(acl_hosts);
	}
	
	XFlush(XtDisplay(wshell));
}

/*
 * This is the wcovers[0] visibility state change callback.
 * It grabs input devices when cover windows are mapped.
 */
static void covers_up_cb(Widget w, XtPointer p, XEvent *evt, Boolean *dsp)
{
	if(evt->type != VisibilityFullyObscured){
		if(!ptr_grabbed){
			if(XtGrabPointer(w,True,ButtonPressMask|PointerMotionMask,
				GrabModeAsync,GrabModeAsync,
				None,None,CurrentTime) == GrabSuccess) ptr_grabbed = True;
		}
		if(!kbd_grabbed){
			if(XtGrabKeyboard(w,True,GrabModeAsync,
				GrabModeAsync,CurrentTime) == GrabSuccess) kbd_grabbed = True;
		}
		if(!ptr_grabbed || !kbd_grabbed){
			log_msg("Cannot lock! Failed to grab input devices.\n");
			unlock_screen();
		}
	}
}

/*
 * If 'show' is true, maps all cover windows, unmaps them otherwise.
 */
static void show_covers(Boolean show)
{
	int i,ncovers = XScreenCount(XtDisplay(wshell));

	if(show && !covers_up){
		for(i=0; i<ncovers; i++) XtMapWidget(wcovers[i]);
		covers_up = True;
	}else if(!show && covers_up){
		for(i=0; i<ncovers; i++) XtUnmapWidget(wcovers[i]);	
		covers_up = False;
	}
}

/*
 * Displays the 'unlock' widget and sets the timeout
 */
static void show_unlock_widget(void)
{
	XmTextFieldSetString(wpasswd,"");
	unlock_up = True;
	XtMapWidget(wunlock);
	XmProcessTraversal(wunlock,XmTRAVERSE_CURRENT);
	reset_unlock_timer();
	XFlush(XtDisplay(wshell));

}

/*
 * Resets the password input field and hides the 'unlock' widget on timeout
 */
static void unlock_widget_timeout_cb(XtPointer ptr, XtIntervalId *timer)
{
	unlock_widget_timer = None;
	unlock_up = False;
	set_unlock_message(NULL);
	if(covers_up) XtUnmapWidget(wunlock);
	XFlush(XtDisplay(wshell));
}

/*
 * Sets or resets the 'unlock' dialog timer
 */
static void reset_unlock_timer(void)
{
	if(unlock_widget_timer) XtRemoveTimeOut(unlock_widget_timer);
	unlock_widget_timer = XtAppAddTimeOut(app_context,
		app_res.unlock_scr_timeout*1000,unlock_widget_timeout_cb,NULL);
}

/*
 * Set the string for the message area below the password input field
 */
static void set_unlock_message(const char *msg)
{
	XmString str;

	if(msg){
		str = XmStringCreateLocalized((char*)msg);
	}else{
		str = XmStringCreateSimple(" ");
	}
	XtVaSetValues(wmessage,XmNlabelString,str,NULL);
	XmStringFree(str);
}

/*
 * Creates password entry and confirmation dialog widgets.
 */
static void create_locking_widgets(void)
{
	char *login;
	char host[256]="localhost";
	char *locked_by;
	Display *dpy = XtDisplay(wshell);
	XColor bg_color;
	XmString label;
	XtTranslations drawing_area_tt;
	XtTranslations passwd_input_tt;
	Widget wrowcol;
	Widget wtmp;
	Dimension swidth, width;
	Dimension sheight, height;
	Position xoff,yoff;
	char *pwb;
	long pwb_size;
	int nscreens;
	int i;

	nscreens = XScreenCount(dpy);
	wcovers = calloc(nscreens,sizeof(Widget));
	if(!wcovers) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	
	/* Have drawing areas generate pointer movement events */
	drawing_area_tt = XtParseTranslationTable("<Motion>:DrawingAreaInput()\n");
	
	/* Fetch the RGB value of the background pixel so we can
	 * make pixels for other screens */
	bg_color.pixel = app_res.lock_bg_pixel;
	XQueryColor(dpy,DefaultColormap(dpy,DefaultScreen(dpy)),&bg_color);

	for(i=0; i < nscreens; i++){
		Arg args[18];
		int n = 0;
		Widget w;
		Pixmap bg_pixmap = XmUNSPECIFIED_PIXMAP;

	
		XAllocColor(dpy,DefaultColormap(dpy,i),&bg_color);
		
		if(app_res.lock_bg_image){
			Pixel shadow, fnord;
		
			XmGetColors(ScreenOfDisplay(dpy,i),DefaultColormap(dpy,i),
				bg_color.pixel,&fnord,&fnord,&shadow,&fnord);

			bg_pixmap = XmGetPixmap(ScreenOfDisplay(dpy,i),
				app_res.lock_bg_image,shadow,bg_color.pixel);
			if(bg_pixmap == XmUNSPECIFIED_PIXMAP)
				log_msg("Failed to load pixmap: %s\n",app_res.lock_bg_image);
		}
		
		XtSetArg(args[n],XmNmwmDecorations,0); n++;
		XtSetArg(args[n],XmNmwmFunctions,0); n++;
		XtSetArg(args[n],XmNx,0); n++;
		XtSetArg(args[n],XmNy,0); n++;
		XtSetArg(args[n],XmNwidth,DisplayWidth(dpy,i)); n++;
		XtSetArg(args[n],XmNheight,DisplayHeight(dpy,i)); n++;
		XtSetArg(args[n],XmNuseAsyncGeometry,True); n++;
		XtSetArg(args[n],XmNmappedWhenManaged,False); n++;
		XtSetArg(args[n],XmNresizePolicy,XmRESIZE_NONE); n++;
		XtSetArg(args[n],XmNdepth,DefaultDepth(dpy,i)); n++;
		XtSetArg(args[n],XmNcolormap,DefaultColormap(dpy,i)); n++;
		XtSetArg(args[n],XmNscreen,XScreenOfDisplay(dpy,i)); n++;
		/* can be set at creation time only, so... */
		if(i == 0)
			XtSetArg(args[n],XmNmwmInputMode,MWM_INPUT_SYSTEM_MODAL); n++;
				
		wcovers[i] = XtCreatePopupShell("coverShell",
			topLevelShellWidgetClass,wshell,args,n);
		
		n = 0;
		XtSetArg(args[n],XmNbackground,bg_color.pixel); n++;
		XtSetArg(args[n],XmNbackgroundPixmap,bg_pixmap); n++;
		w = XmCreateDrawingArea(wcovers[i],"coverBackdrop",args,n);
		XtAugmentTranslations(w,drawing_area_tt);
		XtManageChild(w);

		XtRealizeWidget(wcovers[i]);
	}
	
	XtAddEventHandler(wcovers[0],VisibilityChangeMask,False,covers_up_cb,NULL);

	wtmp = XtNameToWidget(wcovers[0],"*coverBackdrop");
	
	wunlock = XmVaCreateManagedFrame(wtmp,
		"unlock",XmNshadowThickness,2,XmNshadowType,XmSHADOW_OUT,
		XmNmappedWhenManaged,False,NULL);

	wrowcol = XmVaCreateManagedRowColumn(wunlock,"rowColumn",
		XmNminWidth,420,XmNminHeight,240,
		XmNorientation,XmVERTICAL,
		XmNentryAlignment,XmALIGNMENT_CENTER,
		XmNmarginHeight,6,XmNmarginWidth,6,NULL);
	
	wtmp = XmCreateLabelGadget(wrowcol,"lockedBy",NULL,0);

	login=getlogin();
	gethostname(host,255);
		
	locked_by=malloc(strlen(login)+strlen(host)+2);
	sprintf(locked_by,"%s@%s",login,host);
	label = XmStringCreateLocalized(locked_by);
	free(locked_by);
	XtVaSetValues(wtmp,XmNlabelString,label,NULL);
	XmStringFree(label);
	XtManageChild(wtmp);
	
	/* Since we're disabling most of the input capabilities for the password
	 * text field, install a more appropriate translation table */
	passwd_input_tt = XtParseTranslationTable(
		"<FocusIn>:focusIn()\n"
		"<FocusOut>:focusOut()\n"
		"<Key>osfActivate:activate()\n"
		"<Key>Return:activate()\n"
		"<Key>osfBackSpace:delete-previous-character()\n"
		"<Key>BackSpace:delete-previous-character()\n"
		"<Key>osfDelete:delete-previous-character()\n"
		"<Key>Delete:delete-previous-character()\n"
		"<Key>:self-insert()\n");
	wpasswd = XmVaCreateManagedTextField(wrowcol,"password",
		XmNtranslations,passwd_input_tt,NULL);
	
	/* Put the unlock box into the center of the screen */
	get_screen_size(&swidth,&sheight,&xoff,&yoff);
	XtVaGetValues(wunlock,XmNwidth,&width,XmNheight,&height,NULL);
	XtVaSetValues(wunlock,XmNx,xoff+(swidth-width)/2,
		XmNy,yoff+(sheight-height)/2,NULL);
		
	/* Allocate password entry buffer to be passed to the modifyVerifyCallback
	 * where it receives entered characters that get replaced with asterisk
	 * in the text field widget */
	pwb_size = sysconf(_SC_GETPW_R_SIZE_MAX);
	if(pwb_size == (-1)) pwb_size = DEF_MAX_PASSWD;
	pwb = malloc(pwb_size+1);
	if(!pwb){
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	pwb[0] = '\0';
	
	XtVaSetValues(wpasswd,XmNmaxLength,(int)pwb_size,NULL);
	XtAddCallback(wpasswd,XmNmodifyVerifyCallback,passwd_modify_cb,pwb);
	XtAddCallback(wpasswd,XmNmotionVerifyCallback,passwd_modify_cb,pwb);
	XtAddCallback(wpasswd,XmNactivateCallback,passwd_enter_cb,pwb);
	
	wmessage = XmCreateLabelGadget(wrowcol,"message",NULL,0);
	XtVaSetValues(wmessage,XmNalignment,XmALIGNMENT_CENTER,
		XmNmappedWhenManaged,False,NULL);
	set_unlock_message(NULL);
	XtManageChild(wmessage);
}

/*
 * Creates a root window property containing the window handle of the
 * session manager shell, which in turn gets a property containing
 * the instance pid. Terminates if another instance is running.
 */
void init_session(void)
{
	Display *dpy = XtDisplay(wshell);
	Window root = DefaultRootWindow(dpy);
	Window shell;
	pid_t pid;
	Atom ret_type;
	int ret_format;
	unsigned long ret_items;
	unsigned long left_items;
	unsigned char *prop_data;
	
	XGetWindowProperty(dpy,root,xa_mgr,0,sizeof(Window),False,XA_WINDOW,
		&ret_type,&ret_format,&ret_items,&left_items,&prop_data);
	
	if(ret_type == XA_WINDOW){
		shell = *((Window*)prop_data);
		XFree(prop_data);

		XSync(dpy,False);
		def_x_err_handler = XSetErrorHandler(local_x_err_handler);

		XGetWindowProperty(dpy,shell,xa_pid,0,sizeof(Window),False,XA_INTEGER,
			&ret_type,&ret_format,&ret_items,&left_items,&prop_data);
		XSync(dpy,False);

		XSetErrorHandler(def_x_err_handler);
			
		if(ret_type == XA_INTEGER){
			pid = *((pid_t*)prop_data);
			log_msg("%s is already running as PID %u\n",bin_name,pid);
			XFree(prop_data);
			exit(EXIT_SUCCESS);
		}
	}
		
	shell = XtWindow(wshell);
	XChangeProperty(dpy,root,xa_mgr,XA_WINDOW,32,
		PropModeReplace,(unsigned char*)&shell,1);
	
	pid = getpid();
	XChangeProperty(dpy,shell,xa_pid,XA_INTEGER,32,
		PropModeReplace,(unsigned char*)&pid,1);
}

/*
 * This is temporarily set in init_session just to catch BadWindow errors
 * originating from a window handle, stored on root in MGR_ATOM_NAME by the
 * previous instance, that is no longer valid.
 */
static int local_x_err_handler(Display *dpy, XErrorEvent *evt)
{
	if(evt->error_code == BadWindow) return 0;
	return def_x_err_handler(dpy,evt);
}


/*
 * Tries to quthenticate using the entered password
 */
static void passwd_enter_cb(Widget w,
	XtPointer client_data, XtPointer call_data)
{
	struct passwd *passwd;
	char *login;
	char *pwb = (char*)client_data;
	char *cpw = NULL;
	char *upw = NULL;
	
	login = getlogin();
	
	set_privileges(True);
	
	passwd = getpwnam(login);
	upw = passwd->pw_passwd;

	#ifdef __linux__
	if(!passwd || passwd->pw_passwd[0] == 'x'){
		struct spwd *spwd;
		spwd = getspnam(login);
		if(spwd) upw = spwd->sp_pwdp;
	}
	#endif /* __linux__ */

	set_privileges(False);
	
	if(!upw || !(cpw = crypt(pwb,upw))){
		log_msg("Failed to retrieve login credentials.\n");
		exit(EXIT_FAILURE);
	}
	
	if(!strcmp(cpw,upw)){
		unlock_screen();
		set_unlock_message(NULL);
	}else{
		XBell(XtDisplay(w),100);
		set_unlock_message(MSG_NOACCESS);
	}

	memset(pwb,0,strlen(pwb));
	XmTextFieldSetString(wpasswd,"");
}

/*
 * Called on password text field modification and cursor movement.
 * Stores entered values in a separate buffer while placing
 * asterisk characters in the text field. Discards any cursor placement.
 */
static void passwd_modify_cb(Widget w,
	XtPointer client_data, XtPointer call_data)
{
	char *pwb = (char*)client_data;
	XmTextVerifyCallbackStruct *cbs =
		(XmTextVerifyCallbackStruct*)call_data;

	if(cbs->reason == XmCR_MODIFYING_TEXT_VALUE){
		if(!cbs->text->length){
			pwb[cbs->startPos] = '\0';
		}else if(cbs->text->length == 1){
			pwb[cbs->currInsert] = *cbs->text->ptr;
			*cbs->text->ptr = '*';
			pwb[cbs->currInsert+1] = '\0';
		}else{
			cbs->doit = False;
		}
	}else if(cbs->reason == XmCR_MOVING_INSERT_CURSOR){
		if(cbs->newInsert != XmTextFieldGetLastPosition(w))
			cbs->doit = False;
	}
}

/*
 * This timeout is set when X screen saver activates
 */
static void lock_timeout_cb(XtPointer ptr, XtIntervalId *id)
{
	lock_screen();
	lock_timer = None;
}

/*
 * Returns size and x/y offsets of the primary xinerama screen,
 * or just the screen size if xinerama isn't active.
 */
static void get_screen_size(Dimension *pwidth, Dimension *pheight,
	Position *px, Position *py)
{
	if(XineramaIsActive(XtDisplay(wshell))){
		int nxis;
		XineramaScreenInfo *xis;
		
		xis=XineramaQueryScreens(XtDisplay(wshell),&nxis);
		
		if(app_res.prim_xinerama_screen >= nxis){
			log_msg("Primary Xinerama screen index %d "
				"is out of range.\n",app_res.prim_xinerama_screen);
			app_res.prim_xinerama_screen = 0;
		}

		*pwidth=xis[app_res.prim_xinerama_screen].width;
		*pheight=xis[app_res.prim_xinerama_screen].height;
		*px=xis[app_res.prim_xinerama_screen].x_org;
		*py=xis[app_res.prim_xinerama_screen].y_org;

		XFree(xis);
		return;
	}else{
		Screen *scr;
		scr=XtScreen(wshell);
		*pwidth=XWidthOfScreen(scr);
		*pheight=XHeightOfScreen(scr);
		*px=0;
		*py=0;
	}
}

/*
 * Drops/restores process privileges. Issues a warning if initial
 * effective uid isn't zero. Any syscall failure is treated as fatal
 * and will terminate the process.
 */
static Boolean set_privileges(Boolean elevate)
{
	static Boolean initialized = False;
	static Boolean can_elevate = False;
	static uid_t orig_uid;
	static gid_t orig_gid;
	int res = 0;
	
	if(!initialized){
		orig_uid = geteuid();
		orig_gid = getegid();

		if(orig_uid != 0){
			log_msg("%s must be setuid root to enable "
				"screen locking capabilities.\n",bin_name);
			initialized = True;
			can_elevate = False;
			return False;
		}
		initialized = True;
		can_elevate = True;
	}
	
	if(!can_elevate) return False;
	
	if(elevate){
		assert(geteuid() != orig_uid);
		
		res = seteuid(orig_uid);
		res |= setegid(orig_gid);
	}else{
		gid_t newgid = getgid();

		assert(geteuid() == orig_uid);
		
		res = setegid(newgid);
		res |= seteuid(getuid());
	}
	
	if(res){
		perror("setgid/uid");
		exit(EXIT_FAILURE);
	}
	return True;
}

/*
 * Register with the XScreenSaver extension for timed locking.
 * Returns True on success.
 */
static void register_screen_saver(void)
{
	Display *dpy = XtDisplay(wshell);
	int i,n;
	
	if(!XScreenSaverQueryExtension(dpy,&xss_event_base,&xss_error_base)){
		log_msg("No XScreenSaver extension available. "
			"Timed locking disabled.\n");
		return;
	}

	n = XScreenCount(XtDisplay(wshell));
	
	for(i = 0; i < n; i++){
		XScreenSaverRegister(dpy,i,XtWindow(wshell),XA_WINDOW);
		XScreenSaverSelectInput(dpy,RootWindow(dpy,i),ScreenSaverNotifyMask);
	}
	
	XSetScreenSaver(dpy,app_res.blank_timeout,0,
		PreferBlanking,DefaultExposures);
}

/*
 * Forks and execvs the specified binary.
 * Returns zero on success, errno otherwise.
 */
static int launch_process(const char *path)
{
	pid_t pid;
	volatile int errval = 0;

	pid=vfork();
	if(pid == 0){
		char *argv[] = {(char*)path,NULL};
		pid_t fpid = getpid();
		
		#ifdef __linux__
		setpgid(fpid,fpid);
		#else
		setpgrp(fpid,fpid);
		#endif
		
		if(execv(path,argv) == (-1))
			errval=errno;
		_exit(0);
	}else if(pid == -1){
		errval=errno;
	}
	return errval;
}

/*
 * Launch a shell to run stuff from ~/.sessionetc, if any.
 */
static void process_sessionetc(void)
{
	char *home;
	char fname[]=".sessionetc";
	char *path;
	char *cmd;
	pid_t pid;
	volatile int errval = 0;
			
	home=getenv("HOME");
	if(!home){
		log_msg("HOME is not set\n");
		return;
	}
	
	path=malloc(strlen(home)+strlen(fname)+2);
	if(!path){
		perror("malloc");
		return;
	}
	sprintf(path,"%s/%s",home,fname);
	if(access(path,R_OK) == (-1)){
		if(errno == EACCES)
			log_msg("No read permission for %s\n",path);
		free(path);
		return;
	}
	pid=vfork();
	if(pid == 0){
		char *argv[] = {"sh", path, NULL};
		pid_t fpid = getpid();
		
		#ifdef __linux__
		setpgid(fpid,fpid);
		#else
		setpgrp(fpid,fpid);
		#endif
				
		if(execvp("sh",argv) == (-1)) errval = errno;
		_exit(0);
	}else if(pid == (-1)){
		errval = errno;
	}
	if(errval){
		log_msg("shell execution failed with: %s\n",strerror(errval));
	}
	free(path);
}

/*
 * Sets the root cursor, background color and, if specified,
 * image on all screens.
 */
static void set_root_background(void)
{
	Cursor cursor;
	XColor bg_color;
	int nscreens, i;
	Display *dpy = XtDisplay(wshell);

	cursor = XCreateFontCursor(dpy,XC_left_ptr);
	bg_color.pixel = app_res.wkspace_bg_pixel;
	XQueryColor(dpy,DefaultColormap(dpy,DefaultScreen(dpy)),&bg_color);
	
	nscreens = XScreenCount(dpy);
	
	for(i=0; i<nscreens; i++){
		XDefineCursor(dpy,RootWindow(dpy,i),cursor);
		XAllocColor(dpy,DefaultColormap(dpy,i),&bg_color);
		XSetWindowBackground(dpy,RootWindow(dpy,i),bg_color.pixel);
		
		if(app_res.wkspace_bg_image){
			Pixel shadow, fnord;
			Pixmap pm;

			XmGetColors(ScreenOfDisplay(dpy,i),DefaultColormap(dpy,i),
				bg_color.pixel,&fnord,&fnord,&shadow,&fnord);
	
			pm = XmGetPixmap(ScreenOfDisplay(dpy,i),
				app_res.wkspace_bg_image,
				shadow,bg_color.pixel);

			if(pm == XmUNSPECIFIED_PIXMAP){
				log_msg("Failed to load \'%s\'\n",app_res.wkspace_bg_image);
				app_res.wkspace_bg_image = NULL;
				continue;
			}
			XSetWindowBackgroundPixmap(dpy,RootWindow(dpy,i),pm);
		}
		XClearWindow(dpy,RootWindow(dpy,i));
	}
	XFlush(dpy);
}

/*
 * Turn NumLock on or off depending on state specified
 */
static void set_numlock_state(void)
{
	Display *dpy = XtDisplay(wshell);
	int xkb_opcode;
	int xkb_event;
	int xkb_error;
	int xkb_major;
	int xkb_minor;
	Boolean on;
	XkbDescPtr desc;
	unsigned int num_lock_mask = 0;
	int i;

	if(!strcasecmp(app_res.numlock_state,"on")){
		on = True;
	}else if(!strcasecmp(app_res.numlock_state,"off")){
		on = False;
	}else if(!strcasecmp(app_res.numlock_state,"keep")){
		return;
	}else{
		log_msg("Illegal numLockState value: %s "
			"Must be either ON, OFF or KEEP.\n",app_res.numlock_state);
		return;
	}

	if(!XkbQueryExtension(dpy,&xkb_opcode,&xkb_event,&xkb_error,
		&xkb_major,&xkb_minor)){
		log_msg("Cannot set NumLock state. XKB extension not available!");
		return;
	}
	
	desc = XkbGetKeyboard(dpy,XkbAllComponentsMask,XkbUseCoreKbd);
	for(i=0; i < XkbNumVirtualMods; i++){
		char *mod_name;
		
		if(desc->names->vmods[i] &&
			(mod_name = XGetAtomName(dpy,desc->names->vmods[i])) &&
			!strcmp(mod_name,"NumLock")){

			if(!XkbVirtualModsToReal(desc,(1<<i),&num_lock_mask) ||
				!XkbLockModifiers(dpy,XkbUseCoreKbd,
					num_lock_mask,on?num_lock_mask:0)){
					log_msg("Cannot determine XKB modifier mapping!\n");
			}
		}
	}
	if(!num_lock_mask) log_msg("Cannot set NumLock state!\n");
	XkbFreeKeyboard(desc,0,True);
}

/*
 * Displays a modal dialog asking the user to confirm leaving the session.
 */
static void xt_sigterm_handler(XtPointer ptr, XtSignalId *id)
{
	static Widget wdlgshell = None;
	static Widget wdialog;
	static Widget wlogout;
	static Widget wshutdown;
	static Widget wreboot;
	static Widget wcancel;
	static Widget wok;
	Widget wresult = None;
	char *command = NULL;
	int rv;

	if(!wdlgshell) {
		Widget wlabel;
		Widget wsep;
		Widget wrc;
		XmString string;
		Dimension swidth, sheight;
		Dimension width, height;
		Position x, y;

		XtCallbackRec button_cb[]={
			{(XtCallbackProc)exit_dialog_cb,(XtPointer)&wresult},
			{(XtCallbackProc)NULL,(XtPointer)NULL}
		};

		wdlgshell = XtVaCreatePopupShell("confirmExitDialog",
			xmDialogShellWidgetClass,wshell,XmNallowShellResize,True,
			XmNmwmFunctions,0,XmNuseAsyncGeometry,True,NULL);
		
		string = XmStringCreateLocalized("Leaving Session");
		wdialog = XmVaCreateManagedForm(wdlgshell,"confirmExit",
			XmNmarginHeight,5,XmNverticalSpacing,2,
			XmNdialogTitle,string,XmNfractionBase,5,
			XmNdialogStyle,XmDIALOG_SYSTEM_MODAL,NULL);
		XmStringFree(string);
		
		string = XmStringCreateLocalized(
			"Choose an action to proceed with:");
		wlabel = XmVaCreateManagedLabelGadget(wdialog,"label",
			XmNlabelString,string,XmNalignment,XmALIGNMENT_BEGINNING,
			XmNtopAttachment,XmATTACH_FORM,XmNtopAttachment,XmATTACH_FORM,
			XmNleftAttachment,XmATTACH_POSITION,XmNleftPosition,1,
			XmNrightAttachment,XmATTACH_POSITION,XmNrightPosition,4,
			NULL);
		XmStringFree(string);
		
	
		wrc = XmVaCreateManagedRowColumn(wdialog,"rowColumn",
			XmNorientation,XmVERTICAL,XmNradioBehavior,True,
			XmNtopAttachment,XmATTACH_WIDGET,XmNtopWidget,wlabel,
			XmNleftAttachment,XmATTACH_POSITION,XmNleftPosition,1,
			XmNrightAttachment,XmATTACH_POSITION,XmNrightPosition,4,
			XmNmarginHeight,1,XmNradioAlwaysOne,True,NULL);
	
		string = XmStringCreateLocalized("Log out");
		wlogout = XmVaCreateManagedToggleButtonGadget(wrc,"logOut",
			XmNlabelString,string,XmNindicatorType,XmONE_OF_MANY,
			XmNset,True,NULL);
		XmStringFree(string);
		
		string = XmStringCreateLocalized("Shut down");
		wshutdown = XmVaCreateToggleButtonGadget(wrc,"shutDown",
			XmNlabelString,string,XmNindicatorType,XmONE_OF_MANY,NULL);
		XmStringFree(string);
		if(app_res.show_shutdown) XtManageChild(wshutdown);
	
		string = XmStringCreateLocalized("Reboot");
		wreboot = XmVaCreateToggleButtonGadget(wrc,"reboot",
			XmNlabelString,string,XmNindicatorType,XmONE_OF_MANY,NULL);
		XmStringFree(string);
		if(app_res.show_reboot) XtManageChild(wreboot);

		wsep = XmVaCreateManagedSeparatorGadget(wdialog,"separator",
			XmNtopAttachment,XmATTACH_WIDGET,XmNtopWidget,wrc,
			XmNleftAttachment,XmATTACH_FORM,
			XmNrightAttachment,XmATTACH_FORM,NULL);
		
		string = XmStringCreateLocalized("OK");
		wok = XmVaCreateManagedPushButtonGadget(wdialog,"ok",
			XmNlabelString,string,XmNactivateCallback,button_cb,
			XmNtopAttachment,XmATTACH_WIDGET,XmNtopWidget,wsep,
			XmNleftAttachment,XmATTACH_POSITION,XmNleftPosition,1,
			XmNrightAttachment,XmATTACH_POSITION,XmNrightPosition,2,
			XmNbottomAttachment,XmATTACH_FORM,NULL);
		XmStringFree(string);

		string = XmStringCreateLocalized("Cancel");
		wcancel = XmVaCreateManagedPushButtonGadget(wdialog,"cancel",
			XmNlabelString,string,XmNactivateCallback,button_cb,
			XmNtopAttachment,XmATTACH_WIDGET,XmNtopWidget,wsep,
			XmNleftAttachment,XmATTACH_POSITION,XmNleftPosition,3,
			XmNrightAttachment,XmATTACH_POSITION,XmNrightPosition,4,
			XmNbottomAttachment,XmATTACH_FORM,NULL);
		XmStringFree(string);
		
		XtVaSetValues(wdialog,XmNinitialFocus,wcancel,
			XmNdefaultButton,wcancel,NULL);
		
		XtRealizeWidget(wdlgshell);
		
		get_screen_size(&swidth,&sheight,&x,&y);
		XtVaGetValues(wdlgshell,XmNwidth,&width,XmNheight,&height,NULL);
		XtMoveWidget(wdlgshell,x+(swidth-width)/2,y+(sheight-height)/2);
	}
	
	XtManageChild(wdialog);

	while(wresult == None)
		XtAppProcessEvent(app_context,XtIMXEvent);
	
	if(wresult == wcancel) return;
	
	if(XmToggleButtonGadgetGetState(wshutdown)){
		command = SHUTDOWN_CMD;
	}else if(XmToggleButtonGadgetGetState(wreboot)){
		command = REBOOT_CMD;
	}

	if(command){
		#ifndef UNPRIVILEGED_SHUTDOWN
		if(set_privileges(True)){
			rv = launch_process(command);
			set_privileges(False);
		}else{
			log_msg("Cannot exec %s with elevated privileges.\n",command);
		}
		#else
		rv = launch_process(command);
		#endif /* UNPRIVILEGED_SHUTDOWN */

		if(rv){
			log_msg("Cannot exec %s: %s\n",command,strerror(rv));
			return;
		}
	}

	XDeleteProperty(XtDisplay(wshell),
		DefaultRootWindow(XtDisplay(wshell)),xa_mgr);

	exit(0);
}

/* Button press handler for xt_sigterm_handler confirmation dialog */
static void exit_dialog_cb(Widget w,
	XtPointer client_data, XtPointer call_data)
{
	*((Widget*)client_data) = w;
}

static void xt_sigusr1_handler(XtPointer ptr, XtSignalId *id)
{
	if(app_res.enable_locking){
		lock_screen();
		if(app_res.blank_on_lock)
			XtAppAddTimeOut(app_context,1000,blank_delay_timeout_cb,NULL);
	}
}

static void blank_delay_timeout_cb(XtPointer ptr, XtIntervalId *iid)
{
	XForceScreenSaver(XtDisplay(wshell),ScreenSaverActive);
}

/*
 * Low level signal handlers.
 * These just turn async signals into Xt callbacks.
 */
static void sigterm_handler(int sig)
{
	XtNoticeSignal(xt_sigterm);
}

static void sigusr_handler(int sig)
{
	if(sig == SIGUSR1) XtNoticeSignal(xt_sigusr);
}

static void sigchld_handler(int sig)
{
	int status;
	pid_t pid;
	if(sig == SIGCHLD) pid=wait(&status);
}
