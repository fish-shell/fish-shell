# localization: skip(private)
function __fish_per_os_bind
    set -l macos $argv[-2]
    set -l non_macos $argv[-1]
    set -e argv[-2..-1]
    for varname in macos non_macos
        if contains -- $$varname (bind --function-names)
            set $varname 'commandline -f '$$varname
        end
    end
    bind $argv "if fish_in_macos_terminal; $macos; else $non_macos; end"
end
