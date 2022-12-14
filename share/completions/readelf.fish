# Completions for the 'readelf' command

# common section names and what they contain
set -l sections '
	.bss\t"Uninitialized read-write data"
	.data\t"Comment section"
	.debug\t"Debugging information"
	.fini\t"Runtime finalization instructions"
	.init\t"Runtime initialization instructions"
	.rodata\t"Read-only data"
	.text\t"Executable text"
	.line\t"Line info for symbolic debugging"
	.note\t"Note information"
	.dynamic\t"Dynamic linking information"
	.dynstr\t"Strings needed for dynamic linking"
	.dynsym\t"Dynamic linking symbol table"
	.got\t"Global offset table"
	.hash\t"Symbol hash table"
	.interp\t"Path name of a program interpreter"
	.plt\t"Procedure linking table"
	.shstrtab\t"String table for the section header table names"
	.strtab\t"String table"
	.symtab\t"Symbol table"
'

complete -c readelf -s a -l all -d "Equivalent to -h -l -S -s -r -d -V -A -I"
complete -c readelf -s h -l file-header -d "Display the ELF file header"
complete -c readelf -s l -l program-headers -l segments -d "Display the program headers"
complete -c readelf -s S -l section-headers -l sections -d "Display the sections' header"
complete -c readelf -s g -l section-groups -d "Display the section groups"
complete -c readelf -s t -l section-details -d "Display the section details"
complete -c readelf -s e -l headers -d "Equivalent to -h -l -S"
complete -c readelf -s s -l syms -l symbols -d "Display the symbol table"
complete -c readelf -l dyn-syms -d "Display the dynamic symbol table"
complete -c readelf -l lto-syms -d "Display LTO symbol tables"
complete -c readelf -l sym-base -d "Force base for symbol sizes" -a '0\t"Mixed" 8\t"Octal" 10\t"Decimal" 16\t"Hexadecimal"' -x
complete -c readelf -s C -l demangle -d "Decode mangled/processed symbol names" -a 'none auto gnu-v3 java gnat dlang rust' -x
complete -c readelf -l no-demangle -d "Don't demangle low-level symbol names"
complete -c readelf -l recurse-limit -d "Enable a demangling recursion limit"
complete -c readelf -l no-recurse-limit -d "Disable a demangling recursion limit"
complete -c readelf -s U -l unicode -d "Controls the display of non-ASCII characters in identifier names" -a 'd\t"default" l\t"locale" e\t"escape" x\t"hex" h\t"highlight" i\t"invalid"' -x
complete -c readelf -s n -l notes -d "Display the core notes"
complete -c readelf -s r -l relocs -d "Display the relocations"
complete -c readelf -s u -l unwind -d "Display the unwind info"
complete -c readelf -s d -l dynamic -d "Display the dynamic section"
complete -c readelf -s V -l version-info -d "Display the version sections"
complete -c readelf -s A -l arch-specific -d "Display architecture specific information"
complete -c readelf -s c -l archive-index -d "Display the symbol/file index in an archive"
complete -c readelf -s D -l use-dynamic -d "Use the dynamic section info when displaying symbols"
complete -c readelf -s L -l lint -l enable-checks -d "Display warning messages for possible problems"
complete -c readelf -s x -l hex-dump -d "Dump the contents of specified section as bytes" -a $sections -x
complete -c readelf -s p -l string-dump -d "Dump the contents of specified section as strings" -a $sections -x
complete -c readelf -s R -l relocated-dump -d "Dump the relocated contents of of specified section" -a $sections -x
complete -c readelf -s z -l decompress -d "Decompress section before dumping it"
complete -c readelf -l debug-dump -d "Display the contents of DWARF debug sections"
complete -c readelf -o wa -l debug-dump=abbrev -d "Displays the contents of the .debug_abbrev section"
complete -c readelf -o wA -l debug-dump=addr -d "Displays the contents of the .debug_addr section"
complete -c readelf -o wc -l debug-dump=cu_index -d "Displays the contents of the .debug_cu_index and/or .debug_tu_index sections"
complete -c readelf -o wf -l debug-dump=frames -d "Display the raw contents of a .debug_frame section"
complete -c readelf -o wF -l debug-dump=frames-interp -d "Display the interpreted contents of a .debug_frame section"
complete -c readelf -o wg -l debug-dump=gdb_index -d " Displays the contents of the .gdb_index and/or .debug_names sections"
complete -c readelf -o wi -l debug-dump=info -d "Displays the contents of the .debug_info section"
complete -c readelf -o wk -l debug-dump=links -d "Display the contents of sections that link to separate debuginfo files"
complete -c readelf -o wK -l debug-dump=follow-links -d "Follow links to separate debug info files"
complete -c readelf -o wN -l debug-dump=no-follow-links -d "Don't follow links to separate debug info files"
complete -c readelf -o wD -l debug-dump=use-debuginfod -d "When following links, also query debuginfod servers"
complete -c readelf -o wE -l debug-dump=do-not-use-debuginfod -d "When following links, don't query debuginfod servers"
complete -c readelf -o wl -l debug-dump=rawline -d "Displays the contents of the .debug_line section in a raw format"
complete -c readelf -o wL -l debug-dump=decodedline -d "Displays the interpreted contents of the .debug_line section"
complete -c readelf -o wm -l debug-dump=macro -d "Displays the contents of the .debug_macro and/or .debug_macinfo sections"
complete -c readelf -o wo -l debug-dump=loc -d "Displays the contents of the .debug_loc and/or .debug_loclists sections"
complete -c readelf -o wO -l debug-dump=str-offsets -d "Displays the contents of the .debug_str_offsets section"
complete -c readelf -o wp -l debug-dump=pubnames -d "Displays the contents of the .debug_pubnames and/or .debug_gnu_pubnames sections"
complete -c readelf -o wr -l debug-dump=aranges -d "Displays the contents of the .debug_aranges section"
complete -c readelf -o wR -l debug-dump=Ranges -d "Displays the contents of the .debug_ranges and/or .debug_rnglists sections"
complete -c readelf -o ws -l debug-dump=str -d "Displays the contents of the .debug_str, .debug_line_str and/or .debug_str_offsets sections"
complete -c readelf -o wt -l debug-dump=pubtype -d "Displays the contents of the .debug_pubtypes and/or .debug_gnu_pubtypes sections"
complete -c readelf -o wT -l debug-dump=trace_aranges -d "Displays the contents of the .trace_aranges section"
complete -c readelf -o wu -l debug-dump=trace_abbrev -d "Displays the contents of the .trace_abbrev section"
complete -c readelf -o wU -l debug-dump=trace_info -d "Displays the contents of the .trace_info section"
complete -c readelf -s P -l process-links -d "Display the contents of non-debug sections in separate debuginfo files"
complete -c readelf -l dwarf-depth -d "Don't display DIEs at greater than specified depth" -x
complete -c readelf -l dwarf-start -d "Display DIEs starting at specified offset" -x
complete -c readelf -l ctf -d "Display CTF info from specified section" -x
complete -c readelf -l ctf-parent -d "Use specified CTF archive member as the CTF parent" -x
complete -c readelf -l ctf-symbols -d "Use specified section as the CTF external symtab" -x
complete -c readelf -l ctf-strings -d "Use specified section as the CTF external strtab" -x
complete -c readelf -s I -l histogram -d "Display histogram of bucket list lengths"
complete -c readelf -s W -l wide -d "Allow output width to exceed 80 characters"
complete -c readelf -s T -l silent-truncation -d "If a symbol name is truncated, don't add [...] suffix"
complete -c readelf -s H -l help -d "Display this information"
complete -c readelf -s v -l version -d "Display the version number of readelf"
