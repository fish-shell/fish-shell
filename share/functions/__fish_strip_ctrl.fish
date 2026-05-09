# localization: skip(private)
function __fish_strip_ctrl --description 'Strip control characters from arguments'
    string replace -ra '[\x00-\x1f\x7f-\x9f\x{f680}-\x{f69f}]' '' -- $argv
end
