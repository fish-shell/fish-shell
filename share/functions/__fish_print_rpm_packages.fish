function __fish_print_rpm_packages
    type -q -f rpm /usr/share/yum-cli/completion-helper.py || return 1

    # We do not use "--installed", but we still allow passing it.
    argparse i/installed -- $argv
    or return 1

    set -l xdg_cache_home (__fish_make_cache_dir)
    or return

    if type -q -f /usr/share/yum-cli/completion-helper.py
        # If the cache is less than six hours old, we do not recalculate it
        set -l cache_file $xdg_cache_home/yum
        if test -f $cache_file
            cat $cache_file
            set -l age (path mtime -R -- $cache_file)
            set -l max_age 21600
            if test $age -lt $max_age
                return
            end
        end

        # Remove package version information from output and pipe into cache file
        /usr/share/yum-cli/completion-helper.py list all -d 0 -C | sed "s/\..*/\tPackage/" >$cache_file &
        return
    end

    # Rpm is too slow for this job, so we set it up to do completions
    # as a background job and cache the results.
    if type -q -f rpm
        # If the cache is less than five minutes old, we do not recalculate it
        set -l cache_file $xdg_cache_home/rpm
        if test -f $cache_file
            cat $cache_file
            set -l age (path mtime -R -- $cache_file)
            set -l max_age 250
            if test $age -lt $max_age
                return
            end
        end

        # Remove package version information from output and pipe into cache file
        rpm -qa | sed -e 's/-[^-]*-[^-]*$/\t'Package'/' >$cache_file &
        return
    end
end
