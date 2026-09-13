# localization: skip(private)
function __fish_supports_version
    command $argv[1] --version &>/dev/null
end
