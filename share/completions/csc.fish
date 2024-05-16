# Completions for the Visual C# Compiler(Roslyn)
# See: https://docs.microsoft.com/en-us/dotnet/csharp/language-reference/compiler-options/listed-by-category

# Optimization
for bytes in 512 1024 2048 4096 8192
    complete -c csc -o "filealign:$bytes" -d "Specifies the size of sections in the output file"
end

complete -c csc -s o -o "o+" -o optimize -o "optimize+" -d "Enable optimizations"
complete -c csc -o o- -o optimize- -d "Disable optimizations"

# Output Files
complete -c csc -o deterministic -d "Make output identical across compilations if inputs are identical"
complete -c csc -o "doc:" -d "Specifies an XML file where processed documentation comments are to be written"
complete -c csc -o "out:" -d "Specifies the output file"
complete -c csc -o "pathmap:" -d "Specify a mapping for source path names output by the compiler"
complete -c csc -o "pdb:" -d "Specifies the file name and location of the .pdb file"
complete -c csc -o "platform:anycpu" -d "Specify any platform as the output platform (default)"
complete -c csc -o "platform:anycpu32bitpreferred" -d "Specify any platform as the output platform"
complete -c csc -o "platform:ARM" -d "Specify ARM as the output platform"
complete -c csc -o "platform:ARM64" -d "Specify ARM64 as the output platform"
complete -c csc -o "platform:x64" -d "Specify AMD64 or EM64T as the output platform"
complete -c csc -o "platform:x86" -d "Specify x86 as the output platform"
complete -c csc -o "platform:Itanium" -d "Specify Itanium as the output platform"
complete -c csc -o "preferreduilang:" -d "Specify a language for compiler output"
complete -c csc -o "refout:" -d "Generate a reference assembly in addition to the primary assembly"
complete -c csc -o refonly -d "Generate a reference assembly instead of a primary assembly"
complete -c csc -o "t:appcontainerexe" -o "target:appcontainerexe" -d "Specify .exe file for Windows 8.x Store apps as the format of the output file"
complete -c csc -o "t:exe" -o "target:exe" -d "Specify .exe file as the format of the output file (default)"
complete -c csc -o "t:library" -o "target:library" -d "Specify code library as the format of the output file"
complete -c csc -o "t:module" -o "target:module" -d "Specify module as the format of the output file"
complete -c csc -o "t:winexe" -o "target:winexe" -d "Specify Windows program as the format of the output file"
complete -c csc -o "t:winmdobj" -o "target:winmdobj" -d "Specify intermediate .winmdobj file as the format of the output file"
complete -c csc -o "modulename:" -d "Specify the name of the source module"

# .NET Framework Assemblies
complete -c csc -o "addmodule:" -d "Specifies one or more modules to be part of this assembly"
complete -c csc -o delaysign -o "delaysign+" -d "Instructs the compiler to add the public key but to leave the assembly unsigned"
complete -c csc -o delaysign- -d "Disable -delaysign"
complete -c csc -o "keycontainer:" -d "Specifies the name of the cryptographic key container"
complete -c csc -o "keyfile:" -d "Specifies the filename containing the cryptographic key"
complete -c csc -o "lib:" -d "Specifies the location of assemblies referenced by means of -reference"
complete -c csc -o nostdlib -o "nostdlib+" -d "Instructs the compiler not to import the standard library (mscorlib.dll)"
complete -c csc -o nostdlib- -d "Disable -nostdlib"
complete -c csc -o publicsign -d "Apply a public key without signing the assembly"
complete -c csc -o "r:" -o "reference:" -d "Imports metadata from a file that contains an assembly"
complete -c csc -o "a:" -o "analyzer:" -d "Run the analyzers from this assembly"
complete -c csc -o "additionalfile:" -d "Names additional files that don't directly affect code generation but may be used by analyzers for producing errors or warnings"
complete -c csc -o embed -d "Embed all source files in the PDB"
complete -c csc -o "embed:" -d "Embed specific files in the PDB"

# Debugging/Error Checking
complete -c csc -o "bugreport:" -d "Creates a file that contains information that makes it easy to report a bug"
complete -c csc -o checked -o "checked+" -d "Generate overflow checks"
complete -c csc -o checked- -d "Disable overflow checks"

complete -c csc -o debug -o "debug+" -d "Instruct the compiler to emit debugging information"
for arguments in full pdbonly
    complete -c csc -o "debug:$arguments" -d "Instruct the compiler to emit debugging information"
end
complete -c csc -o debug- -d "Disable -debug"

for arguments in none prompt queue send
    complete -c csc -o "errorreport:$arguments" -d "Sets error reporting behavior"
end

complete -c csc -o fullpaths -d "Specifies the absolute path to the file in compiler output"
complete -c csc -o "nowarn:" -d "Suppresses the compiler's generation of specified warnings"

for warning_level in (seq 0 4)
    if test $warning_level -ne 4
        complete -c csc -o "w:$warning_level" -o "warn:$warning_level" -d "Sets the warning level to $warning_level"
    else
        complete -c csc -o "w:$warning_level" -o "warn:$warning_level" -d "Sets the warning level to $warning_level (default)"
    end
end

complete -c csc -o warnaserror -o "warnaserror+" -d "Promotes warnings to errors"
complete -c csc -o warnaserror- -d "Disable -warnaserror"
complete -c csc -o "ruleset:" -d "Specify a ruleset file that disables specific diagnostics"

# Preprocessor
complete -c csc -o "d:" -o "define:" -d "Defines preprocessor symbols"

# Resources
complete -c csc -o "l:" -o "link:" -d "Makes COM type information in specified assemblies available to the project"
complete -c csc -o "linkres:" -o "linkresource:" -d "Creates a link to a managed resource"
complete -c csc -o "res:" -o "resource:" -d "Embeds a .NET Framework resource into the output file"
complete -c csc -o "win32icon:" -d "Specifies an .ico file to insert into the output file"
complete -c csc -o "win32res:" -d "Specifies a Win32 resource to insert into the output file"

# Miscellaneous
complete -c csc -s "?" -o help -d "Lists compiler options to stdout"
complete -c csc -o "baseaddress:" -d "Specifies the preferred base address at which to load a DLL"
complete -c csc -o "codepage:" -d "Specifies the code page to use for all source code files in the compilation"
complete -c csc -o highentropyva -o "highentropyva+" -d "Specifies that the executable file supports address space layout randomization (ASLR)"
complete -c csc -o highentropyva- -d "Disable -highentropyva"

complete -c csc -o "langversion:?" -d "Display the allowed values for language version"
complete -c csc -o "langversion:default" -d "Specify latest major version as language version"
complete -c csc -o "langversion:latest" -d "Specify latest version (including minor releases) as language version"
complete -c csc -o "langversion:latestMajor" -d "Specify latest major version as language version"
complete -c csc -o "langversion:preview" -d "Specify latest preview version as language version"
complete -c csc -o "langversion:ISO-1" -d "Specify ISO/IEC 23270:2003 C# (1.0/1.2) as language version"
complete -c csc -o "langversion:ISO-2" -d "Specify ISO/IEC 23270:2006 C# (2.0) as language version"
for version_number in (seq 3 8)
    switch $version_number
        case 7
            complete -c csc -o "langversion:$version_number" -d "Specify C# $version_number.0 as language version"
            complete -c csc -o "langversion:$version_number.1" -d "Specify C# $version_number.1 as language version"
            complete -c csc -o "langversion:$version_number.2" -d "Specify C# $version_number.2 as language version"
            complete -c csc -o "langversion:$version_number.3" -d "Specify C# $version_number.3 as language version"
        case 8
            complete -c csc -o "langversion:$version_number.0" -d "Specify C# $version_number.0 as language version"
        case "*"
            complete -c csc -o "langversion:$version_number" -d "Specify C# $version_number.0 as language version"
    end
end

complete -c csc -o "m:" -o "main:" -d "Specifies the location of the Main method"
complete -c csc -o noconfig -d "Instructs the compiler not to compile with csc.rsp"
complete -c csc -o nologo -d "Suppresses compiler banner information"
complete -c csc -o "recurse:" -d "Searches subdirectories for source files to compile"
complete -c csc -o "subsystemversion:" -d "Specifies the minimum version of the subsystem that the executable file can use"
complete -c csc -o unsafe -d "Enables compilation of code that uses the unsafe keyword"
complete -c csc -o utf8output -d "Displays compiler output using UTF-8 encoding"
complete -c csc -o parallel -o "parallel+" -d "Specifies whether to use concurrent build"
complete -c csc -o parallel- -d "Disable -parallel"
complete -c csc -o "checksumalgorithm:SHA1" -d "Specify SHA1 as the algorithm for calculating the source file checksum stored in PDB (default)"
complete -c csc -o "checksumalgorithm:SHA256" -d "Specify SHA256 as the algorithm for calculating the source file checksum stored in PDB"
