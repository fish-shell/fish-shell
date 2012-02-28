/** \file postfork.cpp

	Functions that we may safely call after fork().
*/

#include "postfork.h"

/**
   This function should be called by both the parent process and the
   child right after fork() has been called. If job control is
   enabled, the child is put in the jobs group, and if the child is
   also in the foreground, it is also given control of the
   terminal. When called in the parent process, this function may
   fail, since the child might have already finished and called
   exit. The parent process may safely ignore the exit status of this
   call.

   Returns 0 on sucess, -1 on failiure. 
*/
int set_child_group( job_t *j, process_t *p, int print_errors )
{
	int res = 0;
	
	if( job_get_flag( j, JOB_CONTROL ) )
	{
		if (!j->pgid)
		{
			j->pgid = p->pid;
		}
		
		if( setpgid (p->pid, j->pgid) )
		{
			if( getpgid( p->pid) != j->pgid && print_errors )
			{
				debug( 1, 
				       _( L"Could not send process %d, '%ls' in job %d, '%ls' from group %d to group %d" ),
				       p->pid,
				       p->argv0(),
				       j->job_id,
				       j->command_cstr(),
				       getpgid( p->pid),
				       j->pgid );

				wperror( L"setpgid" );
				res = -1;
			}
		}
	}
	else
	{
		j->pgid = getpid();
	}

	if( job_get_flag( j, JOB_TERMINAL ) && job_get_flag( j, JOB_FOREGROUND ) )
	{
		if( tcsetpgrp (0, j->pgid) && print_errors )
		{
			debug( 1, _( L"Could not send job %d ('%ls') to foreground" ), 
				   j->job_id, 
				   j->command_cstr() );
			wperror( L"tcsetpgrp" );
			res = -1;
		}
	}

	return res;
}

