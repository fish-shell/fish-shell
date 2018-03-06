
# magic completion safety check (do not remove this comment)
if not type -q acat
    exit
end
complete -c acat -w atool
complete -c acat -a '(__fish_complete_atool_archive_contents)' -d 'Archive content'
