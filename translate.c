/** \file translate.c

Translation library, internally uses catgets

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>

#if HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include "common.h"
#include "util.h"

#if HAVE_GETTEXT

/**
   Number of string_buffer_t in the ring of buffers
*/
#define BUFF_COUNT 64

/**
   The ring of string_buffer_t
*/
static string_buffer_t buff[BUFF_COUNT];
/**
   Current position in the ring
*/
static int curr_buff=0;

/**
   Buffer used by translate_wcs2str
*/
static char *wcs2str_buff=0;
/**
   Size of buffer used by translate_wcs2str
*/
static size_t wcs2str_buff_count=0;

static int is_init = 0;

static void internal_init()
{
	int i;

	is_init = 1;
	
	for(i=0; i<BUFF_COUNT; i++ )
		sb_init( &buff[i] );
	
	bindtextdomain( PACKAGE_NAME, LOCALEDIR );
	textdomain( PACKAGE_NAME );
}


/**
   Wide to narrow character conversion. Internal implementation that
   avoids exessive calls to malloc
*/
static char *translate_wcs2str( const wchar_t *in )
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

	if( !is_init )
		internal_init();
		
	char *mbs_in = translate_wcs2str( in );	
	char *out = gettext( mbs_in );
	wchar_t *wres=0;

	sb_clear( &buff[curr_buff] );
	
	sb_printf( &buff[curr_buff], L"%s", out );
	wres = (wchar_t *)buff[curr_buff].buff;
	curr_buff = (curr_buff+1)%BUFF_COUNT;

	return wres;
}


void translate_init()
{
}

void translate_destroy()
{
	int i;

	if( !is_init )
		return;
	
	is_init = 0;
	
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
