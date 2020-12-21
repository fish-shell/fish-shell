# Completions for the F# Interactive
# See: https://docs.microsoft.com/en-us/dotnet/fsharp/language-reference/fsharp-interactive-options

# Input files
complete -c fsharpi -l "use:" -d "Use the given file on startup"
complete -c fsharpi -l "load:" -d "#load the given file on startup"
complete -c fsharpi -o "r:" -l "reference:" -d "Reference an assembly"

# Code generation
complete -c fsharpi -s g -o "g+" -l debug -l "debug+" -d "Emit debug information"
complete -c fsharpi -o g- -l debug- -d "Disable --debug"
for arguments in full pdbonly portable embedded
    complete -c fsharpi -o "g:$arguments" -l "debug:$arguments" -d "Specify debugging type"
end

complete -c fsharpi -s O -o "O+" -l optimize -l "optimize+" -d "Enable optimizations"
complete -c fsharpi -o O- -l optimize- -d "Disable --optimize"
complete -c fsharpi -l tailcalls -l "tailcalls+" -d "Enable or disable tailcalls"
complete -c fsharpi -l tailcalls- -d "Disable --tailcalls"
complete -c fsharpi -l deterministic -l "deterministic+" -d "Produce a deterministic assembly"
complete -c fsharpi -l deterministic- -d "Disable --deterministic"
complete -c fsharpi -l crossoptimize -l "crossoptimize+" -d "Enable or disable cross-module optimizations"
complete -c fsharpi -l crossoptimize- -d "Disable --crossoptimize"

# Errors and warnings
complete -c fsharpi -l warnaserror -l "warnaserror+" -d "Report all warnings as errors"
complete -c fsharpi -l warnaserror- -d "Disable --warnaserror"
complete -c fsharpi -l "warnaserror:" -l "warnaserror+:" -d "Report specific warnings as errors"
complete -c fsharpi -l "warnaserror-:" -d "Disable --warnaserror:"

for warning_level in (seq 0 5)
    complete -c fsharpi -l "warn:$warning_level" -d "Set a warning level to $warning_level"
end

complete -c fsharpi -l "nowarn:" -d "Disable specific warning messages"
complete -c fsharpi -l "warnon:" -d "Enable specific warnings that may be off by default"
complete -c fsharpi -l consolecolors -l "consolecolors+" -d "Output warning and error messages in color"
complete -c fsharpi -l consolecolors- -d "Disable --consolecolors"

# Language
complete -c fsharpi -l checked -l "checked+" -d "Generate overflow checks"
complete -c fsharpi -l checked- -d "Disable --checked"
complete -c fsharpi -o "d:" -l "define:" -d "Define conditional compilation symbols"
complete -c fsharpi -l mlcompatibility -d "Ignore ML compatibility warnings"

# Miscellaneous
complete -c fsharpi -l nologo -d "Suppress compiler copyright message"
complete -c fsharpi -s "?" -l help -d "Display this usage message"

# Advanced
complete -c fsharpi -l "codepage:" -d "Specify the codepage used to read source files"
complete -c fsharpi -l utf8output -d "Output messages in UTF-8 encoding"
complete -c fsharpi -l fullpaths -d "Output messages with fully qualified paths"
complete -c fsharpi -o "I:" -l "lib:" -d "Specify a dir for the include path for source files and assemblies"
complete -c fsharpi -l simpleresolution -d "Resolve assembly references using directory-based rules"
complete -c fsharpi -l "targetprofile:" -d "Specify target framework profile: mscorlib, netcore or netstandard"
complete -c fsharpi -l noframework -d "Do not reference the default CLI assemblies by default"
complete -c fsharpi -l exec -d "Exit fsi after loading files or running the .fsx script"
complete -c fsharpi -l gui -l "gui+" -d "Execute interactions on a Windows Forms event loop"
complete -c fsharpi -l gui- -d "Disable --gui"
complete -c fsharpi -l quiet -d "Suppress fsi writing to stdout"
complete -c fsharpi -l readline -l "readline+" -d "Support TAB completion in console"
complete -c fsharpi -l readline- -d "Disable --readline"
complete -c fsharpi -l quotations-debug -l "quotations-debug+" -d "Emit debug information in quotations"
complete -c fsharpi -l quotations-debug- -d "Disable --quotations-debug"
complete -c fsharpi -l shadowcopyreferences -l "shadowcopyreferences+" -d "Prevents references being locked by the F# Interactive process"
complete -c fsharpi -l shadowcopyreferences- -d "Disable --shadowcopyreferences"
