function __fish_check_libcdb_subcommand -a sub
    set -l subcommand (commandline -opc)
    if test (count $subcommand) -le 2
        return 1
    end
    if test "$subcommand[3]" = "$sub"
        return 0
    else
        return 1
    end
end

complete -c pwn -f

complete -c pwn -n __fish_use_subcommand -s h -l help -d "show this help message and exit"

complete -c pwn -f -n __fish_use_subcommand -a asm -d "Assemble shellcode into bytes"
complete -c pwn -f -n __fish_use_subcommand -a checksec -d "Check binary security settings"
complete -c pwn -f -n __fish_use_subcommand -a constgrep -d "Looking up constants from header files"
complete -c pwn -f -n __fish_use_subcommand -a cyclic -d "Cyclic pattern creator/finder"
complete -c pwn -f -n __fish_use_subcommand -a debug -d "Debug a binary in GDB"
complete -c pwn -f -n __fish_use_subcommand -a disasm -d "Disassemble bytes into text format"
complete -c pwn -f -n __fish_use_subcommand -a disablenx -d "Disable NX for an ELF binary"
complete -c pwn -f -n __fish_use_subcommand -a elfdiff -d "Compare two ELF files"
complete -c pwn -f -n __fish_use_subcommand -a elfpatch -d "Patch an ELF file"
complete -c pwn -f -n __fish_use_subcommand -a errno -d "Prints out error messages"
complete -c pwn -f -n __fish_use_subcommand -a hex -d "Hex-encodes data provided on the command line or stdin"
complete -c pwn -f -n __fish_use_subcommand -a libcdb -d "Print various information about a libc binary"
complete -c pwn -f -n __fish_use_subcommand -a phd -d "Pretty hex dump"
complete -c pwn -f -n __fish_use_subcommand -a pwnstrip -d "Strip binaries for CTF usage"
complete -c pwn -f -n __fish_use_subcommand -a scramble -d "Shellcode encoder"
complete -c pwn -f -n __fish_use_subcommand -a shellcraft -d "Microwave shellcode -- Easy, fast and delicious"
complete -c pwn -f -n __fish_use_subcommand -a template -d "Generate an exploit template"
complete -c pwn -f -n __fish_use_subcommand -a unhex -d "Decodes hex-encoded data provided on the command line or via stdin"
complete -c pwn -f -n __fish_use_subcommand -a update -d "Check for pwntools updates"
complete -c pwn -f -n __fish_use_subcommand -a version -d "Pwntools version"

complete -c pwn -f -n "__fish_seen_subcommand_from asm" -s f -l format -ra "raw hex string elf" -d "Output format"
complete -c pwn -n "__fish_seen_subcommand_from asm" -s o -l output -d "Output file"
complete -c pwn -f -n "__fish_seen_subcommand_from asm" -s c -l context -ra "16 32 64 baremetal freebsd windows android darwin linux cgc powerpc64 aarch64 powerpc riscv32 riscv64 sparc64 mips64 msp430 alpha amd64 sparc thumb cris i386 ia64 m68k mips s390 none avr arm vax little big be eb le el " -d "The os/architecture/endianness/bits the shellcode will run in"
complete -c pwn -f -n "__fish_seen_subcommand_from asm" -s v -l avoid -d "Encode the shellcode to avoid the listed bytes"
complete -c pwn -f -n "__fish_seen_subcommand_from asm" -s n -l newline -d "Encode the shellcode to avoid newlines"
complete -c pwn -f -n "__fish_seen_subcommand_from asm" -s z -l zero -d "Encode the shellcode to avoid NULL bytes"
complete -c pwn -f -n "__fish_seen_subcommand_from asm" -s d -l debug -d "Debug the shellcode with GDB"
complete -c pwn -f -n "__fish_seen_subcommand_from asm" -s e -l encoder -d "Specific encoder to use"
complete -c pwn -n "__fish_seen_subcommand_from asm" -s i -l infile -d "Specify input file"
complete -c pwn -f -n "__fish_seen_subcommand_from asm" -s r -l run -d "Run output"

complete -c pwn -n "__fish_seen_subcommand_from checksec" -F

complete -c pwn -n "__fish_seen_subcommand_from constgrep" -s e -l exact -d "Do an exact match for a constant instead of searching for a regex"
complete -c pwn -n "__fish_seen_subcommand_from constgrep" -s i -l case-insensitive -d "Search case insensitive"
complete -c pwn -n "__fish_seen_subcommand_from constgrep" -s m -l mask-mode -d "search for values not containing strictly less bits that the given value."
complete -c pwn -n "__fish_seen_subcommand_from constgrep" -s c -l context -ra "16 32 64 baremetal freebsd windows android darwin linux cgc powerpc64 aarch64 powerpc riscv32 riscv64 sparc64 mips64 msp430 alpha amd64 sparc thumb cris i386 ia64 m68k mips s390 none avr arm vax little big be eb le el" -d "The os/architecture/endianness/bits the shellcode will run in"

complete -c pwn -f -n "__fish_seen_subcommand_from cyclic" -s a -l alphabet -d "The alphabet to use in the cyclic pattern"
complete -c pwn -f -n "__fish_seen_subcommand_from cyclic" -s n -l length -d "Size of the unique subsequences"
complete -c pwn -n "__fish_seen_subcommand_from cyclic" -s c -l context -ra "16 32 64 baremetal freebsd windows android darwin linux cgc powerpc64 aarch64 powerpc riscv32 riscv64 sparc64 mips64 msp430 alpha amd64 sparc thumb cris i386 ia64 m68k mips s390 none avr arm vax little big be eb le el" -d "The os/architecture/endianness/bits the shellcode will run in"
complete -c pwn -f -n "__fish_seen_subcommand_from cyclic" -s o -l offset -d "Do a lookup instead printing the alphabet"
complete -c pwn -f -n "__fish_seen_subcommand_from cyclic" -s l -l lookup -d "Do a lookup instead printing the alphabet"

complete -c pwn -n "__fish_seen_subcommand_from debug" -s x -rF -d "Execute GDB commands from this file."
complete -c pwn -f -n "__fish_seen_subcommand_from debug" -l pid -d "PID to attach to"
complete -c pwn -n "__fish_seen_subcommand_from debug" -s c -l context -ra "16 32 64 baremetal freebsd windows android darwin linux cgc powerpc64 aarch64 powerpc riscv32 riscv64 sparc64 mips64 msp430 alpha amd64 sparc thumb cris i386 ia64 m68k mips s390 none avr arm vax little big be eb le el" -d "The os/architecture/endianness/bits the shellcode will run in"
complete -c pwn -n "__fish_seen_subcommand_from debug" -l exec -rF -d "File to debug"
complete -c pwn -f -n "__fish_seen_subcommand_from debug" -l process -d "Name of the process to attach to"
complete -c pwn -n "__fish_seen_subcommand_from debug" -l sysroot -rF -d "GDB sysroot path"

complete -c pwn -n "__fish_seen_subcommand_from disasm" -s c -l context -ra "16 32 64 baremetal freebsd windows android darwin linux cgc powerpc64 aarch64 powerpc riscv32 riscv64 sparc64 mips64 msp430 alpha amd64 sparc thumb cris i386 ia64 m68k mips s390 none avr arm vax little big be eb le el" -d "The os/architecture/endianness/bits the shellcode will run in"
complete -c pwn -f -n "__fish_seen_subcommand_from disasm" -s a -l address -d "Base address"
complete -c pwn -f -n "__fish_seen_subcommand_from disasm" -l color -d "Color output"
complete -c pwn -f -n "__fish_seen_subcommand_from disasm" -l no-color -d "Disable color output"

complete -c pwn -n "__fish_seen_subcommand_from disablenx" -F

complete -c pwn -n "__fish_seen_subcommand_from elfdiff" -F

complete -c pwn -n "__fish_seen_subcommand_from elfpatch" -F

complete -c pwn -f -n "__fish_seen_subcommand_from hex" -s p -l prefix -d "Insert a prefix before each byte"
complete -c pwn -f -n "__fish_seen_subcommand_from hex" -s s -l separator -d "Add a separator between each byte"

complete -c pwn -f -n "__fish_seen_subcommand_from libcdb; and not __fish_seen_subcommand_from lookup hash file fetch" -ra "lookup hash file fetch"
complete -c pwn -f -n "__fish_check_libcdb_subcommand lookup" -s f -l fetch -d "Fetch the libc.so from the libc database"
complete -c pwn -f -n "__fish_check_libcdb_subcommand lookup" -l no-unstrip -d "Do NOT attempt to unstrip the libc binary with debug symbols from adebuginfod server"
complete -c pwn -f -n "__fish_check_libcdb_subcommand lookup" -l offline-only -d "Attempt to searching with offline only mode"
complete -c pwn -f -n "__fish_check_libcdb_subcommand lookup" -s o -l output -d "Output file"
complete -c pwn -f -n "__fish_check_libcdb_subcommand hash" -s t -l hash_type -ra "id buildid md5 sha1 sha256" -d "The type of the provided hash value"
complete -c pwn -f -n "__fish_check_libcdb_subcommand hash" -s d -l download-libc -d "Attempt to download the matching libc.so"
complete -c pwn -f -n "__fish_check_libcdb_subcommand hash" -l no-unstrip -d "Do NOT attempt to unstrip the libc binary with debug symbols from adebuginfod server"
complete -c pwn -f -n "__fish_check_libcdb_subcommand hash" -l offline-only -d "Attempt to searching with offline only mode"
complete -c pwn -f -n "__fish_check_libcdb_subcommand file" -s s -l symbols -d "List of symbol offsets to dump in addition to the common ones"
complete -c pwn -f -n "__fish_check_libcdb_subcommand file" -s o -l offset -d "Display all offsets relative to this symbol"
complete -c pwn -f -n "__fish_check_libcdb_subcommand file" -l unstrip -d "Attempt to unstrip the libc binary inplace with debug symbols from a debuginfod server"
complete -c pwn -n "__fish_check_libcdb_subcommand file" -F
complete -c pwn -f -n "__fish_check_libcdb_subcommand fetch" -s u -l update -d "Fetch the desired libc categories"
complete -c pwn -n "__fish_check_libcdb_subcommand fetch" -F

complete -c pwn -n "__fish_seen_subcommand_from phd" -F
complete -c pwn -f -n "__fish_seen_subcommand_from phd" -s w -l width -d "Number of bytes per line."
complete -c pwn -f -n "__fish_seen_subcommand_from phd" -s l -l highlight -d "Byte to highlight."
complete -c pwn -f -n "__fish_seen_subcommand_from phd" -s s -l skip -d "Skip this many initial bytes."
complete -c pwn -f -n "__fish_seen_subcommand_from phd" -s c -l count -d 'Only show this many bytes'
complete -c pwn -f -n "__fish_seen_subcommand_from phd" -s o -l offset -d "Addresses in left hand column starts at this address."
complete -c pwn -f -n "__fish_seen_subcommand_from phd" -l color -d "Colorize the output. When 'auto' output is colorized exactly when stdout is a TTY."

complete -c pwn -n "__fish_seen_subcommand_from pwnstrip" -F
complete -c pwn -f -n "__fish_seen_subcommand_from pwnstrip" -s o -l output
complete -c pwn -f -n "__fish_seen_subcommand_from pwnstrip" -s b -l build-id -d "Strip build ID"
complete -c pwn -f -n "__fish_seen_subcommand_from pwnstrip" -s p -l patch -d "Patch function"

complete -c pwn -f -n "__fish_seen_subcommand_from scramble" -s f -l format -ra "raw hex string elf" -d "Output format"
complete -c pwn -f -n "__fish_seen_subcommand_from scramble" -s o -l output -d "Output file"
complete -c pwn -n "__fish_seen_subcommand_from scramble" -s c -l context -ra "16 32 64 baremetal freebsd windows android darwin linux cgc powerpc64 aarch64 powerpc riscv32 riscv64 sparc64 mips64 msp430 alpha amd64 sparc thumb cris i386 ia64 m68k mips s390 none avr arm vax little big be eb le el" -d "The os/architecture/endianness/bits the shellcode will run in"
complete -c pwn -f -n "__fish_seen_subcommand_from scramble" -s p -l alphanumeric -d "Encode the shellcode with an alphanumeric encoder"
complete -c pwn -f -n "__fish_seen_subcommand_from scramble" -s v -l avoid -d "Encode the shellcode to avoid the listed bytes"
complete -c pwn -f -n "__fish_seen_subcommand_from scramble" -s n -l newline -d "Encode the shellcode to avoid newlines"
complete -c pwn -f -n "__fish_seen_subcommand_from scramble" -s z -l zero -d "Encode the shellcode to avoid NULL bytes"
complete -c pwn -f -n "__fish_seen_subcommand_from scramble" -s d -l debug -d "Debug the shellcode with GDB"

complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s "?" -l show -d "Show shellcode documentation"
complete -c pwn -n "__fish_seen_subcommand_from shellcraft" -s o -l out -rF -d "Output file"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s f -l format -d "Output format"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s f -l format -ra e -d elf
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s f -l format -ra r -d raw
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s f -l format -ra s -d string
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s f -l format -ra c -d "c-style array"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s f -l format -ra h -d "hex string"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s f -l format -ra i -d hexii
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s f -l format -ra a -d "assmebly code"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s f -l format -ra p -d "preprocssed code"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s f -l format -ra d -d "escaped hex string"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s d -l debug -d "Debug the shellcode with GDB"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -l delim -d "Set the delimiter between multilple shellcodes"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s b -l before -d "Insert a debug trap before the code"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s a -l after -d "Insert a debug trap after the code"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s v -l avoid -d "Encode the shellcode to avoid the listed bytes"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s n -l newline -d "Encode the shellcode to avoid newlines"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s z -l zero -d "Encode the shellcode to avoid NULL bytes"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s r -l run -d "Run output"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -l color -d "Color output"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -l no-color -d "Disable Color output"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -l syscalls -d "List Syscalls"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -l address -d "Load address"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s l -l list -d "List available shellcodes, optionally provide a filter"
complete -c pwn -f -n "__fish_seen_subcommand_from shellcraft" -s s -l shared -d "Generated ELF is a shared library"

complete -c pwn -n "__fish_seen_subcommand_from template" -F
complete -c pwn -f -n "__fish_seen_subcommand_from template" -l host -d "Remote host / SSH server"
complete -c pwn -f -n "__fish_seen_subcommand_from template" -l port -d "Remote host / SSH port"
complete -c pwn -f -n "__fish_seen_subcommand_from template" -l user -d "SSH Username"
complete -c pwn -f -n "__fish_seen_subcommand_from template" -l pass -d "SSH Password"
complete -c pwn -f -n "__fish_seen_subcommand_from template" -l password -d "SSH Password"
complete -c pwn -n "__fish_seen_subcommand_from template" -l libc -rF -d "Path to libc binary to use. If not given, the current directory is searched for a libc binary."
complete -c pwn -n "__fish_seen_subcommand_from template" -l path -rF -d "Remote path of file on SSH server"
complete -c pwn -f -n "__fish_seen_subcommand_from template" -l quiet -d "Less verbose template comments"
complete -c pwn -f -n "__fish_seen_subcommand_from template" -l color -ra "never always auto" -d "Print the output in color"
complete -c pwn -n "__fish_seen_subcommand_from template" -l template -rF -d "Path to a custom template."
complete -c pwn -n "__fish_seen_subcommand_from template" -l no-auto -rF -d "Do not automatically detect missing binaries"

complete -c pwn -f -n "__fish_seen_subcommand_from update" -l install -d "Install the update automatically"
complete -c pwn -f -n "__fish_seen_subcommand_from update" -l pre -d "Check for pre-releases"
