# Only GNU and BSD sysctl seem to know "-h", so others should exit non-zero
if sysctl -h >/dev/null 2>/dev/null
    # Print sysctl keys and values, separated by a tab
    function __fish_sysctl_values
        sysctl -a 2>/dev/null | string replace -a " = " \t
    end

    complete -c sysctl -a '(__fish_sysctl_values)' -f

    complete -c sysctl -s n -l values -d 'Only print values'
    complete -c sysctl -s e -l ignore -d 'Ignore errors about unknown keys'
    complete -c sysctl -s N -l names -d 'Only print names'
    complete -c sysctl -s q -l quiet -d 'Be quiet when setting values'
    complete -c sysctl -s w -l write -d 'Write value'
    complete -c sysctl -s p -l load -d 'Load in sysctl settings from the file specified or /etc/sysctl'
    complete -c sysctl -s a -l all -d 'Display all values currently available'
    complete -c sysctl -l deprecated -d 'Include deprecated parameters too'
    complete -c sysctl -s b -l binary -d 'Print value without new line'
    complete -c sysctl -l system -d 'Load settings from all system configuration files'
    complete -c sysctl -s r -l pattern -d 'Only apply settings that match pattern'
    # Don't include these as they don't do anything
    # complete -c sysctl -s A -d 'Alias of -a'
    # complete -c sysctl -s d -d 'Alias of -h'
    # complete -c sysctl -s f -d 'Alias of -p'
    # complete -c sysctl -s X -d 'Alias of -a'
    # complete -c sysctl -s o -d 'Does nothing, exists for BSD compatibility'
    # complete -c sysctl -s x -d 'Does nothing, exists for BSD compatibility'
    complete -c sysctl -s h -l help -d 'Display help text and exit.'
    complete -c sysctl -s V -l version -d 'Display version information and exit.'
else
    # OSX sysctl
    function __fish_sysctl_values
        sysctl -a 2>/dev/null | string replace -a ":" \t
    end

    complete -c sysctl -a '(__fish_sysctl_values)' -f
    complete -c sysctl -s a -d 'Display all non-opaque values currently available'
    complete -c sysctl -s A -d 'Display all MIB variables'
    complete -c sysctl -s b -d 'Output values in a binary format'
    complete -c sysctl -s n -d 'Show only values, not names'
    complete -c sysctl -s w -d 'Set values'
    complete -c sysctl -s X -d 'Like -A, but prints a hex dump'
end
