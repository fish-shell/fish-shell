#RUN: %ghoti -C 'set -g ghoti %ghoti' %s
begin
    set -l dir $PWD/(dirname (status -f))
    set -gx XDG_CONFIG_HOME $dir/broken-config/
    set -gx HOME $dir/broken-config/
    $ghoti -l -c 'echo but still going'
    # CHECK: broken
    # CHECK: but still going
    # CHECKERR: ghoti: Unknown command: syntax-error
    # CHECKERR: ~//ghoti/config.ghoti (line {{\d+}}):
    # CHECKERR: syntax-error
    # CHECKERR: ^~~~~~~~~~~^
    # CHECKERR: from sourcing file ~//ghoti/config.ghoti
    # CHECKERR: called during startup

    $ghoti -c "echo normal command" -C "echo init"
    # CHECK: broken
    # CHECK: init
    # CHECK: normal command
end
