function __fish_reg__complete_keys
    set -l current_token (commandline -tc | string unescape)

    set -l default_keys HKEY_CLASSES_ROOT \
        HKEY_CURRENT_USER \
        HKEY_LOCAL_MACHINE \
        HKEY_USERS \
        HKEY_CURRENT_CONFIG \
        HKEY_DYN_DATA

    if test -z "$current_token"
        printf '%s\n' $default_keys
    else if string match --quiet --entire --regex '\\\\' -- "$current_token"
        set current_token (string replace --regex '\\\\[^\\\\]*$' '' -- "$current_token")

        set -l keys (WINEDEBUG=-all wine reg query "$current_token" |
            string replace --regex '\r' '' |
            string match --entire --regex '\w')

        test $pipestatus[1] != 0 && return

        string join \n -- $keys |
            string match --invert "$current_token" |
            string replace --all --regex '[\\\\]+' '\\\\'
    else
        printf '%s\n' $default_keys
    end
end

complete -c true -a '(__fish_reg__complete_keys)'
