# spctl - SecAssessment system policy security (man 8 spctl)
#
# spctl is a flag-style tool: the principal operation is a long flag
# (--assess, --status, --global-enable, etc.); modifier flags may follow.
# Deprecated rule-database operations are included but annotated as such.

# ── predicates ───────────────────────────────────────────────────────────────

# True when the assess operation is present on the command line.
function __fish_spctl_has_assess
    contains -- --assess (commandline -opc)
    or contains -- -a (commandline -opc)
end

# True when any deprecated rule-modification operation is present.
function __fish_spctl_has_rule_op
    set -l toks (commandline -opc)
    contains -- --add $toks
    or contains -- --remove $toks
    or contains -- --enable $toks
    or contains -- --disable $toks
    or contains -- --reset-default $toks
end

# ── principal operations ──────────────────────────────────────────────────────

complete -c spctl -l assess -s a -d 'Assess one or more files against system policy'
complete -c spctl -l status -f -d 'Query whether the assessment subsystem is enabled or disabled'
complete -c spctl -l global-enable -f -d 'Enable the assessment subsystem (requires root)'
complete -c spctl -l global-disable -f -d 'Reveal the "allow apps from anywhere" option in Privacy & Security'
complete -c spctl -l disable-status -f -d 'Query whether the "allow apps from anywhere" option is available'

# ── assessment modifiers (active when --assess / -a is present) ───────────────

complete -c spctl -n __fish_spctl_has_assess \
    -l type -s t -x \
    -a 'execute\t"Assess code execution (default)" install\t"Assess installer packages" open\t"Assess document opening"' \
    -d 'Type of assessment to perform'

complete -c spctl -n __fish_spctl_has_assess \
    -l verbose -s v -f \
    -d 'Request more verbose output (repeat to increase verbosity)'

complete -c spctl -n __fish_spctl_has_assess \
    -l continue -f \
    -d 'Continue assessing remaining files after a failed assessment'

complete -c spctl -n __fish_spctl_has_assess \
    -l ignore-cache -f \
    -d 'Do not query the assessment object cache (may be slower)'

complete -c spctl -n __fish_spctl_has_assess \
    -l no-cache -f \
    -d 'Do not store assessment outcome in the cache'

complete -c spctl -n __fish_spctl_has_assess \
    -l raw -f \
    -d 'Display assessment outcome as raw XML plist'

# ── deprecated rule-database operations (macOS 15+) ──────────────────────────

complete -c spctl -l add -f -d '(Deprecated macOS 15+) Add rule(s) to the assessment rule database'
complete -c spctl -l remove -f -d '(Deprecated macOS 15+) Remove rule(s) from the assessment rule database'
complete -c spctl -l enable -f -d '(Deprecated macOS 15+) Enable rule(s) in the assessment rule database'
complete -c spctl -l disable -f -d '(Deprecated macOS 15+) Disable rule(s) in the assessment rule database'
complete -c spctl -l reset-default -f -d '(Deprecated macOS 15+) Reset system policy database to default (requires root, then reboot)'

# ── deprecated rule-database modifiers ───────────────────────────────────────

complete -c spctl -n __fish_spctl_has_rule_op \
    -l label -x \
    -d '(Deprecated macOS 15+) String label to attach to or find in rules'

complete -c spctl -n __fish_spctl_has_rule_op \
    -l priority -x \
    -d '(Deprecated macOS 15+) Priority (floating-point) for rules created or changed'

complete -c spctl -n __fish_spctl_has_rule_op \
    -l path \
    -d '(Deprecated macOS 15+) Rule subject: path(s) to files on disk'

complete -c spctl -n __fish_spctl_has_rule_op \
    -l anchor -x \
    -d '(Deprecated macOS 15+) Rule subject: hash(es) of anchor certificates or DER certificate paths'

complete -c spctl -n __fish_spctl_has_rule_op \
    -l hash -x \
    -d '(Deprecated macOS 15+) Rule subject: CodeDirectory hash(es) of programs'

complete -c spctl -n __fish_spctl_has_rule_op \
    -l requirement -x \
    -d '(Deprecated macOS 15+) Rule subject: code requirement source string(s)'

complete -c spctl -n __fish_spctl_has_rule_op \
    -l rule -x \
    -d '(Deprecated macOS 15+) Rule subject: index number(s) of existing rules'
