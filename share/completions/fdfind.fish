# `fdfind` is the name `fd` is installed as on Debian,
# since `fd` name is reserved by `fdclone` package.
# Ideally, since it's a Debian caveat, then Debian package should ship `fdfind` completions,
# but for years it hasn't, so we provide them here.
fdfind --gen-completions fish | source
