# Environment containing all dependencies needed for
# - building fish,
# - building documentation,
# - running all tests,
# - formatting and checking lints.
#
# enter interactive bash shell:
#   nix-shell contrib/shell.nix
#
# using system nixpkgs (otherwise fetches pinned version):
#   nix-shell contrib/shell.nix --arg pkgs 'import <nixpkgs> {}'
#
# run single command:
#   nix-shell contrib/shell --run "cargo xtask check"
{ pkgs ? (import (builtins.fetchTarball {
  url = "https://github.com/NixOS/nixpkgs/archive/nixos-25.11.tar.gz";
  sha256 = "1ia5kjykm9xmrpwbzhbaf4cpwi3yaxr7shl6amj8dajvgbyh2yh4";
}) { }), ... }:
pkgs.mkShell {
  buildInputs = [
    (pkgs.python3.withPackages (pyPkgs: [ pyPkgs.pexpect ]))
    pkgs.cargo
    pkgs.clippy
    pkgs.cmake
    pkgs.gettext
    pkgs.pcre2
    pkgs.procps # tests use pgrep/pkill
    pkgs.ruff
    pkgs.rustc
    pkgs.rustfmt
    pkgs.sphinx
  ];
}
