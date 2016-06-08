set __fish_lscpu_columns CPU\tLogical\ CPU\ number\ of\ a\ CPU\nCORE\tLogical\ core\ number\nSOCKET\tLogical\ socket\ number\nBOOK\tLogical\ book\ number\nNODE\tLogical\ NUMA\ node\ number\nCACHE\tInformation\ about\ how\ caches\ are\ shared\nPOLARIZATION\tCPU\ dispatching\ mode\ on\ virtual\ hardware\nADDRESS\tPhysical\ address\nCONFIGURED\tShows\ if\ the\ hypervisor\ has\ allocated\ the\ CPU\nONLINE\tShows\ if\ Linux\ currently\ use\ the\ CPU\nMAXMHZ\tShows\ the\ maximum\ MHz\nMINMHZ\tShows\ the\ minimum\ MHz

complete -c lscpu -l all -s a -d "Print both online and offline CPUs"
complete -c lscpu -l online -s b -d "Print online CPUs only"
complete -c lscpu -l offline -s c -d "Print offline CPUs only"
complete -c lscpu -l extended -s e -x -a '(__fish_append , $__fish_lscpu_columns)' -d "Print out an extended readable format"
complete -c lscpu -l parse -s p -x -a '(__fish_append , $__fish_lscpu_columns)' -d "Print out a parseble format"
complete -c lscpu -l sysroot -s s -r -d "Use specified directory as system root"
complete -c lscpu -l hex -s x -d "Print hexadecimal masks rather thans lists of CPUs"
complete -c lscpu -l help -s h -d "Display help"
complete -c lscpu -l version -s V -d "Display version"
