# This function only emits completions that might result from matches against $CDPATH. We rely on
# the core file name completion logic to include all other possible matches.
function __fish_complete_cd -d "Completions for the cd command"
    set -q CDPATH[1]
    or return 0 # no CDPATH so rely solely on the core file name completions

    set -l token (commandline -ct)
    if string match -qr '^\.{0,2}/.*' -- $token
        # Absolute path or explicitly relative to the current directory. Rely on the builtin file
        # name completions since we no longer exclude them from the `cd` argument completion.
        return
    end

    # Relative path. Check $CDPATH and use that as the description for any possible matches.
    # We deliberately exclude the `.` path because the core file name completion logic will include
    # it when presenting possible matches.
    set -l cdpath (string match -v '.' -- $CDPATH)

    # Remove the CWD if it is in CDPATH since, again, the core file name completion logic will
    # handle it.
    set -l cdpath (string match -v -- $PWD $cdpath)
    set -q cdpath[1]
    or return 0

    # TODO: There's a subtlety regarding descriptions - if $cdpath[1]/foo and $cdpath[2]/foo
    # exist, we print both but want the first description to win - this currently works, but
    # is not guaranteed.
    for cdpath in $cdpath
        # Replace $HOME with "~".
        set -l desc (string replace -r -- "^$HOME" "~" "$cdpath")
        # This assumes the CDPATH component itself is cd-able.
        for d in $cdpath/$token*/
            # Remove the cdpath component again.
            test -x $d
            and printf "%s\tCDPATH %s\n" (string replace -r "^$cdpath/" "" -- $d) $desc
        end
    end
end
