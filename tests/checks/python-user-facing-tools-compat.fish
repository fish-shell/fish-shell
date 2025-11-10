#RUN: %fish %s
#REQUIRES: command -v uvx

set -l webconfig (status dirname)/../../share/tools/web_config/webconfig.py
set -l create_manpage (status dirname)/../../share/tools/create_manpage_completions.py
set -l min_version 3.5

# use vermin to detect minimum Python version violations
# features enabled:
#   - union-types: catch `str | None` syntax (3.10+)
#   - fstring-self-doc: catch f'{var=}' syntax (3.8+)
set -l output (uvx vermin \
    --no-tips \
    --violations \
    --feature union-types \
    --feature fstring-self-doc \
    --target=$min_version- \
    $webconfig \
    $create_manpage \
    2>&1 | string collect)
set -l exit_code $pipestatus[1]

echo $exit_code
# CHECK: 0

if test $exit_code -ne 0
    echo $output >&2
end
