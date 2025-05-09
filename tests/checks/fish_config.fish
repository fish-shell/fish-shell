#RUN: %fish %s

mkdir $__fish_config_dir/themes
echo >$__fish_config_dir/themes/foo.theme 'fish_color_normal cyan'

set -g fish_pager_color_secondary_background custom-value
fish_config theme choose foo

set -S fish_color_normal
# CHECK: $fish_color_normal: set in global scope, unexported, with 1 elements
# CHECK: $fish_color_normal[1]: |cyan|
set -S fish_pager_color_secondary_background
# CHECK: $fish_pager_color_secondary_background: set in global scope, unexported, with 0 elements

function change-theme
    echo >$__fish_config_dir/themes/fake-default.theme 'fish_color_command' $argv
end

change-theme 'green'
echo yes | fish_config theme save --track fake-default
echo $fish_color_command
# CHECK: green --track=fake-default

change-theme 'green --bold'
fish_config theme update

echo $fish_color_command
# CHECK: green --bold --track=fake-default

# Test that we silently update when there is a shadowing global.
change-theme 'green --italics'
set -g fish_color_command normal
fish_config theme update
set -S fish_color_command
# CHECK: $fish_color_command: set in global scope, unexported, with 1 elements
# CHECK: $fish_color_command[1]: |normal|
# CHECK: $fish_color_command: set in universal scope, unexported, with 3 elements
# CHECK: $fish_color_command[1]: |green|
# CHECK: $fish_color_command[2]: |--italics|
# CHECK: $fish_color_command[3]: |--track=fake-default|
