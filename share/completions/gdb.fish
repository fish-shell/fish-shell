#
# Command specific completions for the gdb command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c gdb -o help -s h -d 'List all options, with brief explanations'
complete -c gdb -o symbols -s s -d 'Read symbol table from file file' -r
complete -c gdb -o write -d 'Enable writing into executable and core files'
complete -c gdb -o exec -s e -d 'Use file file as the executable file to execute when appropri ate, and for examining pure data in conjunction with a core dump' -r
complete -c gdb -o se -d 'Read symbol table from file file and use it as the executable file' -r
complete -c gdb -o core -s c -d 'Use file file as a core dump to examine' -r
complete -c gdb -o command -s x -d 'Execute GDB commands from file file' -r
complete -c gdb -o directory -s d -d 'Add directory to the path to search for source files' -x -a '(__fish_complete_directories (commandline -ct))'
complete -c gdb -o nx -s n -d 'Do not execute commands from any .gdbinit files'
complete -c gdb -o quiet -s q -d 'Quiet'
complete -c gdb -o batch -d 'Run in batch mode'
complete -c gdb -o cd -d 'Run GDB using directory as its working directory, instead of the current directory' -x -a '(__fish_complete_directories (commandline -ct))'
complete -c gdb -o fullname -s f -d 'Emacs sets this option when it runs GDB as a subprocess'
complete -c gdb -s b -d 'Bps Set the line speed (baud rate or bits per second) of any serial interface used by GDB for remote debugging'
complete -c gdb -o tty -d 'Run using device for your programs standard input and output' -r
complete -c gdb -l args -d 'Pass arguments after the program name to the program when it is run'
complete -c gdb -o tui -d 'Run GDB using a text (console) user interface'
