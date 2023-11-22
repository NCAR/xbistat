// DisplayInfo.hh
// Geometry information for xbistat's window
//
//
// Copyright © 1998 Binet Incorporated
// Copyright © 1998 University Corporation for Atmospheric Research
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.


// Geometry information for xbistat's window.  We're working in X Window System
// coordinates here, so y is zero at the top and increases downwards.
//
//      -------------------------  < 0
//	|			|
//	|			|
//	| ---------------	|		\
//	| |Radar	|	|		|
//	| |  Display	|	|		|
//	| |	Area	|	|		|
//	| |		|	|  < RD_YCENTER |
//	| |		|	|		| RD_HEIGHT
//	| |		|	|		|
//	| |		|	|		|
//	| |		|	|		/
//      -------------------------  < DHEIGHT - 1
//      ^	 ^		^
//      0     RD_XCENTER    DWIDTH - 1
//
//	  \____________/
//	     RD_WIDTH

//
// Overall display window size
//
static const int D_WIDTH = 900;
static const int D_HEIGHT = 675;

//
// Center of radar display area relative to the upper left corner of the
// whole window
//
static const int RD_XCENTER = 450;
static const int RD_YCENTER = 340;
 
//
// Width and height of radar display area
//
static const int RD_WIDTH = 800;
static const int RD_HEIGHT = 600;

//
// The rest are defined based on what comes before
//
# define RD_LEFT (RD_XCENTER - RD_WIDTH / 2)
# define RD_RIGHT (RD_LEFT + RD_WIDTH - 1)
# define RD_TOP (RD_YCENTER - RD_HEIGHT / 2)
# define RD_BOTTOM (RD_TOP + RD_HEIGHT - 1)
