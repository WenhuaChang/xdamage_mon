#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>

typedef struct _win {
	struct _win *next;
	Window id;
	Damage damage;
	XWindowAttributes a;
} WIN;


static int damage_event, damage_error;
static Display *dpy;
static int scr;
static int scr_width;
static int scr_height;
static Window root;
static WIN *list;
static XserverRegion allDamage;

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
	WIN *new = malloc (sizeof (WIN));

	if (!XGetWindowAttributes (dpy, id, &new->a))
		goto fail;

	if (new->a.class == InputOnly)
		goto fail;

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

	return;

fail:
	free (new);
	return;
}

static void
draw_rect (Window win, XRectangle *r)
{
	Pixmap	 pixmap;
	Picture	 picture;
	XRenderColor color;
	XRenderPictFormat *pictureFormat;
	XGCValues gcv;
	GC gc;

    	gcv.graphics_exposures = False;
	gc = XCreateGC (dpy, win, GCGraphicsExposures, &gcv);
	pixmap  = XCreatePixmap (dpy, win, 640, 480, 24);
	XFillRectangle (dpy, pixmap, gc, 0, 0, 640, 480);

	pictureFormat = XRenderFindStandardFormat (dpy, PictStandardRGB24);
	picture = XRenderCreatePicture (dpy, pixmap, pictureFormat, 0, NULL);

	color.red   = 0x0000;
	color.green = 0x0000;
	color.blue  = 0xff00;

	XRenderFillRectangle (dpy,
			PictOpSrc,
			picture,
			&color,
			r->x, r->y,
			r->width,
			r->height);

	XCopyArea (dpy, pixmap, win, gc, 0, 0, 640, 480, 0, 0);
	XRenderFreePicture (dpy, picture);
	XFreePixmap (dpy, pixmap);
	XFreeGC (dpy, gc);
}

int
main (int argc, char **argv)
{
	char *display = NULL;
	Window watchWindow = None;
	Window wsta = None;
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
	scr_width = XDisplayWidth (dpy, scr);
	scr_height = XDisplayHeight (dpy, scr);
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
	
	wsta = XCreateSimpleWindow (dpy, root, 0, 0, 640, 480, 0, 0, 0x0);
    	XMapWindow (dpy, wsta);

	for (;;) {
	
		while (XPending (dpy)) {

			XNextEvent (dpy, &ev);
			if (ev.type == damage_event + XDamageNotify) {
				XDamageNotifyEvent *de = (XDamageNotifyEvent *) &ev;
				XserverRegion parts;
				WIN *win;

				win = find_win (list, de->drawable);
				parts = XFixesCreateRegion (dpy, NULL, 0);
				XDamageSubtract (dpy, win->damage, None, parts);
				XFixesTranslateRegion (dpy, parts, win->a.x + win->a.border_width, win->a.y + win->a.border_width);
				if (allDamage) {
					XFixesUnionRegion (dpy, allDamage, allDamage, parts);
					XFixesDestroyRegion (dpy, parts);
				}
				else
					allDamage = parts;
			}
		}

		if (allDamage) {
			XRectangle *rectlist;
			int howmany, i;
			
			rectlist = XFixesFetchRegion (dpy, allDamage, &howmany);
			for (i = 0; i < howmany; ++i) {
				XRectangle *r = rectlist + i;
				fprintf (stderr, "[%d] %d %d %d %d\n", i, r->x, r->y, r->width, r->height);
				if (wsta) {
					draw_rect (wsta, r);
				}
			}
			
			XFixesDestroyRegion (dpy, allDamage);
			allDamage = None;
		}

	}

	free_list();
	XDestroyWindow (dpy, wsta);
	XCloseDisplay (dpy);
	return 0;
}

