#completion for pkg_add

# magic completion safety check (do not remove this comment)
if not type -q pkg_add
    exit
end

complete -c pkg_add -o D -d 'failsafe to overwrite' -xa 'allversions arch checksum dontmerge donttie downgrade installed libdepends nonroot paranoid repair scripts SIGNER snap unsigned updatedepends'
complete -c pkg_add -o V -d 'Turn on stats'
complete -c pkg_add -o a -d 'Automated package installation'
complete -c pkg_add -o h -d 'Print help'
complete -c pkg_add -o u -d 'Update packages'
complete -c pkg_add -o z -d 'Fuzzy match'

