# localization: tier1

if not status is-interactive
    exit
end

function value --description "Output values as list elements"
    if set -q fish_value_show; and isatty stdout; and string match -q 'value *' -- (status current-commandline)
        printf (_ '%d elements\n') (count $argv)
        builtin value (set -S argv | string replace -r '^\$argv' '' -- $argv)[2..]
    else
        builtin value $argv
    end
end
