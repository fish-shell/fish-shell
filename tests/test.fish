#!/usr/local/bin/fish
#
# Main loop of the test suite. I wrote this
# instad of using autotest to provide additional
# testing for fish. :-)


if [ "$argv" != '-n' ]
    set -l res ok

    # begin...end has bug in error redirecting...
    begin
        ../fish -n ./test.fish ^top.tmp.err
        ../fish -n ./test.fish -n ^^top.tmp.err
        ../fish ./test.fish -n ^^top.tmp.err
    end  | tee top.tmp.out
    set -l tmp_status $status
    if not diff top.tmp.out top.out >/dev/null
        set res fail
        echo "Output differs for file test.fish. Diff follows:"
        diff -u top.out top.tmp.out
    end

    if not diff top.tmp.err top.err >/dev/null
        set res fail
        echo "Error output differs for file test.fish. Diff follows:"
        diff -u top.err top.tmp.err
    end

    if test $tmp_status -ne (cat top.status)
        set res fail
        echo "Exit status differs for file test.fish"
    end

    if not ../fish -p /dev/null -c 'echo testing' >/dev/null
        set res fail
        echo "Profiling failed"
    end

    if test $res = ok
        echo "File test.fish tested ok"
        exit 0
    else
        echo "File test.fish failed tests"
        exit 1
    end
end

echo "Testing high level script functionality"

for i in *.in
    set -l res ok

    set -l base (basename $i .in)
    set template_out (basename $i .in).out
    set template_err (basename $i .in).err
    set template_status (basename $i .in).status

    ../fish <$i >tmp.out ^tmp.err
    set -l tmp_status $status
    if not diff tmp.out $base.out >/dev/null
        set res fail
        echo "Output differs for file $i. Diff follows:"
        diff -u tmp.out $base.out
    end

    if not diff tmp.err $base.err >/dev/null
        set res fail
        echo "Error output differs for file $i. Diff follows:"
        diff -u tmp.err $base.err
    end

    if test $tmp_status -ne (cat $template_status)
        set res fail
        echo "Exit status differs for file $i"
    end

    if test $res = ok
        echo "File $i tested ok"
    else
        echo "File $i failed tests"
    end
end
