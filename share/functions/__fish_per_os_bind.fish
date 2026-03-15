# localization: skip(private)
function __fish_per_os_bind
    echo -e '
        set -l macos $argv[-2] \n
        set -l non_macos $argv[-1] \n
        set -e argv[-2..-1] \n
        for varname in macos non_macos \n
            if contains -- $$varname (bind --function-names) \n
                set $varname \'commandline -f \'$$varname \n
            end \n
        end \n

        if fish_in_macos_terminal \n
            bind $argv $macos \n
        else \n
            bind $argv $non_macos \n
        end \n

        # bind $argv if fish_in_macos_terminal; $macos; else; $non_macos; end \n
    '
end
