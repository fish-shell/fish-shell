term_256_colors =
    [
        // 247
        'ffd7d7', 'd7afaf', 'af8787', '875f5f', '5f0000', '870000', 'af0000', 'd70000', 'ff0000',
        'ff5f5f', 'd75f5f', 'd78787', 'ff8787', 'ffafaf', 'ffaf87', 'ffaf5f', 'ffaf00', 'ff875f',
        'ff8700', 'ff5f00', 'd75f00', 'af5f5f', 'af5f00', 'd78700', 'd7875f', 'af875f', 'af8700',
        '875f00', 'd7af87', 'ffd7af', 'ffd787', 'ffd75f', 'd7af00', 'd7af5f', 'ffd700', 'ffff5f',
        'ffff00', 'ffff87', 'ffffaf', 'ffffd7', 'd7ff00', 'afd75f', 'd7d700', 'd7d787', 'd7d7af',
        'afaf87', '87875f', '5f5f00', '878700', 'afaf00', 'afaf5f', 'd7d75f', 'd7ff5f', 'd7ff87',
        '87ff00', 'afff00', 'afff5f', 'afd700', '87d700', '87af00', '5f8700', '87af5f', '5faf00',
        'afd787', 'd7ffd7', 'd7ffaf', 'afffaf', 'afff87', '5fff00', '5fd700', '87d75f', '5fd75f',
        '87ff5f', '5fff5f', '87ff87', 'afd7af', '87d787', '87d7af', '87af87', '5f875f', '5faf5f',
        '005f00', '008700', '00af00', '00d700', '00ff00', '00ff5f', '5fff87', '00ff87', '87ffaf',
        'afffd7', '5fd787', '00d75f', '5faf87', '00af5f', '5fffaf', '00ffaf', '5fd7af', '00d787',
        '00875f', '00af87', '00d7af', '5fffd7', '87ffd7', '00ffd7', 'd7ffff', 'afd7d7', '87afaf',
        '5f8787', '5fafaf', '87d7d7', '5fd7d7', '5fffff', '00ffff', '87ffff', 'afffff', '00d7d7',
        '00d7ff', '5fd7ff', '5fafd7', '00afd7', '00afff', '0087af', '00afaf', '008787', '005f5f',
        '005f87', '0087d7', '0087ff', '5fafff', '87afff', '5f87d7', '5f87ff', '005fd7', '005fff',
        '005faf', '5f87af', '87afd7', 'afd7ff', '87d7ff', 'd7d7ff', 'afafd7', '8787af', 'afafff',
        '8787d7', '8787ff', '5f5fff', '5f5fd7', '5f5faf', '5f5f87', '00005f', '000087', '0000af',
        '0000d7', '0000ff', '5f00ff', '5f00d7', '5f00af', '5f0087', '8700af', '8700d7', '8700ff',
        'af00ff', 'af00d7', 'd700ff', 'd75fff', 'd787ff', 'ffafd7', 'ffafff', 'ffd7ff', 'd7afff',
        'd7afd7', 'af87af', 'af87d7', 'af87ff', '875fd7', '875faf', '875fff', 'af5fff', 'af5fd7',
        'af5faf', 'd75fd7', 'd787d7', 'ff87ff', 'ff5fff', 'ff5fd7', 'ff00ff', 'ff00af', 'ff00d7',
        'd700af', 'd700d7', 'af00af', '870087', '5f005f', '87005f', 'af005f', 'af0087', 'd70087',
        'd7005f', 'ff0087', 'ff005f', 'ff5f87', 'd75f87', 'd75faf', 'ff5faf', 'ff87af', 'ff87d7',
        'd787af', 'af5f87', '875f87', '000000', '080808', '121212', '1c1c1c', '262626', '303030',
        '3a3a3a', '444444', '4e4e4e', '585858', '5f5f5f', '626262', '6c6c6c', '767676', '808080',
        '878787', '8a8a8a', '949494', '9e9e9e', 'a8a8a8', 'afafaf', 'b2b2b2', 'bcbcbc', 'c6c6c6',
        'd0d0d0', 'd7d7d7', 'dadada', 'e4e4e4', 'eeeeee', 'ffffff',
    ]

/* Given a color setting name like 'autosuggestion', return the user visible name we present */
function user_visible_title_for_setting_name(name) {
    if (!name) return '';
    switch (name) {
        case 'param':
            return 'parameters';
        case 'escape':
            return 'escape sequences';
        case 'end':
            return 'statement terminators';
        default:
            return name + 's';
    }
}

/* Returns array of values from a dictionary (or any object) */
function dict_values(dict) {
    var result = [];
    for (var i in dict) result.push(dict[i]);
    return result;
}

/* Return the array of colors as an array of N arrays of length items_per_row */
function get_colors_as_nested_array(colors, items_per_row) {
    var result = new Array();
    for (var idx = 0; idx < colors.length; idx += items_per_row) {
        var row = new Array();
        for (var subidx = 0; subidx < items_per_row && idx + subidx < colors.length; subidx++) {
            row.push(colors[idx + subidx]);
        }
        result.push(row);
    }
    return result;
}

/* Given an RGB color as a hex string, like FF0033, convert to HSL, apply the function to adjust its
   lightness, then return the new color as an RGB string */
function adjust_lightness(color_str, func) {
    /* Strip off hash prefix */
    if (color_str[0] == '#') {
        color_str = color_str.substring(1);
    }

    /* Hack to handle for example F00 */
    if (color_str.length == 3) {
        color_str = color_str[0] + color_str[0] + color_str[1] + color_str[1] + color_str[2] + color_str[2]
    }

    /* More hacks */
    if (color_str == 'black') color_str = '000000';
    if (color_str == 'white') color_str = 'c0c0c0';


    var rgb = parseInt(color_str, 16)
    var r = (rgb >> 16) & 0xFF
    var g = (rgb >> 8) & 0xFF
    var b = (rgb >> 0) & 0xFF

    var hsl = rgb_to_hsl(r, g, b)
    var new_lightness = func(hsl[2])
    function to_int_str(val) {
        var str = Math.round(val).toString(16)
        while (str.length < 2)
            str = '0' + str
        return str
    }

    var new_rgb = hsl_to_rgb(hsl[0], hsl[1], new_lightness)
    return to_int_str(new_rgb[0]) + to_int_str(new_rgb[1]) + to_int_str(new_rgb[2])
}

/* Given a color, compute a "border color" for it that can show it selected */
function border_color_for_color(color_str) {
    return adjust_lightness(color_str, function (lightness) {
        var adjust = .5
        var new_lightness = lightness + adjust
        if (new_lightness > 1.0 || new_lightness < 0.0) {
            new_lightness -= 2 * adjust
        }
        return new_lightness
    })
}

/* Use this function to make a color that contrasts well with the given color */
function text_color_for_color(color_str) {
    var adjust = .7
    function compute_constrast(lightness) {
        var new_lightness = lightness + adjust
        if (new_lightness > 1.0 || new_lightness < 0.0) {
            new_lightness -= 2 * adjust
        }
        return new_lightness
    }
    return adjust_lightness(color_str, compute_constrast);
}

function rgb_to_hsl(r, g, b) {
    r /= 255, g /= 255, b /= 255;
    var max = Math.max(r, g, b), min = Math.min(r, g, b);
    var h, s, l = (max + min) / 2;

    if (max == min) {
        h = s = 0; // achromatic
    } else {
        var d = max - min;
        s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
        switch (max) {
            case r:
                h = (g - b) / d + (g < b ? 6 : 0);
                break;
            case g:
                h = (b - r) / d + 2;
                break;
            case b:
                h = (r - g) / d + 4;
                break;
        }
        h /= 6;
    }

    return [h, s, l];
}

function hsl_to_rgb(h, s, l) {
    var r, g, b;

    if (s == 0) {
        r = g = b = l; // achromatic
    } else {
        function hue2rgb(p, q, t) {
            if (t < 0) t += 1;
            if (t > 1) t -= 1;
            if (t < 1 / 6) return p + (q - p) * 6 * t;
            if (t < 1 / 2) return q;
            if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
            return p;
        }

        var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        var p = 2 * l - q;
        r = hue2rgb(p, q, h + 1.0 / 3);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0 / 3);
    }

    return [r * 255, g * 255, b * 255]
}


/* Given a color, compute the master text color for it, by giving it a minimum brightness */
function master_color_for_color(color_str) {
    return adjust_lightness(color_str, function (lightness) {
        if (lightness < .33) {
            lightness = .33
        }
        return lightness
    })
}

/* Given a color name, like 'normal' or 'red' or 'FF00F0', return an RGB color string (or empty
   string). If multiple colors are given, as in '555 brblack', interpret the first color. */
function interpret_color(str) {
    if (!str) return '';
    str = str.toLowerCase().split(" ")[0];
    if (str == 'black') return '#000000';
    if (str == 'red') return '#800000';
    if (str == 'green') return '#008000';
    if (str == 'brown') return '#725000';
    if (str == 'yellow') return '#808000';
    if (str == 'blue') return '#000080';
    if (str == 'magenta') return '#800080';
    if (str == 'purple') return '#800080';
    if (str == 'cyan') return '#008080';
    if (str == 'white') return '#c0c0c0';
    if (str == 'grey') return '#e5e5e5';
    if (str == 'brgrey') return '#555555';
    if (str == 'brblack') return '#808080';
    if (str == 'brred') return '#ff0000';
    if (str == 'brgreen') return '#00ff00';
    if (str == 'brbrown') return '#ffff00';
    if (str == 'bryellow') return '#ffff00';
    if (str == 'brblue') return '#0000ff';
    if (str == 'brmagenta') return '#ff00ff';
    if (str == 'brpurple') return '#ff00ff';
    if (str == 'brcyan') return '#00ffff';
    if (str == 'brwhite') return '#ffffff';
    if (str == 'normal') return '#ffffff';
    if (str == 'reset') return '';
    return '#' + str
}

var nord = {
    nord0: '2e3440',
    nord1: '3b4252',
    nord2: '434c5e',
    nord3: '4c566a',
    nord4: 'd8dee9',
    nord5: 'e5e9f0',
    nord6: 'eceff4',
    nord7: '8fbcbb',
    nord8: '88c0d0',
    nord9: '81a1c1',
    nord10: '5e81ac',
    nord11: 'bf616a',
    nord12: 'd08770',
    nord13: 'ebcb8b',
    nord14: 'a3be8c',
    nord15: 'b48ead',
};

var solarized = {
    base03: '002b36', base02: '073642', base01: '586e75', base00: '657b83', base0: '839496', base1: '93a1a1', base2: 'eee8d5', base3: 'fdf6e3', yellow: 'b58900', orange: 'cb4b16', red: 'dc322f', magenta: 'd33682', violet: '6c71c4', blue: '268bd2', cyan: '2aa198', green: '859900'
};
