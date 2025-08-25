#RUN: %fish %s

set -l repo_root (realpath (status dirname)/../..)
set -l po_dir $repo_root/po

set -l main_cargo_toml $repo_root/Cargo.toml
set -l gettext_maps_cargo_toml $repo_root/crates/gettext-maps/Cargo.toml

for po_file in $po_dir/*
    # Ignore non-PO files
    string match --quiet --regex '\.po$' $po_file || continue

    set -l lang (basename $po_file .po)

    grep -q ^localize-$lang $main_cargo_toml || begin echo "Feature for $lang missing in $main_cargo_toml"; exit 1; end
    grep -q ^$lang $gettext_maps_cargo_toml || begin echo "Feature for $lang missing in $gettext_maps_cargo_toml"; exit 1; end
end
