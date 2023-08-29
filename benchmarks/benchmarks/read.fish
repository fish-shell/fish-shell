set -l tmp (mktemp)
string repeat -n 2000 >$tmp
for i in (seq 1000)
    cat $tmp | read -l foo
end

true
