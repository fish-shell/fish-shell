# RUN: %fish %s

touch $__fish_config_dir/functions/delta-test-custom-function.fish
for path in fish_greeting fish_job_summary
    __fish_data_with_file functions/$path.fish \
        cat >$__fish_config_dir/functions/$path.fish
end
set -l tmp (sed 's/$/ # Modified/' $__fish_config_dir/functions/fish_greeting.fish)
string join -- \n $tmp >$__fish_config_dir/functions/fish_greeting.fish

fish_delta --no-diff --new |
    string match -r '.*\b(?:delta-test|fish_greeting|fish_job_summary)\b.*'
# CHECK: New: {{.*}}/fish/functions/delta-test-custom-function.fish
# CHECK: Changed: {{.*}}/fish/functions/fish_greeting.fish
# CHECK: Unmodified: {{.*}}/fish/functions/fish_job_summary.fish

fish_delta | grep -A1 '\bfunction fish_greeting\b'
# CHECK: -function fish_greeting
# CHECK: -    if not set -q fish_greeting
# CHECK: --
# CHECK: +function fish_greeting # Modified
# CHECK: +    if not set -q fish_greeting # Modified
