# localization: skip(private)
function __fish_change_key_bindings --argument-names bindings
    set -g __fish_active_key_bindings $bindings
    # Allow the user to set the variable universally
    set -l scope
    if not set -q fish_key_bindings
        set scope -g
    end
    set --no-event $scope fish_key_bindings $bindings
end
