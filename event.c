/** \file event.c

	Functions for handling event triggers

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
#include "wutil.h"
#include "function.h"
#include "proc.h"
#include "parser.h"
#include "common.h"
#include "event.h"
#include "signal.h"
#include "translate.h"

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
	/**
	   Number of delivered signals
	*/
	int count;
	/**
	   Whether signals have been skipped
	*/
	int overflow;	
	/**
	   Array of signal events
	*/
	int signal[SIG_UNHANDLED_MAX];	
}
	signal_list_t;

/**
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
   List of events that have been sent but have not yet been delivered because they are blocked.
*/
static array_list_t *blocked;

/**
   String buffer used for formating event descriptions in event_get_desc()
*/
static string_buffer_t *get_desc_buff=0;

/**
   Tests if one event instance matches the definition of a event
   class. If both the class and the instance name a function,
   they must name the same function.

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
static event_t *event_copy( event_t *event, int copy_arguments )
{
	event_t *e = malloc( sizeof( event_t ) );
	if( !e )
		die_mem();
	memcpy( e, event, sizeof(event_t));

	if( e->function_name )
		e->function_name = wcsdup( e->function_name );

	if( e->type == EVENT_VARIABLE )
		e->param1.variable = wcsdup( e->param1.variable );
		
	al_init( &e->arguments );
	if( copy_arguments )
	{
		int i;
		for( i=0; i<al_get_count( &event->arguments ); i++ )
		{
			al_push( &e->arguments, wcsdup( (wchar_t *)al_get( &event->arguments, i ) ) );
		}
				
	}
	
	return e;
}

/**
   Test if specified event is blocked
*/
static int event_is_blocked( event_t *e )
{
	block_t *block;
	event_block_t *eb;
	
	for( block = current_block; block; block = block->outer )
	{
		for( eb = block->first_event_block; eb; eb=eb->next )
		{
			if( eb->type & (1<<EVENT_ANY ) )
				return 1;
			if( eb->type & (1<<e->type) )
				return 1;
		}
	}
	for( eb = global_event_block; eb; eb=eb->next )
	{
		if( eb->type & (1<<EVENT_ANY ) )
			return 1;
		if( eb->type & (1<<e->type) )
			return 1;
		return 1;
		
	}
	
	return 0;
}

const wchar_t *event_get_desc( event_t *e )
{
	if( !get_desc_buff )
	{
		get_desc_buff=malloc(sizeof(string_buffer_t) );
		if( !get_desc_buff )
			die_mem();
		sb_init( get_desc_buff );
	}
	else
	{
		sb_clear( get_desc_buff );
	}
	
	switch( e->type )
	{
		
		case EVENT_SIGNAL:
			sb_printf( get_desc_buff, _(L"signal handler for %ls (%ls)"), sig2wcs(e->param1.signal ), signal_get_desc( e->param1.signal ) );
			break;
		
		case EVENT_VARIABLE:
			sb_printf( get_desc_buff, _(L"handler for variable '%ls'"), e->param1.variable );
			break;
		
		case EVENT_EXIT:
			if( e->param1.pid > 0 )
			{
				sb_printf( get_desc_buff, _(L"exit handler for process %d"), e->param1.pid );
			}
			else
			{
				job_t *j = job_get_from_pid( -e->param1.pid );
				if( j )
					sb_printf( get_desc_buff, _(L"exit handler for job %d, '%ls'"), j->job_id, j->command );
				else
					sb_printf( get_desc_buff, _(L"exit handler for job with process group %d"), -e->param1.pid );
			}
			
			break;
		
		case EVENT_JOB_ID:
		{
			job_t *j = job_get( e->param1.job_id );
			if( j )
				sb_printf( get_desc_buff, _(L"exit handler for job %d, '%ls'"), j->job_id, j->command );
			else
				sb_printf( get_desc_buff, _(L"exit handler for job with job id %d"), j->job_id );

			break;
		}
		
	}
	
	return (const wchar_t *)get_desc_buff->buff;
}


void event_add_handler( event_t *event )
{
	event_t *e;

	e = event_copy( event, 0 );

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
static void event_fire_internal( event_t *event )
{
	int i, j;
	string_buffer_t *b=0;
	array_list_t *fire=0;

	int was_interactive = is_interactive;
	
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
		
		for( j=0; j<al_get_count(&event->arguments); j++ )
		{
			wchar_t *arg_esc = escape( (wchar_t *)al_get( &event->arguments, j), 0 );		
			sb_append( b, L" " );
			sb_append( b, arg_esc );
			free( arg_esc );				
		}

//		debug( 1, L"Event handler fires command '%ls'", (wchar_t *)b->buff );
		
		/*
		  Event handlers are not part of the main flow of code, so
		  they are marked as non-interactive and as a subshell
		*/
		is_interactive=0;
		parser_push_block( EVENT );
		current_block->param1.event = event;
		eval( (wchar_t *)b->buff, 0, TOP );
		parser_pop_block();
				
	}

	/*
	  Restore interactivity flags
	*/
	is_interactive = was_interactive;

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
static void event_fire_delayed()
{

	int i;

	if( blocked && is_event==1)
	{
		array_list_t *new_blocked = 0;
		
		for( i=0; i<al_get_count( blocked ); i++ )
		{
			event_t *e = (event_t *)al_get( blocked, i );
			if( event_is_blocked( e ) )
			{
				if( !new_blocked )
					new_blocked = al_new();
				al_push( new_blocked, e );			
			}
			else
			{
				event_fire_internal( e );
				event_free( e );
			}
		}		
		al_destroy( blocked );
		free( blocked );
		blocked = new_blocked;
	}

	while( sig_list[active_list].count > 0 )
	{
		signal_list_t *lst;
		event_t e;
		al_init( &e.arguments );		

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
			debug( 0, _( L"Signal list overflow. Signals have been ignored." ) );
		}
		
		/*
		  Send all signals in our private list
		*/
		for( i=0; i<lst->count; i++ )
		{
			e.param1.signal = lst->signal[i];
			al_set( &e.arguments, 0, sig2wcs( e.param1.signal ) );			
			if( event_is_blocked( &e ) )
			{
				if( !blocked )
					blocked = al_new();
				al_push( blocked, event_copy(&e, 1) );			
			}
			else
			{
				event_fire_internal( &e );
			}
		}
		
		al_destroy( &e.arguments );
		
	}	
}


void event_fire( event_t *event )
{
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
		
		event_fire_delayed();
			
		if( event )
		{
			if( event_is_blocked( event ) )
			{
				if( !blocked )
					blocked = al_new();
				
				al_push( blocked, event_copy(event, 1) );
			}
			else
			{
				event_fire_internal( event );
			}
		}
		
	}	
	is_event--;
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

	if( get_desc_buff )
	{
		sb_destroy( get_desc_buff );
		free( get_desc_buff );
	}
	
}

void event_free( event_t *e )
{
	/*
	  When apropriate, we clear the argument vector
	*/
	al_foreach( &e->arguments, (void (*)(const void *))&free );
	al_destroy( &e->arguments );

	free( (void *)e->function_name );
	if( e->type == EVENT_VARIABLE )
		free( (void *)e->param1.variable );
	free( e );
}
