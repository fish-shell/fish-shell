# completions for Python pkginfo
if pkginfo --help 2>/dev/null | string match -qr Python
    complete -f -c pkginfo -s h -l help -d 'Print help'
    complete -x -c pkginfo -s m -l metadata-version -d 'Override metadata version'
    complete -x -c pkginfo -s f -l field -d 'Specify an output field (repeatable)'
    complete -x -c pkginfo -s d -l download-url-prefix -d 'Download URL prefix'
    complete -f -c pkginfo -l simple -d 'Output as simple key-value pairs'
    complete -f -c pkginfo -s s -l skip -d 'Skip missing values in simple output'
    complete -f -c pkginfo -s S -l no-skip -d 'Do not skip missing values in simple output'
    complete -f -c pkginfo -l single -d 'Output delimited values'
    complete -x -c pkginfo -l item-delim -d 'Delimiter for fields in single-line output'
    complete -x -c pkginfo -l sequence-delim -d 'Delimiter for multi-valued fields'
    complete -f -c pkginfo -l csv -d 'Output as CSV'
    complete -f -c pkginfo -l ini -d 'Output as INI'
    complete -f -c pkginfo -l json -d 'Output as JSON'
    # completions for CRUX Linux' pkginfo
else
    complete -f -c pkginfo -o i -l installed -d 'List installed packages'
    complete -f -c pkginfo -o l -l list -a '(__fish_crux_packages)' -r -d 'Package whose files to list'
    complete -f -c pkginfo -o o -l owner -d 'Print the package owning file matching pattern'
    complete -f -c pkginfo -o f -l footprint -d 'Print footprint for file'
    complete -f -c pkginfo -o r -l root -d 'Specify alternative installation root'
    complete -f -c pkginfo -o v -l version -d 'Print version of pkgutils'
    complete -f -c pkginfo -o h -l help -d 'Print help'
end
