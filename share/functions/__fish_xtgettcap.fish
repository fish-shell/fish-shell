function __fish_xtgettcap --argument-names terminfo_code
    if test $FISH_UNIT_TESTS_RUNNING = 1
        return 1
    end
    set -l varname __fish_tcap_$terminfo_code
    if not set -q $varname\[0\]
        set --global $varname (
            status xtgettcap $terminfo_code && echo true || echo false
        )
    end
    test "$$varname" = true
end
