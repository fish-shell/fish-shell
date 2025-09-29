# localization: skip(private)
function __fish_print_rpm_packages
    type -q -f rpm /usr/share/yum-cli/completion-helper.py || return 1

    # We do not use "--installed", but we still allow passing it.
    argparse i/installed -- $argv
    or return 1

    if type -q -f /usr/share/yum-cli/completion-helper.py
        # Remove package version information from output and pipe into cache file
        __fish_cached -t 21600 -k yum-completion-helper -- '/usr/share/yum-cli/completion-helper.py list all -d 0 -C | sed "s/\\..*/\\tPackage/"'
        return
    end

    # Rpm is too slow for this job, so we set it up to do completions
    # as a background job and cache the results.
    if type -q -f rpm
        # Remove package version information from output and pipe into cache file
        __fish_cached -t 250 -- "rpm -qa | sed -e 's/-[^-]*-[^-]*\$/\\t'Package'/'"
        return
    end
end
