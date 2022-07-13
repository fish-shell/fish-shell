if readelf --version 2>/dev/null >/dev/null
    complete -c readelf -s a -l all -d 'Display all information about the ELF file'
    complete -c readelf -s e -l headers -d 'Display all the headers present in the ELF file'
    complete -c readelf -s s -l symbols -d 'Display the entries in symbol table section of the ELF file, if it has one'
    complete -c readelf -s h -l file-header -d 'Display the information contained in the ELF header at the start of the file'
end
