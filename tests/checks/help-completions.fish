# RUN: %fish %s
# REQUIRES: command -v sphinx-build
# REQUIRES: command -v diff

__fish_data_with_file help_sections (command -v cat) | grep -v ^cmds/ >expected

__fish_data_with_file completions/help.fish cat |
    awk '
        / case / && $2 != "'\''cmds/*'\''" {
            sub(/^introduction/, "index", $2);
            print $2
        }
    ' >actual

diff -u expected actual
