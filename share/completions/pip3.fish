# pip[2|3] emits (or emitted) completions with the wrong command name
# See discussion at https://github.com/fish-shell/fish-shell/pull/4448
# and pip bug at https://github.com/pypa/pip/pull/4755
# Keep this even after pip fix is upstreamed for users not on the latest pip
pip3 completion --fish 2>/dev/null | string replace -r -- " -c\s+pip\b" " -c pip3" | source
