# osascript - execute OSA scripts (AppleScript, JavaScript, etc.) (man 1 osascript)
#
# osascript is a flag-driven tool; there are no subcommands or single-dash
# long verbs.  Flags: -e -i -l -s.  Operands are script files.

# ── helper: enumerate installed OSA languages ────────────────────────
function __fish_osascript_languages
    # osalang(1) lists one language per line; fast and unprivileged
    osalang 2>/dev/null
end

# ── flags ────────────────────────────────────────────────────────────
complete -c osascript -s e -r \
    -d 'Enter one line of script (may be repeated to build multi-line script)'

complete -c osascript -s i -f \
    -d 'Interactive mode: prompt for one line at a time, print result after each'

complete -c osascript -s l -x \
    -a '(__fish_osascript_languages)' \
    -d 'Override language for plain-text files (default: AppleScript)'

# -s takes a string of modifier characters: h, s, e, o (pairs are exclusive)
#   h = human-readable output (default)   s = recompilable source form
#   e = errors to stderr (default)        o = errors to stdout
complete -c osascript -s s -x \
    -a 'h\t"Human-readable output (default)" s\t"Recompilable source form" e\t"Print errors to stderr (default)" o\t"Print errors to stdout" hs\t"Human-readable + errors to stderr" ho\t"Human-readable + errors to stdout" sh\t"Source form + errors to stderr" so\t"Source form + errors to stdout"' \
    -d 'Output style modifier(s): h/s = value format, e/o = error destination'

# ── operands: script files ───────────────────────────────────────────
complete -c osascript -f -a '(__fish_complete_suffix .scpt)' \
    -d 'Compiled script'
complete -c osascript -f -a '(__fish_complete_suffix .applescript)' \
    -d 'AppleScript source file'
complete -c osascript -f -a '(__fish_complete_suffix .js)' \
    -d 'JavaScript for Automation source file'
