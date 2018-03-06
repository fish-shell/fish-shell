#completion for pkgrm

# magic completion safety check (do not remove this comment)
if not type -q pkgrm
    exit
end

complete -f -c pkgrm -a '(__fish_crux_packages)' -d 'Package to remove'

complete -c pkgrm -o r -l root -d 'Alternative installation root'
complete -f -c pkgrm -o v -l version -d 'Print version'
complete -f -c pkgrm -o h -l help -d 'Print help'
