# a function to obtain a list of ports with prt-get

function __fish_prt_ports -d 'Obtain a list of ports'
    prt-get list
end
