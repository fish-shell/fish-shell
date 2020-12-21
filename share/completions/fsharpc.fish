# Completions for the F# Compiler
# See: https://docs.microsoft.com/en-us/dotnet/fsharp/language-reference/compiler-options

# Output files
complete -c fsharpc -o "o:" -l "out:" -d "Name of the output file"
complete -c fsharpc -o "a:exe" -l "target:exe" -d "Build a console executable"
complete -c fsharpc -o "a:winexe" -l "target:winexe" -d "Build a Windows executable"
complete -c fsharpc -o "a:library" -l "target:library" -d "Build a library"
complete -c fsharpc -o "a:module" -l "target:module" -d "Build a module that can be added to another assembly"
complete -c fsharpc -l delaysign -l "delaysign+" -d "Delay-sign the assembly using only the public portion of the strong name key"
complete -c fsharpc -l delaysign- -d "Disable --delaysign"
complete -c fsharpc -l "doc:" -d "Write the xmldoc of the assembly to the given file"
complete -c fsharpc -l "keyfile:" -d "Specify a strong name key file"
complete -c fsharpc -l "keycontainer:" -d "Specify a strong name key container"
complete -c fsharpc -l "platform:x86" -d "Limit the platform to x86"
complete -c fsharpc -l "platform:Itanium" -d "Limit the platform to Itanium"
complete -c fsharpc -l "platform:x64" -d "Limit the platform to x64"
complete -c fsharpc -l "platform:anycpu32bitpreferred" -d "Limit the platform to anycpu32bitpreferred"
complete -c fsharpc -l "platform:anycpu" -d "Limit the platform to anycpu (default)"
complete -c fsharpc -l nooptimizationdata -d "Only include optimization information for inlined constructs"
complete -c fsharpc -l nointerfacedata -d "Don't add a resource to the assembly containing F#-specific metadata"
complete -c fsharpc -l "sig:" -d "Print the inferred interface of the assembly to a file"

# Input files
complete -c fsharpc -o "r:" -l "reference:" -d "Reference an assembly"

# Resources
complete -c fsharpc -l "win32res:" -d "Specify a Win32 resource file (.res)"
complete -c fsharpc -l "win32manifest:" -d "Specify a Win32 manifest file"
complete -c fsharpc -l nowin32manifest -d "Do not include the default Win32 manifest"
complete -c fsharpc -l "resource:" -d "Embed the specified managed resource"
complete -c fsharpc -l "linkresource:" -d "Link the specified resource to this assembly"

# Code generation
complete -c fsharpc -s g -o "g+" -l debug -l "debug+" -d "Emit debug information"
complete -c fsharpc -o g- -l debug- -d "Disable --debug"
for arguments in full pdbonly portable embedded
    complete -c fsharpc -o "g:$arguments" -l "debug:$arguments" -d "Specify debugging type"
end

complete -c fsharpc -s O -o "O+" -l optimize -l "optimize+" -d "Enable optimizations"
complete -c fsharpc -o O- -l optimize- -d "Disable --optimize"
complete -c fsharpc -l tailcalls -l "tailcalls+" -d "Enable or disable tailcalls"
complete -c fsharpc -l tailcalls- -d "Disable --tailcalls"
complete -c fsharpc -l deterministic -l "deterministic+" -d "Produce a deterministic assembly"
complete -c fsharpc -l deterministic- -d "Disable --deterministic"
complete -c fsharpc -l crossoptimize -l "crossoptimize+" -d "Enable or disable cross-module optimizations"
complete -c fsharpc -l crossoptimize- -d "Disable --crossoptimize"

# Errors and warnings
complete -c fsharpc -l warnaserror -l "warnaserror+" -d "Report all warnings as errors"
complete -c fsharpc -l warnaserror- -d "Disable --warnaserror"
complete -c fsharpc -l "warnaserror:" -l "warnaserror+:" -d "Report specific warnings as errors"
complete -c fsharpc -l "warnaserror-:" -d "Disable --warnaserror:"

for warning_level in (seq 0 5)
    complete -c fsharpc -l "warn:$warning_level" -d "Set a warning level to $warning_level"
end

complete -c fsharpc -l "nowarn:" -d "Disable specific warning messages"
complete -c fsharpc -l "warnon:" -d "Enable specific warnings that may be off by default"
complete -c fsharpc -l consolecolors -l "consolecolors+" -d "Output warning and error messages in color"
complete -c fsharpc -l consolecolors- -d "Disable --consolecolors"

# Language
complete -c fsharpc -l checked -l "checked+" -d "Generate overflow checks"
complete -c fsharpc -l checked- -d "Disable --checked"
complete -c fsharpc -o "d:" -l "define:" -d "Define conditional compilation symbols"
complete -c fsharpc -l mlcompatibility -d "Ignore ML compatibility warnings"

# Miscellaneous
complete -c fsharpc -l nologo -d "Suppress compiler copyright message"
complete -c fsharpc -s "?" -l help -d "Display this usage message"

# Advanced
complete -c fsharpc -l "codepage:" -d "Specify the codepage used to read source files"
complete -c fsharpc -l utf8output -d "Output messages in UTF-8 encoding"
complete -c fsharpc -l fullpaths -d "Output messages with fully qualified paths"
complete -c fsharpc -o "I:" -l "lib:" -d "Specify a directory for include path for resolving source files and assemblies"
complete -c fsharpc -l simpleresolution -d "Resolve assembly references using directory-based rules"
complete -c fsharpc -l "baseaddress:" -d "Base address for the library to be built"
complete -c fsharpc -l noframework -d "Do not reference the default CLI assemblies by default"
complete -c fsharpc -l standalone -d "Statically link F# library and referenced DLLs into the generated assembly"
complete -c fsharpc -l "staticlink:" -d "Statically link the assembly and referenced DLLs that depend on this assembly"
complete -c fsharpc -l "pdb:" -d "Name the output debug file"
complete -c fsharpc -l highentropyva -l "highentropyva+" -d "Enable high-entropy ASLR"
complete -c fsharpc -l highentropyva- -d "Disable --highentropyva"
complete -c fsharpc -l "subsystemversion:" -d "Specify subsystem version of this assembly"
complete -c fsharpc -l quotations-debug -l "quotations-debug+" -d "Emit debug information in quotations"
complete -c fsharpc -l quotations-debug- -d "Disable --quotations-debug"
