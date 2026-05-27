function fish_set_key_bindings -d "Switch between named key-binding presets"
    if contains -- -h $argv
        or contains -- --help $argv
        echo 'Usage: fish_set_key_bindings STYLE'
        echo ''
        echo 'Sets the key-binding preset for the current and all future fish sessions.'
        echo ''
        echo 'Available styles:'
        echo '  emacs  Emacs-style bindings (default)'
        echo '  vim    Vi/Vim-style bindings'
        echo '  cua    CUA/Windows-standard bindings (Ctrl+C/X/V for clipboard, Shift for selection)'
        return 0
    end

    switch $argv[1]
        case emacs default ''
            set -U fish_key_bindings fish_default_key_bindings
            fish_default_key_bindings
        case vi vim
            set -U fish_key_bindings fish_vi_key_bindings
            fish_vi_key_bindings
        case cua windows
            set -U fish_key_bindings fish_cua_key_bindings
            fish_cua_key_bindings
        case '*'
            echo "fish_set_key_bindings: unknown style '$argv[1]'" >&2
            echo "Valid styles: emacs, vim, cua" >&2
            return 1
    end
end
