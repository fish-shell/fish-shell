#!/usr/local/bin/fish
#
# Interactive tests using `expect`

function die
    echo $argv[1] >&2
    exit 1
end

for i in *.expect
    rm -Rf tmp.interactive.config; or die "Couldn't remove tmp.interactive.config"
    mkdir -p tmp.interactive.config/fish; or die "Couldn't create tmp.interactive.config/fish"
    cp interactive.config tmp.interactive.config/fish/config.fish; or die "Couldn't create tmp.interactive.config/fish/config.fish"

    begin
        set -lx XDG_CONFIG_HOME $PWD/tmp.interactive.config
        set -lx TERM dumb
        expect -n -c 'source interactive.expect.rc' -f $i >tmp.out ^tmp.err
    end
    set -l tmp_status $status
    set res ok
    if not diff tmp.out $i.out >/dev/null
        set res fail
        echo "Output differs for file $i. Diff follows:"
        diff -u tmp.out $i.out
    end

    if not diff tmp.err $i.err >/dev/null
        set res fail
        echo "Error output differs for file $i. Diff follows:"
        diff -u tmp.err $i.err
    end

    if test $tmp_status != (cat $i.status)
        set res fail
        echo "Exit status differs for file $i."
    end

    if test $res = ok
        echo "File $i tested ok"
    else
        echo "File $i failed tests"
    end
end
