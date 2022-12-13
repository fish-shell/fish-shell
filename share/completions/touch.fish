complete touch -d "change file access and modification times"
# common options
complete touch -s a -d "change access time (atime)"
complete touch -s m -d "change modification time (mtime)"
complete touch -s t -d "use specified time [[CC]YY]MMDDhhmm[.SS]"

if touch --version 2>/dev/null >/dev/null # GNU
    complete touch -s B -l backward -x -d "set date back"
    complete touch -s c -l no-create -d "don't create file if it doesn't exist"
    complete touch -s d -l date -x -d "set to specified YYYY-MM-DDThh:mm:SS[.frac][tz]"
    complete touch -s f -l forward -x -d "set date forward"
    complete touch -s r -l reference -d "use times from specified reference file"
    # TODO these may require that = but builtin complete doesn't seem to infer it should
    # use = here
    complete touch -l time -x -d "change specified kind of timestamp" -a \
        "atime\t'change access time (atime) only'
    access\t'change access time (atime) only'
    use\t'change access time (atime) only'
    mtime\t'change modification time (mtime) only'
    modify\t'change modification time (mtime) only'"
    complete touch -l help -d "display help"
    complete touch -l version -d "display version"
else # not GNU
    set -l uname (uname -s)
    contains $uname Darwin DragonFly FreeBSD
    and complete touch -s A -d "adjust timestamps by relative value [-][hh]mm]SS" -x
    contains $uname Darwin DragonFly FreeBSD NetBSD
    and complete touch -s h -d "act on symbolic links themselves"
    contains $uname Darwin DragonFly FreeBSD OpenBSD SunOS
    and complete touch -s d -d "set to specified YYYY-MM-DDThh:mm:SS[.frac][tz]" -x
    # common BSD options
    complete touch -s c -d "don't create file if it doesn't exist"
    complete touch -s r -d "use times from specified reference file" -r
end
