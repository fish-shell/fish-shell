#!/usr/bin/env fish

# Copyright (C) 2012-2014 Dmitry Medvinsky <me@dmedvinsky.name>. All Rights Reserved.
# This file is licensed under the GPLv2+. Please see COPYING for more information.

set PROG 'pass'

function __fish_pass_get_prefix
    set -l prefix "$PASSWORD_STORE_DIR"
    if [ -z "$prefix" ]
        set prefix "$HOME/.password-store"
    end
    echo "$prefix"
end

function __fish_pass_needs_command
    set -l cmd (commandline -opc)
    if [ (count $cmd) -eq 1 -a $cmd[1] = $PROG ]
        return 0
    end
    return 1
end

function __fish_pass_uses_command
    set cmd (commandline -opc)
    if [ (count $cmd) -gt 1 ]
        if [ $argv[1] = $cmd[2] ]
            return 0
        end
    end
    return 1
end

function __fish_pass_print_gpg_keys
    gpg2 --list-keys | grep uid | sed 's/.*<\(.*\)>/\1/'
end

function __fish_pass_print_entry_dirs
    set -l prefix (__fish_pass_get_prefix)
    set -l dirs
    eval "set dirs "$prefix"/**/"
    for dir in $dirs
        set entry (echo "$dir" | sed "s#$prefix/\(.*\)#\1#")
        echo "$entry"
    end
end

function __fish_pass_print_entries
    set -l prefix (__fish_pass_get_prefix)
    set -l files
    eval "set files "$prefix"/**.gpg"
    for file in $files
        set file (echo "$file" | sed "s#$prefix/\(.*\)\.gpg#\1#")
        echo "$file"
    end
end

function __fish_pass_print_entries_and_dirs
    __fish_pass_print_entry_dirs
    __fish_pass_print_entries
end

