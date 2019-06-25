#RUN: %fish -C 'set -g fish %fish' %s
begin
    set -l dir $PWD/(dirname (status -f))
    set -gx XDG_CONFIG_HOME $dir/broken-config/
    set -gx HOME $dir/broken-config/
    $fish -l -c 'echo but still going'
    # CHECK: broken
    # CHECK: but still going
    # CHECKERR: fish: Unknown command: syntax-error
    # CHECKERR: ~//fish/config.fish (line {{\d+}}):
    # CHECKERR: syntax-error
    # CHECKERR: ^
    # CHECKERR: from sourcing file ~//fish/config.fish
    # CHECKERR: called during startup

    $fish -c "echo normal command" -C "echo init"
    # CHECK: broken
    # CHECK: init
    # CHECK: normal command
end
