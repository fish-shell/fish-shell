/** \file kill.c
	The killring.

	Works like the killring in emacs and readline. The killring is cut
	and paste with a memory of previous cuts. It supports integration
	with the X clipboard.
*/

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>


#include "config.h"

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "kill.h"
#include "proc.h"
#include "sanity.h"
#include "common.h"
#include "env.h"
#include "exec.h"
#include "parser.h"
#include "halloc.h"

/**
   Maximum entries in killring
*/
#define KILL_MAX 8192


static ll_node_t /** Last kill string */*kill_last=0, /** Current kill string */*kill_current =0;
/**
   Contents of the X clipboard, at last time we checked it
*/
static wchar_t *cut_buffer=0;

/**
   Test if the xsel command is installed
*/
static int has_xsel()
{
	void *context = halloc(0, 0);
	wchar_t *path = parser_get_filename( context, L"xsel" );
	int res = !!path;
	halloc_free( context );
	return res;
}



/**
   Add the string to the internal killring
*/
static void kill_add_internal( wchar_t *str )
{
	if( wcslen( str ) == 0 )
		return;
	
	if( kill_last == 0 )
	{
		kill_current = kill_last=malloc( sizeof( ll_node_t ) );
		kill_current->data = wcsdup(str);
		kill_current->prev = kill_current;
	}
	else
	{
		kill_current = malloc( sizeof( ll_node_t ) );
		kill_current->data = kill_last->data;
		kill_last->data = wcsdup(str);
		kill_current->prev = kill_last->prev;
		kill_last->prev = kill_current;
		kill_current = kill_last;
	}
}


void kill_add( wchar_t *str )
{
	kill_add_internal(str);

	if( !has_xsel() )
		return;

	/* This is for sending the kill to the X copy-and-paste buffer */
	wchar_t *disp;
	if( (disp = env_get( L"DISPLAY" )) )
	{
		wchar_t *escaped_str = escape( str, 1 );
		wchar_t *cmd = wcsdupcat2(L"echo ", escaped_str, L"|xsel -b",0);
		exec_subshell( cmd, 0 );
		free( cut_buffer );
		free( cmd );
	
		cut_buffer = escaped_str;
	}
}


wchar_t *kill_yank_rotate()
{
	if( kill_current == 0 )
		return L"";
	kill_current = kill_current->prev;
	return (wchar_t *)kill_current->data;
}

/**
   Check the X clipboard. If it has been changed, add the new
   clipboard contents to the fish killring.
*/
static void kill_check_x_buffer()
{	
	wchar_t *disp;
	
	if( !has_xsel() )
		return;
	
	
	if( (disp = env_get( L"DISPLAY" )) )
	{
		int i;
		wchar_t *cmd = L"xsel -t 500 -b";
		wchar_t *new_cut_buffer=0;
		array_list_t list;
		al_init( &list );
		exec_subshell( cmd, &list );
		
		for( i=0; i<al_get_count( &list ); i++ )
		{
			wchar_t *next_line = escape( (wchar_t *)al_get( &list, i ), 0 );
			if( i==0 )
			{
				new_cut_buffer = next_line;
			}
			else
			{
				wchar_t *old = new_cut_buffer;
				new_cut_buffer= wcsdupcat2( new_cut_buffer, L"\\n", next_line, 0 );
				free( old );
				free( next_line );	
			}
		}
		
		if( new_cut_buffer )
		{
			/*  
				The buffer is inserted with backslash escapes,
				since we don't really like tabs, newlines,
				etc. anyway.
			*/
			
			if( cut_buffer != 0 )
			{      
				if( wcscmp( new_cut_buffer, cut_buffer ) == 0 )
				{
					free( new_cut_buffer );
					new_cut_buffer = 0;
				}
				else
				{
					free( cut_buffer );
					cut_buffer = 0;								
				}
			}
			if( cut_buffer == 0 )
			{
				cut_buffer = new_cut_buffer;
				kill_add_internal( cut_buffer );
			}
		}
		
		al_foreach( &list, &free );
		al_destroy( &list );
	}
}


wchar_t *kill_yank()
{
	kill_check_x_buffer();
	if( kill_current == 0 )
		return L"";
	kill_current=kill_last;
	return (wchar_t *)kill_current->data;
}

void kill_sanity_check()
{
	int i;
	if( is_interactive )
	{
		/* Test that the kill-ring is consistent */
		if( kill_current != 0 )
		{
			int kill_ok = 0;
			ll_node_t *tmp = kill_current->prev;
			for( i=0; i<KILL_MAX; i++ )
			{
				if( tmp == 0 )
					break;
				if( tmp->data == 0 )
					break;
				
				if( tmp == kill_current )
				{
					kill_ok = 1;
					break;
				}
				tmp = tmp->prev;				
			}
			if( !kill_ok )
			{
				debug( 0, 
					   L"Killring inconsistent" );
				sanity_lose();
			}
		}
		
	}
}

void kill_init()
{
}

void kill_destroy()
{
	if( cut_buffer )
		free( cut_buffer );
	
	if( kill_current != 0 )
	{
		kill_current = kill_last->prev;
		kill_last->prev = 0;
		
		while( kill_current )
		{
			ll_node_t *tmp = kill_current;
			kill_current = kill_current->prev;
			free( tmp->data );
			free( tmp );
		}
	}	
	
}

