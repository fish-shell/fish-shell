#RUN: %fish %s
# Test fish_typo_completion_distance (#10571): tab completion can correct small typos.
mkdir -p typo-completion-test
cd typo-completion-test
mkdir -p Desktop

# Off by default: a typo of "Desktop" gets no completion.
complete -C'cd Deks'

set fish_typo_completion_distance 1
complete -C'cd Deks'
#CHECK: Desktop/

# An exact prefix still wins over, and isn't reported as, a typo match.
complete -C'cd Desk'
#CHECK: Desktop/

# Too many edits: still no match.
complete -C'cd Xyz'

set fish_typo_completion_distance 0
complete -C'cd Deks'
