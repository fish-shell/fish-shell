function __fish_change_key_bindings --argument-names bindings
    set -g __fish_active_key_bindings $bindings
    # Allow the user to set the variable universally
    set -l scope
    set -q fish_key_bindings
    or set scope -g
    # We try to use `set --no-event`, but to avoid leaving the user without bindings
    # if they run this with an older version we fall back on setting the variable
    # with an event.
    if ! set --no-event $scope fish_key_bindings $bindings 2>/dev/null
        # This triggers the handler, which calls us again
        set $scope fish_key_bindings $bindings
        # unless the handler somehow doesn't exist, which would leave us without bindings.
        # this happens in no-config mode.
        functions -q __fish_reload_key_bindings
        and return 1
    end
end
