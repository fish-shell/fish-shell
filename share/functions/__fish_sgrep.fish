# This function is deprecated. Do not introduce new uses. Use the `string` command instead.
function __fish_sgrep -d "Call grep without honoring GREP_OPTIONS settings"
    set -l GREP_OPTIONS
    command grep $argv
end
