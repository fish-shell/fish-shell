function _justfile_targets
    just --summary 2>/dev/null | string split ' '
end
complete -c just -a '(_justfile_targets)' -f
