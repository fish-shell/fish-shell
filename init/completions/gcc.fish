#
# The gcc completion list is incomplete. There are just so many of them...
#

complete -c gcc -s x -d 'Language' -x -a '
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
complete -c gcc -o pass-exit-codes -d 'Pass program exit codes'
complete -c gcc -s c -d 'Stop after assembler'
complete -c gcc -s S -d 'Stop after compile'
complete -c gcc -s E -d 'Stop after preprocesswor'
complete -c gcc -s o -r -d 'Output file'
complete -c gcc -s v -d 'Print commands to stderr'
complete -c gcc -o \#\#\# -d 'Print quoted commands to stderr, do not run'
complete -c gcc -o pipe -d 'Use pipes'
complete -c gcc -o ansi -d 'Use ansi mode'
complete -c gcc -o std -d 'Standard mode' -x -a '
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
complete -c gcc -o aux-info -r -d 'Write prototypes to file'
complete -c gcc -o fno-asm -d 'Do not recognize asm, inline or typeof keywords'
complete -c gcc -o fno-builtin -d 'Do not use builtin functions'
complete -c gcc -o fhosted -d 'Assert hosted environment'
complete -c gcc -o ffreestanding -d 'Assert freestanding environment'
complete -c gcc -o fms-extensions -d 'Use Microsoft extensions'
complete -c gcc -o trigraphs -d 'Use ANSI trigraphs'
complete -c gcc -o no-integrated-cpp -d 'Do not use integrated preprocessor'
complete -c gcc -o funsigned-char -d 'char is unsigned'
complete -c gcc -o fsigned-char -d 'char is signed'
complete -c gcc -o funsigned-bitfields -d 'bifield is unsigned'
complete -c gcc -o fsigned-bitfields -d 'bifield is signed'
complete -c gcc -o fno-unsigned-bitfields -d 'All bifields are signed'
complete -c gcc -o fno-signed-bitfields -d 'All bifield are signed'
complete -c gcc -o fwritable-strings -d 'String constants are not const'
complete -c gcc -o fabi-version -d 'C++ ABI version' -r -x -a '1 0'
complete -c gcc -o fno-access-control -d 'Turn off access checking'
complete -c gcc -o fcheck-new -d 'Check pointer returned by new'
complete -c gcc -o fconserve-space -d 'Put globals in the common segment'
complete -c gcc -o fno-const-strings -d 'String constants are not const'
complete -c gcc -o fdollars-in-identifiers -d 'Accept $ in identifiers'
complete -c gcc -o fno-dollars-in-identifiers -d 'Reject $ in identifiers'
complete -c gcc -o fno-elide-constructors -d 'Do not omit unneeded temporarys'
complete -c gcc -o fno-enforce-eh-specs -d 'Allow exception violations'
complete -c gcc -o ffor-scope -d 'Do not extend for-loop scope'
complete -c gcc -o fno-for-scope -d 'Extend for-loop scope'
complete -c gcc -o fno-gnu-keywords -d 'Do not recognize typeof as keyword'
complete -c gcc -o fno-implicit-templates -d 'Do not emit code for implicit templates'
complete -c gcc -o fno-implicit-inline-templates -d 'Do not emit code for implicit inline templates'
complete -c gcc -o fno-implement-inlines -d 'Do not emit out-of-line code for inline functions'
complete -c gcc -o fms-extensions -d 'Disable warnings about MFC'
complete -c gcc -o fno-nonansi-builtins -d 'Disable some built-in functions'
complete -c gcc -o fno-operator-names -d 'Disable operator keywords'
complete -c gcc -o fno-optional-diags -d 'Disable optional diagnostics'
complete -c gcc -o fpermissive -d 'Downgrade some errors to warnings'
complete -c gcc -o frepo -d 'Enable automatic template instantiation at link time'
complete -c gcc -o fno-rtti -d 'Disable generation of C++ runtime type information'
#complete -c gcc -o fstats -d 'Emit front-end usage statistics'

for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17;
	complete -c gcc -o ftemplate-depth-1 -d Set\ maximum\ template\ depth\ to\ $i;
end;

complete -c gcc -o fno-threadsafe-statistics -d 'Do not emit code for thread-safe initialization of local statics'
complete -c gcc -o fuse-cxa-atexit -d 'Use __cxa_atexit for destructors'
complete -c gcc -o fvisibility-inlines-hidden -d 'Hides inline methods from export table'
complete -c gcc -o fno-weak -d 'Do not use weak symbol support'
# gcc completion listing is incomplete.
#complete -c gcc -o -d ''


