/*
 * $Id: DisplayInfo.h,v 1.1 2000/08/29 21:03:43 burghart Exp $
 *
 * Copyright (C) 1998
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

/*
 * Geometry information for xbistat's window.  We're working is X ccordinates
 * here, so y is zero at the top and increases downwards.
 *
 *      -------------------------  < 0
 *	|			|
 *	|			|
 *	| ---------------	|		\
 *	| |Radar	|	|		|
 *	| |  Display	|	|		|
 *	| |	Area	|	|		|
 *	| |		|	|  < RD_YCENTER |
 *	| |		|	|		| RD_HEIGHT
 *	| |		|	|		|
 *	| |		|	|		|
 *	| |		|	|		/
 *      -------------------------  < DHEIGHT - 1
 *      ^	 ^		^
 *      0     RD_XCENTER    DWIDTH - 1
 *
 *	  \____________/
 *	     RD_WIDTH
 */

/*
 * Overall display window size
 */
static const int D_WIDTH = 900;
static const int D_HEIGHT = 675;

/* 
 * Center of radar display area relative to the upper left corner of the 
 * whole window
 */
static const int RD_XCENTER = 450;
static const int RD_YCENTER = 340;
 
/* 
 * Width and height of radar display area
 */
static const int RD_WIDTH = 800;
static const int RD_HEIGHT = 600;

/*
 * The rest are defined based on what comes before
 */
# define RD_LEFT (RD_XCENTER - RD_WIDTH / 2)
# define RD_RIGHT (RD_LEFT + RD_WIDTH - 1)
# define RD_TOP (RD_YCENTER - RD_HEIGHT / 2)
# define RD_BOTTOM (RD_TOP + RD_HEIGHT - 1)
