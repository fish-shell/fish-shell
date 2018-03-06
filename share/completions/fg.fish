
# magic completion safety check (do not remove this comment)
if not type -q fg
    exit
end
complete -c fg -x -a "(__fish_complete_job_pids)"
complete -c fg -s h -l help -d 'Display help and exit'
