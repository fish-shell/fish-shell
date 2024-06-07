# Completions for `actionlint` (https://github.com/rhysd/actionlint)
# Based on version 1.7.1

complete -c actionlint -o color -d "enable colorful output"
complete -r -c actionlint -o config-file -d "path to config file"
complete -c actionlint -o debug -d "enable debug output"
complete -x -c actionlint -o format -d "custom template to format error messages"
complete -x -c actionlint -o ignore -d "regex matching to error messages you want to ignore"
complete -c actionlint -o init-config -d "generate default config file"
complete -c actionlint -o no-color -d "disable colorful output"
complete -c actionlint -o oneline -d "use one line per one error"
complete -x -c actionlint -o pyflakes -d "command name or path of `pyflakes`"
complete -x -c actionlint -o shellcheck -d "command name or path of `shellcheck`"
complete -r -c actionlint -o stdin-filename -d "filename when reading input from stdin"
complete -c actionlint -o verbose -d "enable verbose output"
complete -c actionlint -o version -d "show version"
complete -c actionlint -s h -o help -d "show help message"
