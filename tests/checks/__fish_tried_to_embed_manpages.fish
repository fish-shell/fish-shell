# RUN: %fish %s

set -l value (string join , \
    embed-manpages="$(__fish_tried_to_embed_manpages && echo true || echo false)" \
    "build-system=$(status build-info | string replace -rf 'Build system: (.*)' '$1')"
)
switch $value
    case embed-manpages=true,build-system=Cargo
    case embed-manpages=false,build-system=CMake
    case '*'
        echo Expected embedded man pages to be detected as true iff using Cargo
        echo got: $value
end
