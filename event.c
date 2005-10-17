/** \file function.c

	Functions for storing and retrieving function information.

*/
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <string.h>

#include "config.h"
#include "util.h"
#include "function.h"
#include "proc.h"
#include "parser.h"
#include "common.h"
#include "event.h"
#include "signal.h"

/**
   Number of signals that can be queued before an overflow occurs
*/
#define SIG_UNHANDLED_MAX 64

/**
   This struct contains a list of generated signals waiting to be
   dispatched
*/
typedef struct
{
	int count;
	int overflow;	
	int signal[SIG_UNHANDLED_MAX];	
}
	signal_list_t;

/*
  The signal event list. Actually two separate lists. One which is
  active, which is the one that new events is written to. The inactive
  one contains the events that are currently beeing performed.
*/
static signal_list_t sig_list[2];

/**
   The index of sig_list that is the list of signals currently written to
*/
static int active_list=0;

/**
   List of event handlers
*/
static array_list_t *events;
/**
   List of event handlers that should be removed
*/
static array_list_t *killme;

/**
   Tests if one event instance matches the definition of a event
   class. If the class defines a function name, that will also be a
   match criterion.

*/
static int event_match( event_t *class, event_t *instance )
{
	if( class->function_name && instance->function_name )
	{
		if( wcscmp( class->function_name, instance->function_name ) != 0 )
			return 0;
	}

	if( class->type == EVENT_ANY )
		return 1;
	
	if( class->type != instance->type )
		return 0;
	
	
	switch( class->type )
	{
			
		case EVENT_SIGNAL:
			if( class->param1.signal == EVENT_ANY_SIGNAL )
				return 1;
			return class->param1.signal == instance->param1.signal;
		
		case EVENT_VARIABLE:
			return wcscmp( instance->param1.variable,
				       class->param1.variable )==0;
			
		case EVENT_EXIT:
			if( class->param1.pid == EVENT_ANY_PID )
				return 1;
			return class->param1.pid == instance->param1.pid;

		case EVENT_JOB_ID:
			return class->param1.job_id == instance->param1.job_id;
	}
	
	/**
	   This should never be reached
	*/
	return 0;	
}


/**
   Create an identical copy of an event. Use deep copying, i.e. make
   duplicates of any strings used as well.
*/
static event_t *event_copy( event_t *event )
{
	event_t *e = malloc( sizeof( event_t ) );
	if( !e )
		die_mem();
	memcpy( e, event, sizeof(event_t));

	if( e->function_name )
		e->function_name = wcsdup( e->function_name );

	if( e->type == EVENT_VARIABLE )
		e->param1.variable = wcsdup( e->param1.variable );
		
	return e;
}

void event_add_handler( event_t *event )
{
	event_t *e = event_copy( event );

	if( !events )
		events = al_new();	

	if( e->type == EVENT_SIGNAL )
	{
		signal_handle( e->param1.signal, 1 );
	}
	
	al_push( events, e );	
}

void event_remove( event_t *criterion )
{
	int i;
	array_list_t *new_list=0;
	event_t e;
	
	/*
	  Because of concurrency issues, env_remove does not actually free
	  any events - instead it simply moves all events that should be
	  removed from the event list to the killme list.
	*/
	
	if( !events )
		return;	

	for( i=0; i<al_get_count( events); i++ )
	{
		event_t *n = (event_t *)al_get( events, i );		
		if( event_match( criterion, n ) )
		{
			if( !killme )
				killme = al_new();
			
			al_push( killme, n );			

			/*
			  If this event was a signal handler and no other handler handles
			  the specified signal type, do not handle that type of signal any
			  more.
			*/
			if( n->type == EVENT_SIGNAL )
			{
				e.type = EVENT_SIGNAL;
				e.param1.signal = n->param1.signal;
				e.function_name = 0;
				
				if( event_get( &e, 0 ) == 1 )
				{
					signal_handle( e.param1.signal, 0 );
				}		
			}
		}
		else
		{
			if( !new_list )
				new_list = al_new();
			al_push( new_list, n );
		}
	}
	al_destroy( events );
	free( events );	
	events = new_list;
}

int event_get( event_t *criterion, array_list_t *out )
{
	int i;
	int found = 0;
	
	if( !events )
		return 0;	
	
	for( i=0; i<al_get_count( events); i++ )
	{
		event_t *n = (event_t *)al_get( events, i );		
		if( event_match(criterion, n ) )
		{
			found++;
			if( out )
				al_push( out, n );
		}		
	}
	return found;
}

/**
   Free all events in the kill list
*/
static void event_free_kills()
{
	int i;
	if( !killme )
		return;
	
	for( i=0; i<al_get_count( killme ); i++ )
	{
		event_t *roadkill = (event_t *)al_get( killme, i );
		event_free( roadkill );
	}
	al_truncate( killme, 0 );
}

/**
   Test if the specified event is waiting to be killed
*/
static int event_is_killed( event_t *e )
{
	int i;
	if( !killme )
		return 0;
	
	for( i=0; i<al_get_count( killme ); i++ )
	{
		event_t *roadkill = (event_t *)al_get( events, i );
		if( roadkill ==e )
			return 1;
		
	}
	return 0;
}	

/**
   Perform the specified event. Since almost all event firings will
   not match a single event handler, we make sureto optimize the 'no
   matches' path. This means that nothing is allocated/initialized
   unless that is needed.
*/
static void event_fire_internal( event_t *event, array_list_t *arguments )
{
	int i, j;
	string_buffer_t *b=0;
	array_list_t *fire=0;
	
	/*
	  First we free all events that have been removed
	*/
	event_free_kills();	

	if( !events )
		return;

	/*
	  Then we iterate over all events, adding events that should be
	  fired to a second list. We need to do this in a separate step
	  since an event handler might call event_remove or
	  event_add_handler, which will change the contents of the \c
	  events list.
	*/
	for( i=0; i<al_get_count( events ); i++ )
	{
		event_t *criterion = (event_t *)al_get( events, i );
		
		/*
		  Check if this event is a match
		*/
		if(event_match( criterion, event ) )
		{
			if( !fire )
				fire = al_new();
			al_push( fire, criterion );
		}
	}
	
	/*
	  No matches. Time to return.
	*/
	if( !fire )
		return;
		
	/*
	  Iterate over our list of matching events
	*/
	
	for( i=0; i<al_get_count( fire ); i++ )
	{
		event_t *criterion = (event_t *)al_get( fire, i );
		
		/*
		  Check if this event has been removed, if so, dont fire it
		*/
		if( event_is_killed( criterion ) )
			continue;

		/*
		  Fire event
		*/
		if( !b )
			b = sb_new();
		else
			sb_clear( b );
		
		sb_append( b, criterion->function_name );
		
		for( j=0; j<al_get_count(arguments); j++ )
		{
			wchar_t *arg_esc = escape( (wchar_t *)al_get( arguments, j), 0 );		
			sb_append( b, L" " );
			sb_append( b, arg_esc );
			free( arg_esc );				
		}

//		debug( 1, L"Event handler fires command '%ls'", (wchar_t *)b->buff );
		
		is_subshell=1;
		is_interactive=1;
				
		eval( (wchar_t *)b->buff, 0, TOP );
		is_subshell=0;
		is_interactive=1;
		
	}

	if( b )
	{
		sb_destroy( b );
		free( b );		
	}
	
	if( fire )
	{
		al_destroy( fire );
		free( fire );
	}
	
	/*
	  Free killed events
	*/
	event_free_kills();	
	
}

/**
   Handle all pending signal events
*/
static void event_fire_signal_events()
{
	while( sig_list[active_list].count > 0 )
	{
		int i;
		signal_list_t *lst;
		event_t e;
		array_list_t a;
		al_init( &a );		

		/*
		  Switch signal lists
		*/
		sig_list[1-active_list].count=0;
		sig_list[1-active_list].overflow=0;
		active_list=1-active_list;

		/*
		  Set up 
		*/
		e.type=EVENT_SIGNAL;
		e.function_name=0;
		
		lst = &sig_list[1-active_list];
		
		if( lst->overflow )
		{
			debug( 0, L"Signal list overflow. Signals have been ignored" );
		}
		
		/*
		  Send all signals in our private list
		*/
		for( i=0; i<lst->count; i++ )
		{
			e.param1.signal = lst->signal[i];
			al_set( &a, 0, sig2wcs( e.param1.signal ) );			
			event_fire_internal( &e, &a );
		}
		
		al_destroy( &a );
		
	}	
}


void event_fire( event_t *event, array_list_t *arguments )
{
	//int is_event_old = is_event;
	is_event++;
	
	if( event && (event->type == EVENT_SIGNAL) )
	{
		/*
		  This means we are in a signal handler. We must be very
		  careful not do do anything that could cause a memory
		  allocation or something else that might be illegal in a
		  signal handler.
		*/
		if( sig_list[active_list].count < SIG_UNHANDLED_MAX )
			sig_list[active_list].signal[sig_list[active_list].count++]=event->param1.signal;
		else
			sig_list[active_list].overflow=1;
		
	}
	else
	{
		event_fire_signal_events();
		
		if( event )
			event_fire_internal( event, arguments );
		
	}	
	is_event--;// = is_event_old;	
}


void event_init()
{
	sig_list[active_list].count=0;	
}

void event_destroy()
{
	if( events )
	{
		al_foreach( events, (void (*)(const void *))&event_free );
		al_destroy( events );
		free( events );		
		events=0;
	}
	if( killme )
	{
		al_foreach( killme, (void (*)(const void *))&event_free );
		al_destroy( killme );
		free( killme );		
		killme=0;		
	}	
}

void event_free( event_t *e )
{
	free( (void *)e->function_name );
	if( e->type == EVENT_VARIABLE )
		free( (void *)e->param1.variable );
	free( e );
}
		
