#RUN: %fish %s
#REQUIRES: command -v tmux
#REQUIRES: uname -r | grep -qv Microsoft

# Start a clean tmux session running fish and create predictable completion sets.
# Disable autosuggestions to avoid races and keep captures stable.
isolated-tmux-start -C '
    set -g fish_autosuggestion_enabled 0
    set -g fish_pager_color_prefix --underline
    set -g fish_pager_color_completion normal
    set -g fish_pager_color_description normal
    rm -rf test_autoshow

    # Dataset for toggle-on/off behavior.
    mkdir -p test_autoshow/dirA test_autoshow/dirB
    for i in (seq 1 160)
        set n (printf "%03d" $i)
        touch test_autoshow/file$n
    end

    # Dataset for backspace/update behavior.
    # Typing ".../ap" should show ap*; backspacing to ".../a" should expand to include aonly*.
    mkdir -p test_autoshow/backspace
    touch test_autoshow/backspace/aonly001 test_autoshow/backspace/aonly002 test_autoshow/backspace/aonly003
    for i in (seq 1 40)
        set n (printf "%03d" $i)
        touch test_autoshow/backspace/ap$n
    end

    # Dataset for "stable filepaths while typing" and "completed directory token shows its contents".
    mkdir -p test_autoshow/stable
    touch test_autoshow/stable/test1.txt
    touch test_autoshow/stable/test2.txt
    touch test_autoshow/stable/test2.md
    for i in (seq 1 40)
        set n (printf "%03d" $i)
        touch test_autoshow/stable/test2suffix$n
    end
    mkdir -p test_autoshow/history_case/context
    touch test_autoshow/history_case/contrib

    # Dataset for "tab completion ambiguous list" behavior.
    mkdir -p test_autoshow/tab_ambig/collection
    touch test_autoshow/tab_ambig/collection-plan.docx
    mkdir -p test_autoshow/prefixcase/Bdir test_autoshow/prefixcase/bdir


    # Dataset for "subcommand parser" behavior.
    # Use a completion generator that relies on command substitution.
    function autoshowcmd; end
    function __autoshowcmd_subcmds
        printf "add\ncommit\n"
        for i in (seq 1 40)
            printf "dummy_subcmd%03d\n" $i
        end
    end
    complete -c autoshowcmd -f -n "__fish_use_subcommand" -a "(__autoshowcmd_subcmds)"

    # Dataset for "history builtin from autoshow completion command substitution" behavior.
    function autoshowhistcmd; end
    function __autoshowhistcmd_args
        # Directly exercise the history builtin from completion command substitution.
        # The marker is validated later from the interactive shell.
        builtin history append autoshowhist_probe_marker_001 >/dev/null 2>/dev/null; or true
        for i in (seq 1 20)
            printf "histprobe%03d\n" $i
        end
    end
    complete -c autoshowhistcmd -f -a "(__autoshowhistcmd_args)"

    # Seed history so the blocklist-clearing test can take a history-fast path if applicable.
    # (We do not assert autosuggestion text; we only need this to make the codepath possible.)
    history append "blockedcmd test_autoshow/dirA/"
    # Seed history so autoshow can take the whole-history path while still showing completions.
    history append "cat test_autoshow/history_case/context/"
'

tmux-sleep
isolated-tmux send-keys C-l
tmux-sleep

# Helpers (run in the outer test process).
function __pane_tokens
    isolated-tmux capture-pane -p | tr -s " \t" "\n"
end

function __pane_has_token --argument token
    __pane_tokens | grep -Fqx -- $token
end

function __pane_print_token --argument token
    __pane_tokens | grep -Fx -- $token | head -1
end

function __pane_has_escape_pattern --argument pattern
    isolated-tmux capture-pane -ep | string match -rq -- $pattern
end

function __pane_print_first_file
    __pane_tokens | grep -E '^file[0-9]{3}$' | head -1
end

# Test 1, Part 1: Enable autoshow and verify it renders completion candidates
isolated-tmux send-keys C-c
isolated-tmux send-keys 'set -g fish_autocomplete_autoshow 1' Enter
tmux-sleep
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'cat test_autoshow/'
tmux-sleep
sleep-until '__pane_has_token dirA/'

isolated-tmux capture-pane -p | grep -E '^prompt [0-9]+> cat test_autoshow/' | head -1
# CHECK: prompt {{\d+}}> cat test_autoshow/

__pane_print_token dirA/
# CHECK: dirA/
__pane_print_token dirB/
# CHECK: dirB/

__pane_print_first_file
# CHECK: file{{\d\d\d}}

# Test 1, Part 2: Disable autoshow and verify candidates do NOT appear (without CHECK-NOT)
isolated-tmux send-keys C-c
isolated-tmux send-keys 'set -g fish_autocomplete_autoshow 0' Enter
tmux-sleep
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'cat test_autoshow/'
tmux-sleep
sleep-until 'isolated-tmux capture-pane -p | grep -Fq "cat test_autoshow/"'

if __pane_has_token dirA/
    echo 'autoshow-off: FAIL (dirA present)'
else
    echo 'autoshow-off: OK'
end
# CHECK: autoshow-off: OK

# Test 1, Part 3: Re-enable autoshow and confirm it renders again
isolated-tmux send-keys C-c
isolated-tmux send-keys 'set -g fish_autocomplete_autoshow 1' Enter
tmux-sleep
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'cat test_autoshow/'
tmux-sleep
sleep-until '__pane_has_token dirB/'

isolated-tmux capture-pane -p | grep -E '^prompt [0-9]+> cat test_autoshow/' | head -1
# CHECK: prompt {{\d+}}> cat test_autoshow/

__pane_print_token dirA/
# CHECK: dirA/
__pane_print_token dirB/
# CHECK: dirB/

__pane_print_first_file
# CHECK: file{{\d\d\d}}

# Test 2: Backspacing during autoshow updates the shown candidates
# Start with a narrower prefix (ap) then backspace to a broader prefix (a) and
# verify the newly-eligible candidates (aonly*) appear
isolated-tmux send-keys C-c
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'cat test_autoshow/backspace/ap'
tmux-sleep
sleep-until '__pane_has_token ap001'

__pane_print_token ap001
# CHECK: ap001

# Backspace deletes the 'p' -> now completing for ".../a"
isolated-tmux send-keys BSpace
tmux-sleep
sleep-until '__pane_has_token aonly001'

__pane_print_token aonly001
# CHECK: aonly001

# Test 3: Showing stable filepaths while typing (typed text + suggested suffix)
# The candidate must remain "test2.txt" as the user types more of the prefix.
isolated-tmux send-keys C-c
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'cat test_autoshow/stable/'
tmux-sleep
sleep-until '__pane_has_token test2.txt'

isolated-tmux capture-pane -p | grep -E '^prompt [0-9]+> cat test_autoshow/stable/' | head -1
# CHECK: prompt {{\d+}}> cat test_autoshow/stable/

__pane_print_token test2.txt
# CHECK: test2.txt

# Type a prefix that still matches test2.txt.
isolated-tmux send-keys 'te'
tmux-sleep
sleep-until '__pane_has_token test2.txt'

isolated-tmux capture-pane -p | grep -E '^prompt [0-9]+> cat test_autoshow/stable/te' | head -1
# CHECK: prompt {{\d+}}> cat test_autoshow/stable/te

__pane_print_token test2.txt
# CHECK: test2.txt

# Type more characters; the displayed candidate should still be whole.
isolated-tmux send-keys 'st2'
tmux-sleep
sleep-until '__pane_has_token test2.txt'

isolated-tmux capture-pane -p | grep -E '^prompt [0-9]+> cat test_autoshow/stable/test2' | head -1
# CHECK: prompt {{\d+}}> cat test_autoshow/stable/test2

__pane_print_token test2.txt
# CHECK: test2.txt

# Test 4: Tab completion ambiguous list owns the pager (autoshow must not overwrite it)
isolated-tmux send-keys C-c
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

# Enter a directory where a directory and file share a prefix.
isolated-tmux send-keys 'cd test_autoshow/tab_ambig' Enter
tmux-sleep
isolated-tmux send-keys C-l
tmux-sleep

# Press Tab on an ambiguous prefix. Fish inserts the directory completion but keeps the full list visible.
isolated-tmux send-keys 'ls colle' Tab
tmux-sleep
sleep-until 'isolated-tmux capture-pane -p | grep -Eq "^prompt [0-9]+> ls collection/?"'
sleep-until '__pane_has_token collection/'
sleep-until '__pane_has_token collection-plan.docx'

isolated-tmux capture-pane -p | grep -E '^prompt [0-9]+> ls collection' | head -1
# CHECK: prompt {{\d+}}> ls collection

__pane_print_token collection/
# CHECK: collection/
__pane_print_token collection-plan.docx
# CHECK: collection-plan.docx

# Return to test root.
isolated-tmux send-keys C-c
isolated-tmux send-keys 'cd ../..' Enter
tmux-sleep


# Test 5 (Missing Test #8): Completing a directory token causes autoshow to list that directory's contents
isolated-tmux send-keys C-c
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'cat test_autoshow/sta' Tab
tmux-sleep

sleep-until 'isolated-tmux capture-pane -p | grep -Fq "cat test_autoshow/stable/"'
sleep-until '__pane_has_token test1.txt'
sleep-until '__pane_has_token test2.txt'

isolated-tmux capture-pane -p | grep -E '^prompt [0-9]+> cat test_autoshow/stable/' | head -1
# CHECK: prompt {{\d+}}> cat test_autoshow/stable/

__pane_print_token test1.txt
# CHECK: test1.txt
__pane_print_token test2.txt
# CHECK: test2.txt
__pane_print_token test2.md
# CHECK: test2.md

# Test 6: Autoshow applies fish_pager_color_prefix to typed file path prefixes (case-insensitive)
isolated-tmux send-keys C-c
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'cd test_autoshow/prefixcase' Enter
tmux-sleep
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'cd b'
tmux-sleep
sleep-until '__pane_has_token Bdir/'
sleep-until '__pane_has_token bdir/'

set -l esc (printf '\e')
set -l underline_pat "$esc\\[(?:[0-9]*;)*4(?:;[0-9]*)*m"
set -l sgr_any "$esc\\[[0-9;]*m"
if __pane_has_escape_pattern "$underline_pat""B(?:$sgr_any)*dir/"
    echo 'autoshow-prefix-highlight-upper: OK'
else
    echo 'autoshow-prefix-highlight-upper: FAIL'
end
# CHECK: autoshow-prefix-highlight-upper: OK
if __pane_has_escape_pattern "$underline_pat""b(?:$sgr_any)*dir/"
    echo 'autoshow-prefix-highlight-lower: OK'
else
    echo 'autoshow-prefix-highlight-lower: FAIL'
end
# CHECK: autoshow-prefix-highlight-lower: OK

# Return to test root.
isolated-tmux send-keys C-c
isolated-tmux send-keys 'cd ../..' Enter
tmux-sleep

# Test 7: Fuzzy interior matches display full items without prefixing typed characters
isolated-tmux send-keys C-c
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'autoshowcmd om'
tmux-sleep
tmux-sleep
sleep-until '__pane_has_token commit'

if not __pane_has_token commit
    echo 'autoshow-fuzzy-display-full-items: FAIL (missing commit)'
else if __pane_has_token omcommit
    echo 'autoshow-fuzzy-display-full-items: FAIL (prefixed omcommit)'
else
    echo 'autoshow-fuzzy-display-full-items: OK'
end
# CHECK: autoshow-fuzzy-display-full-items: OK

# Test 8: Whole-history autoshow path preserves full display candidates
isolated-tmux send-keys C-c
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'cat test_autoshow/history_case/co'
tmux-sleep
tmux-sleep
sleep-until 'isolated-tmux capture-pane -p | grep -Eq "^prompt [0-9]+> cat test_autoshow/history_case/co"'

if not __pane_has_token context/
    echo 'autoshow-history-display-full-items: FAIL (missing context/)'
else if not __pane_has_token contrib
    echo 'autoshow-history-display-full-items: FAIL (missing contrib)'
else if __pane_has_token ntext/
    echo 'autoshow-history-display-full-items: FAIL (suffix-only ntext/)'
else if __pane_has_token ntrib
    echo 'autoshow-history-display-full-items: FAIL (suffix-only ntrib)'
else
    echo 'autoshow-history-display-full-items: OK'
end
# CHECK: autoshow-history-display-full-items: OK

# Test 9 (Missing Test #6): Autoshow parser for subcommands renders command-substitution subcommand completions
# The completion list for the subcommand position should include both literal and generated candidates.
isolated-tmux send-keys C-c
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'autoshowcmd '
tmux-sleep
sleep-until '__pane_has_token add'
sleep-until '__pane_has_token commit'
sleep-until '__pane_has_token dummy_subcmd001'

# Wait until the commandline is visible in the captured pane.
sleep-until 'isolated-tmux capture-pane -p | grep -Eq "^prompt [0-9]+> autoshowcmd"'

isolated-tmux capture-pane -p | grep -E '^prompt [0-9]+> autoshowcmd' | head -1
# CHECK: prompt {{\d+}}> autoshowcmd{{.*}}

__pane_print_token add
# CHECK: add
__pane_print_token commit
# CHECK: commit
__pane_print_token dummy_subcmd001
# CHECK: dummy_subcmd001

# Test 10: Autoshow Tab navigation keeps the capped menu stable
#
# autoshowcmd has 42 subcommand completions. Autoshow is capped at 40 items, so the initial menu
# should omit the tail entries. Tab should begin navigating that already-maximized autoshow pager
# without expanding it on the first or second press.
isolated-tmux send-keys C-c
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'autoshowcmd '
tmux-sleep
sleep-until '__pane_has_token add'
sleep-until '__pane_has_token dummy_subcmd001'

set -l autoshow_menu_before (__pane_tokens | grep -E '^(add|commit|dummy_subcmd[0-9]{3})$' | sort -u | string join ' ')

isolated-tmux send-keys Tab
tmux-sleep
sleep-until 'isolated-tmux capture-pane -p | grep -Eq "^prompt [0-9]+> autoshowcmd add ?\$"'

set -l autoshow_menu_after (__pane_tokens | grep -E '^(add|commit|dummy_subcmd[0-9]{3})$' | sort -u | string join ' ')

if test "$autoshow_menu_before" = "$autoshow_menu_after"
    echo 'autoshow-first-tab-keeps-menu: OK'
else
    echo 'autoshow-first-tab-keeps-menu: FAIL (first Tab changed visible menu)'
end
# CHECK: autoshow-first-tab-keeps-menu: OK

isolated-tmux send-keys Tab
tmux-sleep

set -l autoshow_menu_after_second_tab (__pane_tokens | grep -E '^(add|commit|dummy_subcmd[0-9]{3})$' | sort -u | string join ' ')

if test "$autoshow_menu_before" = "$autoshow_menu_after_second_tab"
    echo 'autoshow-second-tab-keeps-menu: OK'
else
    echo 'autoshow-second-tab-keeps-menu: FAIL (second Tab changed visible menu)'
end
# CHECK: autoshow-second-tab-keeps-menu: OK

# Test 11: Blocklist clears an already-visible autoshow pager (stale candidates must disappear)
#
# This is specifically meant to catch the case where autoshow stops producing updates (e.g. returns early
# via history) but the pager is not explicitly cleared and stale candidates remain on screen.
#
# Approach:
#  - Show autoshow candidates for a normal commandline ("cat test_autoshow/"), ensuring the pager is visible.
#  - With the same screen content (no C-l), edit ONLY the command token to a blocklisted command ("blockedcmd"),
#    keeping the rest of the line intact.
#  - Verify that a token that was visible only because of the pager (dirA/) is no longer present on the screen.
isolated-tmux send-keys C-c
isolated-tmux send-keys 'set -g fish_autoshow_blocklist blockedcmd' Enter
tmux-sleep
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

# First show autoshow candidates (pager visible).
isolated-tmux send-keys 'cat test_autoshow/'
tmux-sleep
sleep-until '__pane_has_token dirA/'

# Now, without clearing the screen, change "cat" -> "blockedcmd" in-place.
# We do this by deleting the three characters "cat" at the start, then inserting "blockedcmd".
isolated-tmux send-keys C-a DC DC DC blockedcmd
tmux-sleep

# Ensure the edited commandline is visible.
sleep-until 'isolated-tmux capture-pane -p | grep -Eq "^prompt [0-9]+> blockedcmd test_autoshow/"'

# If autoshow correctly clears on blocklist, the old pager tokens should disappear from the visible pane.
if __pane_has_token dirA/
    echo 'autoshow-blocklist-clears: FAIL (stale pager token present)'
else
    echo 'autoshow-blocklist-clears: OK'
end
# CHECK: autoshow-blocklist-clears: OK

# Test 12: History builtin in autoshow completion command substitution is threadsafe
# Trigger autoshow on a command with command-substitution completions that call `builtin history`,
# then verify the marker was appended and fish remains responsive.
isolated-tmux send-keys C-c
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'autoshowhistcmd '
tmux-sleep
tmux-sleep

isolated-tmux send-keys C-c
isolated-tmux send-keys 'history search --exact autoshowhist_probe_marker_001 >/dev/null; and echo autoshow-history-probe-hit; or echo autoshow-history-probe-miss' Enter
tmux-sleep
sleep-until '__pane_has_token autoshow-history-probe-hit'

__pane_print_token autoshow-history-probe-hit
# CHECK: autoshow-history-probe-hit

# Test 13: Editing an existing command to prepend sudo does not crash fish
isolated-tmux send-keys C-c
isolated-tmux send-keys C-u
isolated-tmux send-keys C-l
tmux-sleep

isolated-tmux send-keys 'cp test_autoshow/file001 .'
tmux-sleep
sleep-until 'isolated-tmux capture-pane -p | grep -Fq "cp test_autoshow/file001 ."'

# Jump to start of line and prepend sudo, matching the reported crash pattern.
isolated-tmux send-keys C-a 'sudo '
tmux-sleep

# Confirm shell remains alive and responsive.
isolated-tmux send-keys C-c
isolated-tmux send-keys 'echo autoshow-sudo-edit-no-crash' Enter
tmux-sleep
sleep-until '__pane_has_token autoshow-sudo-edit-no-crash'

__pane_print_token autoshow-sudo-edit-no-crash
# CHECK: autoshow-sudo-edit-no-crash
