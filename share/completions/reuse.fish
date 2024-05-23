# Completions for `reuse` (https://reuse.software/)
# Based on version 3.0.2

set -l comment_styles \
    applescript \
    aspx \
    bat \
    bibtex \
    c \
    csingle \
    css \
    f \
    ftl \
    handlebars \
    haskell \
    html \
    jinja \
    julia \
    lisp \
    m4 \
    ml \
    f90 \
    plantuml \
    python \
    rst \
    semicolon \
    tex \
    vst \
    vim \
    xquery
set -l copyright_styles \
    spdx \
    spdx-c \
    spdx-symbol \
    string \
    string-c \
    string-symbol \
    symbol

complete -c reuse -s h -l help -d "show help message"

complete -c reuse -n __fish_no_arguments -l debug -d "enable debug logging"
complete -c reuse -n __fish_no_arguments -l suppress-deprecation -d "hide deprecation warnings"
complete -c reuse -n __fish_no_arguments -l include-submodules -d "do not ignore Git submodules"
complete -c reuse -n __fish_no_arguments -l include-meson-subprojects -d "do not ignore Meson subprojects"
complete -c reuse -n __fish_no_arguments -l no-multiprocessing -d "disable multiprocessing"
complete -r -c reuse -n __fish_no_arguments -l root -d "set root of project"
complete -c reuse -n __fish_no_arguments -l version -d "show version number"

complete -x -c reuse -n __fish_use_subcommand -a annotate -d "add REUSE information to files"
complete -x -c reuse -n __fish_use_subcommand -a download -d "download license files"
complete -x -c reuse -n __fish_use_subcommand -a init -d "initialize REUSE project"
complete -x -c reuse -n __fish_use_subcommand -a lint -d "list all non-compliant files"
complete -x -c reuse -n __fish_use_subcommand -a spdx -d "generate SPDX bill of materials"
complete -x -c reuse -n __fish_use_subcommand -a supported-licenses -d "list all supported licenses"

complete -x -c reuse -n "__fish_seen_subcommand_from annotate" -s c -l copyright -d "copyright statement"
complete -x -c reuse -n "__fish_seen_subcommand_from annotate" -s l -l license -d "SPDX Identifier"
complete -x -c reuse -n "__fish_seen_subcommand_from annotate" -l contributor -d "file contributor"
complete -x -c reuse -n "__fish_seen_subcommand_from annotate" -s y -l year -d "year of copyright statement"
complete -x -c reuse -n "__fish_seen_subcommand_from annotate" -s s -l style -a "$comment_styles" -d "comment style to use"
complete -x -c reuse -n "__fish_seen_subcommand_from annotate" -l copyright-style -a "$copyright_styles" -d "copyright style to use"
complete -x -c reuse -n "__fish_seen_subcommand_from annotate" -s t -l template -d "name of template to use"
complete -c reuse -n "__fish_seen_subcommand_from annotate" -l exclude-year -d "do not include year in statement"
complete -c reuse -n "__fish_seen_subcommand_from annotate" -l merge-copyrights -d "merge copyright lines if copyright statements are identical"
complete -c reuse -n "__fish_seen_subcommand_from annotate" -l single-line -d "force single-line comment style"
complete -c reuse -n "__fish_seen_subcommand_from annotate" -l multi-line -d "force multi-line comment style"
complete -c reuse -n "__fish_seen_subcommand_from annotate" -s r -l recursive -d "annotate all files recursively"
complete -c reuse -n "__fish_seen_subcommand_from annotate" -l no-replace -d "do not replace the first header in the file"
complete -c reuse -n "__fish_seen_subcommand_from annotate" -l force-dot-license -d "always write a .license file"
complete -c reuse -n "__fish_seen_subcommand_from annotate" -l fallback-dot-license -d "write a .license file to files with unrecognised comment styles"
complete -c reuse -n "__fish_seen_subcommand_from annotate" -l skip-unrecognised -d "skip files with unrecognised comment styles"
complete -c reuse -n "__fish_seen_subcommand_from annotate" -l skip-existing -d "skip files that already contain REUSE information"

complete -c reuse -n "__fish_seen_subcommand_from download" -l all -d "download all missing licenses"
complete -r -c reuse -n "__fish_seen_subcommand_from download" -s o -l output -d "output license to specified file"
complete -x -c reuse -n "__fish_seen_subcommand_from download" -l source -d "source from which to copy custom `LicenseRef-` licenses"

complete -c reuse -n "__fish_seen_subcommand_from lint" -s q -l quiet -d "prevents output"
complete -c reuse -n "__fish_seen_subcommand_from lint" -s j -l json -d "formats output as JSON"
complete -c reuse -n "__fish_seen_subcommand_from lint" -s p -l plain -d "formats output as plain text"

complete -r -c reuse -n "__fish_seen_subcommand_from spdx" -s o -l output -d "write the bill of materials to specified file"
complete -c reuse -n "__fish_seen_subcommand_from spdx" -l add-license-concluded -d "populate the LicenseConcluded field"
complete -x -c reuse -n "__fish_seen_subcommand_from spdx" -l creator-person -d "name of the person signing off on the SPDX report"
complete -x -c reuse -n "__fish_seen_subcommand_from spdx" -l creator-organization -d "name of the organization signing off on the SPDX report"
