if command -s uname > /dev/null ^/dev/null
    if test (uname) = "Linux"
        complete -c modinfo -a "(__fish_print_modules)"
        complete -c modinfo -l author -s a -d "Print only 'author'"
        complete -c modinfo -l description -s d -d "Print only 'description'"
        complete -c modinfo -l license -s l -d "Print only 'license'"
        complete -c modinfo -l parameters -s p -d "Print only 'parm'"
        complete -c modinfo -l filename -s n -d "Print only 'filename'"
        complete -c modinfo -l null -s 0 -d "Use \\0 instead of \\n"
        complete -c modinfo -l field -s F -x -d "Print only provided FIELD" -a "author description license parm depends alias intree vermagic vermagic"
        complete -c modinfo -l set-version -s k -x -d "Use VERSION instead of `uname -r`"
        complete -c modinfo -l basedir -s b -r -d "Use DIR as filesystem root for /lib/modules"
        complete -c modinfo -l version -s V -d "Show version"
        complete -c modinfo -l help -s h -d "Show help"
    end
end
