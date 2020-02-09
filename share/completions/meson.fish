# Completions for the meson build system (http://mesonbuild.com/)

set -l basic_arguments \
    "h,help,show help message and exit" \
    ",stdsplit,Split stdout and stderr in test logs" \
    ",errorlogs,Print logs from failing test(s)" \
    ",werror,Treat warnings as errors" \
    ",strip,Strip targets on install" \
    "v,version,Show version number and exit"

set -l dir_arguments \
    ",localedir,Locale data directory [share/locale]" \
    ",sbindir,System executable directory [sbin]" \
    ",infodir,Info page directory [share/info]" \
    ",prefix,Installation prefix [/usr/local]" \
    ",mandir,Manual page directory [share/man]" \
    ",datadir,Data file directory [share]" \
    ",bindir,Executable directory [bin]" \
    ",sharedstatedir,Arch-agnostic data directory [com]" \
    ",libdir,Library directory [system default]" \
    ",localstatedir,Localstate data directory [var]" \
    ",libexecdir,Library executable directory [libexec]" \
    ",includedir,Header file directory [include]" \
    ",sysconfdir,Sysconf data directory [etc]"

for arg in $basic_arguments
    set -l parts (string split , -- $arg)
    if not string match -q "" -- $parts[1]
        complete -c meson -s "$parts[1]" -l "$parts[2]" -d "$parts[3]"
    else
        complete -c meson -l "$parts[2]" -d "$parts[3]"
    end
end

for arg in $dir_arguments
    set -l parts (string split , -- $arg)
    complete -c meson -l "$parts[2]" -d "$parts[3]" -xa '(__fish_complete_directories)'
end

complete -c meson -s "D" -d "Set value of an option (-D foo=bar)"

complete -c meson -l buildtype -xa 'plain debug debugoptimized release minsize' -d "Set build type [debug]"
complete -c meson -l layout -xa 'mirror flat' -d "Build directory layout [mirror]"
complete -c meson -l backend -xa 'ninja vs vs2010 vs2015 vs2017 xcode' -d "Compilation backend [ninja]"
complete -c meson -l default-library -xa 'shared static both' -d "Default library type [shared]"
complete -c meson -l warning-level -xa '1 2 3' -d "Warning level [1]"
complete -c meson -l unity -xa 'on off subprojects' -d "Unity build [off]"
complete -c meson -l cross-file -r -d "File describing cross-compilation environment"
complete -c meson -l wrap-mode -xa 'WrapMode.{default,nofallback,nodownload,forcefallback}' -d "Special wrap mode to use"

# final parameter
complete -c meson -n '__fish_is_first_token' -xa '(__fish_complete_directories)'
