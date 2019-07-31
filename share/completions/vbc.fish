# Completions for the Visual Basic Compiler(Roslyn)
# See: https://docs.microsoft.com/en-us/dotnet/visual-basic/reference/command-line-compiler/compiler-options-listed-by-category

# Compiler output
complete -c vbc -o nologo -d "Suppresses compiler banner information"
complete -c vbc -o utf8output -o "utf8output+" -d "Displays compiler output using UTF-8 encoding"
complete -c vbc -o "utf8output-" -d "Disable -utf8output"
complete -c vbc -o verbose -o "verbose+" -d "Outputs extra information during compilation"
complete -c vbc -o "verbose-" -d "Disable -verbose"
complete -c vbc -o "modulename:" -d "Specify the name of the source module"
complete -c vbc -o "preferreduilang:" -d "Specify a language for compiler output"

# Optimization
for bytes in 512 1024 2048 4096 8192
    complete -c vbc -o "filealign:$bytes" -d "Specifies where to align the sections of the output file"
end

complete -c vbc -o optimize -o "optimize+" -d "Enable optimizations"
complete -c vbc -o "optimize-" -d "Disable optimizations"

# Output files
complete -c vbc -o doc -o "doc:" -o "doc+" -d "Process documentation comments to an XML file"
complete -c vbc -o "doc-" -d "Disable -doc"
complete -c vbc -o deterministic -d "Causes the compiler to output an assembly whose binary content is identical across compilations if inputs are identical"
complete -c vbc -o netcf -d "Sets the compiler to target the .NET Compact Framework"
complete -c vbc -o "out:" -d "Specifies an output file"
complete -c vbc -o refonly -d "Outputs only a reference assembly"
complete -c vbc -o "refout:" -d "Specifies the output path of a reference assembly"
complete -c vbc -o "t:exe" -o "target:exe" -d "Causes the compiler to create an executable console application (default)"
complete -c vbc -o "t:library" -o "target:library" -d "Causes the compiler to create a dynamic-link library (DLL)"
complete -c vbc -o "t:module" -o "target:module" -d "Causes the compiler to generate a module that can be added to an assembly"
complete -c vbc -o "t:winexe" -o "target:winexe" -d "Causes the compiler to create an executable Windows-based application"
complete -c vbc -o "t:appcontainerexe" -o "target:appcontainerexe" -d "Causes the compiler to create an executable Windows-based application that must be run in an app container"
complete -c vbc -o "t:winmdobj" -o "target:winmdobj" -d "Causes the compiler to create an intermediate file that you can convert to a Windows Runtime binary (.winmd) file"

# .NET assemblies
complete -c vbc -o "addmodule:" -d "Causes the compiler to make all type information from the specified file(s) available to the project you are currently compiling"
complete -c vbc -o delaysign -o "delaysign+" -d "Specifies whether the assembly will be fully or partially signed"
complete -c vbc -o "delaysign-" -d "Disable -delaysign"
complete -c vbc -o "imports:" -d "Imports a namespace from a specified assembly"
complete -c vbc -o "keycontainer:" -d "Specifies a key container name for a key pair to give an assembly a strong name"
complete -c vbc -o "keyfile:" -d "Specifies a file containing a key or key pair to give an assembly a strong name"
complete -c vbc -o "libpath:" -d "Specifies the location of assemblies referenced by the -reference option"
complete -c vbc -o "r:" -o "reference:" -d "Imports metadata from an assembly"
complete -c vbc -o "moduleassemblyname:" -d "Specifies the name of the assembly that a module will be a part of"
complete -c vbc -o "a:" -o "analyzer:" -d "Run the analyzers from this assembly"
complete -c vbc -o "additionalfile:" -d "Names additional files that don't directly affect code generation but may be used by analyzers for producing errors or warnings"

# Debugging/error checking
complete -c vbc -o "bugreport:" -d "Creates a file that contains information that makes it easy to report a bug"

complete -c vbc -o debug -o "debug+" -d "Produces debugging information"
for arguments in full pdbonly
    complete -c vbc -o "debug:$arguments" -d "Produces debugging information"
end
complete -c vbc -o "debug-" -d "Disable -debug"

complete -c vbc -o nowarn -o "nowarn:" -d "Suppresses the compiler's ability to generate warnings"
complete -c vbc -o quiet -d "Prevents the compiler from displaying code for syntax-related errors and warnings"
complete -c vbc -o removeintchecks -o "removeintchecks+" -d "Disables integer overflow checking"
complete -c vbc -o "removeintchecks-" -d "Disable -removeintchecks"
complete -c vbc -o warnaserror -o "warnaserror:" -o "warnaserror+" -o "warnaserror+:" -d "Promotes warnings to errors"
complete -c vbc -o "warnaserror-" -o "warnaserror-:" -d "Disable -warnaserror"
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
complete -c vbc -o "optionexplicit-" -d "Disable -optionexplicit"
complete -c vbc -o optionstrict -o "optionstrict+" -o "optionstrict:" -d "Enforces strict type semantics"
complete -c vbc -o "optionstrict-" -d "Disable -optionstrict"

for arguments in binary text
    complete -c vbc -o "optioncompare:$arguments" -d "Specifies whether string comparisons should be binary or use locale-specific text semantics"
end

complete -c vbc -o optioninfer -o "optioninfer+" -d "Enables the use of local type inference in variable declarations"
complete -c vbc -o "optioninfer-" -d "Disable -optioninfer"

# Preprocessor
complete -c vbc -o "d:" -o "define:" -d "Defines symbols for conditional compilation"

# Resources
complete -c vbc -o "linkres:" -o "linkresource:" -d "Creates a link to a managed resource"
complete -c vbc -o "res:" -o "resource:" -d "Embeds a managed resource in an assembly"
complete -c vbc -o "win32icon:" -d "Inserts an .ico file into the output file"
complete -c vbc -o "win32resource:" -d "Inserts a Win32 resource into the output file"

# Miscellaneous
complete -c vbc -o "baseaddress:" -d "Specifies the base address of a DLL"
complete -c vbc -o "codepage:" -d "Specifies the code page to use for all source code files in the compilation"

for arguments in prompt queue send none
    complete -c vbc -o "errorreport:$arguments" -d "Specifies how the Visual Basic compiler should report internal compiler errors"
end

complete -c vbc -o highentropyva -o "highentropyva+" -d "Tells the Windows kernel whether a particular executable supports high entropy Address Space Layout Randomization (ASLR)"
complete -c vbc -o "highentropyva-" -d "Disable -highentropyva"
complete -c vbc -o "m:" -o "main:" -d "Specifies the class that contains the Sub Main procedure to use at startup"
complete -c vbc -o noconfig -d "Do not compile with Vbc.rsp"
complete -c vbc -o nostdlib -d "Causes the compiler not to reference the standard libraries"
complete -c vbc -o nowin32manifest -d "Instructs the compiler not to embed any application manifest into the executable file"
complete -c vbc -o "platform:x86" -d "Specify x86 as the processor platform the compiler targets for the output file"
complete -c vbc -o "platform:x64" -d "Specify AMD64 or EM64T as the processor platform the compiler targets for the output file"
complete -c vbc -o "platform:Itanium" -d "Specify Itanium as the processor platform the compiler targets for the output file"
complete -c vbc -o "platform:arm" -d "Specify ARM as the processor platform the compiler targets for the output file"
complete -c vbc -o "platform:anycpu" -d "Specify any platform as the processor platform the compiler targets for the output file (default)"
complete -c vbc -o "platform:anycpu32bitpreferred" -d "Specify any platform as the processor platform the compiler targets for the output file"
complete -c vbc -o "recurse:" -d "Searches subdirectories for source files to compile"
complete -c vbc -o "rootnamespace:" -d "Specifies a namespace for all type declarations"
complete -c vbc -o "sdkpath:" -d "Specifies the location of Mscorlib.dll and Microsoft.VisualBasic.dll"

complete -c vbc -o "vbruntime:" -d "Specifies that the compiler should compile without a reference to the Visual Basic Runtime Library, or with a reference to a specific runtime library"
for arguments in - + "*"
    complete -c vbc -o "vbruntime:$arguments" -d "Specifies that the compiler should compile without a reference to the Visual Basic Runtime Library, or with a reference to a specific runtime library"
end

complete -c vbc -o "win32manifest:" -d "Identifies a user-defined Win32 application manifest file to be embedded into a project's portable executable (PE) file"
complete -c vbc -o parallel -o "parallel+" -d "Specifies whether to use concurrent build"
complete -c vbc -o "parallel-" -d "Disable -parallel"
complete -c vbc -o "checksumalgorithm:SHA1" -d "Specify SHA1 as the algorithm for calculating the source file checksum stored in PDB (default)"
complete -c vbc -o "checksumalgorithm:SHA256" -d "Specify SHA256 as the algorithm for calculating the source file checksum stored in PDB"
