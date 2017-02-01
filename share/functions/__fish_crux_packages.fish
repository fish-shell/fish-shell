# a function to obtain a list of installed packages with CRUX pkgutils
function __fish_crux_packages -d 'Obtain a list of installed packages'
    pkginfo -i | cut -d' ' -f1
end
