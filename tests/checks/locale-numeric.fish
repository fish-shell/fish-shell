# RUN: %fish %s
# musl currently does not have a `locale` command, so we skip this test there.
# REQUIRES: command -v locale
# We need a comma-using locale we know.
# REQUIRES: locale -a | grep -Ei '(bg_BG|de_DE|es_ES|fr_FR|ru_RU)\.(UTF-8|utf8)'
# OpenBSD does not use LC_NUMERIC, this test is pointless there.
# REQUIRES: test "$(uname)" != OpenBSD

set -l locales (locale -a)
set -l acceptable_locales bg_BG de_DE es_ES fr_FR ru_RU
set -l numeric_locale
for locale in {$acceptable_locales}.{UTF-8,UTF8}
    if string match -i -q $locale $locales
        set numeric_locale $locale
        break
    end
end

if not set -q numeric_locale[1]
    # Note we tried to check for a locale above,
    # but in case this fails because of e.g. case issues,
    # let's error again.
    echo Please install one of the following locales: >&2
    printf '%s\n' $acceptable_locales".utf-8" >&2
    exit 127
end

begin
    set -x LC_NUMERIC $numeric_locale
    printf '%e\n' "3,45" # should succeed, output should be 3,450000e+00
    # CHECK: 3,450000e+00
    printf '%e\n' "4.56" # should succeed, output should be 4,560000e+00
    # CHECK: 4,560000e+00

    # This should succeed without output
    test 42.5 -gt 37.2
end
