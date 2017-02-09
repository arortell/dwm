/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance.  Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag.  Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->window_x+(m)->window_width) - MAX((x),(m)->window_x)) \
                               * MAX(0, MIN((y)+(h),(m)->window_y+(m)->window_height) - MAX((y),(m)->window_y)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->width + 2 * (X)->bw + window_gap)
#define HEIGHT(X)               ((X)->height + 2 * (X)->bw + window_gap)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_text(drw, 0, 0, 0, 0, (X), 0) + drw->fonts[0]->h)

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeLast }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetWMWindowTypeNotification, NetClientList, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
    int x_pos, y_pos, width, height;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	Client *next;
	Client *snext;
	Monitor *mon;
	Window win;
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

struct Monitor {
	char ltsymbol[16];
	float mfact;
	int nmaster;
	int num;
    int bar_y;                                                 /* bar geometry */
    int mon_x, mon_y, mon_width, mon_height;                /* screen size */
    int window_x, window_y, window_width, window_height;   /* window area  */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int topbar;
	Client *clients;
    Client *selected_client;
	Client *stack;
	Monitor *next;
	Window barwin;
	const Layout *lt[2];
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int monitor;
} Rule;

/* function declarations */
static void applyrules(Client *client);
static int applysizehints(Client *client, int *x_pos, int *y_pos, int *width, int *height, int interact);
static void arrange(Monitor *monitor);
static void arrangemon(Monitor *monitor);
static void attach(Client *client);
static void attachstack(Client *client);
static void bstack(Monitor *monitor);
static void bstackhoriz(Monitor *monitor);
static void buttonpress(XEvent *event);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clearurgent(Client *client);
static void clientmessage(XEvent *event);
static void configure(Client *client);
static void configurenotify(XEvent *event);
static void configurerequest(XEvent *event);
static Monitor *createmon(void);
static void destroynotify(XEvent *event);
static void detach(Client *client);
static void detachstack(Client *client);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *monitor);
static void drawbars(void);
static void enternotify(XEvent *event);
static void expose(XEvent *event);
static void focus(Client *client);
static void focusin(XEvent *event);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static int getrootptr(int *x_pos, int *y_pos);
static long getstate(Window window);
static int gettextprop(Window window, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *client, int focused);
static void grabkeys(void);
static void keypress(XEvent *event);
static void killclient(const Arg *arg);
static void manage(Window window, XWindowAttributes *window_attributes);
static void mappingnotify(XEvent *event);
static void maprequest(XEvent *event);
static void monocle(Monitor *monitor);
static void motionnotify(XEvent *event);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *client);
static void pop(Client *client);
static void propertynotify(XEvent *event);
static void quit(const Arg *arg);
static Monitor *recttomon(int x_pos, int y_pos, int width, int height);
static void resize(Client *client, int x_pos, int y_pos, int width, int height, int interact);
static void resizeclient(Client *client, int x_pos, int y_pos, int width, int height);
static void resizemouse(const Arg *arg);
static void restack(Monitor *monitor);
static void run(void);
static void scan(void);
static int sendevent(Client *client, Atom proto);
static void sendmon(Client *client, Monitor *monitor);
static void setclientstate(Client *client, long state);
static void setfocus(Client *client);
static void setfullscreen(Client *client, int fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *monitor);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unfocus(Client *client, int setfocus);
static void unmanage(Client *client, int destroyed);
static void unmapnotify(XEvent *event);
static int updategeom(void);
static void updatebarpos(Monitor *monitor);
static void updatebars(void);
static void updateclientlist(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *client);
static void updatestatus(void);
static void updatewindowtype(Client *client);
static void updatetitle(Client *client);
static void updatewmhints(Client *client);
static void view(const Arg *arg);
static void warp(const Client *client);
static Client *wintoclient(Window window);
static Monitor *wintomon(Window window);
static int xerror(Display *dpy, XErrorEvent *error_event);
static int xerrordummy(Display *dpy, XErrorEvent *error_event);
static int xerrorstart(Display *dpy, XErrorEvent *error_event);
static void zoom(const Arg *arg);

/* variables */
static const char broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;      /* X display screen geometry width, height */
static int bh, blw = 0; /* bar geometry */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static int running = 1;
static Cur *cursor[CurLast];
static ClrScheme scheme[SchemeLast];
static Display *dpy;
static Drw *drw;
static Monitor *monitor_start, *selected_monitor;
static Window root;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */
void
applyrules(Client *client)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
    client->isfloating = 0;
    client->tags = 0;
    XGetClassHint(dpy, client->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
        if ((!r->title || strstr(client->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
            client->isfloating = r->isfloating;
            client->tags |= r->tags;
            for (m = monitor_start; m && m->num != r->monitor; m = m->next);
			if (m)
                client->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
    client->tags = client->tags & TAGMASK ? client->tags & TAGMASK : client->mon->tagset[client->mon->seltags];
}

int
applysizehints(Client *client, int *x_pos, int *y_pos, int *width, int *height, int interact)
{
	int baseismin;
    Monitor *m = client->mon;

	/* set minimum possible */
    *width = MAX(1, *width);
    *height = MAX(1, *height);
	if (interact) {
        if (*x_pos > sw)
            *x_pos = sw - WIDTH(client);
        if (*y_pos > sh)
            *y_pos = sh - HEIGHT(client);
        if (*x_pos + *width + 2 * client->bw < 0)
            *x_pos = 0;
        if (*y_pos + *height + 2 * client->bw < 0)
            *y_pos = 0;
	} else {
        if (*x_pos >= m->window_x + m->window_width)
            *x_pos = m->window_x + m->window_width - WIDTH(client);
        if (*y_pos >= m->window_y + m->window_height)
            *y_pos = m->window_y + m->window_height - HEIGHT(client);
        if (*x_pos + *width + 2 * client->bw <= m->window_x)
            *x_pos = m->window_x;
        if (*y_pos + *height + 2 * client->bw <= m->window_y)
            *y_pos = m->window_y;
	}
    if (*height < bh)
        *height = bh;
    if (*width < bh)
        *width = bh;
    if (resizehints || client->isfloating || !client->mon->lt[client->mon->sellt]->arrange) {
		/* see last two sentences in ICCCM 4.1.2.3 */
        baseismin = client->basew == client->minw && client->baseh == client->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
            *width -= client->basew;
            *height -= client->baseh;
		}
		/* adjust for aspect limits */
        if (client->mina > 0 && client->maxa > 0) {
            if (client->maxa < (float)*width / *height)
                *width = *height * client->maxa + 0.5;
            else if (client->mina < (float)*height / *width)
                *height = *width * client->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
            *width -= client->basew;
            *height -= client->baseh;
		}
		/* adjust for increment value */
        if (client->incw)
            *width -= *width % client->incw;
        if (client->inch)
            *height -= *height % client->inch;
		/* restore base dimensions */
        *width = MAX(*width + client->basew, client->minw);
        *height = MAX(*height + client->baseh, client->minh);
        if (client->maxw)
            *width = MIN(*width, client->maxw);
        if (client->maxh)
            *height = MIN(*height, client->maxh);
	}
    return *x_pos != client->x_pos || *y_pos != client->y_pos || *width != client->width || *height != client->height;
}

void
arrange(Monitor *monitor)
{
    if (monitor)
        showhide(monitor->stack);
    else for (monitor = monitor_start; monitor; monitor = monitor->next)
        showhide(monitor->stack);
    if (monitor) {
        arrangemon(monitor);
        restack(monitor);
    } else for (monitor = monitor_start; monitor; monitor = monitor->next)
        arrangemon(monitor);
}

void
arrangemon(Monitor *monitor)
{
    strncpy(monitor->ltsymbol, monitor->lt[monitor->sellt]->symbol, sizeof monitor->ltsymbol);
    if (monitor->lt[monitor->sellt]->arrange)
        monitor->lt[monitor->sellt]->arrange(monitor);
}

void
attach(Client *client)
{
    client->next = client->mon->clients;
    client->mon->clients = client;
}

void
attachstack(Client *client)
{
    client->snext = client->mon->stack;
    client->mon->stack = client;
}

static void
bstack(Monitor *monitor) {
    int width, height, monitor_height, monitor_x, tx, ty, tw;
    unsigned int i, number_of_clients;
    Client *client;

    for (number_of_clients = 0, client = nexttiled(monitor->clients); client; client = nexttiled(client->next), number_of_clients++);
    if (number_of_clients == 0)
        return;
    if (number_of_clients > monitor->nmaster) {
        monitor_height = monitor->nmaster ? monitor->mfact * monitor->window_height : 0;
        tw = monitor->window_width / (number_of_clients - monitor->nmaster);
        ty = monitor->window_y + monitor_height;
    } else {
        monitor_height = monitor->window_height;
        tw = monitor->window_width;
        ty = monitor->window_y;
    }
    for (i = monitor_x = 0, tx = monitor->window_x, client = nexttiled(monitor->clients); client; client = nexttiled(client->next), i++) {
        if (i < monitor->nmaster) {
            width = (monitor->window_width - monitor_x) / (MIN(number_of_clients, monitor->nmaster) - i);
            resize(client, monitor->window_x + monitor_x, monitor->window_y, width - (2 * client->bw), monitor_height - (2 * client->bw), 0);
            monitor_x += WIDTH(client);
        } else {
            height = monitor->window_height - monitor_height;
            resize(client, tx, ty, tw - (2 * client->bw), height - (2 * client->bw), 0);
            if (tw != monitor->window_width)
                tx += WIDTH(client);
        }
    }
}

static void
bstackhoriz(Monitor *monitor) {
    int width, monitor_height, monitor_x, tx, ty, th;
    unsigned int i, number_of_clients;
    Client *client;

    for (number_of_clients = 0, client = nexttiled(monitor->clients); client; client = nexttiled(client->next), number_of_clients++);
    if (number_of_clients == 0)
        return;
    if (number_of_clients > monitor->nmaster) {
        monitor_height = monitor->nmaster ? monitor->mfact * monitor->window_height : 0;
        th = (monitor->window_height - monitor_height) / (number_of_clients - monitor->nmaster);
        ty = monitor->window_y + monitor_height;
    } else {
        th = monitor_height = monitor->window_height;
        ty = monitor->window_y;
    }
    for (i = monitor_x = 0, tx = monitor->window_x, client = nexttiled(monitor->clients); client; client = nexttiled(client->next), i++) {
        if (i < monitor->nmaster) {
            width = (monitor->window_width - monitor_x) / (MIN(number_of_clients, monitor->nmaster) - i);
            resize(client, monitor->window_x + monitor_x, monitor->window_y, width - (2 * client->bw), monitor_height - (2 * client->bw), 0);
            monitor_x += WIDTH(client);
        } else {
            resize(client, tx, ty, monitor->window_width - (2 * client->bw), th - (2 * client->bw), 0);
            if (th != monitor->window_height)
                ty += HEIGHT(client);
        }
    }
}

void
buttonpress(XEvent *event)
{
	unsigned int i, x, click;
	Arg arg = {0};
	Client *c;
	Monitor *m;
    XButtonPressedEvent *ev = &event->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
    if ((m = wintomon(ev->window)) && m != selected_monitor) {
        unfocus(selected_monitor->selected_client, 1);
        selected_monitor = m;
		focus(NULL);
	}
    if (ev->window == selected_monitor->barwin) {
		i = x = 0;
		do
			x += TEXTW(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x < x + blw)
			click = ClkLtSymbol;
        else if (ev->x > selected_monitor->window_width - TEXTW(stext))
			click = ClkStatusText;
		else
			click = ClkWinTitle;
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	view(&a);
    selected_monitor->lt[selected_monitor->sellt] = &foo;
    for (m = monitor_start; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
    while (monitor_start)
        cleanupmon(monitor_start);
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < SchemeLast; i++) {
		drw_clr_free(scheme[i].border);
		drw_clr_free(scheme[i].bg);
		drw_clr_free(scheme[i].fg);
	}
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
    Monitor *monitor;

    if (mon == monitor_start)
        monitor_start = monitor_start->next;
	else {
        for (monitor = monitor_start; monitor && monitor->next != mon; monitor = monitor->next);
        monitor->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	free(mon);
}

void
clearurgent(Client *client)
{
	XWMHints *wmh;

    client->isurgent = 0;
    if (!(wmh = XGetWMHints(dpy, client->win)))
		return;
	wmh->flags &= ~XUrgencyHint;
    XSetWMHints(dpy, client->win, wmh);
	XFree(wmh);
}

void
clientmessage(XEvent *event)
{
    XClientMessageEvent *cme = &event->xclient;
	Client *c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen] || cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
			              || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (!ISVISIBLE(c)) {
			c->mon->seltags ^= 1;
			c->mon->tagset[c->mon->seltags] = c->tags;
		}
		pop(c);
	}
}

void
configure(Client *client)
{
    XConfigureEvent config_event;

    config_event.type = ConfigureNotify;
    config_event.display = dpy;
    config_event.event = client->win;
    config_event.window = client->win;
    config_event.x = client->x_pos;
    config_event.y = client->y_pos;
    config_event.width = client->width;
    config_event.height = client->height;
    config_event.border_width = client->bw;
    config_event.above = None;
    config_event.override_redirect = False;
    XSendEvent(dpy, client->win, False, StructureNotifyMask, (XEvent *)&config_event);
}

void
configurenotify(XEvent *event)
{
    Monitor *monitor;
    XConfigureEvent *config_event = &event->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
    if (config_event->window == root) {
        dirty = (sw != config_event->width || sh != config_event->height);
        sw = config_event->width;
        sh = config_event->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
            for (monitor = monitor_start; monitor; monitor = monitor->next)
                XMoveResizeWindow(dpy, monitor->barwin, monitor->window_x, monitor->bar_y, monitor->window_width, bh);
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *event)
{
    Client *client;
    Monitor *monitor;
    XConfigureRequestEvent *config_request = &event->xconfigurerequest;
    XWindowChanges window_changes;

    if ((client = wintoclient(config_request->window))) {
        if (config_request->value_mask & CWBorderWidth)
            client->bw = config_request->border_width;
        else if (client->isfloating || !selected_monitor->lt[selected_monitor->sellt]->arrange) {
            monitor = client->mon;
            if (config_request->value_mask & CWX) {
                client->oldx = client->x_pos;
                client->x_pos = monitor->mon_x + config_request->x;
			}
            if (config_request->value_mask & CWY) {
                client->oldy = client->y_pos;
                client->y_pos = monitor->mon_y + config_request->y;
			}
            if (config_request->value_mask & CWWidth) {
                client->oldw = client->width;
                client->width = config_request->width;
			}
            if (config_request->value_mask & CWHeight) {
                client->oldh = client->height;
                client->height = config_request->height;
			}
            if ((client->x_pos + client->width) > monitor->mon_x + monitor->mon_width && client->isfloating)
                client->x_pos = monitor->mon_x + (monitor->mon_width / 2 - WIDTH(client) / 2); /* center in x direction */
            if ((client->y_pos + client->height) > monitor->mon_y + monitor->mon_height && client->isfloating)
                client->y_pos = monitor->mon_y + (monitor->mon_height / 2 - HEIGHT(client) / 2); /* center in y direction */
            if ((config_request->value_mask & (CWX|CWY)) && !(config_request->value_mask & (CWWidth|CWHeight)))
                configure(client);
            if (ISVISIBLE(client))
                XMoveResizeWindow(dpy, client->win, client->x_pos, client->y_pos, client->width, client->height);
		} else
            configure(client);
	} else {
        window_changes.x = config_request->x;
        window_changes.y = config_request->y;
        window_changes.width = config_request->width;
        window_changes.height = config_request->height;
        window_changes.border_width = config_request->border_width;
        window_changes.sibling = config_request->above;
        window_changes.stack_mode = config_request->detail;
        XConfigureWindow(dpy, config_request->window, config_request->value_mask, &window_changes);
	}
	XSync(dpy, False);
}

Monitor *
createmon(void)
{
    Monitor *monitor;

    monitor = ecalloc(1, sizeof(Monitor));
    monitor->tagset[0] = monitor->tagset[1] = 1;
    monitor->mfact = mfact;
    monitor->nmaster = nmaster;
    monitor->showbar = showbar;
    monitor->topbar = topbar;
    monitor->lt[0] = &layouts[0];
    monitor->lt[1] = &layouts[1 % LENGTH(layouts)];
    strncpy(monitor->ltsymbol, layouts[0].symbol, sizeof monitor->ltsymbol);
    return monitor;
}

void
destroynotify(XEvent *event)
{
    Client *client;
    XDestroyWindowEvent *destroy_event = &event->xdestroywindow;

    if ((client = wintoclient(destroy_event->window)))
        unmanage(client, 1);
}

void
detach(Client *client)
{
	Client **tc;

    for (tc = &client->mon->clients; *tc && *tc != client; tc = &(*tc)->next);
    *tc = client->next;
}

void
detachstack(Client *client)
{
	Client **tc, *t;

    for (tc = &client->mon->stack; *tc && *tc != client; tc = &(*tc)->snext);
    *tc = client->snext;

    if (client == client->mon->selected_client) {
        for (t = client->mon->stack; t && !ISVISIBLE(t); t = t->snext);
        client->mon->selected_client = t;
	}
}

Monitor *
dirtomon(int dir)
{
    Monitor *monitor = NULL;

    for (monitor = monitor_start; monitor->next; monitor = monitor->next)
        if (dir == monitor->num)
            return monitor;

    return monitor;
}

void
drawbar(Monitor *monitor)
{
	int x, xx, w, dx;
	unsigned int i, occ = 0, urg = 0;
    Client *client;

	dx = (drw->fonts[0]->ascent + drw->fonts[0]->descent + 2) / 4;

    for (client = monitor->clients; client; client = client->next) {
        occ |= client->tags;
        if (client->isurgent)
            urg |= client->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
        drw_setscheme(drw, monitor->tagset[monitor->seltags] & 1 << i ? &scheme[SchemeSel] : &scheme[SchemeNorm]);
		drw_text(drw, x, 0, w, bh, tags[i], urg & 1 << i);
        drw_rect(drw, x + 1, 1, dx, dx, monitor == selected_monitor && selected_monitor->selected_client && selected_monitor->selected_client->tags & 1 << i,
		           occ & 1 << i, urg & 1 << i);
		x += w;
	}
    w = blw = TEXTW(monitor->ltsymbol);
	drw_setscheme(drw, &scheme[SchemeNorm]);
    drw_text(drw, x, 0, w, bh, monitor->ltsymbol, 0);
	x += w;
	xx = x;
    if (monitor == selected_monitor)
    { /* status is only drawn on selected monitor */
		w = TEXTW(stext);
        x = monitor->window_width - w;
		if (x < xx) {
			x = xx;
            w = monitor->window_width - xx;
		}
		drw_text(drw, x, 0, w, bh, stext, 0);
	} else
        x = monitor->window_width;
	if ((w = x - xx) > bh) {
		x = xx;
        if (monitor->selected_client) {
            drw_setscheme(drw, monitor == selected_monitor ? &scheme[SchemeSel] : &scheme[SchemeNorm]);
            drw_text(drw, x, 0, w, bh, monitor->selected_client->name, 0);
            drw_rect(drw, x + 1, 1, dx, dx, monitor->selected_client->isfixed, monitor->selected_client->isfloating, 0);
		} else {
			drw_setscheme(drw, &scheme[SchemeNorm]);
			drw_rect(drw, x, 0, w, bh, 1, 0, 1);
		}
	}
    drw_map(drw, monitor->barwin, 0, 0, monitor->window_width, bh);
}

void
drawbars(void)
{
    Monitor *monitor;

    for (monitor = monitor_start; monitor; monitor = monitor->next)
        drawbar(monitor);
}

void
enternotify(XEvent *event)
{
    Client *client;
    Monitor *monitor;
    XCrossingEvent *crossing_event = &event->xcrossing;

    if ((crossing_event->mode != NotifyNormal || crossing_event->detail == NotifyInferior) && crossing_event->window != root)
		return;
    client = wintoclient(crossing_event->window);
    monitor = client ? client->mon : wintomon(crossing_event->window);
    if (monitor != selected_monitor) {
        unfocus(selected_monitor->selected_client, 1);
        selected_monitor = monitor;
    } else if (!client || client == selected_monitor->selected_client)
		return;
    focus(client);
}

void
expose(XEvent *event)
{
    Monitor *monitor;
    XExposeEvent *expose_event = &event->xexpose;

    if (expose_event->count == 0 && (monitor = wintomon(expose_event->window)))
        drawbar(monitor);
}

void
focus(Client *client)
{
    if (!client || !ISVISIBLE(client))
        for (client = selected_monitor->stack; client && !ISVISIBLE(client); client = client->snext);
	/* was if (selmon->sel) */
    if (selected_monitor->selected_client && selected_monitor->selected_client != client)
        unfocus(selected_monitor->selected_client, 0);
    if (client) {
        if (client->mon != selected_monitor)
            selected_monitor = client->mon;
        if (client->isurgent)
            clearurgent(client);
        detachstack(client);
        attachstack(client);
        grabbuttons(client, 1);
        XSetWindowBorder(dpy, client->win, scheme[SchemeSel].border->pix);
        setfocus(client);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
    selected_monitor->selected_client = client;
	drawbars();
}

/* there are some broken focus acquiring clients */
void
focusin(XEvent *event)
{
    XFocusChangeEvent *focus_change_event = &event->xfocus;

    if (selected_monitor->selected_client && focus_change_event->window != selected_monitor->selected_client->win)
        setfocus(selected_monitor->selected_client);
}

void
focusmon(const Arg *arg)
{
    Monitor *monitor;

    if (!monitor_start->next)
		return;
    if ((monitor = dirtomon(arg->i)) == selected_monitor)
		return;
    unfocus(selected_monitor->selected_client, 0); /* s/1/0/ fixes input focus issues
					in gedit and anjuta */
    selected_monitor = monitor;
	focus(NULL);
    warp(selected_monitor->selected_client);
}

void
focusstack(const Arg *arg)
{
    Client *client = NULL, *i;

    if (!selected_monitor->selected_client)
		return;
	if (arg->i > 0) {
        for (client = selected_monitor->selected_client->next; client && !ISVISIBLE(client); client = client->next);
        if (!client)
            for (client = selected_monitor->clients; client && !ISVISIBLE(client); client = client->next);
	} else {
        for (i = selected_monitor->clients; i != selected_monitor->selected_client; i = i->next)
			if (ISVISIBLE(i))
                client = i;
        if (!client)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
                    client = i;
	}
    if (client) {
        focus(client);
        restack(selected_monitor);
	}
}

Atom
getatomprop(Client *client, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

    if (XGetWindowProperty(dpy, client->win, prop, 0L, sizeof atom, False, XA_ATOM,
	                      &da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
}

int
getrootptr(int *x_pos, int *y_pos)
{
	int di;
	unsigned int dui;
	Window dummy;

    return XQueryPointer(dpy, root, &dummy, &dummy, x_pos, y_pos, &di, &di, &dui);
}

long
getstate(Window window)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

    if (XGetWindowProperty(dpy, window, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
	                      &real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

int
gettextprop(Window window, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
    XGetTextProperty(dpy, window, &name, atom);
	if (!name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *client, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
        XUngrabButton(dpy, AnyButton, AnyModifier, client->win);
		if (focused) {
			for (i = 0; i < LENGTH(buttons); i++)
				if (buttons[i].click == ClkClientWin)
					for (j = 0; j < LENGTH(modifiers); j++)
						XGrabButton(dpy, buttons[i].button,
						            buttons[i].mask | modifiers[j],
                                    client->win, False, BUTTONMASK,
						            GrabModeAsync, GrabModeSync, None, None);
		} else
            XGrabButton(dpy, AnyButton, AnyModifier, client->win, False,
			            BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						 True, GrabModeAsync, GrabModeAsync);
	}
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *event)
{
	unsigned int i;
	KeySym keysym;
    XKeyEvent *key_event;

    key_event = &event->xkey;
    keysym = XKeycodeToKeysym(dpy, (KeyCode)key_event->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
        && CLEANMASK(keys[i].mod) == CLEANMASK(key_event->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg)
{
    if (!selected_monitor->selected_client)
		return;
    if (!sendevent(selected_monitor->selected_client, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
        XKillClient(dpy, selected_monitor->selected_client->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
manage(Window window, XWindowAttributes *window_attributes)
{
    Client *client, *t = NULL;
	Window trans = None;
    XWindowChanges window_changes;

    client = ecalloc(1, sizeof(Client));
    client->win = window;
    updatetitle(client);
    if (XGetTransientForHint(dpy, window, &trans) && (t = wintoclient(trans))) {
        client->mon = t->mon;
        client->tags = t->tags;
	} else {
        client->mon = selected_monitor;
        applyrules(client);
	}
	/* geometry */
    client->x_pos = client->oldx = window_attributes->x;
    client->y_pos = client->oldy = window_attributes->y;
    client->width = client->oldw = window_attributes->width;
    client->height = client->oldh = window_attributes->height;
    client->oldbw = window_attributes->border_width;

    if (client->x_pos + WIDTH(client) > client->mon->mon_x + client->mon->mon_width)
        client->x_pos = client->mon->mon_x + client->mon->mon_width - WIDTH(client);
    if (client->y_pos + HEIGHT(client) > client->mon->mon_y + client->mon->mon_height)
        client->y_pos = client->mon->mon_y + client->mon->mon_height - HEIGHT(client);
    client->x_pos = MAX(client->x_pos, client->mon->mon_x);
	/* only fix client y-offset, if the client center might cover the bar */
    client->y_pos = MAX(client->y_pos, ((client->mon->bar_y == client->mon->mon_y) && (client->x_pos + (client->width / 2) >= client->mon->window_x)
               && (client->x_pos + (client->width / 2) < client->mon->window_x + client->mon->window_width)) ? bh : client->mon->mon_y);
    client->bw = borderpx;

    window_changes.border_width = client->bw;
    XConfigureWindow(dpy, window, CWBorderWidth, &window_changes);
    XSetWindowBorder(dpy, window, scheme[SchemeNorm].border->pix);
    configure(client); /* propagates border_width, if size doesn't change */
    updatewindowtype(client);
    updatesizehints(client);
    updatewmhints(client);
    XSelectInput(dpy, window, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
    grabbuttons(client, 0);
    if (!client->isfloating)
        client->isfloating = client->oldstate = trans != None || client->isfixed;
    if (client->isfloating)
        XRaiseWindow(dpy, client->win);
    attach(client);
    attachstack(client);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
                    (unsigned char *) &(client->win), 1);
    XMoveResizeWindow(dpy, client->win, client->x_pos + 2 * sw, client->y_pos, client->width, client->height); /* some windows require this */
    setclientstate(client, NormalState);
    if (client->mon == selected_monitor)
        unfocus(selected_monitor->selected_client, 0);
    client->mon->selected_client = client;
    arrange(client->mon);
    XMapWindow(dpy, client->win);
	focus(NULL);
}

void
mappingnotify(XEvent *event)
{
    XMappingEvent *mapping_event = &event->xmapping;

    XRefreshKeyboardMapping(mapping_event);
    if (mapping_event->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *event)
{
    static XWindowAttributes window_attributes;
    XMapRequestEvent *map_request_ev = &event->xmaprequest;

    if (!XGetWindowAttributes(dpy, map_request_ev->window, &window_attributes))
		return;
    if (window_attributes.override_redirect)
		return;
    if (!wintoclient(map_request_ev->window))
        manage(map_request_ev->window, &window_attributes);
}

void
monocle(Monitor *monitor)
{
	unsigned int n = 0;
    Client *client;

    for (client = monitor->clients; client; client = client->next)
        if (ISVISIBLE(client))
			n++;
	if (n > 0) /* override layout symbol */
        snprintf(monitor->ltsymbol, sizeof monitor->ltsymbol, "[%d]", n);
    for (client = nexttiled(monitor->clients); client; client = nexttiled(client->next))
        resize(client, monitor->window_x, monitor->window_y, monitor->window_width - 2 * client->bw, monitor->window_height - 2 * client->bw, 0);
}

void
motionnotify(XEvent *event)
{
	static Monitor *mon = NULL;
    Monitor *monitor;
    XMotionEvent *motion_event = &event->xmotion;

    if (motion_event->window != root)
		return;
    if ((monitor = recttomon(motion_event->x_root, motion_event->y_root, 1, 1)) != mon && mon) {
        unfocus(selected_monitor->selected_client, 1);
        selected_monitor = monitor;
		focus(NULL);
	}
    mon = monitor;
}

void
movemouse(const Arg *arg)
{
    int x_pos, y_pos, ocx, ocy, nx, ny;
    Client *client;
    Monitor *monitor;
    XEvent event;
	Time lasttime = 0;

    if (!(client = selected_monitor->selected_client))
		return;
    if (client->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
    restack(selected_monitor);
    ocx = client->x_pos;
    ocy = client->y_pos;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
	None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
    if (!getrootptr(&x_pos, &y_pos))
		return;
	do {
        XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &event);
        switch(event.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
            handler[event.type](&event);
			break;
		case MotionNotify:
            if ((event.xmotion.time - lasttime) <= (1000 / 60))
				continue;
            lasttime = event.xmotion.time;

            nx = ocx + (event.xmotion.x - x_pos);
            ny = ocy + (event.xmotion.y - y_pos);
            if (nx >= selected_monitor->window_x && nx <= selected_monitor->window_x + selected_monitor->window_width
            && ny >= selected_monitor->window_y && ny <= selected_monitor->window_y + selected_monitor->window_height) {
                if (abs(selected_monitor->window_x - nx) < snap)
                    nx = selected_monitor->window_x;
                else if (abs((selected_monitor->window_x + selected_monitor->window_width) - (nx + WIDTH(client))) < snap)
                    nx = selected_monitor->window_x + selected_monitor->window_width - WIDTH(client);
                if (abs(selected_monitor->window_y - ny) < snap)
                    ny = selected_monitor->window_y;
                else if (abs((selected_monitor->window_y + selected_monitor->window_height) - (ny + HEIGHT(client))) < snap)
                    ny = selected_monitor->window_y + selected_monitor->window_height - HEIGHT(client);
                if (!client->isfloating && selected_monitor->lt[selected_monitor->sellt]->arrange
                && (abs(nx - client->x_pos) > snap || abs(ny - client->y_pos) > snap))
					togglefloating(NULL);
			}
            if (!selected_monitor->lt[selected_monitor->sellt]->arrange || client->isfloating)
                resize(client, nx, ny, client->width, client->height, 1);
			break;
		}
    } while (event.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
    if ((monitor = recttomon(client->x_pos, client->y_pos, client->width, client->height)) != selected_monitor) {
        sendmon(client, monitor);
        selected_monitor = monitor;
		focus(NULL);
	}
}

Client *
nexttiled(Client *client)
{
    for (; client && (client->isfloating || !ISVISIBLE(client)); client = client->next);
    return client;
}

void
pop(Client *client)
{
    detach(client);
    attach(client);
    focus(client);
    arrange(client->mon);
}

void
propertynotify(XEvent *event)
{
    Client *client;
	Window trans;
    XPropertyEvent *property_event = &event->xproperty;

    if ((property_event->window == root) && (property_event->atom == XA_WM_NAME))
		updatestatus();
    else if (property_event->state == PropertyDelete)
		return; /* ignore */
    else if ((client = wintoclient(property_event->window))) {
        switch(property_event->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
            if (!client->isfloating && (XGetTransientForHint(dpy, client->win, &trans)) &&
               (client->isfloating = (wintoclient(trans)) != NULL))
                arrange(client->mon);
			break;
		case XA_WM_NORMAL_HINTS:
            updatesizehints(client);
			break;
		case XA_WM_HINTS:
            updatewmhints(client);
			drawbars();
			break;
		}
        if (property_event->atom == XA_WM_NAME || property_event->atom == netatom[NetWMName]) {
            updatetitle(client);
            if (client == client->mon->selected_client)
                drawbar(client->mon);
		}
        if (property_event->atom == netatom[NetWMWindowType])
            updatewindowtype(client);
	}
}

void
quit(const Arg *arg)
{
	running = 0;
}

Monitor *
recttomon(int x_pos, int y_pos, int width, int height)
{
    Monitor *m, *r = selected_monitor;
	int a, area = 0;

    for (m = monitor_start; m; m = m->next)
        if ((a = INTERSECT(x_pos, y_pos, width, height, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
resize(Client *client, int x_pos, int y_pos, int width, int height, int interact)
{
    if (applysizehints(client, &x_pos, &y_pos, &width, &height, interact))
        resizeclient(client, x_pos, y_pos, width, height);
}

void
resizeclient(Client *client, int x_pos, int y_pos, int width, int height)
{
    XWindowChanges window_changes;
    unsigned int n;
    unsigned int gapoffset;
    unsigned int gapincr;
    Client *nbc;

    window_changes.border_width = client->bw;

    /* Get number of clients for the selected monitor */
    for (n = 0, nbc = nexttiled(selected_monitor->clients); nbc; nbc = nexttiled(nbc->next), n++);

    /* Do nothing if layout is floating */
    if (client->isfloating || selected_monitor->lt[selected_monitor->sellt]->arrange == NULL)
    {
        gapincr = gapoffset = 0;
    } else {
        /* Remove border and gap if layout is monocle or only one client */
        if (selected_monitor->lt[selected_monitor->sellt]->arrange == monocle || n == 1)
        {
            gapoffset = 0;
            gapincr = -2 * borderpx;
            window_changes.border_width = 0;
        } else {
            gapoffset = window_gap;
            gapincr = 2 * window_gap;
        }
    }

    client->oldx = client->x_pos; client->x_pos = window_changes.x = x_pos + gapoffset;
    client->oldy = client->y_pos; client->y_pos = window_changes.y = y_pos + gapoffset;
    client->oldw = client->width; client->width = window_changes.width = width - gapincr;
    client->oldh = client->height; client->height = window_changes.height = height - gapincr;

    XConfigureWindow(dpy, client->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &window_changes);
    configure(client);
    XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
    Client *client;
    Monitor *monitor;
    XEvent event;
	Time lasttime = 0;

    if (!(client = selected_monitor->selected_client))
		return;
    if (client->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
    restack(selected_monitor);
    ocx = client->x_pos;
    ocy = client->y_pos;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
	                None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
    XWarpPointer(dpy, None, client->win, 0, 0, 0, 0, client->width + client->bw - 1, client->height + client->bw - 1);
	do {
        XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &event);
        switch(event.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
            handler[event.type](&event);
			break;
		case MotionNotify:
            if ((event.xmotion.time - lasttime) <= (1000 / 60))
				continue;
            lasttime = event.xmotion.time;

            nw = MAX(event.xmotion.x - ocx - 2 * client->bw + 1, 1);
            nh = MAX(event.xmotion.y - ocy - 2 * client->bw + 1, 1);
            if (client->mon->window_x + nw >= selected_monitor->window_x && client->mon->window_x + nw <= selected_monitor->window_x + selected_monitor->window_width
            && client->mon->window_y + nh >= selected_monitor->window_y && client->mon->window_y + nh <= selected_monitor->window_y + selected_monitor->window_height)
			{
                if (!client->isfloating && selected_monitor->lt[selected_monitor->sellt]->arrange
                && (abs(nw - client->width) > snap || abs(nh - client->height) > snap))
					togglefloating(NULL);
			}
            if (!selected_monitor->lt[selected_monitor->sellt]->arrange || client->isfloating)
                resize(client, client->x_pos, client->y_pos, nw, nh, 1);
			break;
		}
    } while (event.type != ButtonRelease);
    XWarpPointer(dpy, None, client->win, 0, 0, 0, 0, client->width + client->bw - 1, client->height + client->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &event));
    if ((monitor = recttomon(client->x_pos, client->y_pos, client->width, client->height)) != selected_monitor) {
        sendmon(client, monitor);
        selected_monitor = monitor;
		focus(NULL);
	}
}

void
restack(Monitor *monitor)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

    drawbar(monitor);
    if (!monitor->selected_client)
		return;
    if (monitor->selected_client->isfloating || !monitor->lt[monitor->sellt]->arrange)
        XRaiseWindow(dpy, monitor->selected_client->win);
    if (monitor->lt[monitor->sellt]->arrange) {
		wc.stack_mode = Below;
        wc.sibling = monitor->barwin;
        for (c = monitor->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
    if (monitor == selected_monitor && (monitor->tagset[monitor->seltags] & monitor->selected_client->tags))
        warp(monitor->selected_client);
}

void
run(void)
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
sendmon(Client *client, Monitor *monitor)
{
    if (client->mon == monitor)
		return;
    unfocus(client, 1);
    detach(client);
    detachstack(client);
    client->mon = monitor;
    client->tags = monitor->tagset[monitor->seltags]; /* assign tags of target monitor */
    attach(client);
    attachstack(client);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *client, long state)
{
	long data[] = { state, None };

    XChangeProperty(dpy, client->win, wmatom[WMState], wmatom[WMState], 32,
			PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Client *client, Atom proto)
{
	int n;
	Atom *protocols;
	int exists = 0;
    XEvent event;

    if (XGetWMProtocols(dpy, client->win, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
        event.type = ClientMessage;
        event.xclient.window = client->win;
        event.xclient.message_type = wmatom[WMProtocols];
        event.xclient.format = 32;
        event.xclient.data.l[0] = proto;
        event.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, client->win, False, NoEventMask, &event);
	}
	return exists;
}

void
setfocus(Client *client)
{
    if (!client->neverfocus) {
        XSetInputFocus(dpy, client->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
		                XA_WINDOW, 32, PropModeReplace,
                        (unsigned char *) &(client->win), 1);
	}
    sendevent(client, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *client, int fullscreen)
{
    if (fullscreen && !client->isfullscreen) {
        XChangeProperty(dpy, client->win, netatom[NetWMState], XA_ATOM, 32,
		                PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
        client->isfullscreen = 1;
        client->oldstate = client->isfloating;
        client->oldbw = client->bw;
        client->bw = 0;
        client->isfloating = 1;
        resizeclient(client, client->mon->mon_x, client->mon->mon_y, client->mon->mon_width, client->mon->mon_height);
        XRaiseWindow(dpy, client->win);
    } else if (!fullscreen && client->isfullscreen){
        XChangeProperty(dpy, client->win, netatom[NetWMState], XA_ATOM, 32,
		                PropModeReplace, (unsigned char*)0, 0);
        client->isfullscreen = 0;
        client->isfloating = client->oldstate;
        client->bw = client->oldbw;
        client->x_pos = client->oldx;
        client->y_pos = client->oldy;
        client->width = client->oldw;
        client->height = client->oldh;
        resizeclient(client, client->x_pos, client->y_pos, client->width, client->height);
        arrange(client->mon);
	}
}

void
setlayout(const Arg *arg)
{
    if (!arg || !arg->v || arg->v != selected_monitor->lt[selected_monitor->sellt])
        selected_monitor->sellt ^= 1;
	if (arg && arg->v)
        selected_monitor->lt[selected_monitor->sellt] = (Layout *)arg->v;
    strncpy(selected_monitor->ltsymbol, selected_monitor->lt[selected_monitor->sellt]->symbol, sizeof selected_monitor->ltsymbol);
    if (selected_monitor->selected_client)
        arrange(selected_monitor);
	else
        drawbar(selected_monitor);
}

/* arg > 1.0 will set mfact absolutly */
void
setmfact(const Arg *arg)
{
	float f;

    if (!arg || !selected_monitor->lt[selected_monitor->sellt]->arrange)
		return;
    f = arg->f < 1.0 ? arg->f + selected_monitor->mfact : arg->f - 1.0;
	if (f < 0.1 || f > 0.9)
		return;
    selected_monitor->mfact = f;
    arrange(selected_monitor);
}

void
setup(void)
{
    XSetWindowAttributes window_attributes;

	/* clean up any zombies immediately */
	sigchld(0);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	drw_load_fonts(drw, fonts, LENGTH(fonts));
	if (!drw->fontcount)
		die("no fonts could be loaded.\n");
	bh = drw->fonts[0]->h + 2;
	updategeom();
	/* init atoms */
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	scheme[SchemeNorm].border = drw_clr_create(drw, normbordercolor);
	scheme[SchemeNorm].bg = drw_clr_create(drw, normbgcolor);
	scheme[SchemeNorm].fg = drw_clr_create(drw, normfgcolor);
	scheme[SchemeSel].border = drw_clr_create(drw, selbordercolor);
	scheme[SchemeSel].bg = drw_clr_create(drw, selbgcolor);
	scheme[SchemeSel].fg = drw_clr_create(drw, selfgcolor);
	/* init bars */
	updatebars();
	updatestatus();
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
			PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select for events */
    window_attributes.cursor = cursor[CurNormal]->cursor;
    window_attributes.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|PointerMotionMask
	                |EnterWindowMask|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
    XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &window_attributes);
    XSelectInput(dpy, root, window_attributes.event_mask);
	grabkeys();
	focus(NULL);
}

void
showhide(Client *client)
{
    if (!client)
		return;
    if (ISVISIBLE(client)) {
		/* show clients top down */
        XMoveWindow(dpy, client->win, client->x_pos, client->y_pos);
        if ((!client->mon->lt[client->mon->sellt]->arrange || client->isfloating) && !client->isfullscreen)
            resize(client, client->x_pos, client->y_pos, client->width, client->height, 0);
        showhide(client->snext);
	} else {
		/* hide clients bottom up */
        showhide(client->snext);
        XMoveWindow(dpy, client->win, WIDTH(client) * -2, client->y_pos);
	}
}

void
sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
spawn(const Arg *arg)
{
	if (arg->v == dmenucmd)
        dmenumon[0] = '0' + selected_monitor->num;
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

void
tag(const Arg *arg)
{
    if (selected_monitor->selected_client && arg->ui & TAGMASK)
    {
        selected_monitor->selected_client->tags = arg->ui & TAGMASK;
		focus(NULL);
        arrange(selected_monitor);
	}
}

void
tagmon(const Arg *arg)
{
    if (!selected_monitor->selected_client || !monitor_start->next)
		return;
    sendmon(selected_monitor->selected_client, dirtomon(arg->i));
}

void
tile(Monitor *monitor)
{
    unsigned int i, n, h, mw, my, ty;
    Client *client;

    for (n = 0, client = nexttiled(monitor->clients); client; client = nexttiled(client->next), n++);
    if (n == 0)
        return;

    if (n > monitor->nmaster)
        mw = monitor->nmaster ? monitor->window_width * monitor->mfact : 0;
    else
        mw = monitor->window_width;
    for (i = my = ty = 0, client = nexttiled(monitor->clients); client; client = nexttiled(client->next), i++)
        if (i < monitor->nmaster) {
            h = (monitor->window_height - my) / (MIN(n, monitor->nmaster) - i);
            resize(client, monitor->window_x, monitor->window_y + my, mw - (2*client->bw), h - (2*client->bw), 0);
            my += HEIGHT(client);
        } else {
            h = (monitor->window_height - ty) / (n - i);
            resize(client, monitor->window_x + mw, monitor->window_y + ty, monitor->window_width - mw - (2*client->bw), h - (2*client->bw), 0);
            ty += HEIGHT(client);
        }
}

void
togglebar(const Arg *arg)
{
    selected_monitor->showbar = !selected_monitor->showbar;
    updatebarpos(selected_monitor);
    XMoveResizeWindow(dpy, selected_monitor->barwin, selected_monitor->window_x, selected_monitor->bar_y, selected_monitor->window_width, bh);
    arrange(selected_monitor);
}

void
togglefloating(const Arg *arg)
{
    if (!selected_monitor->selected_client)
		return;
    if (selected_monitor->selected_client->isfullscreen) /* no support for fullscreen windows */
		return;
    selected_monitor->selected_client->isfloating = !selected_monitor->selected_client->isfloating || selected_monitor->selected_client->isfixed;
    if (selected_monitor->selected_client->isfloating)
        resize(selected_monitor->selected_client, selected_monitor->selected_client->x_pos, selected_monitor->selected_client->y_pos,
               selected_monitor->selected_client->width, selected_monitor->selected_client->height, 0);
    arrange(selected_monitor);
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;

    if (!selected_monitor->selected_client)
		return;
    newtags = selected_monitor->selected_client->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
        selected_monitor->selected_client->tags = newtags;
		focus(NULL);
        arrange(selected_monitor);
	}
}

void
toggleview(const Arg *arg)
{
    unsigned int newtagset = selected_monitor->tagset[selected_monitor->seltags] ^ (arg->ui & TAGMASK);

	if (newtagset) {
        selected_monitor->tagset[selected_monitor->seltags] = newtagset;
		focus(NULL);
        arrange(selected_monitor);
	}
}

void
unfocus(Client *client, int setfocus)
{
    if (!client)
		return;
    grabbuttons(client, 0);
    XSetWindowBorder(dpy, client->win, scheme[SchemeNorm].border->pix);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *client, int destroyed)
{
    Monitor *m = client->mon;
	XWindowChanges wc;

	/* The server grab construct avoids race conditions. */
    detach(client);
    detachstack(client);
	if (!destroyed) {
        wc.border_width = client->oldbw;
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
        XConfigureWindow(dpy, client->win, CWBorderWidth, &wc); /* restore border */
        XUngrabButton(dpy, AnyButton, AnyModifier, client->win);
        setclientstate(client, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
    free(client);
	focus(NULL);
	updateclientlist();
	arrange(m);
}

void
unmapnotify(XEvent *event)
{
    Client *client;
    XUnmapEvent *unmap_event = &event->xunmap;

    if ((client = wintoclient(unmap_event->window))) {
        if (unmap_event->send_event)
            setclientstate(client, WithdrawnState);
		else
            unmanage(client, 0);
	}
}

void
updatebars(void)
{
    Monitor *monitor;
    XSetWindowAttributes window_attributes = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
    for (monitor = monitor_start; monitor; monitor = monitor->next) {
        if (monitor->barwin)
			continue;
        monitor->barwin = XCreateWindow(dpy, root, monitor->window_x, monitor->bar_y, monitor->window_width, bh, 0, DefaultDepth(dpy, screen),
		                          CopyFromParent, DefaultVisual(dpy, screen),
                                  CWOverrideRedirect|CWBackPixmap|CWEventMask, &window_attributes);
        XDefineCursor(dpy, monitor->barwin, cursor[CurNormal]->cursor);
        XMapRaised(dpy, monitor->barwin);
	}
}

void
updatebarpos(Monitor *monitor)
{
    monitor->window_y = monitor->mon_y;
    monitor->window_height = monitor->mon_height;
    if (monitor->showbar) {
        monitor->window_height -= bh;
        monitor->bar_y = monitor->topbar ? monitor->window_y : monitor->window_y + monitor->window_height;
        monitor->window_y = monitor->topbar ? monitor->window_y + bh : monitor->window_y;
	} else
        monitor->bar_y = -bh;
}

void
updateclientlist()
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
    for (m = monitor_start; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
			                XA_WINDOW, 32, PropModeAppend,
			                (unsigned char *) &(c->win), 1);
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
        int i, j, num_mons, num_screens;
        Client *client;
        Monitor *monitor;
        XineramaScreenInfo *info = XineramaQueryScreens(dpy, &num_screens);
		XineramaScreenInfo *unique = NULL;

        for (num_mons = 0, monitor = monitor_start; monitor; monitor = monitor->next, num_mons++);
		/* only consider unique geometries as separate screens */
        unique = ecalloc(num_screens, sizeof(XineramaScreenInfo));
        for (i = 0, j = 0; i < num_screens; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
        num_screens = j;
        if (num_mons <= num_screens) {
            for (i = 0; i < (num_screens - num_mons); i++) { /* new monitors available */
                for (monitor = monitor_start; monitor && monitor->next; monitor = monitor->next);
                if (monitor)
                    monitor->next = createmon();
				else
                    monitor_start = createmon();
			}
            for (i = 0, monitor = monitor_start; i < num_screens && monitor; monitor = monitor->next, i++)
                if (i >= num_mons
                || (unique[i].x_org != monitor->mon_x || unique[i].y_org != monitor->mon_y
                    || unique[i].width != monitor->mon_width || unique[i].height != monitor->mon_height))
				{
					dirty = 1;
                    monitor->num = i;
                    monitor->mon_x = monitor->window_x = unique[i].x_org;
                    monitor->mon_y = monitor->window_y = unique[i].y_org;
                    monitor->mon_width = monitor->window_width = unique[i].width;
                    monitor->mon_height = monitor->window_height = unique[i].height;
                    updatebarpos(monitor);
				}
		} else {
			/* less monitors available nn < n */
            for (i = num_screens; i < num_mons; i++) {
                for (monitor = monitor_start; monitor && monitor->next; monitor = monitor->next);
                while (monitor->clients) {
					dirty = 1;
                    client = monitor->clients;
                    monitor->clients = client->next;
                    detachstack(client);
                    client->mon = monitor_start;
                    attach(client);
                    attachstack(client);
				}
                if (monitor == selected_monitor)
                    selected_monitor = monitor_start;
                cleanupmon(monitor);
			}
		}
		free(unique);
	} else
#endif /* XINERAMA */
	/* default monitor setup */
	{
        if (!monitor_start)
            monitor_start = createmon();
        if (monitor_start->mon_width != sw || monitor_start->mon_height != sh) {
			dirty = 1;
            monitor_start->mon_width = monitor_start->window_width = sw;
            monitor_start->mon_height = monitor_start->window_height = sh;
            updatebarpos(monitor_start);
		}
	}
	if (dirty) {
        selected_monitor = monitor_start;
        selected_monitor = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
			   == XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *client)
{
	long msize;
	XSizeHints size;

    if (!XGetWMNormalHints(dpy, client->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
        client->basew = size.base_width;
        client->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
        client->basew = size.min_width;
        client->baseh = size.min_height;
	} else
        client->basew = client->baseh = 0;
	if (size.flags & PResizeInc) {
        client->incw = size.width_inc;
        client->inch = size.height_inc;
	} else
        client->incw = client->inch = 0;
	if (size.flags & PMaxSize) {
        client->maxw = size.max_width;
        client->maxh = size.max_height;
	} else
        client->maxw = client->maxh = 0;
	if (size.flags & PMinSize) {
        client->minw = size.min_width;
        client->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
        client->minw = size.base_width;
        client->minh = size.base_height;
	} else
        client->minw = client->minh = 0;
	if (size.flags & PAspect) {
        client->mina = (float)size.min_aspect.y / size.min_aspect.x;
        client->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
        client->maxa = client->mina = 0.0;
    client->isfixed = (client->maxw && client->minw && client->maxh && client->minh
                 && client->maxw == client->minw && client->maxh == client->minh);
}

void
updatetitle(Client *client)
{
    if (!gettextprop(client->win, netatom[NetWMName], client->name, sizeof client->name))
        gettextprop(client->win, XA_WM_NAME, client->name, sizeof client->name);
    if (client->name[0] == '\0') /* hack to mark broken clients */
        strcpy(client->name, broken);
}

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "dwm-"VERSION);
    drawbar(selected_monitor);
}

void
updatewindowtype(Client *client)
{
    Atom state = getatomprop(client, netatom[NetWMState]);
    Atom wtype = getatomprop(client, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
        setfullscreen(client, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
        client->isfloating = 1;
}

void
updatewmhints(Client *client)
{
	XWMHints *wmh;

    if ((wmh = XGetWMHints(dpy, client->win))) {
        if (client == selected_monitor->selected_client && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
            XSetWMHints(dpy, client->win, wmh);
		} else
            client->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
            client->neverfocus = !wmh->input;
		else
            client->neverfocus = 0;
		XFree(wmh);
	}
}

void
view(const Arg *arg)
{
    if ((arg->ui & TAGMASK) == selected_monitor->tagset[selected_monitor->seltags])
		return;
    selected_monitor->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK)
        selected_monitor->tagset[selected_monitor->seltags] = arg->ui & TAGMASK;
	focus(NULL);
    arrange(selected_monitor);
}

/* bring mouse pointer to client */
void
warp(const Client *client)
{
    int x, y;

    if (!client)
    {
        XWarpPointer(dpy, None, root, 0, 0, 0, 0, selected_monitor->window_x + selected_monitor->window_width/2, selected_monitor->window_y + selected_monitor->window_height/2);
        return;
    }

//    Atom wtype = getatomprop((Client *)client, netatom[NetWMWindowType]);
//    if (wtype == netatom[NetWMWindowTypeDialog] ||
//        wtype == netatom[NetWMWindowTypeNotification])
//        return;

    if (!getrootptr(&x, &y) ||
        (x > client->x_pos - client->bw &&
         y > client->y_pos - client->bw &&
         x < client->x_pos + client->width + client->bw*2 &&
         y < client->y_pos + client->height + client->bw*2) ||
        (y > client->mon->bar_y && y < client->mon->bar_y + bh) ||
        (client->mon->topbar && !y))
        return;

    XWarpPointer(dpy, None, client->win, 0, 0, 0, 0, client->width / 2, client->height / 2);
}


Client *
wintoclient(Window window)
{
    Client *client;
    Monitor *monitor;

    for (monitor = monitor_start; monitor; monitor = monitor->next)
        for (client = monitor->clients; client; client = client->next)
            if (client->win == window)
                return client;
	return NULL;
}

Monitor *
wintomon(Window window)
{
    int x_pos, y_pos;
    Client *client;
    Monitor *monitor;

    if (window == root && getrootptr(&x_pos, &y_pos))
        return recttomon(x_pos, y_pos, 1, 1);
    for (monitor = monitor_start; monitor; monitor = monitor->next)
        if (window == monitor->barwin)
            return monitor;
    if ((client = wintoclient(window)))
        return client->mon;
    return selected_monitor;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit.  */
int
xerror(Display *dpy, XErrorEvent *error_event)
{
    if (error_event->error_code == BadWindow
    || (error_event->request_code == X_SetInputFocus && error_event->error_code == BadMatch)
    || (error_event->request_code == X_PolyText8 && error_event->error_code == BadDrawable)
    || (error_event->request_code == X_PolyFillRectangle && error_event->error_code == BadDrawable)
    || (error_event->request_code == X_PolySegment && error_event->error_code == BadDrawable)
    || (error_event->request_code == X_ConfigureWindow && error_event->error_code == BadMatch)
    || (error_event->request_code == X_GrabButton && error_event->error_code == BadAccess)
    || (error_event->request_code == X_GrabKey && error_event->error_code == BadAccess)
    || (error_event->request_code == X_CopyArea && error_event->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
            error_event->request_code, error_event->error_code);
    return xerrorxlib(dpy, error_event); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *error_event)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *error_event)
{
	die("dwm: another window manager is already running\n");
	return -1;
}

void
zoom(const Arg *arg)
{
    Client *client = selected_monitor->selected_client;

    if (!selected_monitor->lt[selected_monitor->sellt]->arrange
    || (selected_monitor->selected_client && selected_monitor->selected_client->isfloating))
		return;
    if (client == nexttiled(selected_monitor->clients))
        if (!client || !(client = nexttiled(client->next)))
			return;
    pop(client);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION "\n");
	else if (argc != 1)
		die("usage: dwm [-v]\n");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display\n");
	checkotherwm();
	setup();
	scan();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
