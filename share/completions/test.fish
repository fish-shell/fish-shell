
# magic completion safety check (do not remove this comment)
if not type -q test
    exit
end

complete -c test -l help -d "Display help and exit"
#complete -c test -l version -d "Display version and exit"
complete -c test -a ! -d "Negate expression"
complete -c test -s a -d "Logical AND"
complete -c test -s o -d "Logical OR"
complete -c test -s n -d "String length is non-zero"
complete -c test -s z -d "String length is zero"
complete -c test -a = -d "Strings are equal"
complete -c test -a != -d "Strings are not equal"
complete -c test -o eq -d "Integers are equal"
complete -c test -o ge -d "Left integer larger than or equal to right integer"
complete -c test -o gt -d "Left integer larger than right integer"
complete -c test -o le -d "Left integer less than or equal to right integer"
complete -c test -o lt -d "Left integer less than right integer"
complete -c test -o ne -d "Left integer not equal to right integer"
# builtin test does not do these
#complete -c test -o ef -d "Left file equal to right file"
#complete -c test -o nt -d "Left file newer than right file"
#complete -c test -o ot -d "Left file older than right file"
complete -c test -s b -d "File is block device"
complete -c test -s c -d "File is character device"
complete -c test -s d -d "File is directory"
complete -c test -s e -d "File exists"
complete -c test -s f -d "File is regular"
complete -c test -s g -d "File is set-group-ID"
complete -c test -s G -d "File owned by our effective group ID"
complete -c test -s L -d "File is a symlink"
complete -c test -s O -d "File owned by our effective user ID"
complete -c test -s p -d "File is a named pipe"
complete -c test -s r -d "File is readable"
complete -c test -s s -d "File size is non-zero"
complete -c test -s S -d "File is a socket"
complete -c test -s t -d "FD is a terminal"
complete -c test -s u -d "File set-user-ID bit is set"
complete -c test -s w -d "File is writable"
complete -c test -s x -d "File is executable"

