#!/usr/local/bin/fish
#
# Fishscript tests

# Change to directory containing this script
cd (dirname (status -f))

# Test files specified on commandline, or all *.in files
if set -q argv[1]
    set files_to_test $argv.in
else
    set files_to_test *.in
end

source test_util.fish (status -f) $argv; or exit

say -o cyan "Testing high level script functionality"

function test_file
    set -l file $argv[1]
    set -l base (basename $file .in)

    echo -n "Testing file $file ... "

    ../fish <$file >$base.tmp.out ^$base.tmp.err
    set -l tmp_status $status
    set -l res ok

    diff $base.tmp.out $base.out >/dev/null
    set -l out_status $status
    diff $base.tmp.err $base.err >/dev/null
    set -l err_status $status
    set -l exp_status (cat $base.status)[1]

    if test $out_status -eq 0 -a $err_status -eq 0 -a $exp_status -eq $tmp_status
        say green "ok"
        # clean up tmp files
        rm -f $base.tmp.{err,out}
        return 0
    else
        say red "fail"
        if test $out_status -ne 0
            say yellow "Output differs for file $file. Diff follows:"
            colordiff -u $base.tmp.out $base.out
        end
        if test $err_status -ne 0
            say yellow "Error output differs for file $file. Diff follows:"
            colordiff -u $base.tmp.err $base.err
        end
        if test $exp_status -ne $tmp_status
            say yellow "Exit status differs for file $file."
            echo "Expected $exp_status, got $tmp_status."
        end
        return 1
    end
end

set -l failed
for i in $files_to_test
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
