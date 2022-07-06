#
# Command specific completions for the gdb command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c gdb -o help -s h -d 'List all options, with brief explanations'
complete -c gdb -o symbols -s s -d 'Read symbol table from <file>' -r
complete -c gdb -o write -d 'Enable writing into executable and core files'
complete -c gdb -o exec -s e -d 'Set executable' -r
complete -c gdb -o se -d 'Read symbol table from <file> and execute it' -r
complete -c gdb -o core -s c -d 'Use <file> as a core dump to examine' -r
complete -c gdb -o command -s x -d 'Execute GDB commands from <file>' -r
complete -c gdb -o directory -s d -d 'Add directory with source files' -x -a '(__fish_complete_directories (commandline -ct))'
complete -c gdb -o nx -s n -d 'Do not execute commands from any .gdbinit files'
complete -c gdb -o quiet -s q -d Quiet
complete -c gdb -o batch -d 'Run in batch mode'
complete -c gdb -o cd -d 'Set GDB\'s working directory' -x -a '(__fish_complete_directories (commandline -ct))'
complete -c gdb -o fullname -s f -d 'Used by Emacs'
complete -c gdb -s b -d 'Set throughput in bps for remote debugging'
complete -c gdb -o tty -d 'Set device for the program\'s stdin/stdout' -r
complete -c gdb -l args -d 'Pass arguments to the program after its name'
complete -c gdb -o tui -d 'Run GDB using a text (console) user interface'
