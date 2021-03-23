# Completions for the Visual Basic Compiler(Roslyn)
# See: https://docs.microsoft.com/en-us/dotnet/visual-basic/reference/command-line-compiler/compiler-options-listed-by-category

# Compiler output
complete -c vbc -o nologo -d "Suppresses compiler banner information"
complete -c vbc -o utf8output -o "utf8output+" -d "Displays compiler output using UTF-8 encoding"
complete -c vbc -o utf8output- -d "Disable -utf8output"
complete -c vbc -o verbose -o "verbose+" -d "Outputs extra information during compilation"
complete -c vbc -o verbose- -d "Disable -verbose"
complete -c vbc -o "modulename:" -d "Specify the name of the source module"
complete -c vbc -o "preferreduilang:" -d "Specify a language for compiler output"

# Optimization
for bytes in 512 1024 2048 4096 8192
    complete -c vbc -o "filealign:$bytes" -d "Specifies where to align the sections of the output file"
end

complete -c vbc -o optimize -o "optimize+" -d "Enable optimizations"
complete -c vbc -o optimize- -d "Disable optimizations"

# Output files
complete -c vbc -o doc -o "doc:" -o "doc+" -d "Process documentation comments to an XML file"
complete -c vbc -o doc- -d "Disable -doc"
complete -c vbc -o deterministic -d "Output identical assemblies for identical inputs"
complete -c vbc -o netcf -d "Sets the compiler to target the .NET Compact Framework"
complete -c vbc -o "out:" -d "Specifies an output file"
complete -c vbc -o refonly -d "Outputs only a reference assembly"
complete -c vbc -o "refout:" -d "Specifies the output path of a reference assembly"
complete -c vbc -o "t:exe" -o "target:exe" -d "Create a console application"
complete -c vbc -o "t:library" -o "target:library" -d "Create a dynamic-link library"
complete -c vbc -o "t:module" -o "target:module" -d "Create a module that can be added to an assembly"
complete -c vbc -o "t:winexe" -o "target:winexe" -d "Create Windows application"
complete -c vbc -o "t:appcontainerexe" -o "target:appcontainerexe" -d "Create Windows application that must be run in an app container"
complete -c vbc -o "t:winmdobj" -o "target:winmdobj" -d "Create an intermediate file that can be converted to .winmd file"

# .NET assemblies
complete -c vbc -o "addmodule:" -d "Add type information from provided files to the project"
complete -c vbc -o delaysign -o "delaysign+" -d "Specifies whether the assembly will be fully or partially signed"
complete -c vbc -o delaysign- -d "Disable -delaysign"
complete -c vbc -o "imports:" -d "Imports a namespace from a specified assembly"
complete -c vbc -o "keycontainer:" -d "Specify a strong name key container"
complete -c vbc -o "keyfile:" -d "Specify a strong name key file"
complete -c vbc -o "libpath:" -d "Specifies the location of assemblies referenced by the -reference option"
complete -c vbc -o "r:" -o "reference:" -d "Imports metadata from an assembly"
complete -c vbc -o "moduleassemblyname:" -d "Specifies the name of the assembly that a module will be a part of"
complete -c vbc -o "a:" -o "analyzer:" -d "Run the analyzers from this assembly"
complete -c vbc -o "additionalfile:" -d "Files that analyzers can use to generate error or warning"

# Debugging/error checking
complete -c vbc -o "bugreport:" -d "Creates a file that contains information that makes it easy to report a bug"

complete -c vbc -o debug -o "debug+" -d "Produces debugging information"
for arguments in full pdbonly
    complete -c vbc -o "debug:$arguments" -d "Produces debugging information"
end
complete -c vbc -o debug- -d "Disable -debug"

complete -c vbc -o nowarn -o "nowarn:" -d "Suppresses the compiler's ability to generate warnings"
complete -c vbc -o quiet -d "Don't display code for syntax-related errors and warnings"
complete -c vbc -o removeintchecks -o "removeintchecks+" -d "Disables integer overflow checking"
complete -c vbc -o removeintchecks- -d "Disable -removeintchecks"
complete -c vbc -o warnaserror -o "warnaserror:" -o "warnaserror+" -o "warnaserror+:" -d "Promotes warnings to errors"
complete -c vbc -o warnaserror- -o "warnaserror-:" -d "Disable -warnaserror"
complete -c vbc -o "ruleset:" -d "Specify a ruleset file that disables specific diagnostics"

# Help
complete -c vbc -s "?" -o help -d "Displays the compiler options"

# Language
complete -c vbc -o "langversion:?" -d "Display the allowed values for language version"
complete -c vbc -o "langversion:default" -d "Specify latest major version as language version"
complete -c vbc -o "langversion:latest" -d "Specify latest version (including minor releases) as language version"
for version_number in (seq 9 15)
    switch $version_number
        case (seq 9 12) 14
            complete -c vbc -o "langversion:$version_number" -d "Specify $version_number as language version"
            complete -c vbc -o "langversion:$version_number.0" -d "Specify $version_number.0 as language version"
        case 15
            complete -c vbc -o "langversion:$version_number" -d "Specify $version_number as language version"
            complete -c vbc -o "langversion:$version_number.0" -d "Specify $version_number.0 as language version"
            complete -c vbc -o "langversion:$version_number.3" -d "Specify $version_number.3 as language version"
            complete -c vbc -o "langversion:$version_number.5" -d "Specify $version_number.5 as language version"
    end
end

complete -c vbc -o optionexplicit -o "optionexplicit+" -d "Enforces explicit declaration of variables"
complete -c vbc -o optionexplicit- -d "Disable -optionexplicit"
complete -c vbc -o optionstrict -o "optionstrict+" -o "optionstrict:" -d "Enforces strict type semantics"
complete -c vbc -o optionstrict- -d "Disable -optionstrict"

for arguments in binary text
    complete -c vbc -o "optioncompare:$arguments" -d "Specify string comparison mode: text or binary"
end

complete -c vbc -o optioninfer -o "optioninfer+" -d "Enables the use of local type inference in variable declarations"
complete -c vbc -o optioninfer- -d "Disable -optioninfer"

# Preprocessor
complete -c vbc -o "d:" -o "define:" -d "Defines symbols for conditional compilation"

# Resources
complete -c vbc -o "linkres:" -o "linkresource:" -d "Creates a link to a managed resource"
complete -c vbc -o "res:" -o "resource:" -d "Embeds a managed resource in an assembly"
complete -c vbc -o "win32icon:" -d "Inserts an .ico file into the output file"
complete -c vbc -o "win32resource:" -d "Inserts a Win32 resource into the output file"

# Miscellaneous
complete -c vbc -o "baseaddress:" -d "Specifies the base address of a DLL"
complete -c vbc -o "codepage:" -d "Specify the code page for source code files"

for arguments in prompt queue send none
    complete -c vbc -o "errorreport:$arguments" -d "Specify how to report internal compiler errors"
end

complete -c vbc -o highentropyva -o "highentropyva+" -d "Specify if an executable supports high entropy ASLR"
complete -c vbc -o highentropyva- -d "Disable -highentropyva"
complete -c vbc -o "m:" -o "main:" -d "Specifies the class that contains the Sub Main procedure to use at startup"
complete -c vbc -o noconfig -d "Do not compile with Vbc.rsp"
complete -c vbc -o nostdlib -d "Causes the compiler not to reference the standard libraries"
complete -c vbc -o nowin32manifest -d "Don't embed any application manifest into the executable"
complete -c vbc -o "platform:x86" -d "Specify x86 as target platform"
complete -c vbc -o "platform:x64" -d "Specify AMD64 or EM64T as target platform"
complete -c vbc -o "platform:Itanium" -d "Specify Itanium as target platform"
complete -c vbc -o "platform:arm" -d "Specify ARM as target platform"
complete -c vbc -o "platform:anycpu" -d "Specify any platform as target"
complete -c vbc -o "platform:anycpu32bitpreferred" -d "Specify any platform as target (32-bit process on 64-bit machine)"
complete -c vbc -o "recurse:" -d "Searches subdirectories for source files to compile"
complete -c vbc -o "rootnamespace:" -d "Specifies a namespace for all type declarations"
complete -c vbc -o "sdkpath:" -d "Specifies the location of Mscorlib.dll and Microsoft.VisualBasic.dll"

complete -c vbc -o "vbruntime:" -d "Specify reference to VB Runtime Library or disable library referencing"
for arguments in - + "*"
    complete -c vbc -o "vbruntime:$arguments" -d "Specify reference to VB Runtime Library or disable library referencing"
end

complete -c vbc -o "win32manifest:" -d "Provide application manifest file"
complete -c vbc -o parallel -o "parallel+" -d "Specifies whether to use concurrent build"
complete -c vbc -o parallel- -d "Disable -parallel"
complete -c vbc -o "checksumalgorithm:SHA1" -d "Use SHA1 to calculate the source file checksum"
complete -c vbc -o "checksumalgorithm:SHA256" -d "Use SHA256 to calculate the source file checksum"
