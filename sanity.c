/** \file sanity.c
	Functions for performing sanity checks on the program state
*/
#include "config.h"

#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>


#include "fallback.h"
#include "util.h"

#include "common.h"
#include "sanity.h"
#include "proc.h"
#include "history.h"
#include "reader.h"
#include "kill.h"
#include "wutil.h"


/**
   Status from earlier sanity checks
*/
static int insane;

void sanity_lose()
{
	debug( 0, _(L"Errors detected, shutting down") );
	insane = 1;
}

int sanity_check()
{
	if( !insane )
		if( is_interactive )
			history_sanity_check();
	if( !insane )
		reader_sanity_check();
	if( !insane )
		kill_sanity_check();
	if( !insane )
		proc_sanity_check();
	
	return insane;
}

void validate_pointer( const void *ptr, const wchar_t *err, int null_ok )
{
	
	/* 
	   Test if the pointer data crosses a segment boundary. 
	*/
	
	if( (0x00000003l & (long)ptr) != 0 )
	{
		debug( 0, _(L"The pointer '%ls' is invalid"), err );
		sanity_lose();		
	}
	
	if((!null_ok) && (ptr==0))
	{
		debug( 0, _(L"The pointer '%ls' is null"), err );
		sanity_lose();		
	}
}


