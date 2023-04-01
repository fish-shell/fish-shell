#RUN: %ghoti %s
set -l filename (echo foo | psub --testing)
test -f $filename
or echo 'psub is not a regular file' >&2
rm $filename

set -l filename (echo foo | psub --testing --file)
test -f $filename
or echo 'psub is not a regular file' >&2
rm $filename

set -l filename (echo foo | psub --testing --fifo)
test -p $filename
or echo 'psub is not a fifo' >&2
# hack: the background write that psub peforms may block
# until someone opens the fifo for reading. So make sure we
# actually read it.
cat $filename >/dev/null
rm $filename

cat (echo foo | psub)
cat (echo bar | psub --fifo)
cat (echo baz | psub)
#CHECK: foo
#CHECK: bar
#CHECK: baz

set -l filename (echo foo | psub)
if test -e $filename
    echo 'psub file was not deleted'
else
    echo 'psub file was deleted'
end
#CHECK: psub file was deleted

# The --file flag is the default behavior.
if count (echo foo | psub -s .cc | string match -r '\.cc$') >/dev/null
    echo 'psub filename ends with .cc'
else
    echo 'psub filename does not end with .cc'
end
#CHECK: psub filename ends with .cc

# Make sure we get the same result if we explicitly ask for a temp file.
if count (echo foo | psub -f -s .cc | string match -r '\.cc$') >/dev/null
    echo 'psub filename ends with .cc'
else
    echo 'psub filename does not end with .cc'
end
#CHECK: psub filename ends with .cc

set -l filename (echo foo | psub -s .ghoti)
if test -e (dirname $filename)
    echo 'psub directory was not deleted'
else
    echo 'psub directory was deleted'
end
#CHECK: psub directory was deleted

set -l diffs (comm -3 (__ghoti_print_help psub | psub) (psub -hs banana | psub))
test -z "$diffs"
