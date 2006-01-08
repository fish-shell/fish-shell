/** \file translate.c

Translation library, internally uses catgets

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>

#include "common.h"
#include "util.h"

#if HAVE_GETTEXT

#define BUFF_COUNT 64

static string_buffer_t buff[BUFF_COUNT];
static int curr_buff=0;

static char *wcs2str_buff=0;
static size_t wcs2str_buff_count=0;

char *translate_wcs2str( const wchar_t *in )
{
	size_t len = MAX_UTF8_BYTES*wcslen(in)+1;
	if( len > wcs2str_buff_count )
	{
		wcs2str_buff = realloc( wcs2str_buff, len );
		if( wcs2str_buff == 0 )
		{
			die_mem();
		}
	}
	
	wcstombs( wcs2str_buff, 
			  in,
			  MAX_UTF8_BYTES*wcslen(in)+1 );
	
	return wcs2str_buff;
}

const wchar_t *wgettext( const wchar_t *in )
{
	if( !in )
		return in;
	
	char *mbs_in = translate_wcs2str( in );	
	char *out = gettext( mbs_in );
	wchar_t *wres=0;

	sb_clear( &buff[curr_buff] );
	
	sb_printf( &buff[curr_buff], L"%s", out );
	wres = (wchar_t *)buff[curr_buff].buff;
	curr_buff = (curr_buff+1)%BUFF_COUNT;

/*	
    write( 2, res, strlen(res) );
*/
//	debug( 1, L"%ls -> %s (%d) -> %ls (%d)\n", in, out, strlen(out) , wres, wcslen(wres) );

	return wres;
}


void translate_init()
{
	int i;
	
	for(i=0; i<BUFF_COUNT; i++ )
		sb_init( &buff[i] );
	
	bindtextdomain( PACKAGE_NAME, LOCALEDIR );
	textdomain( PACKAGE_NAME );

}

void translate_destroy()
{
	int i;
	
	for(i=0; i<BUFF_COUNT; i++ )
		sb_destroy( &buff[i] );

	free( wcs2str_buff );
}

#else

const wchar_t *wgettext( const wchar_t *in )
{
	return in;
}

void translate_init()
{
}

void translate_destroy()
{
}

#endif
