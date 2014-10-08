#!/usr/local/bin/fish
#
# Interactive tests using `expect`

source test_util.fish

say -o cyan "Testing interactive functionality"
if not type -q expect
    say red "Tests disabled: `expect` not found"
    exit 0
end

function test_file
    rm -Rf tmp.interactive.config; or die "Couldn't remove tmp.interactive.config"
    mkdir -p tmp.interactive.config/fish; or die "Couldn't create tmp.interactive.config/fish"
    cp interactive.config tmp.interactive.config/fish/config.fish; or die "Couldn't create tmp.interactive.config/fish/config.fish"

    set -l file $argv[1]

    echo -n "Testing file $file ... "

    begin
        set -lx XDG_CONFIG_HOME $PWD/tmp.interactive.config
        set -lx TERM dumb
        expect -n -c 'source interactive.expect.rc' -f $file >$file.tmp.out ^$file.tmp.err
    end
    set -l tmp_status $status
    set -l res ok
    mv -f interactive.tmp.log $file.tmp.log

    diff $file.tmp.out $file.out >/dev/null
    set -l out_status $status
    diff $file.tmp.err $file.err >/dev/null
    set -l err_status $status
    set -l exp_status (cat $file.status)[1]

    if test $out_status -eq 0 -a $err_status -eq 0 -a $exp_status -eq $tmp_status
        say green "ok"
        # clean up tmp files
        rm -f $file.tmp.{err,out,log}
        return 0
    else
        say red "fail"
        if test $out_status -ne 0
            say yellow "Output differs for file $file. Diff follows:"
            colordiff -u $file.tmp.out $file.out
        end
        if test $err_status -ne 0
            say yellow "Error output differs for file $file. Diff follows:"
            colordiff -u $file.tmp.err $file.err
        end
        if test $exp_status -ne $tmp_status
            say yellow "Exit status differs for file $file."
            echo "Expected $exp_status, got $tmp_status."
        end
        if set -q SHOW_INTERACTIVE_LOG
            # dump the interactive log
            # primarily for use in travis where we can't check it manually
            say yellow "Log for file $file:"
            cat $file.tmp.log
        end
        return 1
    end
end

set -l failed
for i in *.expect
    if not test_file $i
        set failed $failed $i
    end
end

set failed (count $failed)
if test $failed -eq 0
    say green "All tests completed successfully"
    exit 0
else
    set plural (test $failed -eq 1; or echo s)
    say red "$failed test$plural failed"
    exit 1
end
