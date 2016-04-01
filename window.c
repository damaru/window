#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

typedef struct ReasonDesc ReasonDesc;
struct ReasonDesc {
	char	*name;
	int	nargs;
	int	opargs;
	void	(*fn)(char**);
	char	*usage;
};

Display *dpy;
char	*argv0;
static void getWindowProperty(Window window, char * name, Bool delete);
static void listProps(Window window, char * name, char **values);

static Window
wind(char *p) {
	long	l;
	char	*endp;
	
	if (!strcmp(p, "root")) {
		return DefaultRootWindow(dpy);
	}
	
	l = strtol(p, &endp, 0);
	if (*p != '\0' && *endp == '\0') {
		return (Window) l;
	}
	fprintf(stderr, "%s: %s is not a valid window id\n", argv0, p);
	exit(EXIT_FAILURE);
}

static int
listwindows(Window w, Window **ws) {
	unsigned	nkids;
	Window	dumb;

	XQueryTree(dpy, w, &dumb, &dumb, ws, &nkids);
	return nkids;
}

void
XClientMessage( Window win, long type, long l0, long l1, long l2)
{
	XClientMessageEvent xev;

	Window root = DefaultRootWindow(dpy);
	xev.type = ClientMessage;
	xev.window = win;
	xev.message_type = type;
	xev.format = 32;
	xev.data.l[0] = l0;
	xev.data.l[1] = l1;
	xev.data.l[2] = l2;
	xev.data.l[3] = 0;
	xev.data.l[4] = 0;
	XSendEvent(dpy, root, False,
			(SubstructureNotifyMask | SubstructureRedirectMask),
			(XEvent *) & xev);
}


static void	
SetView(char **argv) {
	Window w = wind(argv[0]);
	long desk = atoi(argv[1]);
	Atom  prop  = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
	XClientMessage(w, prop, desk, 0,  0);
}

static void
GetView(char ** argv) {
	getWindowProperty(wind(argv[0]), "_NET_WM_DESKTOP", False);
}

static void	
CurrentView(char **argv) {
	int desk = atoi(argv[0]);
	Window root = DefaultRootWindow(dpy);
	Atom prop, numdesks;
	Atom realType;
	unsigned long n = 0;
	unsigned long extra;
	int format;
	int status;
	char * value;
	prop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	numdesks = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
			
	if(desk < 0) {	
		status = XGetWindowProperty(dpy, root, prop, 0L, 512L, False, AnyPropertyType,
			&realType, &format, &n, &extra, (unsigned char **) &value);
		printf("%d / ",*(int*)value);
		status = XGetWindowProperty(dpy, root, numdesks, 0L, 512L, False, AnyPropertyType,
			&realType, &format, &n, &extra, (unsigned char **) &value);
		printf("%d \n",*(int*)value);

	} else {
		XClientMessage(root, prop, desk, 0, 0);
	}
}


static void	
RaiseWindow(char **argv) {
	Window w = wind(*argv);
	
	XClientMessage(w, XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False), 2, CurrentTime, 0);

	XRaiseWindow(dpy, w);
	XSetInputFocus (dpy, w, RevertToPointerRoot, CurrentTime);
}

static void	
MapRaiseWindow(char **argv) {
	Window w = wind(*argv);
	XMapRaised(dpy, w);
}


static void	
LowerWindow(char **argv) {
	XLowerWindow(dpy, wind(*argv));
}

static void	
IconifyWindow(char **argv) {
	XIconifyWindow(dpy, wind(*argv), DefaultScreen(dpy));
}

static Bool
isprotodel(Window w) {
	int i, n;
	Atom *protocols;
	Bool ret = False;
	Atom wmd = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	if(XGetWMProtocols(dpy, w, &protocols, &n)) {
		for(i = 0; !ret && i < n; i++)
			if(protocols[i] == wmd)
				ret = True;
		XFree(protocols);
	}
	return ret;
}


static void	
KillWindow(char **argv) {

	XEvent ev;
	Window w = wind(*argv);
	

	if(isprotodel(w)) {
		ev.type = ClientMessage;
		ev.xclient.window = w;
		ev.xclient.message_type =  XInternAtom(dpy, "WM_PROTOCOLS", False);
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, w, False, NoEventMask, &ev);
	}
	else 
	{
		XKillClient(dpy, w);
	}

}

static void	
HideWindow(char **argv) {
	Window	w = wind(*argv);
	Atom	wm_change_state;
	wm_change_state = XInternAtom(dpy, "WM_CHANGE_STATE", False);
	XClientMessage(w, wm_change_state,IconicState, CurrentTime, 0);
}

static void	
UnhideWindow(char **argv) {
	Window	w = wind(*argv);
	Atom	wm_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);

	XClientMessage(w, wm_active, 2, CurrentTime, 0);
}

static void
LabelWindow(char **argv) {
	Window	w;

	w = wind(argv[0]);
	XStoreName(dpy, w, argv[1]);
	XSetIconName(dpy, w, argv[1]);
}

/*
 *	This can only give us an approximate geometry since
 *	we can't find out what decoration the window has.
 */
static void
WhereWindow(char **argv) {
	Window root;
	int x, y;
	unsigned w, h, border, depth;

	XGetGeometry(dpy, wind(*argv), &root, &x, &y, &w, &h, &border, &depth);
	printf("%u %u %u %u\n", w, h, x, y);
}

static void
MoveWindow(char **argv) {
       XMoveWindow(dpy, wind(argv[0]), atoi(argv[1]), atoi(argv[2]));
}

static void
ResizeWindow(char **argv) {
       XResizeWindow(dpy, wind(argv[0]), atoi(argv[1]), atoi(argv[2]));
}

static void
Max(char **argv) {
	Window win;
	int x, y;
	unsigned w, h, border, depth;

	XGetGeometry(dpy, DefaultRootWindow(dpy), &win, &x, &y, &w, &h, &border, &depth);
	win = wind(argv[0]);
	XMoveWindow(dpy, win, 0, 0);
	XResizeWindow(dpy, win, w, h);
}

#if 1
static void	
printwindowname(Window w) {
	unsigned char	*name;
	Atom     actual_type;
	int	format;
	unsigned long	n;
	unsigned long	extra;

	/*
         *      This rather unpleasant hack is necessary because xwsh uses
         *      COMPOUND_TEXT rather than STRING for its WM_NAME property,
         *      and anonymous xwsh windows are annoying.
         */
	if(Success == XGetWindowProperty(dpy, w, XA_WM_NAME, 0L, 100L, False, AnyPropertyType,
			&actual_type, &format, &n, &extra, &name) && name != 0)
		printf("%#x\t%s\n", (unsigned)w, (char*)name);
//	else
	//	printf("%#x\t%s\n", (unsigned)w, "(none)");

	if(name)
		XFree(name);
}

static void	
ListWindows(char **argv) {
	char *class = argv[0];
	Window	*wins;
	Window	dumb;
	Atom prop;
	int	i;
	unsigned	nkids;
	int j;

	nkids = listwindows(DefaultRootWindow(dpy), &wins);

	if(class) {
//		printf("class: %s\n",class);
		prop = XInternAtom(dpy, "WM_CLASS", True);
	}


	for(i = nkids-1; i >= 0; i--) {
		XWindowAttributes attr;
		Window	*kids2;
		unsigned	nkids2;
		int	i2;
		char *classes[122];

		if( XGetWindowAttributes(dpy, wins[i], &attr)) {
			if(attr.override_redirect == 0 
				&& attr.map_state != IsUnmapped 
				&& !XGetTransientForHint(dpy, wins[i], &dumb)){

				if(class) {
#if 0
					Atom     actual_type;
					int	format;
					unsigned long	n;
					unsigned long	extra;
					char *value;
					XGetWindowProperty(dpy, wins[i], prop, 0L, 512L, False, AnyPropertyType,
							&actual_type, &format, &n, &extra, (unsigned char **) &value);

					classes[0] = value;
					for(j = 0, i2 = 0; i2 < n; i2++)
					{
						if(value[i2] == 0){
							classes[j+1] = &value[i2+1];
							j++;
						}
					}
					classes[j] = NULL;

					for(i2 = 0; i2 < j && classes[i2]; i2++) {
						if(strstr(classes[i2], class) ){
							printwindowname(wins[i]);
							break;
						}
					}
					XFree(value);
#else
					listProps(wins[i], "WM_CLASS", classes);
					for(j = 0; classes[j]; j++) {
						if(strstr(classes[j], class) ){
						//printf("classb[%X] = \'%s\' %p\n",wins[i],classes[j], classes[j]);
							printwindowname(wins[i]);
							break;
						}
					}
#endif
				} else {
					printwindowname(wins[i]);
				}
			}
			nkids2 = listwindows(wins[i], &kids2);
			for(i2 = 0; i2 < nkids2; i2++) {
				XGetWindowAttributes(dpy, kids2[i2], &attr);
				if(attr.override_redirect == 0 && attr.map_state == IsViewable) {

					if(class) {
						classes[0] = 0;
						listProps(kids2[i2], "WM_CLASS", classes);
						for(j = 0; classes[j]; j++) {
							if(strstr(classes[j], class) ){
						//		printf("classa[%X] = \'%s\'\n",kids2[i2],classes[j]);
								printwindowname(kids2[i2]);
								break;
							}	
						}
					} else {
						printwindowname(kids2[i2]);
					}
				}
					
			}
			if(kids2)
				XFree(kids2);
		}
	}

	if(wins)
		XFree(wins);
}


#else

static void
printwindowname(Window window)
{
    XTextProperty tp;
    if (window) {
	if (!XGetWMName(dpy, window, &tp)) { /* Get window name if any */
	    printf("%#x %s\n",(unsigned)window,"(none)");
        } else if (tp.nitems > 0) {
            printf("%#x ",(unsigned)window);
            {
                int count = 0, i, ret;
                char **list = NULL;
                ret = XmbTextPropertyToTextList(dpy, &tp, &list, &count);
                if((ret == Success || ret > 0) && list != NULL){
                    for(i=0; i<count; i++)
                        printf(list[i]);
                    XFreeStringList(list);
                } else {
                    printf("%s", tp.value);
                }
            }
            printf("\n");
	}
	else
	    printf("%#x %s\n",(unsigned)window," (none)");
    }
    return;
}


static void
ListWindows(char **argv) 
{
	unsigned int i, num;
	Window root;
	Window *wins, d1;
	XWindowAttributes wa;
	root = DefaultRootWindow(dpy);
	if((num = listwindows(root, &wins))) {
		for(i = 0; i < num; i++) {
				//printwindowname(wins[i]);
			if(!XGetWindowAttributes(dpy, wins[i], &wa) 
				|| XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if(wa.override_redirect)
				continue;
			if(wa.map_state == IsViewable){
				printwindowname(wins[i]);
				sel = wins[i];
			}
		}
	}
	if(wins)
		XFree(wins);
}

#endif

Window sel;
void
Root(char **argv)
{
	printf("%#x %s\n",(unsigned)DefaultRootWindow(dpy),"root");
}

void
TopWindow(char **argv)
{
	unsigned int i, num;
	Window root;
	Window *wins, d1;
	XWindowAttributes wa;
	root = DefaultRootWindow(dpy);
	sel = BadWindow;
	if((num = listwindows(root, &wins))) {
		for(i = 0; i < num; i++) {
			if(!XGetWindowAttributes(dpy, wins[i], &wa) 
				|| XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if(wa.override_redirect)
				continue;
			if(wa.map_state == IsViewable){
				sel = wins[i];
			}
		}
	}
	if(sel != BadWindow){
		printwindowname(sel);
	}
	if(wins)
		XFree(wins);
}

#define LONG_MAX (~((unsigned long)0))

#define DEF_ATOM(name) Atom ATOM_##name
     DEF_ATOM(_NET_WM_WINDOW_TYPE);
     DEF_ATOM(_NET_WM_WINDOW_TYPE_DESKTOP);
     DEF_ATOM(_NET_WM_WINDOW_TYPE_DOCK);
     DEF_ATOM(_NET_WM_WINDOW_TYPE_DIALOG);
     DEF_ATOM(_NET_WM_WINDOW_TYPE_SPLASH);
     DEF_ATOM(WM_PROTOCOLS);
     DEF_ATOM(WM_DELETE_WINDOW);
     DEF_ATOM(WM_NAME);
     DEF_ATOM(WM_STATE);
     DEF_ATOM(WM_CHANGE_STATE);
     DEF_ATOM(_NET_WM_NAME);
     DEF_ATOM(_NET_WM_STATE);
     DEF_ATOM(_NET_WM_STATE_MODAL);
     DEF_ATOM(_NET_SUPPORTED);


//#define NATOMS ((Atom*)&ATOM__NET_SUPPORTED - (Atom*)&ATOM__NET_CLIENT_LIST)
#define NATOMS 25

void
zwm_init_atoms(void) {
#define INIT_ATOM(name) ATOM_##name = XInternAtom(dpy, #name, False)
     INIT_ATOM(_NET_WM_WINDOW_TYPE);
     INIT_ATOM(_NET_WM_WINDOW_TYPE_DESKTOP);
     INIT_ATOM(_NET_WM_WINDOW_TYPE_DOCK);
     INIT_ATOM(_NET_WM_WINDOW_TYPE_DIALOG);
     INIT_ATOM(_NET_WM_WINDOW_TYPE_SPLASH);
     INIT_ATOM(WM_PROTOCOLS);
     INIT_ATOM(WM_DELETE_WINDOW);
     INIT_ATOM(WM_NAME);
     INIT_ATOM(WM_STATE);
     INIT_ATOM(_NET_WM_NAME);
     INIT_ATOM(_NET_WM_STATE);
     INIT_ATOM(_NET_WM_STATE_MODAL);
     INIT_ATOM(_NET_SUPPORTED);
}

Bool
zwm_x11_check_atom(Window win, Atom bigatom, Atom smallatom){
    Atom real, *state;
    int format;
    unsigned char *data = NULL;
    unsigned long i, n, extra;
    if(XGetWindowProperty(dpy, win, bigatom, 0L, LONG_MAX, False,
                          XA_ATOM, &real, &format, &n, &extra,
                          (unsigned char **) &data) == Success && data){
        state = (Atom *) data;
        for(i = 0; i < n; i++){
            if(state[i] == smallatom)
                        return True;
        }
    }
    return False;
}


static int
check_panel(Window w)
{
	if(zwm_x11_check_atom(w, ATOM__NET_WM_WINDOW_TYPE,
				ATOM__NET_WM_WINDOW_TYPE_DOCK)){
		return 1;
	}
	return 0;
}

static int
check_desktop(Window w)
{
	if(zwm_x11_check_atom(w, ATOM__NET_WM_WINDOW_TYPE,
			       	ATOM__NET_WM_WINDOW_TYPE_DESKTOP)){
		return 1;
	}
	return 0;
}


void
NextWindow(char **argv)
{
	unsigned int i, num, w, h, d, b;
	int x,y;
	int revertToReturn;
	Window root, focusReturn;
	Window *wins, d1;
	XWindowAttributes wa;
	root = DefaultRootWindow(dpy);
	sel = BadWindow;
    zwm_init_atoms();

	XGetInputFocus(dpy, &focusReturn, &revertToReturn);
			printwindowname(focusReturn);
//	XCirculateSubwindowsUp(dpy, DefaultRootWindow(dpy));
	
	if((num = listwindows(root, &wins))) {
		for(i = 0; i < num; i++) {
			printwindowname(wins[i]);
			if(!XGetWindowAttributes(dpy, wins[i], &wa) 
				|| XGetTransientForHint(dpy, wins[i], &d1)
				|| check_panel(wins[i])
				|| check_desktop(wins[i])
				)
				continue;

			if(wa.override_redirect)
				continue;

			if(wa.map_state == IsViewable && focusReturn == wins[i]){
				i++;
				break;
			}
		}

		for(; i < num; i++) {
			printwindowname(wins[i]);
			if(!XGetWindowAttributes(dpy, wins[i], &wa) 
				|| XGetTransientForHint(dpy, wins[i], &d1)
				|| check_panel(wins[i])
				|| check_desktop(wins[i])
				)
				continue;

			if(wa.override_redirect)
				continue;

			if(wa.map_state == IsViewable ){
				sel = wins[i];
				break;
			}
		}
	}



	if(sel == BadWindow){
		for(i = 0; i < num; i++) {
			printwindowname(wins[i]);
			if(!XGetWindowAttributes(dpy, wins[i], &wa) 
					|| XGetTransientForHint(dpy, wins[i], &d1)
					|| check_panel(wins[i])
					|| check_desktop(wins[i])
					)
				continue;

			if(wa.override_redirect)
				continue;

			if(wa.map_state == IsViewable ){
				sel = wins[i];
				break;
			}
		}
	}

	if (sel != BadWindow) {
		//focus
		XMapWindow(dpy, sel);
		XRaiseWindow(dpy, sel);
		XSetInputFocus (dpy, sel, RevertToPointerRoot, CurrentTime);
		XGetGeometry(dpy, sel, &root, &x, &y, &w, &h, &d, &b);
		XWarpPointer(dpy, None, sel, 0, 0, 0, 0, w/2, h/2);
		printwindowname(sel);
	}

	if(wins)
		XFree(wins);
}

static void
listProps(Window window, char * name, char **values) {
	Atom prop;
	Atom realType;
	unsigned long n;
	unsigned long extra;
	int format;
	int status;
	char * value;
	int i,j = 0;
	
	prop = XInternAtom(dpy, name, True);
	if (prop == None) {
		fprintf(stderr, "%s: no such property '%s'\n", argv0, name);
		return;
	}
	
	status = XGetWindowProperty(dpy, window, prop, 0L, 512L, 0, AnyPropertyType,
		&realType, &format, &n, &extra, (unsigned char **) &value);
	if (status != Success || n == 0) {
		values[0] = 0;
		return;
	}
	
	values[0] = NULL;
	if(format == 8) {
		values[0] = strdup(value);
		for(j = 0, i = 0; i < n; i++)
		{
			if(value[i] == 0){
				values[++j] = strdup(&value[i+1]);
			}
		}
		j++;
	} else if(format == 32) {
		for(j=0, i = 0; i < n; i++) {
#if __x86_64__
			values[j++] = (char*)((uint64_t*)value)[i];
#else
			values[j++] = (char*)((int*)value)[i];
#endif
		}
	}

	values[j] = NULL;
	XFree(value);
}


static void
getWindowProperty(Window window, char * name, Bool delete) {
	Atom prop;
	Atom realType;
	unsigned long n;
	unsigned long extra;
	int format;
	int status;
	char * value;
	char *values[32];
	int i,j;
	
	prop = XInternAtom(dpy, name, True);
	if (prop == None) {
		fprintf(stderr, "%s: no such property '%s'\n", argv0, name);
		return;
	}
	
	status = XGetWindowProperty(dpy, window, prop, 0L, 512L, delete, AnyPropertyType,
		&realType, &format, &n, &extra, (unsigned char **) &value);
	if (status != Success || value == 0 || *value == 0 || n == 0) {
		fprintf(stderr, "%s: couldn't read property on window %lx\n", argv0, window);
		return;
	}
//	printf("%d %d",realType, format);

	if(format == 8) {

		values[0] = value;
		for(j = 0, i = 0; i < n; i++)
		{
			if(value[i] == 0){
				values[j+1] = &value[i+1];
				j++;
			}
		}
		values[j+1] = NULL;

		for(i = 0; values[i]; i++) {
			if(strlen(values[i])){
				if(i>0)
					printf(", ");
				printf("%s",values[i]);
			}
		} 

	} else if(format == 32) {
		for(i = 0; i < n; i++) {
			printf("%d%c",((int*)value)[i],n>1?',':0);
		}
	}

	if(n)printf("\n");
	XFree(value);
}

static void
GetProperty(char ** argv) {
	getWindowProperty(wind(argv[0]), argv[1], False);
}

static void
SetProperty(char ** argv) {
	Atom prop;
	char * name = argv[1];
	char * value = argv[2];
	
	prop = XInternAtom(dpy, name, False);
	if (prop == None) {
		fprintf(stderr, "%s: no such property '%s'\n", argv0, name);
		return;
	}
	
	XChangeProperty(dpy, wind(argv[0]), prop, XA_STRING, 8,
		PropModeReplace, (unsigned char*)value, strlen(value));
}

static void
WarpPointer(char ** argv) {
	XWarpPointer(dpy, None, wind(argv[0]), 0, 0, 0, 0, atoi(argv[1]), atoi(argv[2]));
}

static void
GetFocusWindow(char ** argv) {
	Window focusReturn;
	int revertToReturn;
	XGetInputFocus(dpy, &focusReturn, &revertToReturn);
	printwindowname(focusReturn);
}
static void
FocusWindow(char ** argv) {
	XSetInputFocus(dpy, wind(argv[0]), RevertToPointerRoot, CurrentTime);
}


static void
GetSelection(char ** argv) {
	Window window;
	Atom dataProperty;
	XEvent ev;
	
	/* Create an unmapped invisible window. */
	window = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0,
		CopyFromParent, InputOnly, CopyFromParent, 0, 0);
	
	/* Ask that the current selection be placed in a property on the window. */
	dataProperty = XInternAtom(dpy, "SELECTION", False);
	XConvertSelection(dpy, XA_PRIMARY, XA_STRING, dataProperty,
		window, CurrentTime);
	
	/* Wait for it to arrive. */
	do {
		XNextEvent(dpy, &ev);
	} while (ev.type != SelectionNotify);
	
	getWindowProperty(window, "SELECTION", True);
}

static void
DeleteSelection(char ** argv) {
	XSetSelectionOwner(dpy, XA_PRIMARY, None, CurrentTime);
}

static void
CirculateUp(char ** argv) {
	XCirculateSubwindowsUp(dpy, DefaultRootWindow(dpy));
}

static void
CirculateDown(char ** argv) {
	XCirculateSubwindowsDown(dpy, DefaultRootWindow(dpy));
}
static void
GetDisplayWidth(char ** argv) {
	printf("%d\n", DisplayWidth(dpy, DefaultScreen(dpy)));
}
static void
GetDisplayHeight(char ** argv) {
	printf("%d\n", DisplayHeight(dpy, DefaultScreen(dpy)));
}

#ifdef CONFIG_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
static void
resolution(char **argv)
{
	int vscreen_x,vscreen_y,vscreen_w,vscreen_h;
	int i;

#ifdef CONFIG_XINERAMA
	if ( XineramaIsActive(dpy) ) 
	{
		int nscreen;
		XineramaScreenInfo * info = XineramaQueryScreens( dpy, &nscreen );

		for( i = 0; i< nscreen; i++ ) {
		
			if( i == 0 || (info[i].x_org != info[i-1].x_org)) {

				printf( "Screen %d : %d+%d+%dx%d\n",
				       	info[i].screen_number,
					info[i].x_org, info[i].y_org,
					info[i].width, info[i].height );

				if(vscreen_y >= info[i].y_org)
					vscreen_y = info[i].y_org;
				if(vscreen_x >= info[i].x_org)
					vscreen_x = info[i].x_org;
				vscreen_w += info[i].width ;
				if(vscreen_h < info[i].height)
					vscreen_h = info[i].height;
			}
		}
		XFree(info);
	} else 
#endif
	{
		vscreen_x = 0;
		vscreen_y = 0;
		vscreen_w = DisplayWidth(dpy, DefaultScreen(dpy));
		vscreen_h = DisplayHeight(dpy, DefaultScreen(dpy));

		printf( "Screen %d: %d+%d+%dx%d\n", 0,
			       	 vscreen_x,  vscreen_y,
				 vscreen_w , vscreen_h);
	} 
}
static void
WherePointer(char ** argv) {
	Window rr,ch;
	int x, y;
	Window root = DefaultRootWindow(dpy);
	int wx, wy, mask;
	Bool ret = XQueryPointer(dpy, root, &rr, &ch, &x, &y, &wx, &wy, &mask);
	if(ret){
		printf("%d %d\n", x, y);
	}
}

static ReasonDesc reasons[] = 
{
	{ "-circdown",	0,	0,CirculateDown,		"" },
	{ "-circup",	0,	0,CirculateUp,		"" },
	{ "-delsel",	0,	0,DeleteSelection,	"" },
	{ "-displayheight",0,	0,GetDisplayHeight,	"" },
	{ "-displaywidth",0,	0,GetDisplayWidth,	"" },
	{ "-focus", 	1,	0,FocusWindow,		"<id>" },
	{ "-focused", 	0,	0,GetFocusWindow,		"" },
	{ "-getprop",	2,	0,GetProperty,		"<id> <name>" },
	{ "-getsel",	0,	0,GetSelection,		"" },
	{ "-hide", 	1, 	0,HideWindow,		"<id>" },
	{ "-iconify", 	1, 	0,IconifyWindow,		"<id>" },
	{ "-kill", 	1, 	0,KillWindow,		"<id>" },
	{ "-label",	2,	0,LabelWindow,		"<id> <text>" },
	{ "-leaf", 	0, 	0,TopWindow,			"" },
	{ "-list", 	1, 	1,ListWindows,		"<class>" },
	{ "-lower", 	1, 	0,LowerWindow,		"<id>" },
	{ "-mapraised",	1,	0,MapRaiseWindow,		"<id>" },
	{ "-max", 	1, 	0,Max,			"<id>" },
	{ "-move", 	3, 	0,MoveWindow,		"<id> <x> <y>" },
	{ "-next",	0,	0,NextWindow,		"" },
	{ "-raise", 	1, 	0,RaiseWindow,		"<id>" },
	{ "-resize", 	3, 	0,ResizeWindow,		"<id> <x> <y>" },
	{ "-resolution",0 , 	0,resolution,		"" },
	{ "-root", 	0, 	0,Root,			"" },
	{ "-setprop",	3,	0,SetProperty,		"<id> <name> <value>" },
	{ "-setview",	2,	0,SetView,		"<id> <view>" },
	{ "-getview",	1,	0,GetView,		"<id>" },
	{ "-unhide", 	1, 	0,UnhideWindow,		"<id>" },
	{ "-view",	1,	0,CurrentView,		"<view|-1>" },
	{ "-warppointer", 3,	0, WarpPointer,		"<id> <x> <y>" },
	{ "-where", 	1, 	0,WhereWindow,		"<id>" },
	{ "-pointer", 	0, 	0,WherePointer,		"" },
};

static int
handler(Display *disp, XErrorEvent *err) {
	fprintf(stderr, "%s: no window with id %#x\n", argv0, (int) err->resourceid);
	exit(EXIT_FAILURE);
}

static void
usage(void) {
	ReasonDesc * p;

	fprintf(stderr, "usage: %s\n", argv0);
	for (p = reasons; p < reasons + sizeof reasons/sizeof reasons[0]; p++) {
		fprintf(stderr, "    %s %s %s\n", argv0, p->name, p->usage);
	}
}

extern int
main(int argc, char * argv[]) {
	ReasonDesc * p;
	
	argv0 = argv[0];
	
	dpy = XOpenDisplay("");
	if (dpy == 0) {
		fprintf(stderr, "%s: can't open display.\n", argv0);
		exit(EXIT_FAILURE);
	}
	
	XSetErrorHandler(handler);
	
	if (argc > 1) {
		for (p = reasons; p < reasons + sizeof reasons / sizeof reasons[0]; p++) {
			if (strcmp(p->name, argv[1]) == 0 ||
			    strcmp(&p->name[1], argv[1]) == 0) {
				if (argc - 2 != p->nargs && ((p->opargs && argc - 2 > p->opargs) || p->opargs == 0) ) {
					fprintf(stderr, "%s: the %s option requires %d argument%s.\n",
						argv0, argv[1], p->nargs, p->nargs > 1 ? "s" : "");
					exit(EXIT_FAILURE);
				}
				p->fn(argv + 2);
				XSync(dpy, True);
				return EXIT_SUCCESS;
			}
		}
	}
	
	usage();
	return EXIT_FAILURE;
}
