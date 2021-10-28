# General Information
complete -c solc -l help -d "Show help message and exit."
complete -c solc -l version -d "Show version and exit."
complete -c solc -l license -d "Show licensing information and exit."
complete -c solc -l input-file -d "input file"

# Input Options
complete -c solc -l base-path -d "Use the given path as the root of the source tree instead of the root of the filesystem."
complete -c solc -l include-path -d "Make an additional source directory available to the default import callback. Use this option if you want to import contracts whose location is not fixed in relation to your main source tree, e.g. third-party libraries installed using a package manager. Can be used multiple times. Can only be used if base path has a non-empty value."
complete -c solc -l allow-paths -d "Allow a given path for imports. A list of paths can be supplied by separating them with a comma."
complete -c solc -l ignore-missing -d "Ignore missing files."
complete -c solc -l error-recovery -d "Enables additional parser error recovery."

# Output Options
complete -c solc -s o -l output-dir -d "If given, creates one file per component and contract/file at the specified directory."
complete -c solc -l overwrite -d " Overwrite existing files (used together with -o)."
complete -c solc -l evm-version -d "Select desired EVM version. Either homestead, tangerineWhistle, spuriousDragon, byzantium, constantinople, petersburg, istanbul, berlin or london."
complete -c solc -l experimental-via-ir -d "Turn on experimental compilation mode via the IR (EXPERIMENTAL)."
complete -c solc -l revert-strings -d "Strip revert (and require) reason strings or add additional debugging information."
complete -c solc -l stop-after -d "Stop execution after the given compiler stage. Valid options: 'parsing'."

# Alternative Input Modes
complete -c solc -l standard-json -d "Switch to Standard JSON input / output mode, ignoring all options. It reads from standard input, if no input file was given, otherwise it reads from the provided input file. The result will be written to standardoutput."
complete -c solc -l link -d "Switch to linker mode, ignoring all options apart from --libraries and modify binaries in place."
complete -c solc -l assemble -d "Switch to assembly mode, ignoring all options except --machine, --yul-dialect, --optimize and --yul-optimizations and assumes input is assembly."
complete -c solc -l yul -d "Switch to Yul mode, ignoring all options except --machine, --yul-dialect, --optimize and --yul-optimizations and assumes input is Yul."
complete -c solc -l strict-assembly -d "Switch to strict assembly mode, ignoring all options except --machine, --yul-dialect, --optimize and --yul-optimizations and assumes input is strict assembly."
complete -c solc -l import-ast -d "Import ASTs to be compiled, assumes input holds the AST in compact JSON format. Supported Inputs is the output of the --standard-json or the one produced by --combined-json ast,compact-format"

# Assembly Mode Options
complete -c solc -l machine -d "Target machine in assembly or Yul mode."
complete -c solc -l yul-dialect -d "Input dialect to use in assembly or yul mode."

# Linker Mode Options
complete -c solc -l libraries -d "Direct string or file containing library addresses."

# Output Formatting
complete -c solc -l pretty-json -d "Output JSON in pretty format."
complete -c solc -l json-indent -d "Indent pretty-printed JSON with N spaces. Enables '--pretty-json' automatically."
complete -c solc -l color -d "Force colored output."
complete -c solc -l no-color -d "Explicitly disable colored output, disabling terminal auto-detection."
complete -c solc -l error-codes -d "Output error codes."

# Output Components
complete -c solc -l ast-compact-json -d "AST of all source files in a compact JSON format."
complete -c solc -l asm -d "EVM assembly of the contracts."
complete -c solc -l asm-json -d "EVM assembly of the contracts in JSON format."
complete -c solc -l opcodes -d "Opcodes of the contracts."
complete -c solc -l bin -d "Binary of the contracts in hex."
complete -c solc -l bin-runtime -d "Binary of the runtime part of the contracts in hex."
complete -c solc -l abi -d "ABI specification of the contracts."
complete -c solc -l ir -d "Intermediate Representation (IR) of all contracts (EXPERIMENTAL)."
complete -c solc -l ir-optimized -d "Optimized intermediate Representation (IR) of all contracts (EXPERIMENTAL)."
complete -c solc -l ewasm -d "Ewasm text representation of all contracts (EXPERIMENTAL)."
complete -c solc -l hashes -d "Function signature hashes of the contracts."
complete -c solc -l userdoc -d "Natspec user documentation of all contracts."
complete -c solc -l devdoc -d "Natspec developer documentation of all contracts."
complete -c solc -l metadata -d "Combined Metadata JSON whose Swarm hash is stored on-chain."
complete -c solc -l storage-layout -d "Slots, offsets and types of the contract's state variables."

# Extra Output:
complete -c solc -l gas -d "Print an estimate of the maximal gas usage for each function."
complete -c solc -l combined-json -d "Output a single json document containing the specified information."

# Metadata Options:
complete -c solc -l metadata-hash -d "Choose hash method for the bytecode metadata or disable it."
complete -c solc -l metadata-literal -d " Store referenced sources as literal data in the metadata output."

# Optimizer Options:
complete -c solc -l optimize -d "Enable bytecode optimizer."
complete -c solc -l optimize-runs -d "The number of runs specifies roughly how often each opcode of the deployed code will be executed across the lifetime of the contract. Lower values will optimize more for initial deployment cost, higher values will optimize more for high-frequency usage."
complete -c solc -l optimize-yul -d "Legacy option, ignored. Use the general --optimize to enable Yul optimizer."
complete -c solc -l no-optimize-yul -d "Disable Yul optimizer in Solidity."
complete -c solc -l yul-optimizations -d "Forces yul optimizer to use the specified sequence of optimization steps instead of the built-in one."

# Model Checker Options:
complete -c solc -l model-checker-contracts -d "Select which contracts should be analyzed using the form <source>:<contract>.Multiple pairs <source>:<contract> can be selected at the same time, separated by a comma and no spaces."
complete -c solc -l model-checker-div-mod-no-slacks -d "Encode division and modulo operations with their precise operators instead of multiplication with slack variables."
complete -c solc -l model-checker-engine -d "Select model checker engine."
complete -c solc -l model-checker-show-unproved -d "Show all unproved targets separately."
complete -c solc -l model-checker-solvers -d "Select model checker solvers."
complete -c solc -l model-checker-targets -d "Select model checker verification targets. Multiple targets can be selected at the same time, separated by a comma and no spaces. By default all targets except underflow and overflow are selected."
complete -c solc -l model-checker-timeout -d "Set model checker timeout per query in milliseconds. The default is a deterministic resource limit. A timeout of 0 means no resource/time restrictions for any query."
