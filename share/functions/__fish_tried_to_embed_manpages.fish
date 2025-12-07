# localization: skip(private)
function __fish_tried_to_embed_manpages
    status build-info | string match -rq '(\s)embed-manpages(\s|$)'
end
