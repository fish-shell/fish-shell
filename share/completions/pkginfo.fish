#completion for pkginfo

# magic completion safety check (do not remove this comment)
if not type -q pkginfo
    exit
end

complete -f -c pkginfo -o i -l installed -d 'List installed packages'
complete -f -c pkginfo -o l -l list -a '(__fish_crux_packages)' -r -d 'Package whose files to list'
complete -f -c pkginfo -o o -l owner -d 'Print the package owning file matching pattern'
complete -f -c pkginfo -o f -l footprint -d 'Print footprint for file'
complete -f -c pkginfo -o r -l root -d 'Specify alternative installation root'
complete -f -c pkginfo -o v -l version -d 'Print version of pkgutils'
complete -f -c pkginfo -o h -l help -d 'Print help'
