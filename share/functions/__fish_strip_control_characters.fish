# localization: skip(private)
function __fish_strip_control_characters
    string replace -ra '[[:cntrl:]]' '' $argv | string join ''
end
