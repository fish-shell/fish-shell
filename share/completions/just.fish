function _justfile_targets
    just --summary 2>/dev/null | string split ' ' | string match -v '.*/.*'
end
complete -c just -a '(_justfile_targets)' -f
