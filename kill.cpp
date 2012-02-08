/** \file kill.c
	The killring.

	Works like the killring in emacs and readline. The killring is cut
	and paste with a memory of previous cuts. It supports integration
	with the X clipboard.
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>


#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "kill.h"
#include "proc.h"
#include "sanity.h"
#include "common.h"
#include "env.h"
#include "exec.h"
#include "path.h"

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
   Test if the xsel command is installed.  Since this is called often,
   cache the result.
*/
static int has_xsel()
{
	static int called=0;
	static int res = 0;
    
	if (!called) {
        wchar_t *path = path_get_path( L"xsel" );
		res = !!path;
        free(path);
		called = 1;
    }
    
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
		kill_current = kill_last=(ll_node_t *)malloc( sizeof( ll_node_t ) );
		kill_current->data = wcsdup(str);
		kill_current->prev = kill_current;
	}
	else
	{
		kill_current = (ll_node_t *)malloc( sizeof( ll_node_t ) );
		kill_current->data = kill_last->data;
		kill_last->data = wcsdup(str);
		kill_current->prev = kill_last->prev;
		kill_last->prev = kill_current;
		kill_current = kill_last;
	}
}


void kill_add( wchar_t *str )
{
	wchar_t *cmd = NULL;
	wchar_t *escaped_str;
	kill_add_internal(str);


	/*
	   Check to see if user has set the FISH_CLIPBOARD_CMD variable,
	   and, if so, use it instead of checking the display, etc.

	   I couldn't think of a safe way to allow overide of the echo
	   command too, so, the command used must accept the input via stdin.
	*/

	const env_var_t clipboard_wstr = env_get_string(L"FISH_CLIPBOARD_CMD");
	if( !clipboard_wstr.missing() )
	{
		escaped_str = escape( str, 1 );
		cmd = wcsdupcat(L"echo -n ", escaped_str, clipboard_wstr.c_str());
	}
	else
	{
	/* This is for sending the kill to the X copy-and-paste buffer */
		if( !has_xsel() ) {
			return;
		}

	const env_var_t disp_wstr = env_get_string( L"DISPLAY" );
	if( !disp_wstr.missing() )
	{
			escaped_str = escape( str, 1 );
			cmd = wcsdupcat(L"echo ", escaped_str, L"|xsel -b" );
		}
	}

	if (cmd != NULL)
	{
		if( exec_subshell( cmd) == -1 )
		{
			/* 
			   Do nothing on failiure
			*/
		}
		
		free( cut_buffer );
		free( cmd );
	
		cut_buffer = escaped_str;
	}
}

/**
   Remove the specified node from the circular list
*/
static void kill_remove_node( ll_node_t *n )
{
	if( n->prev == n )
	{
		kill_last=kill_current = 0;
	}
	else
	{
		ll_node_t *nxt = n->prev;
		while( nxt->prev != n )
		{
			nxt=nxt->prev;
		}
		nxt->prev = n->prev;
		if( kill_last == n )
		{
			kill_last = n->prev;
		}
		kill_current=kill_last;
		free( n->data );
		free( n );		
	}	
}

/**
   Remove first match for specified string from circular list
*/
static void kill_remove( wchar_t *s )
{
	ll_node_t *n, *next=0;
	
	if( !kill_last )
	{
		return;
	}
	
	for( n=kill_last; 
		 n!=kill_last || next == 0 ;
		 n=n->prev )
	{
		if( wcscmp( (wchar_t *)n->data, s ) == 0 )
		{
			kill_remove_node( n );
			break;
		}
		next = n;
	}
}
		
		

void kill_replace( wchar_t *old, wchar_t *newv )
{
	kill_remove( old );
	kill_add( newv );	
}

const wchar_t *kill_yank_rotate()
{
	if( kill_current == 0 )
		return L"";
	kill_current = kill_current->prev;
	return (const wchar_t *)kill_current->data;
}

/**
   Check the X clipboard. If it has been changed, add the new
   clipboard contents to the fish killring.
*/
static void kill_check_x_buffer()
{		
	if( !has_xsel() )
		return;
	
	const env_var_t disp = env_get_string(L"DISPLAY");
	if( ! disp.missing())
	{
		size_t i;
		wcstring cmd = L"xsel -t 500 -b";
		wcstring new_cut_buffer=L"";
        wcstring_list_t list;
		if( exec_subshell( cmd, list ) != -1 )
		{
					
			for( i=0; i<list.size(); i++ )
			{
				wcstring next_line = escape_string( list.at(i), 0 );
                if (i > 0) new_cut_buffer += L"\\n";
                new_cut_buffer += next_line;
			}
		
			if( new_cut_buffer.size() > 0 )
			{
				/*  
					The buffer is inserted with backslash escapes,
					since we don't really like tabs, newlines,
					etc. anyway.
				*/
				
                if (cut_buffer == NULL || cut_buffer != new_cut_buffer)
                {
                    free(cut_buffer);
                    cut_buffer = wcsdup(new_cut_buffer.c_str());
                    kill_add_internal( cut_buffer );
                }
			}
		}
	}
}


const wchar_t *kill_yank()
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

