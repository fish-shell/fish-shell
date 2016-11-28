# a function to obtain a list of installed packages with prt-get
function __fish_prt_packages -d 'Obtain a list of installed packages'
    prt-get listinst
end
