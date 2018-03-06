
# magic completion safety check (do not remove this comment)
if not type -q als
    exit
end
complete -c als -w atool
complete -c als -a '(__fish_complete_atool_archive_contents)' -d 'Archive content'
