/*

Copyright (C) 2006 Angel Death 

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (LICENSE.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include <vector>
#include <string>
#include <map>
#include <zlib.h>
typedef unsigned long DWORD;
typedef unsigned char BYTE;
typedef unsigned short WORD;

//================================================
void CreatePath( const char *path )
{
	char   *ofs, c, *str;

	str = strdup( path );

	for ( ofs = str + 1; *ofs; ofs++ )
	{
		c = *ofs;
		if ( c == '/' || c == '\\' )
		{		// create the directory
			*ofs = 0;
			_mkdir( str );
			*ofs = c;
		}
	}
	free( str );
}

//================================================
bool SaveFile( const char *fname, int size, BYTE * data )
{
	FILE   *f;

	if ( !data )
		return false;

	CreatePath( fname );
	f = fopen( fname, "wb" );

	if ( !f )
		return false;

	fwrite( data, size, 1, f );
	fclose( f );

	return true;

};

//================================================
struct hash_t {
	BYTE    b[8];
};
bool operator<( const hash_t & h1, const hash_t & h2 )
{
	return memcmp( h1.b, h2.b, 8 ) < 0;
}

char    exts[][10] = {
	"",
	".NIF",
	".KF",
	".DDS",
	".WAV"
};

int     numexts = sizeof( exts ) / sizeof( exts[0] );
DWORD _CalcHash2( const char *name, int len )
{
	if ( len <= 0 )
		return 0;
	int     tt = 0;

	while ( len-- > 0 )
	{
		tt = tt * 0x1003f + tolower( *name++ );
	}
	return tt;
}
void CalcHash( const char *name, const char *ext, BYTE * hash )
{
	int     namelen = strlen( name );

	hash[2] = namelen & 0xff;
	hash[3] = tolower( name[0] );
	hash[0] = tolower( name[namelen - 1] );
	hash[1] = ( namelen <= 2 ) ? 0 : tolower( name[namelen - 2] );
	*( DWORD * ) ( hash + 4 ) = 0;
	if ( namelen > 3 )
	{
		*( DWORD * ) ( hash + 4 ) = _CalcHash2( name + 1, namelen - 3 );
	}
	if ( !ext )
		return;
	int     extlen = strlen( ext );

	*( DWORD * ) ( hash + 4 ) += _CalcHash2( ext, extlen );
	for ( BYTE i = 0; i < numexts; i++ )
	{
		if ( !_stricmp( exts[i], ext ) )
		{
			hash[3] += ( i & 0xfc ) << 5;
			hash[0] += ( i & 0xfe ) << 6;
			hash[1] += i << 7;
			return;
		}
	}
}

#pragma pack(push,1)
typedef struct {
	DWORD   id, id1, id2;
	DWORD   hz1, numdirs, numnames;
	DWORD   hz3;
	DWORD   sizefnames;
	WORD    numhz1, numhz2;
} bsa_hdr_t;
typedef struct {
	hash_t  hash;
	DWORD   numf, off;
} bsa_root_record_t;

typedef struct {
	hash_t  hash;
	DWORD   size, off;
} bsa_file_record_t;
typedef struct {
	std::string dirname;
	hash_t  hash;
        std::vector < bsa_file_record_t > files;
} bsa_dir_t;

#pragma pack(pop)

void main( int argc, char *argv[] )
{
	bsa_hdr_t hdr;

	if ( argc != 2 )
	{
		printf( "Usage: unbsa4 <bsafile>\n" );
		return;
	}
	FILE   *f = fopen( argv[1], "rb" );

	if ( !f )
	{
		printf( "cannot open %s\n", argv[1] );
		return;
	}
	fread( &hdr, sizeof( hdr ), 1, f );
	std::vector < bsa_root_record_t > root;
	root.resize( hdr.numdirs );
	fread( &root[0], hdr.numdirs, sizeof( bsa_root_record_t ), f );
	std::vector < bsa_dir_t > dirs;
	dirs.resize( hdr.numdirs );
	for ( int i = 0; i < root.size(  ); i++ )
	{
		char    dirname[256];
		int     len = fgetc( f );

		fread( dirname, len, 1, f );
		dirs[i].dirname = dirname;
		dirs[i].hash = root[i].hash;
		dirs[i].files.resize( root[i].numf );
		fread( &dirs[i].files[0], root[i].numf, sizeof( bsa_file_record_t ), f );
	}
	char   *fnames = new char[hdr.sizefnames];

	fread( fnames, hdr.sizefnames, 1, f );
	int     off = 0;

	std::map < hash_t, std::string > names;
	for ( int i = 0; i < hdr.numnames; i++ )
	{
		int     len = strlen( fnames + off );

		std::string fname = fnames + off;
		std::string::size_type pos = fname.rfind( '.' );
		std::string name;
		std::string ext;
		if ( pos != std::string::npos )
		{
			name = fname.substr( 0, pos );
			ext = fname.substr( pos );
		} else
			name = fname;

		hash_t  h;

		CalcHash( name.c_str(  ), ext.c_str(  ), h.b );
		names[h] = fname;
		off += len + 1;
	}
	for ( int i = 0; i < dirs.size(  ); i++ )
	{
		for ( int j = 0; j < dirs[i].files.size(  ); j++ )
		{
			bsa_file_record_t & rec = dirs[i].files[j];
			std::map < hash_t, std::string >::iterator ii =
			    names.find( rec.hash );
			if ( ii == names.end(  ) )
			{
				printf( "Warning!!! File %d in dir %s no found\n", j,
					dirs[i].dirname.c_str(  ) );
				continue;
			}
			std::string s =
			    "data\\" + dirs[i].dirname + "\\" + ( *ii ).second;
			printf( "%8x %8x %s\n", rec.off, rec.size, s.c_str(  ) );
			BYTE   *data = new BYTE[rec.size];

			fseek( f, rec.off, 0 );
			fread( data, rec.size, 1, f );
			if ( hdr.hz1 & 4 )
			{
				int     psize = rec.size - 4;
				DWORD   unpsize = *( int * ) ( data );

				if ( unpsize )
				{
					BYTE   *unpdata = new BYTE[unpsize];
					int     t =
					    uncompress( unpdata, &unpsize, data + 4,
							psize );
					if ( t != Z_OK )
					{
						printf( "Error unpack at %8x %s\n",
							rec.off, s.c_str(  ) );
					} else
						SaveFile( s.c_str(  ), unpsize, unpdata );
					delete[]unpdata;
				} else
				{
					SaveFile( s.c_str(  ), 0, data );
				}
			} else
			{
				if ( rec.size >= 6 && data[4] == 0x78 && data[5] == 0x9c )
					printf( "Error: possibly packed at %8x %s\n",
						rec.off, s.c_str(  ) );
				SaveFile( s.c_str(  ), rec.size, data );
			}
			delete[]data;

		}
	}
	fclose( f );
}
