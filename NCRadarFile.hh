// NCRadarFile.hh
//
// Copyright © 2000 Binet Incorporated
// Copyright © 2000 University Corporation for Atmospheric Research
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

# ifndef _NCRADARFILE_HH_
# define _NCRADARFILE_HH_

# include <cstdlib>

class _productvar;	// private class defined in NCRadarFile.cc

class NCRadarFile
{
public:
    NCRadarFile( const char* fname );
    ~NCRadarFile( void );
    inline const char* FileName( void ) { return Filename; }
    int NumTimes( void ) const;
    inline int IsOK( void ) const { return OK; }
    inline int NumSystems( void ) const { return NumSys; }
    inline float FixedAngle( void ) const { return FixedAng; }
    inline int MaxGates( void ) const { return MaxCells; }
// per-system info
    const char* SystemName( int sindex ) const;
    double Latitude( int sindex ) const;	// site latitude
    double Longitude( int sindex ) const;	// site longitude
    double Altitude( int sindex ) const;	// site altitude, m
    int NGates( int sindex ) const;		// site's cell count
    float RangeToFirstGate( int sindex ) const;	// center of first gate, m
    float GateSpacing( int sindex ) const;	// (nominal) gate spacing, m
    float GateTime( int sindex ) const;		// system gate time, s
    float AzFromXmitter( int sindex ) const;	// site azimuth from xmtr, deg
    float RangeFromXmitter( int sindex ) const;	// site range from xmtr, m
    float SampleDelay( int sindex ) const;	// xmit-to-initial-sample 
						// delay, s
    double Time( int tindex ) const;
    float Azimuth( int tindex ) const;
    float Elevation( int tindex ) const;
//
// "product" here refers to any var with dimensions [time, MaxCells]
//
    inline int NumProducts( void ) const { return NProducts; };
    const char* ProductName( int pndx ) const;
    int ProductIndex( const char* varname ) const;
    bool GetProductDetails( int pndx, int* sys_index, char* longname,
			   char* units ) const;
    int GetProduct( int pndx, int tindex, float* vals, int maxvals,
		    float badval ) const;
private:
    int OK;
    char* Filename;
    int NCid;
    int TimeDim;
    int MaxCellDim;
    int NumSysDim;
    int BaseTime;
    size_t MaxCells;
    int TimeVar;
    int AzVar;
    int ElVar;
    float FixedAng;
// per-system stuff
    size_t NumSys;
    char** SysName;		// site name
    int* NCells;		// cell count for this receiver
    double* SysLatitude;	// site latitude
    double* SysLongitude;	// site longitude
    double* SysAltitude;	// site altitude
    float* FirstCellRange;	// range to center of first cell, m
    float* CellSpacing;		// (nominal) cell spacing, m
    float* AzFromXmtr;		// site azimuth from radar, degrees
    float* RangeFromXmtr;	// site range from radar, m
    float* SampDelay;		// xmit-to-initial-sample delay, s
// "product" info
    int NProducts;
    _productvar* ProductVars;
    void MakeProductList( void );
// per-system var methods
    int GetSystemVarid( const char* varname, nc_type type );
    void GetSystemNames( void );
    void GetSystemDoubleVar( const char* varname, double* vals );
    void GetSystemFloatVar( const char* varname, float* vals );
    void GetSystemIntVar( const char* varname, int* vals );

    int StatusOK( int status, const char* where ) const;
};

# endif // ifndef _NCRADARFILE_HH_

