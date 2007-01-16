#
# The gcc completion list is incomplete. There are just so many of them...
#

complete -c gcc -s x --description "Language" -x -a '
	c
	c-header
	cpp-output
	c++
	c++-cpp-output
	objective-c
	objc-cpp-output
	assembler
	assembler-with-cpp
	ada
	f77
	f77-cpp-input
	ratfor
	java
	treelang
	none
'
complete -c gcc -o pass-exit-codes --description "Pass program exit codes"
complete -c gcc -s c --description "Stop after assembler"
complete -c gcc -s S --description "Stop after compile"
complete -c gcc -s E --description "Stop after preprocessor"
complete -c gcc -s o -r --description "Output file"
complete -c gcc -s v --description "Print commands to stderr"
complete -c gcc -o \#\#\# --description "Print quoted commands to stderr, do not run"
complete -c gcc -o pipe --description "Use pipes"
complete -c gcc -o ansi --description "Use ansi mode"
complete -c gcc -o std --description "Standard mode" -x -a '
	c89\t"ISO C90"
	iso9899:1990\t"ISO C90"
	iso9899:199409\t"ISO C90 as modified in amendment 1"
	c99\t"ISO C99"
	c9x \t"ISO C99"
	iso9899:1999\t"ISO C99"
	iso9899:199x\t"ISO C99"
	gnu89\t"ISO C90 plus GNU extentions"
	gnu99\t"ISO C99 plus GNU extentions"
	gnu9x\t"ISO C99 plus GNU extentions"
	c++98\t"ISO C++98"
	gnu++98\t"ISO C++98 plus GNU extentions"
'
complete -c gcc -o aux-info -r --description "Write prototypes to file"
complete -c gcc -o fno-asm --description "Do not recognize asm, inline or typeof keywords"
complete -c gcc -o fno-builtin --description "Do not use builtin functions"
complete -c gcc -o fhosted --description "Assert hosted environment"
complete -c gcc -o ffreestanding --description "Assert freestanding environment"
complete -c gcc -o fms-extensions --description "Use Microsoft extensions"
complete -c gcc -o trigraphs --description "Use ANSI trigraphs"
complete -c gcc -o no-integrated-cpp --description "Do not use integrated preprocessor"
complete -c gcc -o funsigned-char --description "char is unsigned"
complete -c gcc -o fsigned-char --description "char is signed"
complete -c gcc -o funsigned-bitfields --description "bitfield is unsigned"
complete -c gcc -o fsigned-bitfields --description "bitfield is signed"
complete -c gcc -o fno-unsigned-bitfields --description "All bitfields are signed"
complete -c gcc -o fno-signed-bitfields --description "All bitfields are unsigned"
complete -c gcc -o fwritable-strings --description "String constants are not const"
complete -c gcc -o fabi-version --description "C++ ABI version" -r -x -a '1 0'
complete -c gcc -o fno-access-control --description "Turn off access checking"
complete -c gcc -o fcheck-new --description "Check pointer returned by new"
complete -c gcc -o fconserve-space --description "Put globals in the common segment"
complete -c gcc -o fno-const-strings --description "String constants are not const"
complete -c gcc -o fdollars-in-identifiers --description "Accept \$ in identifiers"
complete -c gcc -o fno-dollars-in-identifiers --description "Reject \$ in identifiers"
complete -c gcc -o fno-elide-constructors --description "Do not omit unneeded temporarys"
complete -c gcc -o fno-enforce-eh-specs --description "Allow exception violations"
complete -c gcc -o ffor-scope --description "Do not extend for-loop scope"
complete -c gcc -o fno-for-scope --description "Extend for-loop scope"
complete -c gcc -o fno-gnu-keywords --description "Do not recognize typeof as keyword"
complete -c gcc -o fno-implicit-templates --description "Do not emit code for implicit templates"
complete -c gcc -o fno-implicit-inline-templates --description "Do not emit code for implicit inline templates"
complete -c gcc -o fno-implement-inlines --description "Do not emit out-of-line code for inline functions"
complete -c gcc -o fms-extensions --description "Disable warnings about MFC"
complete -c gcc -o fno-nonansi-builtins --description "Disable some built-in functions"
complete -c gcc -o fno-operator-names --description "Disable operator keywords"
complete -c gcc -o fno-optional-diags --description "Disable optional diagnostics"
complete -c gcc -o fpermissive --description "Downgrade some errors to warnings"
complete -c gcc -o frepo --description "Enable automatic template instantiation at link time"
complete -c gcc -o fno-rtti --description "Disable generation of C++ runtime type information"
#complete -c gcc -o fstats --description "Emit front-end usage statistics"

for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17;
	complete -c gcc -o ftemplate-depth-1 --description "Set maximum template depth"
end;

complete -c gcc -o fno-threadsafe-statistics --description "Do not emit code for thread-safe initialization of local statics"
complete -c gcc -o fuse-cxa-atexit --description "Use __cxa_atexit for destructors"
complete -c gcc -o fvisibility-inlines-hidden --description "Hides inline methods from export table"
complete -c gcc -o fno-weak --description "Do not use weak symbol support"

complete -c gcc -l help --description "Display help and exit"
complete -c gcc -l version --description "Display version and exit"

# gcc completion listing is incomplete.
#complete -c gcc -o --description ""


