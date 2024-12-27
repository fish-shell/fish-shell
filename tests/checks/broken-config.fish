#RUN: fish=%fish %fish %s
begin
    set -l dir (dirname (status -f))
    set -gx XDG_CONFIG_HOME $dir/broken-config/
    set -gx HOME $dir/broken-config/
    $fish -l -c 'echo but still going'
    # CHECK: broken
    # CHECK: but still going
    # CHECKERR: fish: Unknown command: syntax-error
    # CHECKERR: ~//fish/config.fish (line {{\d+}}):
    # CHECKERR: syntax-error
    # CHECKERR: ^~~~~~~~~~~^
    # CHECKERR: from sourcing file ~//fish/config.fish
    # CHECKERR: called during startup

    $fish -c "echo normal command" -C "echo init"
    # CHECK: broken
    # CHECK: init
    # CHECK: normal command
end

# should not crash or segfault in the presence of an invalid locale
LC_ALL=hello echo hello world
# CHECK: hello world
