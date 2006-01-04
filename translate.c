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

const wchar_t *wgettext( const wchar_t *in )
{
	char *mbs_in = wcs2str( in );	
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
