function _justfile_targets
    just -l | tail -n +2 | sed -e 's/^[[:blank:]]*//' -e 's/[[:blank:]]*#/\t/' -e 's/[[:blank:]]*[*+][^[:blank:]]*//'
end
complete -c just -f -a '(_justfile_targets)'
