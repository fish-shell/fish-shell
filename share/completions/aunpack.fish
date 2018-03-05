
# magic completion safety check (do not remove this comment)
if not type -q aunpack
    exit
end
complete -c aunpack -w atool
complete -c aunpack -a '(__fish_complete_atool_archive_contents)' -d 'Archive content'
