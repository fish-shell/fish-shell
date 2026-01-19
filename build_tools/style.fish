#!/usr/bin/env fish
#
# This runs Python files, fish scripts (*.fish), and Rust files
# through their respective code formatting programs.
#
# `--all`: Format all eligible files instead of the ones specified as arguments.
# `--check`: Instead of reformatting, fail if a file is not formatted correctly.
# `--force`: Proceed without asking if uncommitted changes are detected.
#            Only relevant if `--all` is specified but `--check` is not specified.

set -l fish_files
set -l python_files
set -l rust_files
set -l all no

argparse all check force -- $argv
or exit $status

if set -l -q _flag_all
    set all yes
    if set -q argv[1]
        echo "Unexpected arguments: '$argv'"
        exit 1
    end
end

set -l workspace_root (realpath (status dirname)/..)

if test $all = yes
    if not set -l -q _flag_force; and not set -l -q _flag_check
        # Potential for false positives: Not all fish files are formatted, see the `fish_files`
        # definition below.
        set -l relevant_uncommitted_changes (git status --porcelain --short --untracked-files=all | sed -e 's/^ *[^ ]* *//' | grep -E '.*\.(fish|py|rs)$')
        if set -q relevant_uncommitted_changes[1]
            for changed_file in $relevant_uncommitted_changes
                echo $changed_file
            end
            echo
            echo 'You have uncommitted changes (listed above). Are you sure you want to restyle?'
            read -P 'y/N? ' -n1 -l ans
            if not string match -qi y -- $ans
                exit 1
            end
        end
    end
    set fish_files $workspace_root/{benchmarks,build_tools,etc,share}/**.fish
    set python_files $workspace_root
else
    # Format the files specified as arguments.
    set -l files $argv
    set fish_files (string match -r '^.*\.fish$' -- $files)
    set python_files (string match -r '^.*\.py$' -- $files)
    set rust_files (string match -r '^.*\.rs$' -- $files)
end

set -l red (set_color red)
set -l green (set_color green)
set -l yellow (set_color yellow)
set -l normal (set_color normal)

function die -V red -V normal
    echo $red$argv[1]$normal
    exit 1
end

if set -q fish_files[1]
    if not type -q fish_indent
        echo
        echo $yellow'Could not find `fish_indent` in `$PATH`.'$normal
        exit 127
    end
    echo === Running "$green"fish_indent"$normal"
    if set -l -q _flag_check
        fish_indent --check -- $fish_files
        or die "Fish files are not formatted correctly."
    else
        fish_indent -w -- $fish_files
    end
end

if set -q python_files[1]
    if not type -q ruff
        echo
        echo $yellow'Please install `ruff` to style python'$normal
        exit 127
    end
    echo === Running "$green"ruff format"$normal"
    if set -l -q _flag_check
        ruff format --check $python_files
        or die "Python files are not formatted correctly."
    else
        ruff format $python_files
    end
end

if test $all = yes; or set -q rust_files[1]
    if not cargo fmt --version >/dev/null
        echo
        echo $yellow'Please install "rustfmt" to style Rust, e.g. via:'
        echo "rustup component add rustfmt"$normal
        exit 127
    end

    set -l edition_spec string match -r '^edition\s*=.*'
    test "$($edition_spec <Cargo.toml)" = "$($edition_spec <.rustfmt.toml)"
    or die "Cargo.toml and .rustfmt.toml use different editions"

    echo === Running "$green"rustfmt"$normal"
    if set -l -q _flag_check
        if test $all = yes
            cargo fmt --all --check
        else
            rustfmt --check --files-with-diff $rust_files
        end
        or die "Rust files are not formatted correctly."
    else
        if test $all = yes
            cargo fmt --all
        else
            rustfmt $rust_files
        end
    end
end
