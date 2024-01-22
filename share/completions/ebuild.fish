function __fish_seen_ebuild_arg -d "Test if an ebuild-argument has been given in the current commandline"
    commandline -xpc | string match -q '*.ebuild'
end

## Opts
complete -c ebuild -l debug -d "Run bash with the -x option"
complete -c ebuild -l color -d "Enable color" \
    -xa "y n"
complete -c ebuild -l force -d "Force regeneration of digests"
complete -c ebuild -l ignore-default-opts -d "Ignore EBUILD_DEFAULT_OPTS"
complete -c ebuild -l skip-manifest -d "Skip all manifest checks"

## Subcommands
complete -c ebuild -n __fish_seen_ebuild_arg -xa help -d "Show help"
complete -c ebuild -n __fish_seen_ebuild_arg -xa pretend -d "Run pkg_pretend()"
complete -c ebuild -n __fish_seen_ebuild_arg -xa setup -d "Run setup and system checks"
complete -c ebuild -n __fish_seen_ebuild_arg -xa clean -d "Clean build dir"
complete -c ebuild -n __fish_seen_ebuild_arg -xa fetch -d "Fetches all files from SRC_URI"
#complete -c ebuild -n '__fish_seen_ebuild_arg' -xa 'digest'      -d "Deprecared: see manifest"
complete -c ebuild -n __fish_seen_ebuild_arg -xa manifest -d "Update pkg manifest"
complete -c ebuild -n __fish_seen_ebuild_arg -xa unpack -d "Extracts sources"
complete -c ebuild -n __fish_seen_ebuild_arg -xa prepare -d "Run src_prepare()"
complete -c ebuild -n __fish_seen_ebuild_arg -xa configure -d "Run src_configure()"
complete -c ebuild -n __fish_seen_ebuild_arg -xa compile -d "Run src_compile()"
complete -c ebuild -n __fish_seen_ebuild_arg -xa test -d "Run tests"
complete -c ebuild -n __fish_seen_ebuild_arg -xa preinst -d "Run pkg_preinst()"
complete -c ebuild -n __fish_seen_ebuild_arg -xa install -d "Run src_install()"
complete -c ebuild -n __fish_seen_ebuild_arg -xa postinst -d "Run pkg_postinst()"
complete -c ebuild -n __fish_seen_ebuild_arg -xa qmerge -d "Install files to live filesystem"
complete -c ebuild -n __fish_seen_ebuild_arg -xa merge -d "Run fetch, unpack, compile, install and qmerge"
complete -c ebuild -n __fish_seen_ebuild_arg -xa unmerge -d "Uninstall files from live filesystem"
complete -c ebuild -n __fish_seen_ebuild_arg -xa prerm -d "Run pkg_prerm()"
complete -c ebuild -n __fish_seen_ebuild_arg -xa postrm -d "Run pkg_postrm()"
complete -c ebuild -n __fish_seen_ebuild_arg -xa config -d "Run post-install configuration"
complete -c ebuild -n __fish_seen_ebuild_arg -xa package -d "Create a binpkg in PKGDIR"
complete -c ebuild -n __fish_seen_ebuild_arg -xa rpm -d "Builds a RedHat RPM pkg"
