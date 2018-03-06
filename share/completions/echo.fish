
# magic completion safety check (do not remove this comment)
if not type -q echo
    exit
end
complete -c echo -s n -d "Do not output a newline"
complete -c echo -s s -d "Do not separate arguments with spaces"
complete -c echo -s E -d "Disable backslash escapes"
complete -c echo -s e -d "Enable backslash escapes"
complete -c echo -s h -l help -d "Display help and exit"
