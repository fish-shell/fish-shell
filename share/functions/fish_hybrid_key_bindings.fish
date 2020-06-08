function fish_hybrid_key_bindings --description "Vi-style bindings that inherit emacs-style bindings in all modes"
    bind --erase --all --preset # clear earlier bindings, if any

    if test "$fish_key_bindings" != fish_hybrid_key_bindings
        # Allow the user to set the variable universally
        set -q fish_key_bindings
        or set -g fish_key_bindings
        # This triggers the handler, which calls us again and ensures the user_key_bindings
        # are executed.
        set fish_key_bindings fish_hybrid_key_bindings
        return
    end

    for mode in default insert visual
        fish_default_key_bindings -M $mode
    end
    fish_vi_key_bindings --no-erase
end
