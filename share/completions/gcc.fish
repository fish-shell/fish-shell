#
# The gcc completion list is incomplete. There are just so many of them...
#

complete -c gcc -s x -d (_ "Language") -x -a '
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
complete -c gcc -o pass-exit-codes -d (_ "Pass program exit codes")
complete -c gcc -s c -d (_ "Stop after assembler")
complete -c gcc -s S -d (_ "Stop after compile")
complete -c gcc -s E -d (_ "Stop after preprocessor")
complete -c gcc -s o -r -d (_ "Output file")
complete -c gcc -s v -d (_ "Print commands to stderr")
complete -c gcc -o \#\#\# -d (_ "Print quoted commands to stderr, do not run")
complete -c gcc -o pipe -d (_ "Use pipes")
complete -c gcc -o ansi -d (_ "Use ansi mode")
complete -c gcc -o std -d (_ "Standard mode") -x -a '
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
complete -c gcc -o aux-info -r -d (_ "Write prototypes to file")
complete -c gcc -o fno-asm -d (_ "Do not recognize asm, inline or typeof keywords")
complete -c gcc -o fno-builtin -d (_ "Do not use builtin functions")
complete -c gcc -o fhosted -d (_ "Assert hosted environment")
complete -c gcc -o ffreestanding -d (_ "Assert freestanding environment")
complete -c gcc -o fms-extensions -d (_ "Use Microsoft extensions")
complete -c gcc -o trigraphs -d (_ "Use ANSI trigraphs")
complete -c gcc -o no-integrated-cpp -d (_ "Do not use integrated preprocessor")
complete -c gcc -o funsigned-char -d (_ "char is unsigned")
complete -c gcc -o fsigned-char -d (_ "char is signed")
complete -c gcc -o funsigned-bitfields -d (_ "bitfield is unsigned")
complete -c gcc -o fsigned-bitfields -d (_ "bitfield is signed")
complete -c gcc -o fno-unsigned-bitfields -d (_ "All bitfields are signed")
complete -c gcc -o fno-signed-bitfields -d (_ "All bitfields are unsigned")
complete -c gcc -o fwritable-strings -d (_ "String constants are not const")
complete -c gcc -o fabi-version -d (_ "C++ ABI version") -r -x -a '1 0'
complete -c gcc -o fno-access-control -d (_ "Turn off access checking")
complete -c gcc -o fcheck-new -d (_ "Check pointer returned by new")
complete -c gcc -o fconserve-space -d (_ "Put globals in the common segment")
complete -c gcc -o fno-const-strings -d (_ "String constants are not const")
complete -c gcc -o fdollars-in-identifiers -d (_ "Accept \$ in identifiers")
complete -c gcc -o fno-dollars-in-identifiers -d (_ "Reject \$ in identifiers")
complete -c gcc -o fno-elide-constructors -d (_ "Do not omit unneeded temporarys")
complete -c gcc -o fno-enforce-eh-specs -d (_ "Allow exception violations")
complete -c gcc -o ffor-scope -d (_ "Do not extend for-loop scope")
complete -c gcc -o fno-for-scope -d (_ "Extend for-loop scope")
complete -c gcc -o fno-gnu-keywords -d (_ "Do not recognize typeof as keyword")
complete -c gcc -o fno-implicit-templates -d (_ "Do not emit code for implicit templates")
complete -c gcc -o fno-implicit-inline-templates -d (_ "Do not emit code for implicit inline templates")
complete -c gcc -o fno-implement-inlines -d (_ "Do not emit out-of-line code for inline functions")
complete -c gcc -o fms-extensions -d (_ "Disable warnings about MFC")
complete -c gcc -o fno-nonansi-builtins -d (_ "Disable some built-in functions")
complete -c gcc -o fno-operator-names -d (_ "Disable operator keywords")
complete -c gcc -o fno-optional-diags -d (_ "Disable optional diagnostics")
complete -c gcc -o fpermissive -d (_ "Downgrade some errors to warnings")
complete -c gcc -o frepo -d (_ "Enable automatic template instantiation at link time")
complete -c gcc -o fno-rtti -d (_ "Disable generation of C++ runtime type information")
#complete -c gcc -o fstats -d (_ "Emit front-end usage statistics")

for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17;
	complete -c gcc -o ftemplate-depth-1 -d (printf (_ "Set maximum template depth to %s") $i);
end;

complete -c gcc -o fno-threadsafe-statistics -d (_ "Do not emit code for thread-safe initialization of local statics")
complete -c gcc -o fuse-cxa-atexit -d (_ "Use __cxa_atexit for destructors")
complete -c gcc -o fvisibility-inlines-hidden -d (_ "Hides inline methods from export table")
complete -c gcc -o fno-weak -d (_ "Do not use weak symbol support")
# gcc completion listing is incomplete.
#complete -c gcc -o -d (_ "")


