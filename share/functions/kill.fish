# localization: tier1
if command -q kill
    # Only define this if something to wrap exists
    # this allows a nice "command not found" error to be triggered.
    function kill
        set -l args (__fish_expand_pid_args $argv)
        and command kill $args
    end
end
