function _justfile_targets
    just -l | tail -n +2 | string trim -l | string replace -r '(\s*#\s*)' '\t' | string replace -r '(\s*[\*\+][^\s]*)' ''
end
complete -c just -f -a '(_justfile_targets)'
