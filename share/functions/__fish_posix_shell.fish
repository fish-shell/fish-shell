# localization: skip(private)
function __fish_posix_shell
    # NOTE: this is currently duplicated with PATH_BSHELL
    if status build-info | string match -rq '^Target( \(and host\))?: .*-android(eabi)?$'
        echo /system/bin/sh
    else
        echo /bin/sh
    end
end
