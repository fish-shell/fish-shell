set -l compdir (status dirname)/../../share/completions
cd $compdir
for file in *.fish
    set -l bname (string replace -r '.fish$' '' -- $file)
    if type -q $bname
        source $file >/dev/null
        if test $status -gt 0
            echo FAILING FILE $file
        end
    end
end
