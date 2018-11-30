/* TODO: Write an angularjs service to wrap these methods */

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
	if (! name) return '';
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
        for (var subidx = 0; subidx < items_per_row && idx  + subidx < colors.length; subidx++) {
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


    rgb = parseInt(color_str, 16)
    r = (rgb >> 16) & 0xFF
    g = (rgb >> 8) & 0xFF
    b = (rgb >> 0) & 0xFF

    hsl = rgb_to_hsl(r, g, b)
    new_lightness = func(hsl[2])
    function to_int_str(val) {
        str = Math.round(val).toString(16)
        while (str.length < 2)
            str = '0' + str
        return str
    }

    new_rgb = hsl_to_rgb(hsl[0], hsl[1], new_lightness)
    return to_int_str(new_rgb[0]) + to_int_str(new_rgb[1]) + to_int_str(new_rgb[2])
}

/* Given a color, compute a "border color" for it that can show it selected */
function border_color_for_color(color_str) {
    return adjust_lightness(color_str, function(lightness){
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
    var adjust = .5
    function compute_constrast(lightness){
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

    if(max == min){
        h = s = 0; // achromatic
    }else{
        var d = max - min;
        s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
        switch(max){
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

    if(s == 0){
        r = g = b = l; // achromatic
    }else{
        function hue2rgb(p, q, t){
            if(t < 0) t += 1;
            if(t > 1) t -= 1;
            if(t < 1/6) return p + (q - p) * 6 * t;
            if(t < 1/2) return q;
            if(t < 2/3) return p + (q - p) * (2/3 - t) * 6;
            return p;
        }

        var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        var p = 2 * l - q;
        r = hue2rgb(p, q, h + 1.0/3);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0/3);
    }

    return [r * 255, g * 255, b * 255]
}


/* Given a color, compute the master text color for it, by giving it a minimum brightness */
function master_color_for_color(color_str) {
    return adjust_lightness(color_str, function(lightness) {
        if (lightness < .33) {
            lightness = .33
        }
        return lightness
    })
}

/* Given a color name, like 'normal' or 'red' or 'FF00F0', return an RGB color string (or empty
   string) */
function interpret_color(str) {
    str = str.toLowerCase();
    if (str == 'black') return '000000';
    if (str == 'red') return '800000';
    if (str == 'green') return '008000';
    if (str == 'brown') return '725000';
    if (str == 'yellow') return '808000';
    if (str == 'blue') return '000080';
    if (str == 'magenta') return '800080';
    if (str == 'purple') return '800080';
    if (str == 'cyan') return '008080';
    if (str == 'white') return 'c0c0c0';
    if (str == 'grey') return 'e5e5e5';
    if (str == 'brgrey') return '555555';
    if (str == 'brblack') return '808080';
    if (str == 'brred') return 'ff0000';
    if (str == 'brgreen') return '00ff00';
    if (str == 'brbrown') return 'ffff00';
    if (str == 'bryellow') return 'ffff00';
    if (str == 'brblue') return '0000ff';
    if (str == 'brmagenta') return 'ff00ff';
    if (str == 'brpurple') return 'ff00ff';
    if (str == 'brcyan') return '00ffff';
    if (str == 'brwhite') return 'ffffff';
    if (str == 'normal') return '';
    if (str == 'reset') return '';
    return str
}

var color_scheme_fish_default = {
    'name': 'fish default',
    'colors': [],
    'preferred_background': 'black',

    'autosuggestion': '555',
    'command': '005fd7',
    'param': '00afff',
    'redirection': '00afff',
    'comment': '990000',
    'error': 'ff0000',
    'escape': '00a6b2',
    'operator': '00a6b2',
    'quote': '999900',
    'end': '009900'
};


var TomorrowTheme = {
    tomorrow_night: {'Background': '1d1f21', 'Current Line': '282a2e', 'Selection': '373b41', 'Foreground': 'c5c8c6', 'Comment': '969896', 'Red': 'cc6666', 'Orange': 'de935f',  'Yellow': 'f0c674', 'Green': 'b5bd68', 'Aqua': '8abeb7', 'Blue': '81a2be', 'Purple': 'b294bb'
    },

    tomorrow: {'Background': 'ffffff', 'Current Line': 'efefef', 'Selection': 'd6d6d6', 'Foreground': '4d4d4c', 'Comment': '8e908c', 'Red': 'c82829', 'Orange': 'f5871f', 'Yellow': 'eab700', 'Green': '718c00', 'Aqua': '3e999f', 'Blue': '4271ae', 'Purple': '8959a8'
    },

    tomorrow_night_bright: {'Background': '000000', 'Current Line': '2a2a2a', 'Selection': '424242', 'Foreground': 'eaeaea', 'Comment': '969896', 'Red': 'd54e53', 'Orange': 'e78c45', 'Yellow': 'e7c547', 'Green': 'b9ca4a', 'Aqua': '70c0b1', 'Blue': '7aa6da', 'Purple': 'c397d8'},

    apply: function(theme, receiver){
        receiver['autosuggestion'] = theme['Comment']
        receiver['command'] = theme['Purple']
        receiver['comment'] = theme['Yellow'] /* we want to use comment for autosuggestions */
        receiver['end'] = theme['Purple']
        receiver['error'] = theme['Red']
        receiver['param'] = theme['Blue']
        receiver['quote'] = theme['Green']
        receiver['redirection'] = theme['Aqua']

        receiver['colors'] = []
        for (var key in theme) receiver['colors'].push(theme[key])
    }
}


var solarized = {
    base03: '002b36', base02: '073642', base01: '586e75', base00: '657b83', base0: '839496', base1: '93a1a1', base2: 'eee8d5', base3: 'fdf6e3', yellow: 'b58900', orange: 'cb4b16', red: 'dc322f', magenta: 'd33682', violet: '6c71c4', blue: '268bd2', cyan: '2aa198', green: '859900'
};

/* Sample color schemes */
var color_scheme_solarized_light = {
    name: "Solarized Light",
    colors: dict_values(solarized),

    preferred_background: '#' + solarized.base3,

    autosuggestion: solarized.base1,
    command: solarized.base01,
    comment: solarized.base1,
    end: solarized.blue,
    error: solarized.red,
    param: solarized.base00,
    quote: solarized.base0,
    redirection: solarized.violet,
    search_match: 'bryellow --background=white',
    fish_pager_color_completion: 'green',
    fish_pager_color_description: 'B3A06D',
    fish_pager_color_prefix: 'cyan --underline',
    fish_pager_color_progress: 'brwhite --background=cyan',

    url: 'http://ethanschoonover.com/solarized'
};

var color_scheme_solarized_dark = {
    name: "Solarized Dark",
    colors: dict_values(solarized),
    preferred_background: '#' + solarized.base03,

    autosuggestion: solarized.base01,
    command: solarized.base1,
    comment: solarized.base01,
    end: solarized.blue,
    error: solarized.red,
    param: solarized.base0,
    quote: solarized.base00,
    redirection: solarized.violet,
    search_match: 'bryellow --background=black',
    fish_pager_color_completion: 'B3A06D',
    fish_pager_color_description: 'B3A06D',
    fish_pager_color_prefix: 'cyan --underline',
    fish_pager_color_progress: 'brwhite --background=cyan',

    url: 'http://ethanschoonover.com/solarized'
};

var color_scheme_tomorrow = {
    name: 'Tomorrow',
    preferred_background: 'white',
    url: 'https://github.com/chriskempson/tomorrow-theme'
}
TomorrowTheme.apply(TomorrowTheme.tomorrow, color_scheme_tomorrow)

var color_scheme_tomorrow_night = {
    name: 'Tomorrow Night',
    preferred_background: '#232323',
    url: 'https://github.com/chriskempson/tomorrow-theme'
}
TomorrowTheme.apply(TomorrowTheme.tomorrow_night, color_scheme_tomorrow_night)

var color_scheme_tomorrow_night_bright = {
    'name': 'Tomorrow Night Bright',
    'preferred_background': 'black',
    'url': 'https://github.com/chriskempson/tomorrow-theme',

}
TomorrowTheme.apply(TomorrowTheme.tomorrow_night_bright, color_scheme_tomorrow_night_bright)

function construct_scheme_analogous(label, background, color_list) {
    return {
        name: label,
        preferred_background: background,
        colors: color_list,

        command: color_list[0],
        quote: color_list[6],
        param: color_list[5],
        autosuggestion: color_list[4],

        error: color_list[9],
        comment: color_list[12],

        end: color_list[10],
        redirection: color_list[11]
    };
}

function construct_scheme_triad(label, background, color_list) {
    return {
        name: label,
        preferred_background: background,
        colors: color_list,

        command: color_list[0],
        quote: color_list[2],
        param: color_list[1],
        autosuggestion: color_list[3],
        redirection: color_list[4],

        error: color_list[8],
        comment: color_list[10],

        end: color_list[7],
    };
}

function construct_scheme_complementary(label, background, color_list) {
    return {
        name: label,
        preferred_background: background,
        colors: color_list,

        command: color_list[0],
        quote: color_list[4],
        param: color_list[3],
        autosuggestion: color_list[2],
        redirection: color_list[6],

        error: color_list[5],
        comment: color_list[8],

        end: color_list[9],
    };
}

function construct_color_scheme_mono(label, background, from_end) {
    var mono_colors = ['000000', '121212', '1c1c1c', '262626', '303030', '3a3a3a', '444444', '4e4e4e', '585858', '5f5f5f', '626262', '6c6c6c', '767676', '808080', '878787', '8a8a8a', '949494', '9e9e9e', 'a8a8a8', 'afafaf', 'b2b2b2', 'bcbcbc', 'c6c6c6', 'd0d0d0', 'd7d7d7', 'dadada', 'e4e4e4', 'eeeeee', 'ffffff'];

    if (from_end) mono_colors.reverse();

    return {
        name: label,
        preferred_background: background,
        colors: mono_colors,

        autosuggestion: '777777',
        command: mono_colors[0],
        comment: mono_colors[7],
        end: mono_colors[12],
        error: mono_colors[20],
        param: mono_colors[4],
        quote: mono_colors[10],
        redirection: mono_colors[15]
    };
}

var additional_color_schemes = [
    construct_scheme_analogous('Snow Day', 'white', ['164CC9', '325197', '072D83', '4C7AE4', '7596E4', '4319CC', '4C3499', '260885', '724EE5', '9177E5', '02BDBD', '248E8E', '007B7B', '39DEDE', '65DEDE']),

    construct_scheme_analogous('Lava', '#232323', ['FF9400', 'BF8330', 'A66000', 'FFAE40', 'FFC473', 'FFC000', 'BF9C30', 'A67D00', 'FFD040', 'FFDD73', 'FF4C00', 'BF5B30', 'A63100', 'FF7940', 'FF9D73']),

    construct_scheme_analogous('Seaweed', '#232323', ['00BF32', '248F40', '007C21', '38DF64', '64DF85', '04819E', '206676', '015367', '38B2CE', '60B9CE', '8EEB00', '7CB02C', '5C9900', 'ACF53D', 'C0F56E']),

    construct_scheme_triad('Fairground', '#003', ['0772A1', '225E79', '024A68', '3BA3D0', '63AFD0', 'D9005B', 'A3295C', '8D003B', 'EC3B86', 'EC6AA1', 'FFE100', 'BFAE30', 'A69200', 'FFE840', 'FFEE73']),

    construct_scheme_complementary('Bay Cruise', 'black', ['009999', '1D7373', '006363', '33CCCC', '5CCCCC', 'FF7400', 'BF7130', 'A64B00', 'FF9640', 'FFB273']),

{
    'name': 'Old School',
    'preferred_background': 'black',

    colors: ['00FF00', '30BE30', '00A400', '44FF44', '7BFF7B', 'FF0000', 'BE3030', 'A40000', 'FF7B7B', '777777', 'CCCCCC'],

    autosuggestion: '777777',
    command: '00FF00',
    comment: '30BE30',
    end: 'FF7B7B',
    error: 'A40000',
    param: '30BE30',
    quote: '44FF44',
    redirection: '7BFF7B'
},

{
    'name': 'Just a Touch',
    'preferred_background': 'black',

    colors: ['B0B0B0', '969696', '789276', 'F4F4F4', 'A0A0F0', '666A80', 'F0F0F0', 'D7D7D7', 'B7B7B7', 'FFA779', 'FAFAFA'],

    autosuggestion: '9C9C9C',
    command: 'F4F4F4',
    comment: 'B0B0B0',
    end: '969696',
    error: 'FFA779',
    param: 'A0A0F0',
    quote: '666A80',
    redirection: 'FAFAFA'
},

{
    'name': 'Dracula',
    'preferred_background': '#282a36',

    colors: ['282A36', '44475A', '44475A', 'F8F8F2', '6272A4', '8BE9FD', '50FA7B', 'FFB86C', 'FF79C6', 'BD93F9', 'FF5555', 'F1FA8C'],

    autosuggestion: 'BD93F9',
    command: 'F8F8F2',
    comment: '6272A4',
    end: '50FA7B',
    error: 'FFB86C',
    param: 'FF79C6',
    quote: 'F1FA8C',
    redirection: '8BE9FD'
},

construct_color_scheme_mono('Mono Lace', 'white', false),
construct_color_scheme_mono('Mono Smoke', 'black', true)
];

