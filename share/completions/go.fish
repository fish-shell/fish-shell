# go is a tool for managing Go source code.
# See: https://golang.org

# Completions for go build commands
set -l __go_cmds_w_buildflags build get install run test generate
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -s a -d 'force rebuild'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -s n -d 'print the commands but do not run them'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -s p -r -d 'number parallel builds (default=#cpus)'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o race -d 'enable data race detection'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o msan -d 'enable interoperation with memory sanitizer'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -s v -d 'print packages being built'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o work -d 'print and preserve work directory'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -s x -d 'print the commands'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o asmflags -d 'arguments to pass on each go tool asm invocation'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o buildmode -x -d 'build mode to use' -a 'archive c-archive c-shared default shared exe pie plugin'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o compiler -x -d 'name of compiler to use, as in runtime' -a 'gccgo gc'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o gccgoflags -r -d 'gccgo compiler/linker flags'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o gcflags -r -d 'go compiler flags'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o installsuffix -r -d 'suffix for installation directory'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o ldflags -r -d 'linker flags'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o linkshared -r -d 'link against shared libraries previously created with -buildmode=shared'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o mod -x -d 'module download mode to use' -a 'readonly vendor mod'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o modcacherw -d 'leave newly-created directories in the module cache writable'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o modfile -r -d 'read (and possibly write) an alternate go.mod'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o pkgdir -r -d 'install and load all packages from dir instead of the usual locations'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o tags -r -d 'build tags'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o trimpath -d 'remove all file system paths from the resulting executable'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o toolexec -r -d 'a program to use to invoke toolchain programs'
complete -c go -n "__fish_seen_subcommand_from $__go_cmds_w_buildflags" -o mod -x -d 'module download mode to use' -a 'readonly vendor'

# Completions for go cmds that takes file arguments
complete -c go -n "__fish_seen_subcommand_from build compile fix fmt install test vet" -k -x -a "(
            __fish_complete_suffix .go
    )" --description File

# Completions for go cmds that takes pkg arguments
complete -c go -n "__fish_seen_subcommand_from build doc fix fmt install test vet" -x -a "(
            go list -e -f '{{.ImportPath}}	{{or .Doc \"Go package\"}}' (commandline -ct)... 2>/dev/null
    )" --description Package

# build
complete -c go -n __fish_use_subcommand -a build -d 'compile packages and dependencies'

# clean
complete -c go -n __fish_use_subcommand -a clean -d 'remove object files'
complete -c go -n '__fish_seen_subcommand_from clean' -s i -d "remove the corresponding installed archive or binary"
complete -c go -n '__fish_seen_subcommand_from clean' -s n -d "print the command that would be executed without running"
complete -c go -n '__fish_seen_subcommand_from clean' -s r -d "recursively clean all the dependencies of package named by the import paths"
complete -c go -n '__fish_seen_subcommand_from clean' -s x -d "clean to print remove commands as it executes them"
complete -c go -n '__fish_seen_subcommand_from clean' -o cache -d 'remove the entire go build cache'
complete -c go -n '__fish_seen_subcommand_from clean' -o testcache -d 'expire all test results in the go build cache'
complete -c go -n '__fish_seen_subcommand_from clean' -o modcache -d 'remove the entire module download cache'

# doc
complete -c go -n __fish_use_subcommand -a doc -d 'run godoc on package sources'
complete -c go -n '__fish_seen_subcommand_from doc' -o all -d "show all the documentation for the package"
complete -c go -n '__fish_seen_subcommand_from doc' -s c -d 'respect case when matching symbols'
complete -c go -n '__fish_seen_subcommand_from doc' -o cmd -d 'treat a command (package main) like a regular package'
complete -c go -n '__fish_seen_subcommand_from doc' -o short -d 'one-line representation for each symbol'
complete -c go -n '__fish_seen_subcommand_from doc' -o src -d 'show the full source code for the symbol'
complete -c go -n '__fish_seen_subcommand_from doc' -s u -d 'show documentation for unexported as well as exported symbols'

# env
complete -c go -n __fish_use_subcommand -a env -d 'print Go environment information'
complete -c go -n '__fish_seen_subcommand_from env' -o json -d 'print the environment in JSON format'
complete -c go -n '__fish_seen_subcommand_from env' -s u -r -d 'set NAME to default values'
complete -c go -n '__fish_seen_subcommand_from env' -s w -r -d 'set default values for config variables'

# fix
complete -c go -n __fish_use_subcommand -a fix -d 'run go tool fix on packages'

# fmt
complete -c go -n __fish_use_subcommand -a fmt -d 'run gofmt on package sources'
complete -c go -n '__fish_seen_subcommand_from fmt' -s n -d "prints commands that would be executed"
complete -c go -n '__fish_seen_subcommand_from fmt' -s x -d "prints commands as they are executed"

# get
complete -c go -n __fish_use_subcommand -a get -d 'download and install packages and dependencies'
complete -c go -n '__fish_seen_subcommand_from get' -s d -d "stop after downloading the packages; don't install"
complete -c go -n '__fish_seen_subcommand_from get' -o fix -d "run fix tool on packages before resolving dependencies or building"
complete -c go -n '__fish_seen_subcommand_from get' -s u -d "update remote packages"
complete -c go -n '__fish_seen_subcommand_from get' -s v -d "verbose progress and debug output"
complete -c go -n '__fish_seen_subcommand_from get' -s t -d "also download the packages required to build the tests"
complete -c go -n '__fish_seen_subcommand_from get' -o insecure -d "flag permits fetching & resolving domains using insecure schemes"

# generate
complete -c go -n __fish_use_subcommand -a generate -d 'Generate runs commands described by directives within existing files.'
complete -c go -n '__fish_seen_subcommand_from get' -s n -d "prints commands that would be executed"
complete -c go -n '__fish_seen_subcommand_from get' -s x -d "prints commands as they are executed"
complete -c go -n '__fish_seen_subcommand_from get' -s v -d "prints the names of packages and files as they are processed"
complete -c go -n '__fish_seen_subcommand_from get' -o run -d "prints the names of packages and files as they are processed"

# help
complete -c go -n __fish_use_subcommand -a help -d 'get help on topic'
complete -c go -n '__fish_seen_subcommand_from help' -xa bug -d "start a bug report"
complete -c go -n '__fish_seen_subcommand_from help' -xa build -d "compile packages and dependencies"
complete -c go -n '__fish_seen_subcommand_from help' -xa clean -d "remove object files and cached files"
complete -c go -n '__fish_seen_subcommand_from help' -xa doc -d "show documentation for package or symbol"
complete -c go -n '__fish_seen_subcommand_from help' -xa env -d "print Go environment information"
complete -c go -n '__fish_seen_subcommand_from help' -xa fix -d "update packages to use new APIs"
complete -c go -n '__fish_seen_subcommand_from help' -xa fmt -d "gofmt (reformat) package sources"
complete -c go -n '__fish_seen_subcommand_from help' -xa generate -d "generate Go files by processing source"
complete -c go -n '__fish_seen_subcommand_from help' -xa get -d "download and install packages and dependencies"
complete -c go -n '__fish_seen_subcommand_from help' -xa install -d "compile and install packages and dependencies"
complete -c go -n '__fish_seen_subcommand_from help' -xa list -d "list packages or modules"
complete -c go -n '__fish_seen_subcommand_from help' -xa mod -d "module maintenance"
complete -c go -n '__fish_seen_subcommand_from help' -xa run -d "compile and run Go program"
complete -c go -n '__fish_seen_subcommand_from help' -xa test -d "test packages"
complete -c go -n '__fish_seen_subcommand_from help' -xa tool -d "run specified go tool"
complete -c go -n '__fish_seen_subcommand_from help' -xa version -d "print Go version"
complete -c go -n '__fish_seen_subcommand_from help' -xa vet -d "report likely mistakes in packages"
complete -c go -n '__fish_seen_subcommand_from help' -xa buildmode -d "build modes"
complete -c go -n '__fish_seen_subcommand_from help' -xa c -d "calling between Go and C"
complete -c go -n '__fish_seen_subcommand_from help' -xa cache -d "build and test caching"
complete -c go -n '__fish_seen_subcommand_from help' -xa environment -d "environment variables"
complete -c go -n '__fish_seen_subcommand_from help' -xa filetype -d "file types"
complete -c go -n '__fish_seen_subcommand_from help' -xa go.mod -d "the go.mod file"
complete -c go -n '__fish_seen_subcommand_from help' -xa gopath -d "GOPATH environment variable"
complete -c go -n '__fish_seen_subcommand_from help' -xa gopath-get -d "legacy GOPATH go get"
complete -c go -n '__fish_seen_subcommand_from help' -xa goproxy -d "module proxy protocol"
complete -c go -n '__fish_seen_subcommand_from help' -xa importpath -d "import path syntax"
complete -c go -n '__fish_seen_subcommand_from help' -xa modules -d "modules, module versions, and more"
complete -c go -n '__fish_seen_subcommand_from help' -xa module-get -d "module-aware go get"
complete -c go -n '__fish_seen_subcommand_from help' -xa module-auth -d "module authentication using go.sum"
complete -c go -n '__fish_seen_subcommand_from help' -xa module-private -d "module configuration for non-public modules"
complete -c go -n '__fish_seen_subcommand_from help' -xa packages -d "package lists and patterns"
complete -c go -n '__fish_seen_subcommand_from help' -xa testflag -d "testing flags"
complete -c go -n '__fish_seen_subcommand_from help' -xa testfunc -d "testing functions"

# install
complete -c go -n __fish_use_subcommand -a install -d 'compile and install packages and dependencies'

# list
complete -c go -n __fish_use_subcommand -a list -d 'list packages'
complete -c go -n '__fish_seen_subcommand_from list' -s e -d "tolerate erroneous packages"
complete -c go -n '__fish_seen_subcommand_from list' -s f -r -d "pass in template for formatting"
complete -c go -n '__fish_seen_subcommand_from list' -o json -d "print in JSON format"
complete -c go -n '__fish_seen_subcommand_from list' -o tags -r -d 'list of build tags'
complete -c go -n '__fish_seen_subcommand_from list' -s m -d 'list modules instead of packages'

# run
complete -c go -n __fish_use_subcommand -a run -d 'compile and run Go program'

# test
complete -c go -n __fish_use_subcommand -a test -d 'test packages'
complete -c go -n '__fish_seen_subcommand_from test' -s c -r -d "compile the test binary to pkg.test but do not run it"
complete -c go -n '__fish_seen_subcommand_from test' -s i -d "install dependent packages, but don't run tests"

# tool
complete -c go -n __fish_use_subcommand -a tool -d 'run specified go tool'
complete -c go -n '__fish_seen_subcommand_from tool' -a 'addr2line api asm cgo compile dist fix link nm objdump pack pprof prof vet yacc' -d "target tool"
complete -c go -n '__fish_seen_subcommand_from tool' -s n -d "print the command that would be executed but not execute it"

# version
complete -c go -f -n __fish_use_subcommand -a version -d 'print Go version'
complete -c go -f -n '__fish_seen_subcommand_from version'

# vet
complete -c go -n __fish_use_subcommand -a vet -d 'vet packages'
complete -c go -n '__fish_seen_subcommand_from vet' -s n -d "print the command that would be executed"
complete -c go -n '__fish_seen_subcommand_from vet' -s x -d "prints commands as they are executed"

# mod
complete -c go -n __fish_use_subcommand -a mod -d 'module maintenance'
complete -c go -f -n '__fish_seen_subcommand_from mod' -a download -d "download modules to local cache"
complete -c go -f -n '__fish_seen_subcommand_from mod' -a edit -d "edit go.mod from tools or scripts"
complete -c go -f -n '__fish_seen_subcommand_from mod' -a graph -d "print module requirement graph"
complete -c go -f -n '__fish_seen_subcommand_from mod' -a init -d "initialize new module in current directory"
complete -c go -f -n '__fish_seen_subcommand_from mod' -a tidy -d "add missing and remove unused modules"
complete -c go -f -n '__fish_seen_subcommand_from mod' -a vendor -d "make vendored copy of dependencies"
complete -c go -f -n '__fish_seen_subcommand_from mod' -a verify -d "verify dependencies have expected content"
complete -c go -f -n '__fish_seen_subcommand_from mod' -a why -d "explain why packages or modules are needed"
