/*
 * $Id: xbistat.cc,v 1.1 2000/08/29 21:03:44 burghart Exp $
 *
 * Copyright (C) 2000
 * Binet Incorporated 
 *       and 
 * University Corporation for Atmospheric Research
 * 
 * All rights reserved
 *
 * No part of this work covered by the copyrights herein may be reproduced
 * or used in any form or by any means -- graphic, electronic, or mechanical,
 * including photocopying, recording, taping, or information storage and
 * retrieval systems -- without permission of the copyright owners.
 *
 * This software and any accompanying written materials are provided "as is"
 * without warranty of any kind.
 */

# include <string>
# include <cmath>
# include <cstdlib>
# include <fcntl.h>
# include <cstdio>
# include <unistd.h>
# include <csignal>
# include <cerrno>
# include <getopt.h>
# include <cstring>
# include <ctime>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netcdf.h>

# include <X11/X.h>
# include <X11/StringDefs.h>
# include <X11/Intrinsic.h>
# include <X11/Xaw/Simple.h>

# include "DisplayInfo.hh"	/* our window size info */
# include "NCRadarFile.hh"
# include "datanotice.hh"

/*
 * X stuff
 */
XtAppContext	Appc;
Widget		Gw;	/* our graphics widget */
Display		*Disp;
Pixmap		Pm;
Font		SmallFont, MedFont, BigFont;
Colormap	Cmap;

/*
 * Our graphics context.  If you tweak anything other than the foreground 
 * color or font, set it back to the default value when you're done.
 */
GC	Gc;

/*
 * A separate GC clipped for the data region
 */
GC	DataGc;

/*
 * convenience macros
 */
# define NINT(val)	((int)((val) + (((val) < 0.0) ? -0.5 : 0.5)))
# define DEG_TO_RAD(x)	((x)*0.017453293)
# define RAD_TO_DEG(x)	((x)*57.29577951)
# define _c_	(2.998e8)

/*
 * Position of the center of the data area in km w.r.t. the radar, and
 * the plot scaling factor.
 */
float	CenterX = 0.0, CenterY = 0.0;
float	PixPerKm = 2.5;

/*
 * VECSPACING: spacing for vector grid, in pixels
 * FULLVECT: vector scaling factor, in m/s per VECSPACING pixels
 */
const int	VECSPACING = 15;
const float	FULLVECT = 10.0;

/*
 * Wind vector structure
 */
typedef struct _displayvector
{
    float	u, v;		/* wind u and v, m/s	*/
    int		xbase, ybase;	/* base of the vector	*/
    int		xtip, ytip;	/* tip of the vector	*/
    int		xbarb1, ybarb1;	/* first barb endpoint	*/
    int		xbarb2, ybarb2;	/* second barb endpoint	*/
} displayvector;

/*
 * Array of wind vectors, spaced every VECSPACING pixels across the data
 * display
 */
displayvector* WVec = 0;

/*
 * Range ring info
 */
float SpokeStep = 30.0;	/* degrees */
float RingStep = 10.0;	/* km */

/*
 * Input source info
 */
char		**FileNames;
int		NFiles;
int		Repeat = 0;
NCRadarFile*	NCfile = 0;


//
// Realtime operation stuff
//
int Realtime = 0;
int		InSocket;
XtInputId	XInId = 0;
XtIntervalId	XTimer = 0;

/*
 * Is the display stopped?
 */
unsigned char	Stopped = FALSE;

/*
 * Array for our colors, a mask for the reserved bit plane for drawing vectors,
 * and special color identifiers
 */
Pixel		Colors[37];

# define FG_NDX		32
# define BG_NDX		33
# define RANGERING_NDX	34
# define HILIGHT_NDX	35
# define VECTOR_NDX	36

# define FG_COLOR	"black"
# define BG_COLOR	"white"
# define RR_COLOR	"gray50"
# define HILIGHT_COLOR	"red"
# define VECTOR_COLOR	"black"

/*
 * Enable/disable raster or vector portion of the plot
 */
int	ShowRaster = TRUE;
int	ShowVectors = TRUE;

/*
 * We can fake azimuths if necessary
 */
int FakeAzimuths = 0;

/*
 * The range for our raster plot
 */
float	Min, Max;

//
// Var indices and names
//
int RasterFld, UWindFld, VWindFld;
static char RasterFldName[32], UWindFldName[32], VWindFldName[32];
static char WindTitle[32];
static int RasterRcvrNdx, WindRcvrNdx;
static char RasterRcvr[32], WindRcvr[32];
static char RasterUnits[32], WindUnits[32];


/*
 * Scratch string
 */
char	Scratch[80];

/*
 * Color table structure, our current color table, and a few color tables.
 */
typedef struct _Ctable
{
    unsigned char	r, g, b;
} Ctable;

Ctable ReflTbl[] = 
{
    {0,0,0},		{60,60,60},	{0,69,0},	{0,101,10},
    {0,158,30},		{0,177,59},	{0,205,116},	{0,191,150},
    {0,159,206},	{8,127,219},	{28,71,232},	{56,48,222},
    {110,13,198},	{144,12,174},	{200,15,134},	{196,67,134},
    {192,100,135},	{191,104,101},	{190,108,68},	{210,136,59},
    {250,196,49},	{254,217,33},	{254,250,3},	{254,221,28},
    {254,154,88},	{254,130,64},	{254,95,5},	{249,79,8},
    {253,52,28},	{200,117,104},	{215,183,181},	{210,210,210}
};

Ctable VelTbl[] = 
{
    {255,255,255},	{254,0,254},	{253,0,254},	{248,0,254},
    {222,0,254},	{186,0,254},	{175,0,253},	{165,0,252},
    {139,0,248},	{113,1,242},	{71,19,236},	{19,55,229},
    {0,110,229},	{0,182,228},	{4,232,152},	{2,116,76},
    {125,125,125},	{226,193,133},	{217,149,49},	{238,184,31},
    {252,218,18},	{254,218,33},	{254,177,100},	{254,145,150},
    {254,131,131},	{254,108,58},	{254,93,7},	{254,86,0},
    {254,55,0},		{254,13,0},	{254,0,0},	{255,0,0}
};

Ctable	*CurTbl;	/* current color table */

/*
 * Convenient "between" function.  Return true iff a is between b and c
 * (inclusive).
 */
inline int
between (double a, double b, double c)
{
    return (((a-b)*(a-c)) <= 0.0);
}


/*
 * prototypes
 */
void	usage (const char* progname);
void	init_from_file (const char* filename);
int	init_display (int* argc, char** argv);
void	SetupRealtime (void);
void	SetupFileInput (char** srcs, int nsrcs);
void	NextBeam (XtPointer junk0, XtIntervalId* junk1);
void	RTNotification (XtPointer junk0, int* source, XtInputId* junk1);
void	DoNthBeam (int time_index);
void	annotate (Drawable d, int time_index);
void	colorbar (Drawable d);
void	beam_graphics (int time_index, float az_prev_deg, int do_raster);
void	range_rings (Drawable d, double start_ang, double extent, 
		     double start_km, double end_km);
void	incr_field_event (Widget, XEvent *, String *, Cardinal *);
void	incr_field (int step);
void	incr_wind_field_event (Widget, XEvent *, String *, Cardinal *);
void	incr_wind_field (int step);
void	toggle (Widget, XEvent *, String *, Cardinal *);
void	zoom (Widget, XEvent *, String *, Cardinal *);
void	ToggleFakeAzimuths (Widget, XEvent *, String *, Cardinal *);
void	move_center (Widget, XEvent *, String *, Cardinal *);
void	clear_request (Widget, XEvent *, String *, Cardinal *);
void	clear (const char* which);
void	finish (Widget, XEvent *, String *, Cardinal *);
void	done (void);
void	start_or_stop (Widget, XEvent *, String *, Cardinal *);
void	NextFile (void);
void	RedrawDisplay (int time_index);
void	set_vector (int x, int y, float u, float v);
void	DrawVectors (Drawable d, GC gc);
void	draw_vector (Drawable d, GC gc, displayvector* dv);
void	load_table (void);
void	DoGateVectors (XPoint pts[4], float u, float v, int g);





int main (int argc, char *argv[])
{
    char fname[40], c;
    int	src, x, y, option_index;
    static struct option long_options[] =
    {
	{"xcenter", 1, 0, 0},
	{"ycenter", 1, 0, 0},
	{"pixels_per_km", 1, 0, 0},
	{"init_file", 1, 0, 0},
	{"repeat", 0, 0, 0},
	{"help", 0, 0, 0},
	{0, 0, 0, 0}
    };
/*
 * Get our args
 */
    while ((c = getopt_long (argc, argv, "d:x:y:p:i:rh", long_options, 
			     &option_index)) != EOF)
    {
    /*
     * Just turn a long option into the equivalent short option by using the
     * first letter of the long option name.
     */
	if (c == 0)
	    c = long_options[option_index].name[0];
    /*
     * Deal with the short options
     */
	switch (c)
	{
	  case 'x':
	    if (sscanf (optarg, "%f", &CenterX) != 1)
	    {
		fprintf (stderr, "Bad --xcenter or -x value '%s'\n", optarg);
		exit (1);
	    }
	    break;
	  case 'y':
	    if (sscanf (optarg, "%f", &CenterY) != 1)
	    {
		fprintf (stderr, "Bad --ycenter or -y value '%s'\n", optarg);
		exit (1);
	    }
	    break;
	  case 'p':
	    if (sscanf (optarg, "%f", &PixPerKm) != 1)
	    {
		fprintf (stderr, "Bad --pixels_per_km or -p value '%s'\n", 
			 optarg);
		exit (1);
	    }
	    break;
	  case 'i':
	    init_from_file (optarg);
	    break;
	  case 'r':
	    Repeat = 1;
	    break;
	  case 'h':
	    usage (argv[0]);
	    exit (0);
	    break;
	  case '?':
	  default:
	    usage (argv[0]);
	    exit (1);
	}
    }
/*
 * Remove the args we parsed out (leaving argv[0] untouched)
 */
    argc = argc - optind + 1;
    memmove (argv + 1, argv + optind, (argc - 1) * sizeof (*argv));
/*
 * Xt setup
 */
    init_display (&argc, argv);
/*
 * Finally, the optional file list
 */
    if (argc > 1)
	SetupFileInput (argv + 1, argc - 1);
    else
	SetupRealtime ();
/*
 * Zero out our wind vector array
 */
    for (x = 0; x <= RD_WIDTH; x += VECSPACING)
	for (y = 0; y <= RD_HEIGHT; y += VECSPACING)
	    set_vector (x, y, 0.0, 0.0);
/*
 * Initialize field info
 */
    RasterFld = -1;
    UWindFld = -1;
    VWindFld = -1;
/*
 * Update the display
 */
    CurTbl = ReflTbl;
    load_table();
    range_rings (Pm, 0.0, 360.0, 0.0, 100.0);
    RedrawDisplay (-1);
/*
 * Start the Xt loop
 */
    XtAppMainLoop (Appc);
}


void 
usage (const char* progname)
{
    fprintf (stderr, 
	     "Usage: %s [options] [<data_file> ...]\n", 
	     progname);
    fprintf (stderr, "  Options:\n");
    fprintf (stderr, "	-x, --xcenter <val>\n");
    fprintf (stderr, "	-y, --ycenter <val>\n");
    fprintf (stderr, "	-p, --pixels_per_km <val>\n");
    fprintf (stderr, "	-i, --init_file <file>\n");
    fprintf (stderr, "  -r, --repeat\n");
    fprintf (stderr, "  -h, --help\n");
}



void
init_from_file (const char* filename)
{
    char line[128];
    FILE *infile = fopen (filename, "r");

    if (! infile)
    {
	fprintf (stderr, "Error (%s) opening init file %s\n", strerror (errno),
		 filename);
	exit (1);
    }

    while (fgets (line, sizeof (line), infile))
    {
	if (sscanf (line, "xcenter %f", &CenterX) == 1)
	    continue;
	else if (sscanf (line, "ycenter %f", &CenterY) == 1)
	    continue;
	else if (sscanf (line, "pixels_per_km %f", &PixPerKm) == 1)
	    continue;
	else
	{
	    fprintf (stderr, "Bad init file line: %s\n", line);
	    exit (1);
	}
    }

    fclose (infile);
}



void
SetupRealtime ()
//
// Set up to be controlled by real-time notification from the BistaticHub
// program
//
{
    struct sockaddr_in saddr;
    struct hostent* host;
//
// Open an incoming UDP socket at port XBDATANOTICE_PORT, for data
// notification messages
//
    memset ((char*)&saddr, 0, sizeof( saddr ));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons( XBDATANOTICE_PORT );

    if ((InSocket = socket (AF_INET, SOCK_DGRAM, 0)) < 0 ||
	bind (InSocket, (struct sockaddr*)&saddr, sizeof (saddr)))
    {
	fprintf (stderr, 
		 "Error creating or configuring notification socket\n");
	exit (1);
    }

    XInId = XtAppAddInput (Appc, InSocket, (XtPointer) XtInputReadMask,
			   RTNotification, NULL);

    NFiles = 1;
    FileNames = new char*[1];
    FileNames[0] = 0;

    Realtime = 1;
}



void
SetupFileInput (char** srcs, int nsrcs)
//
// Set up for user-specified file input
//
{
    int s;

    NFiles = nsrcs;
    FileNames = new char*[NFiles];
    for (s = 0; s < NFiles; s++)
	FileNames[s] = srcs[s];

    Realtime = 0;
//
// Start the timer loop for displaying beams
//
    if (! Stopped)
	XTimer = XtAppAddTimeOut (Appc, 1, NextBeam, NULL);
}




void
RTNotification (XtPointer junk0, int* source, XtInputId* junk1)
/*
 * Deal with a realtime notification
 */
{
    char buf[256];
    int fd = *source;
    int len = read (fd, buf, sizeof (buf));
    buf[len] = '\0';
//
// Option 1: "Filename: <name>"
//
    if (! strncmp (buf, "Filename", 8))
    {
    //
    // If the filename is different from our current file, stash the
    // new file name, and let NextFile do its stuff.
    //
	char* fname = buf + 10;
	if (! FileNames[0] || strcmp (fname, FileNames[0]))
	{
	    delete[] FileNames[0];
	    FileNames[0] = new char[strlen (fname) + 1];
	    strcpy (FileNames[0], fname);
	    NextFile();
	}
    }
//
// Option 2: "<time_index>"
// This is a notification that a new time index is available in the current
// file.  Pass the new index on to new_beam().
//
    else
    {
	int index = -1;
	sscanf (buf, "%d", &index);
	if (index < 0)
	    fprintf (stderr, "Bad time index '%s' received\n", buf);
	else if (FileNames[0])
	    DoNthBeam (index);
    }
}



void
NextBeam (XtPointer junk0, XtIntervalId* junk1)
//
// Display the next beam from our current file
//
{
    static int curbeam;
    static int ntimes = -1;
    const int beam_interval = 5;	// msec

    if (ntimes < 0 || ++curbeam >= ntimes)
    {
	NextFile();
	ntimes = NCfile->NumTimes();
	curbeam = 0;
    }
    
    DoNthBeam (curbeam);
    XTimer = XtAppAddTimeOut (Appc, beam_interval, NextBeam, NULL);
}



void
NextFile ()
/*
 * Open the next data file
 */
{
    static int current_fndx = -1;
//
// Close out our previous input source, if any
//
    delete NCfile;
//
// Increment current_fndx and quit (or repeat) if we hit the end of the
// source list.
//
    if (++current_fndx == NFiles)
    {
	if (Repeat || Realtime)
	    current_fndx = 0;
	else
	    done ();

	if (Repeat)
	{
	/*
	 * Clear before repeating
	 */
	    clear ("raster");
	    clear ("vector");
	}
    }
//
// Open the current_fndx file
//
    NCfile = new NCRadarFile( FileNames[current_fndx] );
    if (! NCfile->IsOK())
    {
	if (NFiles > 1)
	{
	    fprintf( stderr, "Skipping file %s\n", FileNames[current_fndx] );
	    NextFile();
	}
	else
	    exit( 1 );
    }
//
// Set up fields again, trying to keep the ones we were using for
// the previous file (if any)
//
    if (RasterFld >= 0)
    {
	RasterFld = NCfile->ProductIndex( RasterFldName );
	if (RasterFld < 0)
	    fprintf( stderr, "Field %s does not exist in file %s;  %s",
		     RasterFldName, FileNames[current_fndx], 
		     "changing field");
    }

    if (UWindFld >= 0)
    {
	UWindFld = NCfile->ProductIndex( UWindFldName );
	VWindFld = NCfile->ProductIndex( VWindFldName );
	if (UWindFld < 0 || VWindFld < 0)
	{
	    fprintf( stderr, "Wind field %s does not exist in file %s.\n",
		     UWindFldName, FileNames[current_fndx] );
	    fprintf( stderr, "Changing field.\n" );
	}
    }
//
// If we still have no raster or wind fields, try to find something in the
// file to display
//
    if (RasterFld < 0)
	incr_field( 1 );
    if (UWindFld < 0)
	incr_wind_field( 1 );
}




void
DoNthBeam (int time_index)
/*
 * Read the time_index'th beam from our current source and display it
 */
{
    static float az_prev = -999.0;
    static int bmcount = 0;
/*
 * Otherwise, draw the beam: raster first, then vectors
 * We have to draw separately since the raster and vector fields may be
 * in different gate geometries.
 */
    float az = NCfile->Azimuth(time_index);
    if (az_prev == -999.0 || fabs (az_prev - az) > 5.0)
	az_prev = az + 0.5;

    if (RasterFld >= 0)
	beam_graphics (time_index, az_prev, 1);

    if (UWindFld >= 0 && VWindFld >= 0)
	beam_graphics (time_index, az_prev, 0);

    az_prev = az;
/*
 * Redraw the display on the screen every 5th beam
 */
    if (! (++bmcount % 5))
    	RedrawDisplay (time_index);
}


void
RedrawDisplay (int time_index)
{
/*
 * Copy our raster pixmap into an intermediate pixmap, draw the vectors on top,
 * add annotation, and copy it to the screen.
 */
    static Pixmap inter_pm = 0;

    if (! inter_pm)
    {
	int d = DefaultDepth (Disp, 0);
	inter_pm = XCreatePixmap (Disp, XtWindow (Gw), D_WIDTH, D_HEIGHT, d);
    }

    if (ShowRaster)
	XCopyArea (Disp, Pm, inter_pm, Gc, 0, 0, D_WIDTH, D_HEIGHT, 0, 0);
    else
    {
	XSetForeground (Disp, Gc, Colors[BG_NDX]);
	XFillRectangle (Disp, inter_pm, Gc, RD_LEFT, RD_TOP, RD_WIDTH + 1, 
			RD_HEIGHT + 1);
	range_rings (inter_pm, 0.0, 360.0, 0.0, 100.0);
    }
    
    XSetForeground (Disp, DataGc, Colors[VECTOR_NDX]);
    if (ShowVectors)
	DrawVectors (inter_pm, DataGc);

    annotate (inter_pm, time_index);
    XCopyArea (Disp, inter_pm, XtWindow (Gw), Gc, 0, 0, D_WIDTH, D_HEIGHT, 
	       0, 0);
}


    

void
range_rings (Drawable d, double start_ang, double extent, double start_km, 
	     double end_km)
/*
 * Draw range arcs every 10 km, starting at "start_ang" degrees and extending
 * "extent" degrees clockwise.  Limit distances drawn from start_km to end_km.
 */
{
    float deg;
    float km, spokelen;
    float start, stop;
    int basex = (int)(RD_XCENTER - (CenterX * PixPerKm));
    int basey = (int)(RD_YCENTER + (CenterY * PixPerKm));
    int	radius;
/*
 * Set our color, and leave vectors alone
 */
    XSetForeground (Disp, DataGc, Colors[RANGERING_NDX]);
    XSetLineAttributes (Disp, DataGc, 2, LineSolid, CapButt, JoinMiter);
/*
 * Set up so we're in positive space and going clockwise.  We also add 
 * an extra 2 degrees on either side to overdraw a bit.
 */
    if (extent < 0)
    {
	start_ang += extent;
	extent *= -1;
    }

    start_ang -= 2.0;
    extent += 4.0;

    while (start_ang < 0)
	start_ang += 360.0;
/*
 * ring arcs
 */
    start = RingStep * ((int)(start_km / RingStep) + 1);
    stop = RingStep * (int)(end_km / RingStep);

    for (km = start; km <= stop; km += RingStep)
    {
	radius = (int)(km * PixPerKm);
	XDrawArc (Disp, d, DataGc, basex - radius, basey - radius, 
		  2 * radius, 2 * radius, (int)(64 * (90.0 - start_ang)), 
		  (int)(64 * -extent));
    }
/*
 * spokes
 */
    start = SpokeStep * (int)(start_ang / SpokeStep);
    stop = SpokeStep * (int)((start_ang + extent) / SpokeStep);

    if (start_km < RingStep)
	start_km = RingStep;

    for (deg = start; deg <= stop; deg += SpokeStep)
    {
	float	sine = sin (DEG_TO_RAD (90.0 - deg));
	float	cosine = cos (DEG_TO_RAD (90.0 - deg));
	
	XDrawLine (Disp, d, DataGc, 
		   (int)(basex + start_km * PixPerKm * cosine),
		   (int)(basey - start_km * PixPerKm * sine),
		   (int)(basex + end_km * PixPerKm * cosine),
		   (int)(basey - end_km * PixPerKm * sine));
    }
/*
 * Reset line width to zero
 */
    XSetLineAttributes (Disp, DataGc, 0, LineSolid, CapButt, JoinMiter);
}




int
init_display (int *argc, char **argv)
{
    Widget top;
    Arg	args[10];
    XColor c, exact;
    int	n, ncolors, depth;
    XFontStruct	*finfo;
    XGCValues	gcvals;
    XRectangle	rect;
    Pixmap stipple;
    std::string trans = "<Key>q: finish() \n\
		   <Key>Up: incr_field(1) \n\
		   <Key>Down: incr_field(-1) \n\
		   <Key>Right: incr_wind_field(1) \n\
		   <Key>Left: incr_wind_field(-1) \n\
		   :<Key>c: clear(raster) \n\
		   :<Key>C: clear(vector) \n\
		   <Key>r: toggle(raster) \n\
		   <Key>w: toggle(vector) \n\
		   <Key>space: start_or_stop() \n\
		   :<Key>z: zoom(1.2) \n\
		   :<Key>Z: zoom(2.0) \n\
		   :<Key>u: zoom(0.83333333) \n\
		   :<Key>U: zoom(0.5) \n\
		   :<Key>f: ToggleFakeAzimuths() \n\
		   <Btn1Down>,<Btn1Up>: move_center()";
    XtTranslations	ttable;
    XtActionsRec	actions[] = {
	    {String("incr_field"),	incr_field_event},
	    {String("incr_wind_field"),	incr_wind_field_event},
	    {String("start_or_stop"),	start_or_stop},
	    {String("toggle"),		toggle},
	    {String("finish"),		finish},
	    {String("clear"),		clear_request},
	    {String("zoom"),		zoom},
	    {String("ToggleFakeAzimuths"),	ToggleFakeAzimuths},
	    {String("move_center"),	move_center},
    };
/*
 * Initialize Xt
 */
    top = XtAppInitialize (&Appc, String("top"), NULL, 0, argc, argv, NULL,
			   NULL, 0);
    Disp = XtDisplay (top);
    depth = DefaultDepth (Disp, 0);
/*
 * Register actions and parse the translation table.
 */
    XtAppAddActions (Appc, actions, XtNumber (actions));
    ttable = XtParseTranslationTable (trans.c_str());
/*
 * Create a D_WIDTH x D_HEIGHT simple widget for our display
 */
    n = 0;
    XtSetArg (args[n], XtNwidth, D_WIDTH);	n++;
    XtSetArg (args[n], XtNheight, D_HEIGHT);	n++;
    XtSetArg (args[n], XtNdepth, depth);	n++;
    XtSetArg (args[n], XtNforeground, 0);	n++;
    XtSetArg (args[n], XtNtranslations, ttable);	n++;
    Gw = XtCreateManagedWidget ("graphics", simpleWidgetClass, top, args, n);

    XtRealizeWidget (top);
/*
 * Get space for our color table
 */
    ncolors = sizeof (Colors) / sizeof (Pixel);
    Cmap = DefaultColormap (Disp, 0);
    for (n = 0; n < ncolors; n++)
	Colors[n] = 0;
/*
 * Create the pixmap and generic graphics context
 */
    Pm = XCreatePixmap (Disp, XtWindow (Gw), D_WIDTH, D_HEIGHT, depth);
    stipple = XCreatePixmapFromBitmapData (Disp, Pm, String("\001\002"), 2, 2, 1,
					   0, 1);

    Gc = XCreateGC (Disp, Pm, 0, NULL);
    XSetStipple (Disp, Gc, stipple);
/*
 * A GC clipped for the data region of the display
 */
    DataGc = XCreateGC (Disp, Pm, 0, NULL);
    rect.x = 0;
    rect.y = 0;
    rect.width = RD_WIDTH;
    rect.height = RD_HEIGHT;
    XSetClipRectangles (Disp, DataGc, RD_LEFT, RD_TOP, &rect, 1, Unsorted);
    XSetStipple (Disp, DataGc, stipple);
/*
 * Get small, medium, and large fonts, using the default font for each
 * if the load fails.
 */
    if (! (finfo = XLoadQueryFont (Disp, "*-helvetica-medium-r-normal--10-*")))
    {
	fprintf (stderr, "Can't get my small font!\n");
	exit (1);
    }
    SmallFont = finfo->fid;
	
    if (! (finfo = XLoadQueryFont (Disp, "*-helvetica-medium-r-normal--14-*")))
    {
	fprintf (stderr, "Can't get my medium font!\n");
	exit (1);
    }
    MedFont = finfo->fid;

    if (! (finfo = XLoadQueryFont (Disp, "*-helvetica-bold-r-normal--20-*")))
    {
	fprintf (stderr, "Can't get my big font!\n");
	exit (1);
    }
    BigFont = finfo->fid;
/*
 * Clear the pixmaps and the widget
 */
    XAllocNamedColor (Disp, Cmap, BG_COLOR, &c, &exact);

    XSetForeground (Disp, Gc, c.pixel);
    XFillRectangle (Disp, Pm, Gc, 0, 0, D_WIDTH, D_HEIGHT);
    XFillRectangle (Disp, XtWindow (Gw), Gc, 0, 0, D_WIDTH, D_HEIGHT);

    return (1);
}



void
load_table ()
{
    int	i;
    XColor c, exact;
    static int have_table = 0;
/*
 * Release any colors we allocated previously
 */
    if (have_table)
	XFreeColors (Disp, Cmap, Colors, sizeof (Colors) / sizeof (Pixel), 0);
/*
 * Load the 32 colors in ct
 */
    for (i = 0; i < 32; i++)
    {
	c.red = CurTbl[i].r << 8;
	c.green = CurTbl[i].g << 8;
	c.blue = CurTbl[i].b << 8;
	c.flags = DoRed | DoGreen | DoBlue;

	if (! XAllocColor (Disp, Cmap, &c))
	{
	    fprintf (stderr, "Color allocation failed!\n");
	    exit (1);
	}

	Colors[i] = c.pixel;
    }
/*
 * Add the fixed colors at the top of our list
 */
    for (i = FG_NDX; i <= VECTOR_NDX; i++)
    {
	const char	*cname;

	switch (i)
	{
	  case FG_NDX:
	    cname = FG_COLOR; break;
	  case RANGERING_NDX:
	    cname = RR_COLOR; break;
	  case BG_NDX:
	    cname = BG_COLOR; break;
	  case HILIGHT_NDX:
	    cname = HILIGHT_COLOR; break;
	  case VECTOR_NDX:
	    cname = VECTOR_COLOR; break;
	}
			
	XAllocNamedColor (Disp, Cmap, cname, &c, &exact);
	Colors[i] = c.pixel;
    }

    have_table = 1;
}




void
annotate (Drawable d, int time_index)
{
/*  
 * Erase rectangle for housekeeping display 
 */
    XSetForeground (Disp, Gc, Colors[BG_NDX]);
    XFillRectangle (Disp, d, Gc, D_WIDTH-250, D_HEIGHT-60, 267, 70);
/*
 * Color bar
 */
    colorbar (d);
/*
 * Do the time-dependent stuff if we were given a good time index
 */
    if (time_index >= 0)
    {
    /* 
     * Time, az, and elev labels
     */
	XSetForeground (Disp, Gc, Colors[FG_NDX]);
	XSetFont (Disp, Gc, MedFont);

	sprintf (Scratch, "Time:");
	XDrawString (Disp, d, Gc, D_WIDTH-247, D_HEIGHT-45, Scratch, 
		     strlen (Scratch));

	sprintf (Scratch, "Azimuth:");
	XDrawString (Disp, d, Gc, D_WIDTH-247, D_HEIGHT-28, Scratch, 
		     strlen (Scratch));

	sprintf (Scratch, "Elevation:");
	XDrawString (Disp, d, Gc, D_WIDTH-247, D_HEIGHT-11, Scratch, 
		     strlen (Scratch));
    /*
     * And the values
     */
	time_t itime = (time_t)(NCfile->Time(time_index));
	strftime (Scratch, sizeof (Scratch), "%T %d %b %y", gmtime (&itime));
	XDrawString (Disp, d, Gc, D_WIDTH-140, D_HEIGHT-45, Scratch,
		     strlen(Scratch));

	if (FakeAzimuths)
	{
	    XSetForeground (Disp, Gc, Colors[HILIGHT_NDX]);

	    sprintf (Scratch, "F A K E");
	    XDrawString (Disp, d, Gc, D_WIDTH-140, D_HEIGHT-28, Scratch, 
			 strlen (Scratch));
	
	    XSetForeground (Disp, Gc, Colors[FG_NDX]);
	}
	else
	{
	    sprintf (Scratch, "%6.1f", NCfile->Azimuth(time_index));
	    XDrawString (Disp, d, Gc, D_WIDTH-140, D_HEIGHT-28, Scratch, 
			 strlen (Scratch));
	}

	sprintf (Scratch, "%6.1f", NCfile->Elevation(time_index));
	XDrawString (Disp, d, Gc, D_WIDTH-140, D_HEIGHT-11, Scratch, 
		     strlen (Scratch));
    }
/* 
 * Field name and units for the raster display and the vectors.  Blank the 
 * rectangle then draw the text.  Draw stippled as appropriate to indicate
 * that the raster or vector portion of the is enabled/disabled.
 */
    XSetForeground (Disp, Gc, Colors[BG_NDX]);
    XFillRectangle (Disp, d, Gc, RD_XCENTER-175, D_HEIGHT-60 , 350, 60);
    XSetForeground (Disp, Gc, Colors[FG_NDX]);
    XSetBackground (Disp, Gc, Colors[BG_NDX]);
    XSetFont (Disp, Gc, BigFont);

    XSetFillStyle (Disp, Gc, ShowRaster ? FillSolid : FillOpaqueStippled);
    sprintf (Scratch, "%s (%s), %s", RasterFldName, RasterRcvr, RasterUnits);
    XDrawString (Disp, d, Gc, RD_XCENTER-175+3, D_HEIGHT-37, Scratch, 
		 strlen (Scratch));

    XSetFillStyle (Disp, Gc, ShowVectors ? FillSolid : FillOpaqueStippled);
    sprintf (Scratch, "%s, %s", WindTitle, WindUnits);
    XDrawString (Disp, d, Gc, RD_XCENTER-175+3, D_HEIGHT-11, Scratch, 
		 strlen (Scratch));

    XSetFillStyle (Disp, Gc, FillSolid);
/*
 * Scale vector
 */
    {
	int xbase = D_WIDTH - 105, ybase = 10;
	int xtip = xbase - VECSPACING, ytip = ybase;

	sprintf (Scratch, "%.0f m/s", FULLVECT);
	XSetFont (Disp, Gc, MedFont);
	XDrawString (Disp, d, Gc, xbase + 5, ybase + 5, Scratch, 
		     strlen (Scratch));

	XDrawLine (Disp, d, Gc, xbase, ybase, xtip, ytip);
	XDrawLine (Disp, d, Gc, xtip, ytip, xtip + 5, ytip - 2);
	XDrawLine (Disp, d, Gc, xtip, ytip, xtip + 5, ytip + 2);
    }
/*
 * Range ring info
 */
    sprintf (Scratch, "%.0f km range rings", RingStep);
    XSetFont (Disp, Gc, MedFont);
    XDrawString (Disp, d, Gc, D_WIDTH - 150, 30, Scratch, strlen (Scratch));
/*
 * Title
 */
    XSetForeground (Disp, Gc, Colors[FG_NDX]);
    XSetFont (Disp, Gc, BigFont);

    sprintf (Scratch, "B I S T A T I C   R A D A R   D I S P L A Y");
    XDrawString (Disp, d, Gc, 50, 22, Scratch, strlen (Scratch));
/*
 * Write the message "STOPPED" using color HILIGHT_NDX or BG_NDX, 
 * depending on whether the display is stopped right now.
 */
    XSetForeground (Disp, Gc, Stopped ? Colors[HILIGHT_NDX] : Colors[BG_NDX]);
    XSetFont (Disp, Gc, BigFont);
    sprintf (Scratch, "S T O P P E D");
    XDrawString (Disp, d, Gc, 20, D_HEIGHT - 7, Scratch, strlen (Scratch));
}




/*
 * Do either the raster or vector graphics for the current beam.  For 
 * raster, we draw into pixmap Pm.  For vector, we stash the appropriate
 * stuff in the WVec array.  Actually drawing the results to the screen
 * is handled elsewhere.
 */
void
beam_graphics (int time_index, float az_prev_deg, int do_raster)
{
/*
 * For the time_index'th beam, draw the raster field if do_raster is true, 
 * otherwise wind vectors.
 *
 * Note that all local angle variables except az_deg and az_prev_deg are in
 * radians and in "classical geometry" space, i.e., 0 radians points east
 * and angles increase counterclockwise.
 */
    float *rdata = 0, *udata = 0, *vdata = 0;
    float pixpergate;
    float az, az_deg, az_prev, az_diff;
    int src = do_raster ? RasterRcvrNdx : WindRcvrNdx;
    float samptime = NCfile->GateTime(src);
    float baseline = NCfile->RangeFromXmitter(src);
    float delay = baseline / _c_;
    float traveltime;
    float left, cos_left, sin_left, right, cos_right, sin_right;
    float phi, cos_phi, cos_phi_left, cos_phi_right;
    float d_left, d_right, dprev_left, dprev_right;
    XPoint	pts[4];
    int	g, gmin, gmax;
    int side;
    float traveldist;
    int	gatestep;
/*
 * Effective pixel x and y for the xmitter
 */
    float basex = RD_XCENTER - (CenterX * PixPerKm);
    float basey = RD_YCENTER + (CenterY * PixPerKm);
/*
 * Data scale factor to use below
 */

    float	scale_to_int = 32 / (Max - Min);
/*
 * We should get the real delay at some point, but for now assume the 
 * bistatic receivers start sampling 4 gates before the first pulse arrives
 * from the transmitter.
 */
    if (src > 0)
	delay -= 4.0 * samptime;
/*
 * Get the data
 */
    int ngates;
    float badval = -32767.0;
    
    if (do_raster)
    {
	ngates = NCfile->NGates(src);
	rdata = new float[ngates];
	NCfile->GetProduct(RasterFld, time_index, rdata, ngates, badval);
    }
    else
    {
	ngates = NCfile->NGates(src);
	udata = new float[ngates];
	vdata = new float[ngates];
	NCfile->GetProduct(UWindFld, time_index, udata, ngates, 0.0);
	NCfile->GetProduct(VWindFld, time_index, vdata, ngates, 0.0);
    }
/*
 * Figure out left and right edges of the beam.
 */
    az_deg = FakeAzimuths ? 
	fmod (az_prev_deg + 1.0, 360.0) : NCfile->Azimuth(time_index);

    az = DEG_TO_RAD (90.0 - az_deg);
    az_prev = DEG_TO_RAD (90.0 - az_prev_deg);

    az_diff = az - az_prev;

    if (az_diff < -M_PI)
	az_diff += 2 * M_PI;

    if (az_diff > M_PI)
	az_diff -= 2 * M_PI;

    left = (az_diff < 0) ? az_prev : az;
    right = (az_diff < 0) ? az : az_prev;
/*
 * Keep around some trig values
 */
    cos_left = cos (left);
    cos_right = cos (right);
    sin_left = sin (left);
    sin_right = sin (right);
/*
 * phi is the angle between the beam and the radar/receiver baseline
 */
    cos_phi_left = cos (left - DEG_TO_RAD (90 - NCfile->AzFromXmitter(src)));
    cos_phi_right = cos (right - DEG_TO_RAD (90 - NCfile->AzFromXmitter(src)));
/*
 * Check the effective gate intersection of this beam with the top and bottom
 * sides of the display area and use that to set our gate limits.  We
 * arbitrarily use the left edge of the beam for these gate calculations.
 */
    gmin = 65535;
    gmax = -65535;

    for (side = 0; side < 2; side++)	/* side == 0 -> bottom edge */
    {					/* side == 1 -> top edge */
	float	dx, dy, fgate;
	float	x_to_edge, r_to_edge, cross;

	if (sin_left == 0.0)
	    break;
    /*
     * x and y distance in pixels from the xmitter to the beam intersection
     * with the side of the display.  Remember that y pixels increase downward
     * in the X Window world, so be careful with signs on dy.
     */
	dy = (side == 0) ? RD_BOTTOM - basey : RD_TOP - basey;
	dx = -dy * cos_left / sin_left;

	cross = basex + dx;	/* x pixel of the intersection */
	if (! between (cross, RD_LEFT - 10, RD_RIGHT + 10))
	    continue;
    /*
     * Xmitter and receiver distances to beam intersection with edge, in m
     */
	x_to_edge = -dy * 1000 / (PixPerKm * sin_left);
	r_to_edge = sqrt (baseline * baseline + x_to_edge * x_to_edge - 
			  2 * baseline * x_to_edge * cos_phi_left);
    /*
     * Light travel time xmitter -> edge intersection -> receiver, and
     * effective receiver gate.
     */
	traveltime = (x_to_edge + r_to_edge) / _c_;
	fgate = (traveltime - delay) / samptime;

	if (fgate < gmin)
	    gmin = (int) fgate;

	if (fgate > gmax)
	    gmax = (int) fgate;
    }
/*
 * Same for left and right edges of the display
 */
    for (side = 0; side < 2; side++)	/* side == 0 -> left display edge */
    {					/* side == 1 -> right display edge */
	float	dx, dy, fgate;
	float	x_to_edge, r_to_edge, cross;

	if (cos_left == 0.0)
	    break;
    /*
     * x and y distance in pixels from the xmitter to the beam intersection
     * with the side of the display.  Remember that y pixels increase downward
     * in the X Window world, so be careful with signs on dy.
     */
	dx = (side == 0) ? RD_LEFT - basex : RD_RIGHT - basex;
	dy = -dx * sin_left / cos_left;

	cross = basey + dy;	/* y pixel of the intersection */
	if (! between (cross, RD_BOTTOM + 10, RD_TOP - 10))
	    continue;
    /*
     * Xmitter and receiver distances to beam intersection with edge, in m
     */
	x_to_edge = dx * 1000 / (PixPerKm * cos_left);
	r_to_edge = sqrt (baseline * baseline + x_to_edge * x_to_edge - 
			  2 * baseline * x_to_edge * cos_phi_left);
    /*
     * Light travel time xmitter -> edge intersection -> receiver, and
     * effective receiver gate.
     */
	traveltime = (x_to_edge + r_to_edge) / _c_;
	fgate = (traveltime - delay) / samptime;

	if (fgate < gmin)
	    gmin = (int) fgate;

	if (fgate > gmax)
	    gmax = (int) fgate;
    }

    if (gmin < 0)
	gmin = 0;

    if (gmax > ngates - 1)
	gmax = ngates - 1;
/*
 * If pixels per gate is < 1.5, let's start skipping some of the gates.
 */
    pixpergate = PixPerKm * 0.001 * (0.5 * _c_ * samptime);
    if (pixpergate < 1.5)
	gatestep = (int)(1.5 / pixpergate);
    else
	gatestep = 1;
/*
 * Get the radial distance from the xmtr to the inner edge of our first gate
 */
    traveldist = _c_ * (gmin * samptime + delay);

    if (traveldist >= baseline && ((traveldist != 0) || (baseline != 0)))
    {
	dprev_left = .5 * 
	    ((traveldist * traveldist) - (baseline * baseline)) / 
	    (traveldist - baseline * cos_phi_left) * 0.001 * PixPerKm;

	dprev_right = .5 * 
	    ((traveldist * traveldist) - (baseline * baseline)) / 
	    (traveldist - baseline * cos_phi_right) * 0.001 * PixPerKm;
    }
    else
	dprev_left = dprev_right = 0.0;
/*
 * Loop through the gates
 */
    for (g = gmin; g < gmax + gatestep; g += gatestep)
    {
    /*
     * calculate the corners of the gate
     */
	traveldist = _c_ * ((g + gatestep) * samptime + delay);
	if (traveldist < baseline)
	    continue;
	  
	d_left = 0.5 * ((traveldist * traveldist) - (baseline * baseline)) / 
	    (traveldist - baseline * cos_phi_left) * 0.001 * PixPerKm;
	d_right = 0.5 * ((traveldist * traveldist) - (baseline * baseline)) / 
	    (traveldist - baseline * cos_phi_right) * 0.001 * PixPerKm;

	pts[0].x = (short)(basex + dprev_right * cos_right);
	pts[0].y = (short)(basey - dprev_right * sin_right);
	pts[1].x = (short)(basex + dprev_left * cos_left);
	pts[1].y = (short)(basey - dprev_left * sin_left);
	pts[2].x = (short)(basex + d_left * cos_left);
	pts[2].y = (short)(basey - d_left * sin_left);
	pts[3].x = (short)(basex + d_right * cos_right);
	pts[3].y = (short)(basey - d_right * sin_right);

	dprev_left = d_left;
	dprev_right = d_right;
	if (do_raster)
	{
	/*
	 * Get the raster value at this gate and convert it to a value in the
	 * range 0-31.
	 */
	    int ival;
	    
	    if (rdata[g] == badval)
		ival = BG_NDX;
	    else
	    {
		float fval = (rdata[g] - Min) * scale_to_int;
		ival = (fval >= 0.0 && fval < 32.0) ? (int)fval : BG_NDX;
	    }
	/*
	 * Draw the gate
	 */
	    XSetForeground (Disp, DataGc, Colors[ival]);
	    XFillPolygon (Disp, Pm, DataGc, pts, 4, Convex, CoordModeOrigin);
	}
	else
	{
	/*
	 * Stash the vector info into our array
	 */
	    DoGateVectors (pts, udata[g], vdata[g], g);
	}
    }

    delete[] rdata;
    delete[] udata;
    delete[] vdata;
    
    range_rings (Pm, az_deg, -RAD_TO_DEG(az_diff), 0.0, 100.0);
}



void
DoGateVectors (XPoint pts[4], float u, float v, int g)
{
    int i, x, xlo, xhi;
    int	gx_min = 65535, gx_max = -65535;
    float slope[4], intercept[4];
/*
 * Loop through the four sides of the gate, getting slope and intercept of
 * the four segments.  Also get the min and max x pixel values for the gate.
 */
    for (i = 0; i < 4; i++)
    {
	int next = (i < 3) ? i + 1 : 0;
    /*
     * Slope and intercept for the line containing this segment.
     */
	if (pts[i].x == pts[next].x)
	/*
	 * To make further processing simpler, we give vertical segments a 
	 * big but finite slope.
	 */
	    slope[i] = 1.0e6;
	else
	    slope[i] = (float)(pts[next].y - pts[i].y) / 
		(float)(pts[next].x - pts[i].x);

	intercept[i] = pts[i].y - slope[i] * pts[i].x;
    /*
     * Min and max x
     */
	if (pts[i].x < gx_min)
	    gx_min = pts[i].x;
	if (pts[i].x > gx_max)
	    gx_max = pts[i].x;
    }
/*
 * Adjust into units relative to just the radar display
 */
    if (gx_min < 0)
	gx_min = 0;

    if (gx_max > D_WIDTH)
	gx_max = D_WIDTH;
/*
 * Loop within our x gate limits, in intervals of VECSPACING
 */
    xlo = VECSPACING * ((gx_min / VECSPACING) + 1);
    xhi = VECSPACING * (gx_max / VECSPACING);

    for (x = xlo; x <= xhi; x += VECSPACING)
    {
	int	y, ylo = 65535, yhi = -65535;
    /*
     * Loop through the segments defining the gate
     */
	for (i = 0; i < 4; i++)
	{
	    int next = (i < 3) ? i + 1 : 0;
	    float ycross;
	/*
	 * Find where the current x crosses the line containing this segment.
	 * Just go on if it crosses outside of the segment.  Otherwise, use 
	 * the value to determine the y limits of our gate at this x.
	 */
	    ycross = slope[i] * x + intercept[i];
	    if (! between (ycross, pts[i].y, pts[next].y))
		continue;

	    if (ycross < ylo)
		ylo = (int) ycross;
	    if (ycross > yhi)
		yhi = (int) ycross;
	}
    /*
     * Make sure we stay in the display area
     */
	if (ylo < 0)
	    ylo = 0;

	if (yhi > D_HEIGHT)
	    yhi = D_HEIGHT;
    /*
     * Now we can scan from ylo to yhi, in intervals of VECSPACING, and
     * draw vectors.
     */
	ylo = VECSPACING * ((ylo / VECSPACING) + 1);
	yhi = VECSPACING * (yhi / VECSPACING);

	for (y = ylo; y <= yhi; y += VECSPACING)
	    set_vector (x, y, u, v);
    }
}

	
	

void
colorbar (Drawable d)
{
    int	i, boxheight = 15, boxwidth = 8;

    XSetFont (Disp, Gc, SmallFont);
/*
 * Blank the old labels
 */
    XSetForeground (Disp, Gc, Colors[BG_NDX]);
    XFillRectangle (Disp, d, Gc, 2, 20, boxwidth + 40, 32 * boxheight + 20);
/*
 * New color bar
 */
    for (i = 0; i <= 32; i++)
    {
	int ystart = 30 + boxheight * (32 - i);
    /*
     * Color box
     */
	if (i != 32)
	{
	    XSetForeground (Disp, Gc, Colors[i]);
	    XFillRectangle (Disp, d, Gc, 2, ystart - boxheight, boxwidth, 
			    boxheight + 1);
	}
    /*
     * Line and label
     */
	XSetForeground (Disp, Gc, Colors[FG_NDX]);
	XDrawLine (Disp, d, Gc, 2, ystart, boxwidth + 4, ystart);

	sprintf (Scratch, "%.1f", Min + i * (Max - Min) / 32.0);
	XDrawString (Disp, d, Gc, 2 + boxwidth + 8, ystart + 4, 
		     Scratch, strlen (Scratch));
    }
}



void
finish (Widget w, XEvent* ev, String* s, Cardinal* val)
/*
 * Xt translation table routine to shut down
 */
{
    done ();
}



void
done (void)
{
    XtDestroyApplicationContext (Appc);
    exit (0);
}




void
incr_field_event (Widget w, XEvent* ev, String* param, Cardinal* nparam)
/*
 * Xt translation table routine to switch fields
 */
{
    int	step = atoi (param[0]);
    incr_field (step);
/*
 * Update the screen
 */
    RedrawDisplay (-1);
}



void
incr_field (int step)
{
    int nproducts = NCfile->NumProducts();

    if (! nproducts)
    {
	fprintf( stderr, "No products in file %s\n", NCfile->FileName() );
	return;
    }

    RasterFld += step;

    while (RasterFld < 0)
	RasterFld += nproducts;

    while (RasterFld >= nproducts)
	RasterFld -= nproducts;

    strcpy (RasterFldName, NCfile->ProductName(RasterFld));
    NCfile->GetProductDetails(RasterFld, &RasterRcvrNdx, 0, RasterUnits);
    strcpy (RasterRcvr, NCfile->SystemName(RasterRcvrNdx));
/*
 * Big kluge for setting display limits and color table...
 */
    if (! strncmp (RasterFldName, "DZ", 2))
    {
	Min = -20.0;
	Max = 60.0;
	CurTbl = ReflTbl;
	load_table();
    }
    else if (! strncmp (RasterFldName, "DM", 2))
    {
	Min = -130.0;
	Max = -30.0;
	CurTbl = ReflTbl;
	load_table();
    }
    else if (! strncmp (RasterFldName, "NCP", 3))
    {
	Min = 0.0;
	Max = 1.0;
	CurTbl = ReflTbl;
	load_table();
    }
    else
    {
	Min = -16.5;
	Max = 15.5;
	CurTbl = VelTbl;
	load_table();
    }
}




void
incr_wind_field_event (Widget w, XEvent* ev, String* param, Cardinal* nparam)
/*
 * Xt translation table routine to switch fields
 */
{
    int	step = atoi (param[0]);
    incr_wind_field (step);
/*
 * Update the screen
 */
    RedrawDisplay (-1);
}


void
incr_wind_field (int step)
{
    int nproducts = NCfile->NumProducts();

    if (! nproducts)
    {
	fprintf( stderr, "No products in file %s\n", NCfile->FileName() );
	return;
    }
//
// Find the u/v wind field pairs in the product list and make a
// list of them.  The search is crude, assuming that wind fields begin
// with 'u' and 'v', and that the v component field will always follow
// immediately after its associated u component.  Also, look for our 
// wind fields (if any) and keep an index to their location in the list.
//
    int* windindices = new int[nproducts / 2];
    int nwinds = 0;
    int whichwind = -1;

    for (int p = 0; p < nproducts; p++)
    {
	if (NCfile->ProductName( p )[0] == 'u' &&
	    NCfile->ProductName( p + 1 )[0] == 'v')
	{
	    if (p == UWindFld)
		whichwind = nwinds;
	    windindices[nwinds++] = p;
	}
    }
//
// Bail now if we have no wind field pairs at all
//
    if (! nwinds)
    {
	UWindFld = VWindFld = -1;
	strcpy (UWindFldName, "");
	strcpy (VWindFldName, "");
	strcpy (WindUnits, "");
	strcpy (WindRcvr, "");
	strcpy (WindTitle, "no wind");
	WindRcvrNdx = -1;
	return;
    }
//
// Now increment the whichwind index by the given amount
//
    whichwind += step;

    while (whichwind < 0)
	whichwind += nwinds;

    while (whichwind >= nwinds)
	whichwind -= nwinds;
//
// Finally extract the correct product indices back from the
// windindices array and gather other field info
//
    UWindFld = windindices[whichwind];
    VWindFld = UWindFld + 1;

    strcpy (UWindFldName, NCfile->ProductName(UWindFld));
    strcpy (VWindFldName, NCfile->ProductName(VWindFld));

    char longname[128];
    NCfile->GetProductDetails(UWindFld, &WindRcvrNdx, longname, WindUnits);
    strcpy (WindRcvr, NCfile->SystemName(WindRcvrNdx));
//
// Big kluge: skip "u wind component " at the beginning of the u wind
// field's long name to get our wind title
//
    if (! strncmp (longname, "u wind component ", 17))
	sprintf (WindTitle, "Wind: %s", longname + 17);
    else
	sprintf (WindTitle, "Wind: %s/%s", UWindFldName, VWindFldName);

    delete[] windindices;
}




void
toggle (Widget w, XEvent* ev, String* param, Cardinal* nparam)
/*
 * Toggle on/off for the raster or vector portion of the display
 */
{
    static int	do_raster = TRUE;

    if (! strcmp (param[0], "raster"))
	ShowRaster = ! ShowRaster;
    else
	ShowVectors = ! ShowVectors;
}




void
start_or_stop (Widget w, XEvent* ev, String* s, Cardinal* val)
/*
 * Toggle the start/stop status of the display
 */
{
/*
 * Stop or start our input source, as appropriate
 */
    if (Stopped)
    {
	if (Realtime)
	    XInId = XtAppAddInput (Appc, InSocket, 
				   (XtPointer) XtInputReadMask,
				   RTNotification, NULL);
	else
	    XTimer = XtAppAddTimeOut (Appc, 1, NextBeam, NULL);
    }
    else
    {
	if (Realtime)
	    XtRemoveInput (XInId);
	else
	    XtRemoveTimeOut (XTimer);
    }
/*
 * Toggle the state variable
 */
    Stopped = !Stopped;
/*
 * Update the screen
 */
    RedrawDisplay (-1);
}



void
zoom (Widget w, XEvent *ev, String *param, Cardinal *nparam)
/*
 * Xt translation table routine to switch fields
 */
{
    PixPerKm *= atof (param[0]);

    clear ("raster");
    clear ("vector");
}



void
ToggleFakeAzimuths (Widget w, XEvent *ev, String *param, Cardinal *nparam)
/*
 * Xt translation table routine to toggle FakeAzimuths
 */
{
    FakeAzimuths = !FakeAzimuths;
}



void
move_center (Widget w, XEvent *ev, String *param, Cardinal *nparam)
/*
 * Center the display on the current cursor position
 */
{
    XButtonEvent	*bev = (XButtonEvent*) ev;

    int dx = bev->x - RD_XCENTER;
    int dy = bev->y - RD_YCENTER;

    CenterX += dx / PixPerKm;
    CenterY -= dy / PixPerKm;

    clear ("raster");
    clear ("vector");
}

  
void
set_vector (int x, int y, float u, float v)
/*
 * Set the vector in our array at the given location.
 */
{
    displayvector *dv;
    int	i, j, upix, vpix;
    const int ni = D_WIDTH / VECSPACING + 1;
    const int nj = D_HEIGHT / VECSPACING + 1;
/*
 * Make sure our array has been allocated
 */
    if (! WVec)
	WVec = (displayvector*) malloc (ni * nj * sizeof (displayvector));
/*
 * Don't bother saving if it isn't in the radar display area
 */
    if (x < RD_LEFT || x > RD_RIGHT || y < RD_TOP || y > RD_BOTTOM)
	return;
/*
 * indices into our array
 */    
    i = x / VECSPACING;
    j = y / VECSPACING;
/*
 * Save the given u and v, and calculate the four endpoints (base, tip, and
 * ends of the two barbs) for the three line segments we'll use in drawing
 * the vector . 
 */
    dv = WVec + (j * ni) + i;

    dv->u = u;
    dv->v = v;

    dv->xbase = i * VECSPACING;
    dv->ybase = j * VECSPACING;

    upix = (int)((u / FULLVECT) * VECSPACING);
    vpix = (int)((-v / FULLVECT) * VECSPACING);

    dv->xtip = dv->xbase + upix;
    dv->ytip = dv->ybase + vpix;

    dv->xbarb1 = NINT (dv->xtip - (int)(0.4 * (float)(upix - 0.3 * vpix)));
    dv->ybarb1 = NINT (dv->ytip - (int)(0.4 * (float)(0.3 * upix + vpix)));

    dv->xbarb2 = NINT (dv->xtip - (int)(0.4 * (float)(upix + 0.3 * vpix)));
    dv->ybarb2 = NINT (dv->ytip - (int)(0.4 * (float)(-0.3 * upix + vpix)));
}




void
draw_vector (Drawable d, GC gc, displayvector *dv)
/*
 * Actually draw the given vector
 */
{
    XSetForeground (Disp, Gc, Colors[VECTOR_NDX]);
/*
 * Just draw a little star if things are insane
 */
    if (dv->u < -50.0 || dv->u > 50.0 || 
	dv->v < -50.0 || dv->v > 50.0)
    {
	int x = dv->xbase;
	int y = dv->ybase;
		
	XDrawLine (Disp, d, gc, x - 3, y, x + 3, y);
	XDrawLine (Disp, d, gc, x - 2, y - 3, x + 2, y + 3);
	XDrawLine (Disp, d, gc, x + 2, y - 3, x - 2, y + 3);
	return;
    }
/*
 * Otherwise draw the shaft and two barbs using the endpoints in 
 * the displayvector
 */
    XDrawLine (Disp, d, gc, dv->xbase, dv->ybase, dv->xtip, dv->ytip);
    XDrawLine (Disp, d, gc, dv->xtip, dv->ytip, dv->xbarb1, dv->ybarb1);
    XDrawLine (Disp, d, gc, dv->xtip, dv->ytip, dv->xbarb2, dv->ybarb2);
}



void
DrawVectors (Drawable d, GC gc)
/*
 * Draw all the vectors in our array that lie in the radar display area
 */
{
    int i, j;
    int i0 = RD_LEFT / VECSPACING + 1;
    int i1 = RD_RIGHT / VECSPACING;
    int j0 = RD_TOP / VECSPACING + 1;
    int j1 = RD_BOTTOM / VECSPACING;
    const int ni = D_WIDTH / VECSPACING + 1;

    for (i = i0; i <= i1; i++)
	for (j = j0; j <= j1; j++)
	    draw_vector (d, gc, WVec + (j * ni) + i);
}



void
clear_request (Widget w, XEvent *ev, String *param, Cardinal *nparam)
{
    clear (param[0]);
}


void
clear (const char* which)
/*
 * Clear out either the "raster" or the "vector" portion of the display
 */
{
    int	x, y, width, height;
/*
 * If we're clearing the raster portion, fill the display area with the
 * background color.
 */
    if (! strcmp (which, "raster"))
    {
	XSetForeground (Disp, Gc, Colors[BG_NDX]);
	XFillRectangle (Disp, Pm, Gc, RD_LEFT, RD_TOP, RD_WIDTH + 1, 
			RD_HEIGHT + 1);
    }
/*
 * For vectors, we just fill the vector array with zeroes
 */
    else
    {
	for (x = 0; x <= D_WIDTH; x += VECSPACING)
	    for (y = 0; y <= D_HEIGHT; y += VECSPACING)
		set_vector (x, y, 0.0, 0.0);
    }
/*
 * Redraw the range rings
 */
    range_rings (Pm, 0.0, 360.0, 0.0, 100.0);
/*
 * And now redraw
 */
    RedrawDisplay (-1);
}
