# RUN: %fish %s
# REQUIRES: command -v sphinx-build
# REQUIRES: command -v diff

status get-file help_sections | grep -v ^cmds/ >expected

status get-file completions/help.fish |
    awk '
        / case / && $2 != "'\''cmds/*'\''" {
            sub(/^introduction/, "index", $2);
            print $2
        }
    ' >actual

diff -u expected actual
