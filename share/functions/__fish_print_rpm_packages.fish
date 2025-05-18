function __fish_print_rpm_packages
    type -q -f rpm /usr/share/yum-cli/completion-helper.py || return 1

    # We do not use "--installed", but we still allow passing it.
    argparse i/installed -- $argv
    or return 1

    if type -q -f /usr/share/yum-cli/completion-helper.py
        # If the cache is less than six hours old, we do not recalculate it
        __fish_cached -t 21600 -k yum -- '/usr/share/yum-cli/completion-helper.py list -C | sed "s/\..*/\tPackage/"'
        return
    end

    if type -q -f rpm
        __fish_cached -t 250 -- rpm -qa | sed -e 's/-[^-]*-[^-]*$/\tPackage/'
        return
    end
end
