#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>

typedef struct _win {
	struct _win *next;
	Window id;
	Damage damage;
} WIN;


static int damage_event, damage_error;
static Display *dpy;
static int scr;
static Window root;
static WIN *list;

static void
usage (const char *prgname)
{
	fprintf (stderr, "%s -i window_id\n", prgname);
	exit (1);
}

static WIN *
find_win (WIN *list, Window id)
{
	WIN *w;

	for (w = list; w; w = w->next)
		if (w->id == id)
			return w;
	return NULL;
}

static void
free_list (void)
{
	WIN *w;

	for (w = list; w; w = w->next) {
		if (w->damage != None) {
			XDamageDestroy (dpy, w->damage);
			w->damage = None;
		}
	}

	while ((w = list->next) != NULL) {
		list->next = w->next;
		free (w);	
	}

	free (list);
}


static void
add_list (Window id)
{
	XWindowAttributes a;

	if (!XGetWindowAttributes (dpy, id, &a))
		return;

	if (a.class == InputOnly)
		return;

	WIN *new = malloc (sizeof (WIN));

	new->id = id;
#ifdef DEBUG
	printf ("serial %lu\n", NextRequest (dpy));
#endif
	new->damage = XDamageCreate (dpy, new->id, XDamageReportNonEmpty);
	new->next = NULL;

	if (!list) {
		list = new;
	} else {
		
		WIN *p = list;

		while (p->next) {
			p = p->next;	
		}

		p->next = new;
	}
}

int
main (int argc, char **argv)
{
	char *display = NULL;
	Window watchWindow = None;
	Window root_return, parent_return;
	Window *children = NULL;
	XEvent ev;
        unsigned int nchildren;
	int o, i;

	while ((o = getopt (argc, argv, "i:")) != -1) {

		switch (o) {
		case 'i':
			watchWindow = strtol (optarg, NULL, 0);
			break;
		default:
			usage (argv[0]);
			break;
		}
	}

	dpy = XOpenDisplay (display);

	if (!dpy) {
		fprintf (stderr, "Can't open display\n");
		exit (1);
	}

	scr = DefaultScreen (dpy);
    	root = RootWindow (dpy, scr);

	if (!XDamageQueryExtension (dpy, &damage_event, &damage_error)) {
		fprintf (stderr, "No damage extension\n");
		exit (1);
	}


	if (watchWindow == None) {
		XQueryTree (dpy, root, &root_return, &parent_return, &children, &nchildren);
		for (i = 0; i < nchildren; i++) {
			add_list (children[i]);
		}
		XFree (children);
	} else {
		add_list (watchWindow);
	}
	

	for (;;) {

		XNextEvent (dpy, &ev);

		if (ev.type == damage_event + XDamageNotify) {
			XDamageNotifyEvent *de = (XDamageNotifyEvent *) &ev;
			XRectangle *rectlist;
			XserverRegion parts;
			WIN *win;
			int howmany, i;

			win = find_win (list, de->drawable);

			parts = XFixesCreateRegion (dpy, NULL, 0);
			XDamageSubtract (dpy, win->damage, None, parts);
			rectlist = XFixesFetchRegion (dpy, parts, &howmany);

			for (i = 0; i < howmany; ++i) {

				XRectangle *r = rectlist + i;
				fprintf (stderr, "<0x%x>[%d] %d %d %d %d\n",
					(unsigned int)win->id, i, r->x, r->y, r->width, r->height);
			}
		}
	}

	free_list();
	XCloseDisplay (dpy);
	return 0;
}

