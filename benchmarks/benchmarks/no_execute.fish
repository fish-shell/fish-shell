set -l path (status dirname)
set -l fish (status fish-path)
for f in (seq 100)
    echo $fish -n $path/aliases.fish
    $fish -n $path/aliases.fish
end
