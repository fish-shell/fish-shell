
# magic completion safety check (do not remove this comment)
if not type -q bg
    exit
end
complete -c bg -x -a "(__fish_complete_job_pids)"
complete -c bg -s h -l help -d 'Display help and exit'
