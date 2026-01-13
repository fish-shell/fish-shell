# localization: skip(private)
function __fish_unexpand_tilde --description 'Replace $HOME with "~"'
    set -l realhome (string escape --style=regex -- ~)
    string replace -r -- "^$realhome(\$|/)" '~$1' $argv
end
