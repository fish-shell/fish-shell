#RUN: fish=%fish %fish %s

# ENOTDIR (not on path) -> status 126
$fish -c 'touch file_8de9325e && ./file_8de9325e/bar; echo $status && rm file_8de9325e'
#CHECKERR: fish: Unknown command './file_8de9325e/bar': One of the parent files is not a directory.
#CHECKERR: touch file_8de9325e && ./file_8de9325e/bar; echo $status && rm file_8de9325e
#CHECKERR:                        ^~~~~~~~~~~~~~~~~~^
#CHECK: 126

# ENAMETOOLONG -> status 126
$fish -c 'name=(string repeat -n 300 A) ./$name; echo $status'
#CHECKERR: fish: Unknown command './{{(?:A{300})}}': File name is too long.
#CHECKERR: name=(string repeat -n 300 A) ./$name; echo $status
#CHECKERR:                               ^~~~~~^
#CHECK: 126

# real ENOEXEC from exec -> status 126.
# '.exe' extension is for cygwin_noacl systems where `chmod +x` has no effect
$fish -c 'head -c 1 /dev/zero >./file_8de9325e.exe && chmod +x ./file_8de9325e.exe && ./file_8de9325e.exe; echo $status && rm ./file_8de9325e.exe'
#CHECKERR: exec: Failed to execute process './file_8de9325e.exe': The file could not be run by the operating system.
#CHECKERR: exec: Maybe the interpreter directive (#! line) is broken?
#CHECK: 126

# synthetic ENOEXEC from fish due to wrong file type -> status 126
$fish -c 'mkdir dir_8de9325e && ./dir_8de9325e arg1; echo $status && rmdir dir_8de9325e'
#CHECKERR: fish: Unknown command './dir_8de9325e': Files of this type cannot be executed.
#CHECKERR: mkdir dir_8de9325e && ./dir_8de9325e arg1; echo $status && rmdir dir_8de9325e
#CHECKERR:                       ^~~~~~~~~~~~~^
#CHECK: 126

__fish_theme_migrate # make sure the interactive fish doesn't need mkdir in PATH
if __fish_is_cygwin
    # The Cygwin/MSYS DLLs must be in the path, otherwise fish cannot be
    # executed
    set -g PATH /usr/bin
else
    set -g PATH
end

$fish -c "nonexistent-command-1234 banana rama"
#CHECKERR: fish: Unknown command: nonexistent-command-1234
#CHECKERR: fish:
#CHECKERR: nonexistent-command-1234 banana rama
#CHECKERR: ^~~~~~~~~~~~~~~~~~~~~~~^
$fish -C 'function fish_command_not_found; echo cmd-not-found; end' -ic "nonexistent-command-1234 1 2 3 4"
#CHECKERR: cmd-not-found
#CHECKERR: fish:
#CHECKERR: nonexistent-command-1234 1 2 3 4
#CHECKERR: ^~~~~~~~~~~~~~~~~~~~~~~^
$fish -C 'function fish_command_not_found; echo command-not-found $argv; end' -c "nonexistent-command-abcd foo bar baz"
#CHECKERR: command-not-found nonexistent-command-abcd foo bar baz
#CHECKERR: fish:
#CHECKERR: nonexistent-command-abcd foo bar baz
#CHECKERR: ^~~~~~~~~~~~~~~~~~~~~~~^

$fish -C 'functions --erase fish_command_not_found' -c 'nonexistent-command apple friday'
#CHECKERR: fish: Unknown command: nonexistent-command
#CHECKERR: nonexistent-command apple friday
#CHECKERR: ^~~~~~~~~~~~~~~~~~^

command -v nonexistent-command-1234
echo $status
#CHECK: 127

# ENOTDIR (on path) -> status 126
set -g PATH .
echo banana >foobar
foobar --banana
echo $status
# CHECKERR: {{.*}}checks/command-not-found.fish (line {{\d+}}): Unknown command './foobar': Missing execute permissions for the file or one of its parent directories.
# CHECKERR: foobar --banana
# CHECKERR: ^~~~~^
# CHECK: 126

exit 0
