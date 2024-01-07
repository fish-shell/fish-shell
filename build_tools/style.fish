#!/usr/bin/env fish
#
# This runs C++ files and fish scripts (*.fish) through their respective code
# formatting programs.
#
set -l fish_files
set -l python_files
set -l rust_files
set -l all no

if test "$argv[1]" = --all
    set all yes
    set -e argv[1]
end

if set -q argv[1]
    echo "Unexpected arguments: '$argv'"
    exit 1
end

if test $all = yes
    set -l files (git status --porcelain --short --untracked-files=all | sed -e 's/^ *[^ ]* *//')
    if set -q files[1]
        echo
        echo 'You have uncommitted changes. Are you sure you want to restyle?'
        read -P 'y/N? ' -n1 -l ans
        if not string match -qi y -- $ans
            exit 1
        end
    end
    set fish_files share/**.fish
    set python_files {doc_src,share,tests}/**.py
    set rust_files fish-rust/src/**.rs
else
    # Extract just the fish files.
    set fish_files (string match -r '^.*\.fish$' -- $files)
    set python_files (string match -r '^.*\.py$' -- $files)
    set rust_files (string match -r '^.*\.rs$' -- $files)
end

set -l red (set_color red)
set -l green (set_color green)
set -l blue (set_color blue)
set -l normal (set_color normal)

# Run the fish reformatter if we have any fish files.
if set -q fish_files[1]
    if not type -q fish_indent
        make fish_indent
        set PATH . $PATH
    end
    echo === Running "$green"fish_indent"$normal"
    fish_indent -w -- $fish_files
end

if set -q python_files[1]
    if not type -q black
        echo
        echo Please install "`black`" to style python
        echo
    else
        echo === Running "$blue"black"$normal"
        black $python_files
    end
end

if set -q rust_files[1]
    if not type -q rustfmt
        echo
        echo Please install "`rustfmt`" to style rust
        echo
    else
        echo === Running "$blue"rustfmt"$normal"
        rustfmt $rust_files
    end
end
