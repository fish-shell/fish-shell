#
# Command specific completions for the m4 command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c m4 -l help -d 'Display this help and exit'
complete -c m4 -l version -d 'Output version information and exit'
complete -c m4 -s E -l fatal-warnings -d 'Once: warnings become errors, twice: stop execution at first error'
complete -c m4 -s i -l interactive -d 'Unbuffer output, ignore interrupts'
complete -c m4 -s P -l prefix-builtins -d 'Force a \'m4_\' prefix to all builtins'
complete -c m4 -s Q -l quiet -l silent -d 'Suppress some warnings for builtins'
complete -c m4 -l warn-macro-sequence -d 'Warn if macro definition matches REGEXP, default \$\({[^}]*}\|[0-9][0-9]+\)'
complete -c m4 -s D -l define -d 'Define NAME as having VALUE, or empty'
complete -c m4 -s I -l include -d 'Append DIRECTORY to include path'
complete -c m4 -s s -l synclines -d 'Generate \'#line NUM "FILE"\' lines'
complete -c m4 -s U -l undefine -d 'Undefine NAME'
complete -c m4 -s G -l traditional -d 'Suppress all GNU extensions'
complete -c m4 -s H -l hashsize -d 'Set symbol lookup hash table size [509]'
complete -c m4 -s L -l nesting-limit -d 'Change artificial nesting limit [1024]'
complete -c m4 -s F -l freeze-state -d 'Produce a frozen state on FILE at end'
complete -c m4 -s R -l reload-state -d 'Reload a frozen state from FILE at start'
complete -c m4 -s d -l debug -d 'Set debug level (no FLAGS implies \'aeq\')'
complete -c m4 -l debugfile -d 'Redirect debug and trace output'
complete -c m4 -s l -l arglength -d 'Restrict macro tracing size'
complete -c m4 -s t -l trace -d 'Trace NAME when it is defined'
