#completion for rejmerge

# magic completion safety check (do not remove this comment)
if not type -q rejmerge
    exit
end

complete -f -c rejmerge -o r -l root -d 'Alternative root'
complete -f -c rejmerge -o v -l version -d 'Print version'
complete -f -c rejmerge -o h -l help -d 'Print help'
