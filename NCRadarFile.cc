//
// $Id: NCRadarFile.cc,v 1.1 2000/08/29 21:03:43 burghart Exp $
//
// Copyright (C) 2000
// Binet Incorporated 
//       and 
// University Corporation for Atmospheric Research
// 
// All rights reserved
//
// No part of this work covered by the copyrights herein may be reproduced
// or used in any form or by any means -- graphic, electronic, or mechanical,
// including photocopying, recording, taping, or information storage and
// retrieval systems -- without permission of the copyright owners.
//
// This software and any accompanying written materials are provided "as is"
// without warranty of any kind.
// 

# include <stdio.h>
# include <string.h>
# include <netcdf.h>
# include "NCRadarFile.hh"

const float _c_ = 2.998e8;	// c - m/s

//
// Private class _productvar
//
class _productvar
{
public:
    ~_productvar( void );
    void init( int ncid, int varid );
    char *Name;
    char *LongName;
    char *Units;
    int Varid;
    int SystemIndex;
    short MissingValue;
    short FillValue;
    float Scale;
    float Offset;
};




NCRadarFile::NCRadarFile( const char* fname )
{
    size_t index;
    
    OK = 1;
    Filename = new char[strlen( fname ) + 1];
    strcpy( Filename, fname );
//
// Open the file
//
    OK &= StatusOK( nc_open( fname, NC_NOWRITE | NC_SHARE, &NCid ), 
		    "NCRadarFile(opening)" );
    if (! OK) return;
//
// Time (unlimited) dimension
//
    OK &= StatusOK( nc_inq_unlimdim( NCid, &TimeDim ), 
		    "NCRadarFile: time dim" );
    if (! OK) return;
//
// maxCells
//
    OK &= StatusOK( nc_inq_dimid( NCid, "maxCells", &MaxCellDim ), 
		    "NCRadarFile(maxCells dim)" );
    OK &= StatusOK( nc_inq_dim( NCid, MaxCellDim, 0, &MaxCells ), 
		    "NCRadarFile(maxCells)" );
    if (! OK) return;
//
// numSystems
//
    NumSysDim = -1;
    OK &= StatusOK( nc_inq_dimid( NCid, "numSystems", &NumSysDim ), 
		    "NCRadarFile(numSystems dim)" );
    OK &= StatusOK( nc_inq_dim( NCid, NumSysDim, 0, &NumSys ), 
		    "NCRadarFile(numSystems)" );
    if (! OK) return;
//
// base time
//
    int bt_varid;
    index = 0;
    OK &= StatusOK( nc_inq_varid( NCid, "base_time", &bt_varid ), 
		    "NCRadarFile(base_time varid)" );
    OK &= StatusOK( nc_get_var1_int( NCid, bt_varid, &index, &BaseTime ),
		    "NCRadarFile(base_time get)" );
    if (! OK) return;
//
// fixed angle
//
    int fa_varid;
    index = 0;
    OK &= StatusOK( nc_inq_varid( NCid, "Fixed_Angle", &fa_varid ), 
		    "NCRadarFile(Fixed_Angle varid)" );
    OK &= StatusOK( nc_get_var1_float( NCid, bt_varid, &index, &FixedAng ),
		    "NCRadarFile(Fixed_Angle get)" );
    if (! OK) return;
//
// time_offset var
//
    OK &= StatusOK( nc_inq_varid( NCid, "time_offset", &TimeVar ), 
		    "NCRadarFile(time_offset varid)" );
    if (! OK) return;
//
// azimuth and elevation vars
//
    OK &= StatusOK( nc_inq_varid( NCid, "Azimuth", &AzVar ), 
		    "NCRadarFile(Azimuth varid)" );
    OK &= StatusOK( nc_inq_varid( NCid, "Elevation", &ElVar ), 
		    "NCRadarFile(Elevation varid)" );
    if (! OK) return;
//
// per-system info
//
    SysName = new (char*)[NumSys];
    NCells = new int[NumSys];
    SysLatitude = new double[NumSys];
    SysLongitude = new double[NumSys];
    SysAltitude = new double[NumSys];
    FirstCellRange = new float[NumSys];
    CellSpacing = new float[NumSys];
    AzFromXmtr = new float[NumSys];
    RangeFromXmtr = new float[NumSys];
    SampDelay = new float[NumSys];

    GetSystemNames();
    GetSystemIntVar( "NumCells", NCells );
    GetSystemDoubleVar( "Latitude", SysLatitude );
    GetSystemDoubleVar( "Longitude", SysLongitude );
    GetSystemDoubleVar( "Altitude", SysAltitude );
    GetSystemFloatVar( "Range_to_First_Cell", FirstCellRange );
    GetSystemFloatVar( "Cell_Spacing", CellSpacing );
    GetSystemFloatVar( "AzFromRadar", AzFromXmtr );
    GetSystemFloatVar( "RangeFromRadar", RangeFromXmtr );
    GetSystemFloatVar( "SampleDelay", SampDelay );
//
// Finally, our product list
//
    MakeProductList();
}



NCRadarFile::~NCRadarFile( void )
{
    if (NCid >= 0)
	nc_close( NCid );
    
    delete[] Filename;

    for (int s = 0; s < NumSys; s++)
	delete[] SysName[s];
    delete[] SysName;
    delete[] NCells;
    delete[] SysLatitude;
    delete[] SysLongitude;
    delete[] SysAltitude;
    delete[] FirstCellRange;
    delete[] CellSpacing;
    delete[] AzFromXmtr;
    delete[] RangeFromXmtr;
    delete[] SampDelay;

    delete[] ProductVars;
}



int
NCRadarFile::NumTimes( void ) const
{
    size_t ntimes;
    nc_inq_dim( NCid, TimeDim, 0, &ntimes );
    return( (int)ntimes );
}



int
NCRadarFile::StatusOK( int status, const char* where ) const
//
// Test netCDF status value, printing a message if it's an error.
// Return non-zero for error, otherwise zero.
//
{
    if (status != NC_NOERR)
    {
	fprintf( stderr, "NCRadarFile::%s: file %s, netCDF error (%s)\n", 
		 where, Filename, nc_strerror( status ), Filename );
	return 0;
    }

    return 1;
}



void
NCRadarFile::GetSystemNames( void )
//
// Get the system names from the file.
//
{
    int varid = GetSystemVarid( "system_name", NC_CHAR );
//
// Get the names
//
    int dimids[2];
    size_t maxnamelen;
    nc_inq_vardimid( NCid, varid, dimids );
    nc_inq_dimlen( NCid, dimids[1], &maxnamelen );

    for (int s = 0; s < NumSys; s++)
    {
	size_t start[2] = { s, 0 };
	size_t count[2] = { 1, maxnamelen };
	SysName[s] = new char[maxnamelen];
	OK &= StatusOK( nc_get_vara_text( NCid, varid, start, count, 
					  SysName[s] ), 
		       "GetSystemNames(get_vara)" );
    }
}



int
NCRadarFile::GetSystemVarid( const char* varname, nc_type type )
//
// Look for a per-system var with the given name and type, returning its varid
//
{
    int varid, ndims, dimids[5];
    char where[64];
    nc_type vartype;
//
// Find the requested var
//
    sprintf( where, "GetSystemVarid/%s(inq_varid)", varname );
    OK &= StatusOK( nc_inq_varid( NCid, varname, &varid ), where );
    if (! OK) return -1;
    
    sprintf( where, "GetSystemVarid/%s(inq_var)", varname );
    OK &= StatusOK( nc_inq_var( NCid, varid, 0, &vartype, &ndims, dimids, 0 ), 
		    where );
    if (! OK) return -1;
//
// Make sure that it's of the expected type.
//
    if (vartype != type)
    {
	fprintf( stderr, "NCRadarFile::GetSystemVarid: file %s: ", 
		 Filename );
	fprintf( stderr, "%s is type %d, not the expected %d\n", varname, 
		 vartype, type );
	OK = 0;
	return -1;
    }
//
// Make sure its first dimension is "numSystems".
//
    if (ndims == 0 || dimids[0] != NumSysDim)
    {
	fprintf( stderr, "NCRadarFile::GetSystemVarid: file %s: ", 
		 Filename );
	fprintf( stderr, "%s must have 'numSystems' as its first dimension\n", 
		 varname );
	OK = 0;
	return -1;
    }
//
// For NC_CHAR vars, we allow a second dimension for the string length.
// Others must be one-dimensional.
//
    int expected_dims = (type == NC_CHAR) ? 2 : 1;
    
    if (ndims != expected_dims)
    {
	fprintf( stderr, "NCRadarFile::GetSystemVarid: file %s: ", 
		 Filename );
	fprintf( stderr, "cannot handle %d-dimensional %s\n", ndims, varname );
	OK = 0;
	return -1;
    }

    return varid;
}



void
NCRadarFile::GetSystemDoubleVar( const char* varname, double* vals )
//
// Look for the named per-system var from the file, extracting the
// values.  If we don't find the named var, then the file is no
// longer considered to be OK.  The vals array must be able to 
// hold NumSys values.
//
{
    char where[64];
    int varid = GetSystemVarid( varname, NC_DOUBLE );
    if (! OK)
	return;
//
// Extract the values into the array we were given
//
    size_t start = 0, count = NumSys;

    sprintf( where, "GetSystemDoubleVar/%s(get_vara)", varname );
    OK &= StatusOK( nc_get_vara_double( NCid, varid, &start, &count, vals ), 
		    where );
}



void
NCRadarFile::GetSystemFloatVar( const char* varname, float* vals )
//
// Look for the named per-system var from the file, extracting the
// values.  If we don't find the named var, then the file is no
// longer considered to be OK.  The vals array must be able to 
// hold NumSys values.
//
{
    char where[64];
    int varid = GetSystemVarid( varname, NC_FLOAT );
    if (! OK)
	return;
//
// Extract the values into the array we were given
//
    size_t start = 0, count = NumSys;

    sprintf( where, "GetSystemFloatVar/%s(get_vara)", varname );
    OK &= StatusOK( nc_get_vara_float( NCid, varid, &start, &count, vals ), 
		    where );
}



void
NCRadarFile::GetSystemIntVar( const char* varname, int* vals )
//
// Look for the named per-system var from the file, extracting the
// values.  If we don't find the named var, then the file is no
// longer considered to be OK.  The vals array must be able to 
// hold NumSys values.
//
{
    char where[64];

    int varid = GetSystemVarid( varname, NC_INT );
    if (! OK)
	return;
//
// Extract the values into the array we were given
//
    size_t start = 0, count = NumSys;

    sprintf( where, "GetSystemIntVar/%s(get_vara)", varname );
    OK &= StatusOK( nc_get_vara_int( NCid, varid, &start, &count, vals ), 
		    where );
}



double
NCRadarFile::Time( int tindex ) const
//
// Return the time for the given index (seconds since 1-Jan-1970,00:00 UTC)
//
{
    if (! OK)
	return 0.0;
//
// Get the time offset for the index and add the base time
//  
    double time;
    size_t index = tindex;

    StatusOK( nc_get_var1_double( NCid, TimeVar, &index, &time ), "Time" );
    time += BaseTime;
    return time;
}



float
NCRadarFile::Azimuth( int tindex ) const
{
    if (! OK)
	return 0.0;

    float az = 0.0;
    size_t index = tindex;
    nc_get_var1_float( NCid, AzVar, &index, &az );
    return az;
}



float
NCRadarFile::Elevation( int tindex ) const
{
    if (! OK)
	return 0.0;

    float el = 0.0;
    size_t index = tindex;
    nc_get_var1_float( NCid, ElVar, &index, &el );
    return el;
}



const char*
NCRadarFile::SystemName( int sindex ) const
{
    if (! OK)
	return "";
    else
	return( SysName[sindex] );
}



int
NCRadarFile::NGates( int sindex ) const
{
    if (! OK)
	return 0;
    else
	return( NCells[sindex] );
}



double
NCRadarFile::Latitude( int sindex ) const
{
    if (! OK)
	return 0.0;
    else
	return( SysLatitude[sindex] );
}



double
NCRadarFile::Longitude( int sindex ) const
{
    if (! OK)
	return 0.0;
    else
	return( SysLongitude[sindex] );
}



double
NCRadarFile::Altitude( int sindex ) const
{
    if (! OK)
	return 0.0;
    else
	return( SysAltitude[sindex] );
}



float 
NCRadarFile::RangeToFirstGate( int sindex ) const
{
    if (! OK)
	return 0.0;
    else
	return( FirstCellRange[sindex] );
}



float 
NCRadarFile::GateSpacing( int sindex ) const
{
    if (! OK)
	return 0.0;
    else
	return( CellSpacing[sindex] );
}



float 
NCRadarFile::GateTime( int sindex ) const
{
    if (! OK)
	return 0.0;
    else
	return( 2 * CellSpacing[sindex] / _c_ );
}



float 
NCRadarFile::AzFromXmitter( int sindex ) const
{
    if (! OK)
	return 0.0;
    else
	return( AzFromXmtr[sindex] );
}



float 
NCRadarFile::RangeFromXmitter( int sindex ) const
{
    if (! OK)
	return 0.0;
    else
	return( RangeFromXmtr[sindex] );
}


float
NCRadarFile::SampleDelay( int sindex ) const
{
    if (! OK)
	return 0.0;
    else
	return( SampDelay[sindex] );
}



void
NCRadarFile::MakeProductList( void )
//
// Create a list of "product" vars, i.e., those with dimensions [time,MaxCells]
//
{
    int nvars;

    if (! OK)
	return;
//
// Count product vars
//    
    nc_inq_nvars( NCid, &nvars );

    int* prodvarids = new int[nvars];
    NProducts = 0;

    for (int v = 0; v < nvars; v++)
    {
	int ndims, dimids[5];
	
	nc_inq_var( NCid, v, 0, 0, &ndims, dimids, 0 );
	if (ndims == 2 && dimids[0] == TimeDim && dimids[1] == MaxCellDim)
	    prodvarids[NProducts++] = v;
    }
//
// Now create the _productvar instances
//
    ProductVars = new _productvar[NProducts];
    
    for (int p = 0; p < NProducts; p++)
	ProductVars[p].init( NCid, prodvarids[p] );
    
    delete[] prodvarids;
}



const char*
NCRadarFile::ProductName( int pndx ) const
{
    if (pndx >= 0 && pndx < NProducts)
	return ProductVars[pndx].Name;
    else
	return "";
}



int
NCRadarFile::ProductIndex( const char* varname ) const
{
    for (int p = 0; p < NProducts; p++)
	if (! strcmp( varname, ProductVars[p].Name ))
	    return p;
    
    return -1;
}



int 
NCRadarFile::GetProductDetails( int pndx, int* sys_index, char* longname, 
				char* units ) const
//
// Return the system index, long name, and units for a given product
// var.  Values will not be returned for entries with null return
// pointers.
//
{
    if (! OK || pndx < 0 || pndx >= NProducts)
	return 0;

    if (sys_index)
	*sys_index = ProductVars[pndx].SystemIndex;
    if (longname)
	strcpy( longname, ProductVars[pndx].LongName );
    if (units)
	strcpy( units, ProductVars[pndx].Units );
}



int 
NCRadarFile::GetProduct( int pndx, int tindex, float* vals, int maxvals,
			 float badval ) const
//
// Return an array of data for the given product var at the given time index.
// The return value of the function is the number of values stored in the
// caller-supplied vals array (up to maxvals).
//
{
    static short* svals = 0;
    static int nsvals = 0;
    _productvar *pvar = ProductVars + pndx;

    if (! OK || pndx < 0 || pndx >= NProducts)
	return 0;

    int varid = pvar->Varid;
    int sys_index = pvar->SystemIndex;
    int nvals = (NCells[sys_index] < maxvals) ? NCells[sys_index] : maxvals;

    size_t start[2] = { tindex, 0 };
    size_t count[2] = { 1, nvals };

    if (nsvals < MaxCells)
    {
	delete[] svals;
	svals = new short[MaxCells];
    }
	
    nc_get_vara_short( NCid, varid, start, count, svals );

    float scale = pvar->Scale;
    float offset = pvar->Offset;
    for (int i = 0; i < nvals; i++)
    {
	if (svals[i] == pvar->MissingValue || svals[i] == pvar->FillValue)
	    vals[i] = badval;
	else
	    vals[i] = svals[i] * scale + offset;
    }

    return nvals;
}


//
// Private _productvar class
//
_productvar::~_productvar( void )
{
    delete[] Name;
    delete[] LongName;
    delete[] Units;
}


void
_productvar::init( int ncid, int varid )
{
    char string[128];
    size_t attlen;

    Varid = varid;

    string[0] = '\0';
    nc_inq_varname( ncid, varid, string );
    Name = new char[strlen( string ) + 1];
    strcpy( Name, string );

    SystemIndex = 0;	// fallback
    nc_get_att_int( ncid, varid, "system_index", &SystemIndex );

    string[0] = '\0';
    attlen = 0;
    nc_inq_attlen( ncid, varid, "long_name", &attlen );
    nc_get_att_text( ncid, varid, "long_name", string );
    LongName = new char[attlen + 1];
    strncpy( LongName, string, attlen );
    LongName[attlen] = '\0';
    
    string[0] = '\0';
    attlen = 0;
    nc_inq_attlen( ncid, varid, "units", &attlen );
    nc_get_att_text( ncid, varid, "units", string );
    Units = new char[attlen + 1];
    strncpy( Units, string, attlen );
    Units[attlen] = '\0';

    nc_get_att_short( ncid, varid, "missing_value", &MissingValue );
    nc_get_att_short( ncid, varid, "_FillValue", &FillValue );
    nc_get_att_float( ncid, varid, "scale_factor", &Scale );
    nc_get_att_float( ncid, varid, "add_offset", &Offset );
}

