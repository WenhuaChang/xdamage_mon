#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>


static int damage_event, damage_error;
static Display *dpy;
static int scr;


static void
usage (const char *prgname)
{
	fprintf (stderr, "%s -i window_id\n", prgname);
	exit (1);
}


int
main (int argc, char **argv)
{
	char *display = NULL;
	Window watchWindow = None;
	XEvent ev;
	Damage damage;
	int o;

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

	if (watchWindow == None) {
		fprintf (stderr, "Please specify windows to monitor\n");
		usage (argv[0]);
	}

	dpy = XOpenDisplay (display);

	if (!dpy) {
		fprintf (stderr, "Can't open display\n");
		exit (1);
	}

	scr = DefaultScreen (dpy);

	if (!XDamageQueryExtension (dpy, &damage_event, &damage_error)) {
		fprintf (stderr, "No damage extension\n");
		exit (1);
	}

	damage = XDamageCreate (dpy, watchWindow, XDamageReportNonEmpty);

	for (;;) {

		XNextEvent (dpy, &ev);

		if (ev.type == damage_event + XDamageNotify) {
			XRectangle *rectlist;
			XserverRegion parts;
			int howmany, i;

			parts = XFixesCreateRegion (dpy, NULL, 0);
			XDamageSubtract (dpy, damage, None, parts);
			rectlist = XFixesFetchRegion (dpy, parts, &howmany);

			for (i = 0; i < howmany; ++i) {

				XRectangle *r = rectlist + i;
				printf ("[%d] %d %d %d %d\n", i, r->x, r->y, r->width, r->height);
			}
		}
	}

	XDamageDestroy (dpy, damage);
	XCloseDisplay (dpy);
	return 0;
}

