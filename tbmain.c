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
#include <Xm/MessageB.h>
#include <Xm/MwmUtil.h>
#include <X11/cursorfont.h>
#include <errno.h>
#include "tbparse.h"
#include "smglobal.h"

/* Forward declarations */
static char* find_rc_file(void);
static Boolean construct_menu(void);
static void create_utility_widgets(Widget);
static void setup_hotkeys(void);
static int xgrabkey_err_handler(Display*,XErrorEvent*);
static void handle_root_event(XEvent*);
void raise_and_focus(Widget w);
static void make_button_pixmaps(Widget,const unsigned char*,
	unsigned int,unsigned int,Pixmap*,Pixmap*,Pixmap*);
static void time_update_cb(XtPointer,XtIntervalId*);
static void rcfile_check_cb(XtPointer,XtIntervalId*);
static int exec_command(const char*);
static void report_exec_error(const char*,int);
static void report_rcfile_error(const char*,const char*);
static char* get_user_input(Widget,const char*);
static Boolean message_dialog(Boolean,const char*);
static void wait_state(Boolean);
static void hotkey_cb(Widget,XtPointer,XEvent*,Boolean*);
static void exec_cb(Widget,XtPointer,XtPointer);
static void menu_command_cb(Widget,XtPointer,XtPointer);
static void user_input_cb(Widget,XtPointer,XtPointer);
static void message_dialog_cb(Widget,XtPointer,XtPointer);
static void sig_handler(int);
static void xt_sigusr1_handler(XtPointer,XtSignalId*);
#ifndef NO_SESSIONMGR
static void logout_cb(Widget,XtPointer,XtPointer);
static void lock_cb(Widget,XtPointer,XtPointer);
static Boolean get_xmsm_pid(pid_t*);
static int local_x_err_handler(Display*,XErrorEvent*);
#endif /* NO_SESSIONMGR */


struct tb_resources {
	char *title;
	Boolean show_date_time;
	char *date_time_fmt;
	char *rc_file;
	char *hotkey;
	unsigned int rcfile_check_time;
	Boolean show_commands;
} app_res;

#define RES_FIELD(f) XtOffset(struct tb_resources*,f)
XtResource xrdb_resources[]={
	{ "title","Title",XmRString,sizeof(String),
		RES_FIELD(title),XmRImmediate,(XtPointer)NULL
	},
	{ "dateTimeDisplay","DateTimeDisplay",XmRBoolean,sizeof(Boolean),
		RES_FIELD(show_date_time),XmRImmediate,(XtPointer)True
	},
	{ "dateTimeFormat","DateTimeFormat",XmRString,sizeof(String),
		RES_FIELD(date_time_fmt),XmRImmediate,(XtPointer)"%D %l:%M %p"
	},
	{ "rcFile","RcFile",XmRString,sizeof(String),
		RES_FIELD(rc_file),XmRImmediate,(XtPointer)NULL
	},
	{ "hotkey","Hotkey",XmRString,sizeof(String),
		RES_FIELD(hotkey),XmRImmediate,(XtPointer)"Control Menu"
	},
};
#undef RES_FIELD

static XrmOptionDescRec xrdb_options[]={
	{"-title","title",XrmoptionSepArg,(caddr_t)NULL},
	{"-rcfile","rcFile",XrmoptionSepArg,(caddr_t)NULL},
	{"-hotkey","hotkey",XrmoptionSepArg,(caddr_t)NULL}
};

String fallback_res[]={
	"XmToolbox.x:32",
	"XmToolbox.y:32",
	NULL
};

#define APP_TITLE "Toolbox"
#define APP_NAME "xmtoolbox"
#define RC_NAME	"toolboxrc"

#define RC_UPDATE_REQ_TIME	1000

#ifndef NO_SESSIONMGR
Atom xa_xmsm_mgr=None;
Atom xa_xmsm_pid=None;
int (*def_x_err_handler)(Display*,XErrorEvent*)=NULL;
#endif /* NO_SESSIONMGR */

XtAppContext app_context;
Widget wshell;
Widget wmain=None;
Widget wdate_time=None;
Widget wmenu=None;
String rc_file_path=NULL;
XtSignalId xt_sigusr1;
KeyCode hotkey_code=0;
unsigned int hotkey_mods=0;

int main(int argc, char **argv)
{
	Window root_window;
	Widget wframe;
	char *home;
	
	signal(SIGUSR1,sig_handler);
	signal(SIGCHLD,sig_handler);

	if((home=getenv("HOME"))){
		chdir(home);
	}
	
	XtSetLanguageProc(NULL,NULL,NULL);
	XtToolkitInitialize();
	
	wshell=XtVaAppInitialize(&app_context,"XmToolbox",
		xrdb_options,XtNumber(xrdb_options),&argc,argv,fallback_res,
		XmNiconName,APP_TITLE,XmNallowShellResize,True,
		XmNmwmDecorations,MWM_DECOR_TITLE|MWM_DECOR_MINIMIZE|MWM_DECOR_MENU,
		XmNmwmFunctions,MWM_FUNC_MOVE|MWM_FUNC_MINIMIZE,NULL);
	
	XtGetApplicationResources(wshell,&app_res,xrdb_resources,
		XtNumber(xrdb_resources),NULL,0);

	if(!app_res.title){
		char *title;
		char *login;
		char host[256]="localhost";

		login=getlogin();
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

	wframe=XmVaCreateManagedFrame(wshell,"mainFrame",
		XmNshadowThickness,2,XmNshadowType,XmSHADOW_OUT,NULL);
	
	wmain=XmVaCreateManagedRowColumn(wframe,"main",
		XmNmarginWidth,0,XmNmarginHeight,0,XmNspacing,0,
		XmNorientation,XmVERTICAL,NULL);

	rc_file_path=(app_res.rc_file)?app_res.rc_file:find_rc_file();
	
	if(rc_file_path){
		if(access(rc_file_path,F_OK|R_OK) == -1){
			perror(rc_file_path);
			return EXIT_FAILURE;
		}
		if(!construct_menu()) return EXIT_FAILURE;
	}else{
		fprintf(stderr,"%s not found, nor specified.\n",RC_NAME);
		return EXIT_FAILURE;
	}

	create_utility_widgets(wmain);
	
	xt_sigusr1 = XtAppAddSignal(app_context,xt_sigusr1_handler,NULL);

	#ifndef NO_SESSIONMGR
	xa_xmsm_mgr = XInternAtom(XtDisplay(wshell),XMSM_ATOM_NAME,True);
	xa_xmsm_pid = XInternAtom(XtDisplay(wshell),XMSM_PID_ATOM_NAME,True);
	#endif

	XtRealizeWidget(wshell);
	setup_hotkeys();

	root_window = XDefaultRootWindow(XtDisplay(wshell));
			
	for(;;) {
		XEvent evt;
		XtAppNextEvent(app_context,&evt);
		if(evt.xany.window==root_window)
			handle_root_event(&evt);
		else
			XtDispatchEvent(&evt);
	}
	
	return 0;
}

/*
 * Parse the specified hotkey combination, grab the key and attach a callback.
 */
static void setup_hotkeys(void)
{
	Window root_window;
	char *buf;
	char *token;
	KeySym key_sym=NoSymbol;
	static int (*def_x_err_handler)(Display*,XErrorEvent*)=NULL;
	
	if(!app_res.hotkey || !strcasecmp(app_res.hotkey,"none")) return;

	hotkey_code=0;
	hotkey_mods=0;

	buf=strdup(app_res.hotkey);	
	token=strtok(buf," \t+");
	if(token){
		while(token){
			if(!strcasecmp(token,"alt")){
				hotkey_mods|=Mod1Mask;
			}else if(!strcasecmp(token,"ctrl") ||
				!strcasecmp(token,"control")){
				hotkey_mods|=ControlMask;
			}else if(!strcasecmp(token,"shift")){
				hotkey_mods|=ShiftMask;
			}else{
				key_sym=XStringToKeysym(token);
				break;
			}
			token=strtok(NULL," \t+");
		}
	}else{
		key_sym=XStringToKeysym(buf);
	}
	free(buf);

	if(key_sym == NoSymbol){
		fputs("Invalid hotkey specification\n",stderr);
		return;
	}
	hotkey_code = XKeysymToKeycode(XtDisplay(wshell),key_sym);
	
	root_window = XDefaultRootWindow(XtDisplay(wshell));
	
	XSync(XtDisplay(wshell),False);
	def_x_err_handler = XSetErrorHandler(xgrabkey_err_handler);
	
	/* We need to grab all possible combinations of specified modifiers +
	 * any lock modifiers that may be active. I'm not aware of any better
	 * way to get this working */
	XGrabKey(XtDisplay(wshell),hotkey_code,hotkey_mods,
		root_window,False,GrabModeAsync,GrabModeAsync);
	XGrabKey(XtDisplay(wshell),hotkey_code,hotkey_mods|Mod2Mask,
		root_window,False,GrabModeAsync,GrabModeAsync);
	XGrabKey(XtDisplay(wshell),hotkey_code,hotkey_mods|LockMask,
		root_window,False,GrabModeAsync,GrabModeAsync);
	XGrabKey(XtDisplay(wshell),hotkey_code,hotkey_mods|LockMask|Mod2Mask,
		root_window,False,GrabModeAsync,GrabModeAsync);	
	
	XSync(XtDisplay(wshell),False);
	XSetErrorHandler(def_x_err_handler);
		
	XSelectInput(XtDisplay(wshell),root_window,KeyPressMask);
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
	XKeyEvent *e = (XKeyEvent*)evt;
	
	if(e->keycode == hotkey_code &&
		((e->state & hotkey_mods) || !hotkey_mods)){
		raise_and_focus(wshell);
	}
}

/* 
 * Raise and focus the given shell widget.
 */
void raise_and_focus(Widget w)
{
	static Atom XaWM_STATE=None;
	static Atom XaWM_CHANGE_STATE=None;
	Atom ret_type;
	int ret_fmt;
	unsigned long ret_items;
	unsigned long ret_bytes;
	uint32_t *state=NULL;
	
	if(XaWM_STATE==None){
		XaWM_STATE=XInternAtom(XtDisplay(w),"WM_STATE",False);
		XaWM_CHANGE_STATE=XInternAtom(XtDisplay(w),"WM_CHANGE_STATE",False);
	}
	if(XGetWindowProperty(XtDisplay(w),XtWindow(w),XaWM_STATE,0,1,
		False,XaWM_STATE,&ret_type,&ret_fmt,&ret_items,
		&ret_bytes,(unsigned char**)&state)!=Success) return;
	if(ret_type==XaWM_STATE && ret_fmt && *state==IconicState){
		XClientMessageEvent evt;
		evt.type=ClientMessage;
		evt.message_type=XaWM_CHANGE_STATE;
		evt.window=XtWindow(w);
		evt.format=32;
		evt.data.l[0]=NormalState;
		XSendEvent(XtDisplay(w),XDefaultRootWindow(XtDisplay(w)),True,
			SubstructureNotifyMask|SubstructureRedirectMask,
			(XEvent*)&evt);
	}else{
		XRaiseWindow(XtDisplay(w),XtWindow(w));
		XSync(XtDisplay(w),False);
		XSetInputFocus(XtDisplay(w),XtWindow(w),RevertToParent,CurrentTime);
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
		XmNshadowThickness,0,
		XmNspacing,1,
		XmNmarginWidth,0,
		XmNorientation,XmVERTICAL,
		XmNrowColumnType,XmMENU_BAR,
		XmNpacking,XmPACK_COLUMN,
		XmNpositionIndex,0,
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
	Widget wmenu;
	Widget wpulldown;
	Widget wcascade;
	Widget w;
	XmString title;
	Arg args[10];
	int n;
	
	#ifndef NO_SESSIONMGR
	w=XmCreateSeparatorGadget(wparent,"separator",NULL,0);
	XtManageChild(w);
	
	/* 'Session' menu */
	wmenu=XmVaCreateManagedRowColumn(wparent,"menu",
		XmNshadowThickness,0,
		XmNspacing,1,
		XmNmarginWidth,0,
		XmNorientation,XmVERTICAL,
		XmNrowColumnType,XmMENU_BAR,NULL);
	
	wpulldown=XmCreatePulldownMenu(wmenu,"sessionPulldown",NULL,0);
			
	title=XmStringCreateLocalized("Session");
	n=0;
	XtSetArg(args[n],XmNlabelString,title); n++;
	XtSetArg(args[n],XmNmnemonic,(KeySym)'S'); n++;
	XtSetArg(args[n],XmNsubMenuId,wpulldown); n++;
	wcascade=XmCreateCascadeButtonGadget(wmenu,"cascadeButton",args,n);
	XmStringFree(title);
	XtManageChild(wcascade);
		
	n=0;
	title=XmStringCreateLocalized("Execute...");
	XtSetArg(args[n],XmNlabelString,title); n++;
	XtSetArg(args[n],XmNmnemonic,(KeySym)'E'); n++;
	w=XmCreatePushButtonGadget(wpulldown,"execMenuButton",args,n);
	XtAddCallback(w,XmNactivateCallback,exec_cb,NULL);
	XmStringFree(title);
	XtManageChild(w);

	w=XmCreateSeparatorGadget(wpulldown,"separator",NULL,0);
	XtManageChild(w);

	n=0;
	title=XmStringCreateLocalized("Lock");
	XtSetArg(args[n],XmNlabelString,title); n++;
	XtSetArg(args[n],XmNmnemonic,(KeySym)'L'); n++;
	w=XmCreatePushButtonGadget(wpulldown,"lockMenuButton",args,n);
	XtAddCallback(w,XmNactivateCallback,lock_cb,NULL);
	XmStringFree(title);
	XtManageChild(w);


	n=0;
	title=XmStringCreateLocalized("Logout...");
	XtSetArg(args[n],XmNlabelString,title); n++;
	XtSetArg(args[n],XmNmnemonic,(KeySym)'o'); n++;
	w=XmCreatePushButtonGadget(wpulldown,"logoutMenuButton",args,n);
	XtAddCallback(w,XmNactivateCallback,logout_cb,NULL);
	XmStringFree(title);
	XtManageChild(w);
	#endif /* NO_SESSIONMGR */
	
	/* The time-date display */
	w=XmCreateSeparatorGadget(wparent,"separator",NULL,0);
	XtManageChild(w);

	n=0;
	XtSetArg(args[n],XmNalignment,XmALIGNMENT_CENTER); n++;
	XtSetArg(args[n],XmNmarginWidth,6); n++;
	XtSetArg(args[n],XmNmarginHeight,6); n++;
	wdate_time=XmCreateLabelGadget(wparent,"dateTime",args,n);
	if(app_res.show_date_time){
		XtManageChild(wdate_time);
		time_update_cb(NULL,NULL);
	}
	
}

/*
 * Search home and system directories for the RC file.
 * Returns a malloc()ed full path to the RC file on success,
 * or NULL otherwise.
 */
static char* find_rc_file(void)
{
	char *home=NULL;
	char *lang=NULL;
	char *path;
	int i;
	char *sys_paths[32]={
		"/etc/X11/",
		"/usr/share/X11/",
		"/usr/local/etc/X11/",
		"/usr/lib/X11/",
		"/usr/local/lib/X11/",
		NULL
	};
	
	home=getenv("HOME");
	lang=getenv("LANG");
	
	if(!home){
		fprintf(stderr,"HOME is not set!\n");
		return NULL;
	}

	path=malloc(strlen(home)+
		strlen(RC_NAME)+((lang)?strlen(lang):0)+36);
	if(!path){
		perror("malloc");
		return NULL;
	}

	sprintf(path,"%s/.%s",home,RC_NAME);
	if(!access(path,R_OK)) return path;

	for(i=0; sys_paths[i]!=NULL; i++){
		if(lang){
			sprintf(path,"%s/%s/%s",sys_paths[i],lang,RC_NAME);
			if(!access(path,R_OK)) return path;
		}
		sprintf(path,"%s/%s",sys_paths[i],RC_NAME);
		if(!access(path,R_OK)) return path;
	}

	free(path);
	return NULL;
}

/*
 * Disable/Enable widget sensitivity and set pointer according
 * to the given wait state.
 */
static void wait_state(Boolean enter_wait)
{
	static Cursor watch_cursor=None;
	Display *dpy=XtDisplay(wshell);
	
	if(watch_cursor==None)
		watch_cursor=XCreateFontCursor(dpy,XC_watch);

	if(enter_wait){
		XtSetSensitive(wmain,False);
		XDefineCursor(dpy,XtWindow(wmain),watch_cursor);
	}else{
		XtSetSensitive(wmain,True);
		XUndefineCursor(dpy,XtWindow(wmain));
	}
    while(XtAppPending(app_context) & XtIMXEvent) {
		XtAppProcessEvent(app_context,XtIMXEvent);
		XFlush(dpy);
	}
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
	XtVaSetValues(wdate_time,XmNlabelString,xm_str,NULL);
	XmStringFree(xm_str);
	
	XtAppAddTimeOut(app_context,
		60000-(the_time->tm_sec*1000),time_update_cb,NULL);
}

/*
 * SIGUSR1 RC file reload request handler
 */
static void xt_sigusr1_handler(XtPointer client_data, XtSignalId *id)
{
	wait_state(True);
	/* on parse error, the previous configuration remains active
	 * and the user is informed about */
	construct_menu();
	wait_state(False);

}

/*
 * Fetches a string from an input dialog.
 * Returns heap allocated string, or NULL if user cancels the dialog.
 */
static char* get_user_input(Widget wshell, const char *prompt_str)
{
	static Widget w=None;
	XmString xm_prompt_str;
	Arg args[5];
	int n=0;
	char *result_str=NULL;

	if(w == None){
		XmString xm_title;
		XtCallbackRec callback[]={
			{(XtCallbackProc)user_input_cb,(XtPointer)&result_str},
			{(XtCallbackProc)NULL,(XtPointer)NULL}
		};

		n=0;
		xm_title=XmStringCreateLocalized(APP_TITLE);
		XtSetArg(args[n],XmNdialogTitle,xm_title); n++;
		XtSetArg(args[n],XmNokCallback,callback); n++;
		XtSetArg(args[n],XmNcancelCallback,callback); n++;
		w=XmCreatePromptDialog(wshell,"promptDialog",args,n);
		XmStringFree(xm_title);
		XtUnmanageChild(XmSelectionBoxGetChild(w,XmDIALOG_HELP_BUTTON));
	}
	xm_prompt_str=XmStringCreateLocalized((char*)prompt_str);
	
	n=0;	
	XtSetArg(args[n],XmNselectionLabelString,xm_prompt_str); n++;
	XtSetValues(w,args,n);
	XmStringFree(xm_prompt_str);
	XtManageChild(w);
	
	while(!result_str){
		XtAppProcessEvent(app_context,XtIMAll);
	}

	return (result_str[0]=='\0')?NULL:result_str;
}

/*
 * get_user_input dialog callback
 */
static void user_input_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	XmSelectionBoxCallbackStruct *cbs=
		(XmSelectionBoxCallbackStruct*)call_data;
	char **result=(char**)client_data;

	if(cbs->reason==XmCR_CANCEL)
		*result="\0";
	else
		*result=(char*)XmStringUnparse(cbs->value,NULL,0,
			XmCHARSET_TEXT,NULL,0,XmOUTPUT_ALL);
}

/*
 * Displays a message dialog. If 'confirm' is True, the dialog will
 * have OK+Cancel buttons, OK only otherwise. Returns True if OK was choosen.
 */
static Boolean message_dialog(Boolean confirm, const char *message_str)
{
	static Widget w=None;
	XmString xm_message_str;
	Arg args[5];
	int n=0;
	int result=(-1);

	if(w == None){
		XmString xm_title;
		XtCallbackRec callback[]={
			{(XtCallbackProc)message_dialog_cb,(XtPointer)&result},
			{(XtCallbackProc)NULL,(XtPointer)NULL}
		};

		n=0;
		xm_title=XmStringCreateLocalized(APP_TITLE);
		XtSetArg(args[n],XmNdialogTitle,xm_title); n++;
		XtSetArg(args[n],XmNokCallback,callback); n++;
		XtSetArg(args[n],XmNcancelCallback,callback); n++;

		w=XmCreateMessageDialog(wshell,"messageDialog",args,n);
		XmStringFree(xm_title);
		if(!confirm) XtUnmanageChild(
			XmMessageBoxGetChild(w,XmDIALOG_CANCEL_BUTTON));
		XtUnmanageChild(XmMessageBoxGetChild(w,XmDIALOG_HELP_BUTTON));
	}
	xm_message_str=XmStringCreateLocalized((char*)message_str);
	
	n=0;	
	XtSetArg(args[n],XmNdialogType,
		confirm?XmDIALOG_QUESTION:XmDIALOG_INFORMATION); n++;
	XtSetArg(args[n],XmNdefaultButtonType,
		confirm?XmDIALOG_CANCEL_BUTTON:XmDIALOG_OK_BUTTON); n++;
	XtSetArg(args[n],XmNmessageString,xm_message_str); n++;
	XtSetValues(w,args,n);
	XmStringFree(xm_message_str);
	XtManageChild(w);
	
	while(result == (-1)){
		XtAppProcessEvent(app_context,XtIMAll);
	}

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
	
	
	str=strdup(cmd_spec);
	if(!str) return errno;
	
	p=str;
	t=NULL;
	
	/* split the command string into separate arguments */
	while(!done){
		if(!t){
			while(isblank(*p)) p++;
			t=p;
		}
		
		if(*p == '\"' || *p == '\''){
			if(pc == '\\'){
				/* literal " or ' */
				memmove(p-1,p,strlen(p)+1);
			}else{
				/* parenthese, remove it ignoring blanks within */
				memmove(p,p+1,strlen(p)+1);
				while(*p != '\"' && *p != '\''){
					if(*p == '\0'){
						if(argv) free(argv);
						return EINVAL;
					}
					p++;
				}
				memmove(p,p+1,strlen(p)+1);
			}
		}
		if(isblank(*p) || *p == '\0'){
			if(*p == '\0') done = 1;
			if(argv_size < argc+1){
				char **new_ptr;
				new_ptr=realloc(argv,argv_size+=64);
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
		pid_t fpid = getpid();

		#ifdef __linux__
		setpgid(fpid,fpid);
		#else
		setpgrp(fpid,fpid);
		#endif
		
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
 * Display a message dialog comtaining the failed command name and
 * the system error string.
 */
static void report_exec_error(const char *command, int errno_value)
{
	char *err_msg="Error while executing command";
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
	int err;
	
	if((err=exec_command(client_data)))
		report_exec_error(client_data,err);
	
}

#ifndef NO_SESSIONMGR
static Boolean get_xmsm_pid(pid_t *pid)
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
	
	XGetWindowProperty(dpy,root,xa_xmsm_mgr,0,sizeof(Window),False,XA_WINDOW,
		&ret_type,&ret_format,&ret_items,&left_items,&prop_data);
	
	if(ret_type == XA_WINDOW){
		shell = *((Window*)prop_data);
		XFree(prop_data);

		def_x_err_handler = XSetErrorHandler(local_x_err_handler);
		XGetWindowProperty(dpy,shell,xa_xmsm_pid,0,sizeof(Window),
			False,XA_INTEGER,&ret_type,&ret_format,
			&ret_items,&left_items,&prop_data);
		XSetErrorHandler(def_x_err_handler);
			
		if(ret_type == XA_INTEGER){
			*pid = *((pid_t*)prop_data);
			XFree(prop_data);
			return True;
		}
	}
	return False;
}

/*
 * This is temporarily set in get_xmsm_pid just to catch BadWindow errors
 * originating from a window handle stored on root in MGR_ATOM_NAME by
 * xmsm process that is no longer active
 */
static int local_x_err_handler(Display *dpy, XErrorEvent *evt)
{
	if(evt->error_code == BadWindow) return 0;
	return def_x_err_handler(dpy,evt);
}

static void exec_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	char *command;
	int err;
	command=get_user_input(wshell,"Command to execute");
	if(!command) return;
	
	if((err=exec_command(command)))
		report_exec_error(command,err);
		
	if(command) free(command);
}

static void lock_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	pid_t pid;
	if(get_xmsm_pid(&pid)){
		kill(pid,SIGUSR1);
	}else{
		message_dialog(False,"Cannot retrieve session manager PID.");
	}
}

static void logout_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	pid_t pid;
	if(get_xmsm_pid(&pid)){
		kill(pid,SIGTERM);
	}else{
		message_dialog(False,"Cannot retrieve session manager PID.");
	}
}
#endif /* NO_SESSIONMGR */

static void sig_handler(int sig)
{
	if(sig == SIGCHLD){
		int status;
		pid_t pid;
		pid=wait(&status);
	}else if(sig == SIGUSR1){
		XtNoticeSignal(xt_sigusr1);
	}
}

