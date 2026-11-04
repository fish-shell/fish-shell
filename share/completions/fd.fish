# `fdclone` is also installed as `fd`, e.g. on Debian, so guarding against it.
# `@(#)fd.c` is a string always present in `fdclone` executable:
# https://github.com/knu/FDclone/blob/master/version.h
# If we don't guard, `fd <tab>` might enter `fdclone` TUI.
if grep -aq '@(#)fd\.c' "$(command -s fd)"
    exit 0
end

# Adding completions for sharkdp/fd.
fd --gen-completions fish | source
