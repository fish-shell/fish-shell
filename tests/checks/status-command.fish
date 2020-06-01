#RUN: env FISH_PATH=%fish FILE_PATH=%s %fish %s

status line-number
# CHECK: 3

# Check status fish-path
# No output expected on success
set status_fish_path (realpath (status fish-path))
set env_fish_path (realpath $FISH_PATH)
test "$status_fish_path" = "$env_fish_path"
or echo "Fish path disagreement: $status_fish_path vs $env_fish_path"

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
#CHECK: status-command.fish

status dirname | string match -q '*checks'
echo $status
#CHECK: 0

echo "status dirname" | source
#CHECK: .

$FISH_PATH -c 'status dirname'
#CHECK: Standard input
