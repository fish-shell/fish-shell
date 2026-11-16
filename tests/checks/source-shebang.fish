# RUN: fish=%fish %fish %s

# A file with a non-fish shebang that fails to parse should still report the
# original parse error, plus a hint pointing at the other shell.
set -l interps '/usr/bin/env bash' '/bin/bash' '/usr/bin/env zsh' '/bin/sh' '/usr/bin/env dash'
set -l script $TMPDIR/source-shebang-test.sh
for interp in $interps
    set -l parts (string split ' ' $interp)
    set -l expected (path basename $parts[-1])
    printf '#!%s\necho "unterminated\n' $interp >$script
    $fish -c "source $script" 2> $TMPDIR/source-shebang-error
    set -l st $status
    set -l err (cat $TMPDIR/source-shebang-error)
    set -l ok 1
    if not string match -q -- '*Unexpected end of string*' $err
        set ok 0
    end
    if not string match -q -- "*This file appears to target $expected, not fish*" $err
        set ok 0
    end
    echo $expected status:$st ok:$ok
end
# CHECK: bash status:1 ok:1
# CHECK: bash status:1 ok:1
# CHECK: zsh status:1 ok:1
# CHECK: sh status:1 ok:1
# CHECK: dash status:1 ok:1

# A fish-compatible file with a bash shebang must still be sourced normally,
# without the hint.
set -l good $TMPDIR/source-shebang-good.sh
printf '#!/usr/bin/env bash\nset -gx SOURCE_SHEBANG_OK yes\n' >$good
source $good 2> $TMPDIR/source-shebang-good-error
echo status:$status value:$SOURCE_SHEBANG_OK errors:(count (cat $TMPDIR/source-shebang-good-error))
# CHECK: status:0 value:yes errors:0

# A shebang for a non-shell interpreter must not add a shell hint.
set -l other $TMPDIR/source-shebang-other.sh
printf '#!/usr/bin/env python3\nif\n' >$other
$fish -c "source $other" 2> $TMPDIR/source-shebang-other-error
set -l st $status
set -l err (cat $TMPDIR/source-shebang-other-error)
set -l hint 1
if string match -q -- '*This file appears to target*' $err
    set hint 0
end
echo other status:$st hint:$hint
# CHECK: other status:1 hint:1
