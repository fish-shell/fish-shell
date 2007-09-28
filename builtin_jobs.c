/** \file builtin_jobs.c
	Functions for executing the jobs builtin.
*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <wctype.h>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "builtin.h"
#include "proc.h"
#include "parser.h"
#include "common.h"
#include "wgetopt.h"


/**
   Print modes for the jobs builtin
*/
enum
{
	JOBS_DEFAULT, /**< Print lots of general info */
	JOBS_PRINT_PID, /**< Print pid of each process in job */
	JOBS_PRINT_COMMAND, /**< Print command name of each process in job */
	JOBS_PRINT_GROUP, /**< Print group id of job */
}
	;



#ifdef HAVE__PROC_SELF_STAT
/**
   Calculates the cpu usage (in percent) of the specified job.
*/
static int cpu_use( job_t *j )
{
	double u=0;
	process_t *p;

	for( p=j->first_process; p; p=p->next )
	{
		struct timeval t;
		int jiffies;
		gettimeofday( &t, 0 );
		jiffies = proc_get_jiffies( p );

		double t1 = 1000000.0*p->last_time.tv_sec+p->last_time.tv_usec;
		double t2 = 1000000.0*t.tv_sec+t.tv_usec;

/*		fwprintf( stderr, L"t1 %f t2 %f p1 %d p2 %d\n",
  t1, t2, jiffies, p->last_jiffies );
*/

		u += ((double)(jiffies-p->last_jiffies))/(t2-t1);
	}
	return u*1000000;
}
#endif

/**
   Print information about the specified job
*/
static void builtin_jobs_print( job_t *j, int mode, int header )
{
	process_t *p;
	switch( mode )
	{
		case JOBS_DEFAULT:
		{

			if( header )
			{
				/*
				  Print table header before first job
				*/
				sb_append( sb_out, _( L"Job\tGroup\t" ));
#ifdef HAVE__PROC_SELF_STAT
				sb_append( sb_out, _( L"CPU\t" ) );
#endif
				sb_append( sb_out, _( L"State\tCommand\n" ) );
			}

			sb_printf( sb_out, L"%d\t%d\t", j->job_id, j->pgid );

#ifdef HAVE__PROC_SELF_STAT
			sb_printf( sb_out, L"%d%%\t", cpu_use(j) );
#endif
			sb_append( sb_out,
						job_is_stopped(j)?_(L"stopped"):_(L"running"),
						L"\t",
						j->command,
						L"\n",
						(void *)0 );
			break;
		}

		case JOBS_PRINT_GROUP:
		{
			if( header )
			{
				/*
				  Print table header before first job
				*/
				sb_append( sb_out, _( L"Group\n" ));
			}
			sb_printf( sb_out, L"%d\n", j->pgid );
			break;
		}

		case JOBS_PRINT_PID:
		{
			if( header )
			{
				/*
				  Print table header before first job
				*/
				sb_append( sb_out, _( L"Procces\n" ));
			}

			for( p=j->first_process; p; p=p->next )
			{
				sb_printf( sb_out, L"%d\n", p->pid );
			}
			break;
		}

		case JOBS_PRINT_COMMAND:
		{
			if( header )
			{
				/*
				  Print table header before first job
				*/
				sb_append( sb_out, _( L"Command\n" ));
			}

			for( p=j->first_process; p; p=p->next )
			{
				sb_printf( sb_out, L"%ls\n", p->argv[0] );
			}
			break;
		}
	}

}



/**
   The jobs builtin. Used fopr printing running jobs. Defined in builtin_jobs.c.
*/
static int builtin_jobs( wchar_t **argv )
{
	int argc=0;
	int found=0;
	int mode=JOBS_DEFAULT;
	int print_last = 0;
	job_t *j;

	argc = builtin_count_args( argv );
	woptind=0;

	while( 1 )
	{
		const static struct woption
			long_options[] =
			{
				{
					L"pid", no_argument, 0, 'p'
				}
				,
				{
					L"command", no_argument, 0, 'c'
				}
				,
				{
					L"group", no_argument, 0, 'g'
				}
				,
				{
					L"last", no_argument, 0, 'l'
				}
				,
				{
					L"help", no_argument, 0, 'h'
				}
				,
				{
					0, 0, 0, 0
				}
			}
		;

		int opt_index = 0;

		int opt = wgetopt_long( argc,
								argv,
								L"pclgh",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                sb_printf( sb_err,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );

				builtin_print_help( argv[0], sb_err );


				return 1;


			case 'p':
				mode=JOBS_PRINT_PID;
				break;

			case 'c':
				mode=JOBS_PRINT_COMMAND;
				break;

			case 'g':
				mode=JOBS_PRINT_GROUP;
				break;

			case 'l':
			{
				print_last = 1;
				break;
			}

			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;				

			case '?':
				builtin_unknown_option( argv[0], argv[woptind-1] );
				return 1;

		}
	}


	/*
	  Do not babble if not interactive
	*/
	if( builtin_out_redirect )
	{
		found=1;
	}

	if( print_last )
	{
		/*
		  Ignore unconstructed jobs, i.e. ourself.
		*/
		for( j=first_job; j; j=j->next )
		{
			if( (j->flags & JOB_CONSTRUCTED) && !job_is_completed(j) )
			{
				builtin_jobs_print( j, mode, !found );
				return 0;
			}
		}

	}
	else
	{
		if( woptind < argc )
		{
			int i;

			found = 1;

			for( i=woptind; i<argc; i++ )
			{
				long pid;
				wchar_t *end;
				errno=0;
				pid=wcstol( argv[i], &end, 10 );
				if( errno || *end )
				{
					sb_printf( sb_err,
							   _( L"%ls: '%ls' is not a job\n" ),
							   argv[0],
							   argv[i] );
					return 1;
				}

				j = job_get_from_pid( pid );

				if( j && !job_is_completed( j ) )
				{
					builtin_jobs_print( j, mode, !found );
				}
				else
				{
					sb_printf( sb_err,
							   _( L"%ls: No suitable job: %d\n" ),
							   argv[0],
							   pid );
					return 1;
				}
			}
		}
		else
		{
			for( j= first_job; j; j=j->next )
			{
				/*
				  Ignore unconstructed jobs, i.e. ourself.
				*/
				if( (j->flags & JOB_CONSTRUCTED) && !job_is_completed(j) )
				{
					builtin_jobs_print( j, mode, !found );
					found = 1;
				}
			}
		}
	}

	if( !found )
	{
		sb_printf( sb_out,
				   _( L"%ls: There are no jobs\n" ),
				   argv[0] );
	}

	return 0;
}

