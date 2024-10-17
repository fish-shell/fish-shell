function __fish_reg__complete_keys
    set -l current_token (commandline -tc | string unescape)

    set -l default_keys 'HKEY_CLASSES_ROOT\tThe information about file extension associations' \
        'HKEY_CURRENT_USER\tThe information about a current user' \
        'HKEY_LOCAL_MACHINE\tThe information about a current machine' \
        'HKEY_USERS\tThe information about loaded users' \
        'HKEY_CURRENT_CONFIG\tThe information about hardware used while startup' \
        HKEY_DYN_DATA

    if string match --quiet --entire --regex '\\\\' -- "$current_token"
        set current_token (string replace --regex '\\\\[^\\\\]*$' '' -- "$current_token")

        set -l keys (WINEDEBUG=-all wine reg query "$current_token" |
            string replace --regex '\r' '' |
            string match --entire --regex '\w')

        test $pipestatus[1] != 0 && return

        string join \n -- $keys |
            string match --invert "$current_token" |
            string replace --all --regex '[\\\\]+' '\\\\'
    else
        for key in $default_keys
            echo -e $key
        end
    end
end

complete -c true -a '(__fish_reg__complete_keys)'
