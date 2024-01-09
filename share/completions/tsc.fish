# Completions for the TypeScript compiler
# See: https://www.typescriptlang.org/docs/handbook/compiler-options.html

complete -c tsc -l allowJs -d "Allow JavaScript files to be compiled"
complete -c tsc -l allowSyntheticDefaultImports -d "Allow default imports from modules with no default export"
complete -c tsc -l allowUmdGlobalAccess -d "Allow accessing UMD globals from modules"
complete -c tsc -l allowUnreachableCode -d "Do not report errors on unreachable code"
complete -c tsc -l allowUnusedLabels -d "Do not report errors on unused labels"
complete -c tsc -l alwaysStrict -d "Parse in strict mode and emit \"use strict\" for each source file"
complete -c tsc -l baseUrl -d "Base directory to resolve non-relative module names"
complete -c tsc -s b -l build -d "Builds this project and all of its dependencies specified by Project References"
complete -c tsc -l charset -d "The character set of the input files"
complete -c tsc -l checkJs -d "Report errors in .js files"
complete -c tsc -l composite -d "Enable constraints that allow a TypeScript project to be used with project references."
complete -c tsc -s d -l declaration -d "Generates corresponding .d.ts file"
complete -c tsc -l declarationDir -d "Output directory for generated declaration files"
complete -c tsc -l declarationMap -d "Create sourcemaps for d.ts files."
complete -c tsc -l diagnostics -d "Show diagnostic information"
complete -c tsc -l disableSizeLimit -d "Disable size limitation on JavaScript project"
complete -c tsc -l downlevelIteration -d "Emit more compliant, but verbose and less performant JavaScript for iteration."
complete -c tsc -l emitBOM -d "Emit a UTF-8 Byte Order Mark (BOM) in the beginning of output files"
complete -c tsc -l emitDeclarationOnly -d "Only emit ‘.d.ts’ declaration files"
complete -c tsc -l emitDecoratorMetadata -d "Emit design-type metadata for decorated declarations in source"
complete -c tsc -l esModuleInterop -d "Emit additional JavaScript to ease support for importing CommonJS modules"
complete -c tsc -l experimentalDecorators -d "Enables experimental support for ES decorators"
complete -c tsc -l extendedDiagnostics -d "Show verbose diagnostic information"
complete -c tsc -l forceConsistentCasingInFileNames -d "Disallow inconsistently-cased references to the same file"
complete -c tsc -s h -l help -d "Print help message"
complete -c tsc -l importHelpers -d "Allow importing helper functions from tslib once per project"
complete -c tsc -l incremental -d "Save .tsbuildinfo files to allow for incremental compilation of projects."
complete -c tsc -l inlineSourceMap -d "Include sourcemap files inside the emitted JavaScript."
complete -c tsc -l inlineSources -d "Emit the source alongside the sourcemaps within a single file"
complete -c tsc -l init -d "Initializes a TypeScript project and creates a tsconfig.json file"
complete -c tsc -l isolatedModules -d "Transpile each file as a separate module (similar to “ts.transpileModule”)"
complete -c tsc -l jsx -d "Support JSX in .tsx files: \"react\", \"preserve\", \"react-native\""
complete -c tsc -l jsxFactory -d "Specify the JSX factory function used when targeting React JSX emit"
complete -c tsc -l keyofStringsOnly -d "Resolve keyof to string valued property names only (no numbers or symbols)"
complete -c tsc -l lib -x -a "ES5 ES6 ES2015 ES7 ES2016 ES2017 ES2018 ESNext DOM DOM.Iterable WebWorker ScriptHost ES2015.Core ES2015.Collection ES2015.Generator ES2015.Iterable ES2015.Promise ES2015.Proxy ES2015.Reflect ES2015.Symbol ES2015.Symbol.WellKnown ES2016.Array.Include ES2017.object ES2017.Intl ES2017.SharedMemory ES2017.String ES2017.TypedArrays ES2018.Intl ES2018.Promise ES2018.RegExp ESNext.AsyncIterable ESNext.Array ESNext.Intl ESNext.Symbol" -d "List of library files to be included in the compilation"
complete -c tsc -l listEmittedFiles -d "Print names of generated files part of the compilation"
complete -c tsc -l listFiles -d "Print names of files part of the compilation"

complete -c tsc -l locale -x -a "(
    printf '%s\t%s\n' en 'English (US)'
    printf '%s\t%s\n' cs Czech
    printf '%s\t%s\n' de German
    printf '%s\t%s\n' es Spanish
    printf '%s\t%s\n' fr French
    printf '%s\t%s\n' it Italian
    printf '%s\t%s\n' ja Japanese
    printf '%s\t%s\n' ko Korean
    printf '%s\t%s\n' pl Polish
    printf '%s\t%s\n' pt-BR 'Portuguese(Brazil)'
    printf '%s\t%s\n' ru Russian
    printf '%s\t%s\n' tr Turkish
    printf '%s\t%s\n' zh-CN 'Simplified Chinese'
    printf '%s\t%s\n' zh-TW 'Traditional Chinese'
)" -d "The locale to use to show error messages, e.g. en-us"

complete -c tsc -l mapRoot -d "Specify the location where debugger should locate map files instead of generated locations"
complete -c tsc -l maxNodeModuleJsDepth -d "The maximum folder depth used for checking JavaScript files from node_modules"
complete -c tsc -s m -l module -x -a "None CommonJS AMD System UMD ES6 ES2015 ESNext" -d "Specify module code generation"
complete -c tsc -l moduleResolution -x -a "Node Classic" -d "Determine how modules get resolved"

complete -c tsc -l newLine -x -a "(
    printf '%s\t%s\n' crlf windows
    printf '%s\t%s\n' lf unix
)" -d "Use the specified end of line sequence to be used when emitting files"

complete -c tsc -l noEmit -d "Do not emit outputs"
complete -c tsc -l noEmitHelpers -d "Disable generating custom helper functions in compiled output"
complete -c tsc -l noEmitOnError -d "Do not emit outputs if any errors were reported"
complete -c tsc -l noErrorTruncation -d "Do not truncate error messages"
complete -c tsc -l noFallthroughCasesInSwitch -d "Report errors for fallthrough cases in switch statement"
complete -c tsc -l noImplicitAny -d "Raise error on expressions and declarations with an implied any type"
complete -c tsc -l noImplicitReturns -d "Report an error for codepaths that do not return in a function"
complete -c tsc -l noImplicitThis -d "Raise error on this expressions with an implied any type"
complete -c tsc -l noImplicitUseStrict -d "Do not emit \"use strict\" directives in module output"
complete -c tsc -l noLib -d "Do not include the default library file (lib.d.ts)"
complete -c tsc -l noResolve -d "Do not add triple-slash references or module import targets to the list of compiled files"
complete -c tsc -l noStrictGenericChecks -d "Disable strict checking of generic signatures in function types"
complete -c tsc -l noUnusedLocals -d "Report errors on unused locals"
complete -c tsc -l noUnusedParameters -d "Report errors on unused parameters"
complete -c tsc -l outDir -d "Redirect output structure to the directory"
complete -c tsc -l outFile -d "Concatenate and emit output to single file"
complete -c tsc -l preserveConstEnums -d "Do not erase const enum declarations in generated code"
complete -c tsc -l preserveSymlinks -d "Disable resolving symlinks to their realpath"
complete -c tsc -l preserveWatchOutput -d "Disable wiping the console in watch mode"
complete -c tsc -l pretty -d "Stylize errors and messages using color and context"
complete -c tsc -s p -l project -d "Compile a project given a valid configuration file"
complete -c tsc -l removeComments -d "Remove all comments except copy-right header comments beginning with /*!"
complete -c tsc -l resolveJsonModule -d "Include modules imported with .json extension"
complete -c tsc -l rootDir -d "Specifies the root directory of input files"
complete -c tsc -l showConfig -d "Print the final configuration instead of building"
complete -c tsc -l skipLibCheck -d "Skip type checking of all declaration files (*.d.ts)"
complete -c tsc -l sourceMap -d "Generates corresponding .map file"
complete -c tsc -l sourceRoot -d "Specify the root path for debuggers to find the reference source code"
complete -c tsc -l strict -d "Enable all strict type checking options"
complete -c tsc -l strictBindCallApply -d "Enable stricter checking of the bind, call, and apply methods on functions"
complete -c tsc -l strictFunctionTypes -d "Disable bivariant parameter checking for function types"
complete -c tsc -l strictPropertyInitialization -d "Ensure non-undefined class properties are initialized in the constructor"
complete -c tsc -l strictNullChecks -d "When type checking, take into account null and undefined"
complete -c tsc -l suppressExcessPropertyErrors -d "Suppress excess property checks for object literals"
complete -c tsc -l suppressImplicitAnyIndexErrors -d "Suppress --noImplicitAny errors for indexing objects lacking index signatures"

complete -c tsc -s t -l target -x -a "(
    printf '%s\t%s\n' ES3 'ECMAScript 3 (default)'
    printf '%s\t%s\n' ES5 'ECMAScript 5'
    printf '%s\t%s\n' ES6 'ECMAScript 6'
    printf '%s\t%s\n' ES2015 'ECMAScript 2015'
    printf '%s\t%s\n' ES2016 'ECMAScript 2016'
    printf '%s\t%s\n' ES2017 'ECMAScript 2017'
    printf '%s\t%s\n' ESNext 'Latest supported ECMAScript'
)" -d "Specify ECMAScript target version"

complete -c tsc -l traceResolution -d "Report module resolution log messages"
complete -c tsc -l tsBuildInfoFile -d "Specify what file to store incremental build information in"
complete -c tsc -l types -d "List of names of type definitions to include"
complete -c tsc -l typeRoots -d "List of folders to include type definitions from"
complete -c tsc -s v -l version -d "Print the compiler’s version"
complete -c tsc -s w -l watch -d "Run the compiler in watch mode"
