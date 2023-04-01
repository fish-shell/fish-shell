#RUN: env FISH_PATH=%ghoti FILE_PATH=%s %ghoti %s

status line-number
# CHECK: 3

# Check status ghoti-path
# No output expected on success
#
# argv[0] on OpenBSD is just the filename, not the path
# That means ghoti-path is unsupportable there.
if not contains (uname) OpenBSD
    set status_ghoti_path (realpath (status ghoti-path))
    set env_ghoti_path (realpath $FISH_PATH)
    test "$status_ghoti_path" = "$env_ghoti_path"
    or echo "Fish path disagreement: $status_ghoti_path vs $env_ghoti_path"
end

# Check is-block
status is-block
echo $status
begin
    status is-block
    echo $status
end
# CHECK: 1
# CHECK: 0

# Check filename
set status_filename (status filename)
test (status filename) = "$FILE_PATH"
or echo "File path disagreement: $status_filename vs $FILE_PATH"

function print_my_name
    status function
end
print_my_name
# CHECK: print_my_name

status is-command-substitution
echo $status
echo (status is-command-substitution; echo $status)
# CHECK: 1
# CHECK: 0

test (status filename) = (status dirname)/(status basename)

status basename
#CHECK: status-command.ghoti

status dirname | string match -q '*checks'
echo $status
#CHECK: 0

echo "status dirname" | source
#CHECK: .

$FISH_PATH -c 'status dirname'
#CHECK: Standard input
