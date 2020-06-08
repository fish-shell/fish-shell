# a function to obtain a list of installed packages with CRUX pkgutils
function __fish_crux_packages -d 'Obtain a list of installed packages'
    pkginfo -i | string split -f1 ' '
end
