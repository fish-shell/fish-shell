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

set -l repo_root (status dirname)/..

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
    set fish_files $repo_root/{benchmarks,build_tools,etc,share}/**.fish
    set python_files {doc_src,share,tests}/**.py
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

if set -q fish_files[1]
    if not type -q fish_indent
        echo
        echo $yellow'Could not find `fish_indent` in `$PATH`.'$normal
        exit 127
    end
    echo === Running "$green"fish_indent"$normal"
    if set -l -q _flag_check
        if not fish_indent --check -- $fish_files
            echo $red"Fish files are not formatted correctly."$normal
            exit 1
        end
    else
        fish_indent -w -- $fish_files
    end
end

if set -q python_files[1]
    if not type -q black
        echo
        echo $yellow'Please install `black` to style python'$normal
        exit 127
    end
    echo === Running "$green"black"$normal"
    if set -l -q _flag_check
        if not black --check $python_files
            echo $red"Python files are not formatted correctly."$normal
            exit 1
        end
    else
        black $python_files
    end
end

if not cargo fmt --version >/dev/null
    echo
    echo $yellow'Please install "rustfmt" to style Rust, e.g. via:'
    echo "rustup component add rustfmt"$normal
    exit 127
end
echo === Running "$green"rustfmt"$normal"
if set -l -q _flag_check
    if set -l -q _flag_all
        if not cargo fmt --check
            echo $red"Rust files are not formatted correctly."$normal
            exit 1
        end
    else
        if set -q rust_files[1]
            if not rustfmt --check --files-with-diff $rust_files
                echo $red"Rust files are not formatted correctly."
                exit 1
            end
        end
    end
else
    if set -l -q _flag_all
        cargo fmt
    else
        if set -q rust_files[1]
            rustfmt $rust_files
        end
    end
end
