
# magic completion safety check (do not remove this comment)
if not type -q golint
    exit
end
complete -c golint -l min_confidence -o min_confidence -d "Minimum confidence of a problem to print it" -x
complete -c golint -l set_exit_status -o set_exit_status -d "Set exit status to 1 if any issues are found"
complete -c golint -l help -o help -s h -d "Show help"
