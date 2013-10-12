fishconfig = angular.module("fishconfig", []);

fishconfig.filter("filterVariable", function() {
    return function(variables, query) {
        var result = []
        if (variables == undefined) return result;
        if (query == null) { return variables };

        for(i=0; i<variables.length; ++i) {
            variable = variables[i];
            if (variable.name.indexOf(query) != -1 ||  variable.value.indexOf(query) != -1) {
                result.push(variable);
            }
        }

        return result;
    }
});

fishconfig.config(
    ["$routeProvider", function($routeProvider) {
        $routeProvider
        .when("/colors", {
            controller: "colorsController",
            templateUrl: "partials/colors.html"
        })
        .when("/prompt", {
            controller: "promptController",
            templateUrl: "partials/prompt.html"
        })
        .when("/functions", {
            controller: "functionsController",
            templateUrl: "partials/functions.html"
        })
        .when("/variables", {
            controller: "variablesController",
            templateUrl: "partials/variables.html"
        })
        .when("/history", {
            controller: "historyController",
            templateUrl: "partials/history.html"
        })
        .otherwise({
            redirectTo: "/colors"
        })
    }]);

fishconfig.controller("main", function($scope, $location) {
    $scope.currentTab = "colors";

    $scope.changeView = function(view) {
        $location.path(view);
        $scope.currentTab = view;
    }

    $scope.tabCssClass = function(view) {
        var cls = "tab";
        if ($scope.currentTab == view) {
            cls += " selected_tab";
        }
        return cls;
    }

})

fishconfig.controller("colorsController", function($scope, $http) {
    $scope.term256Colors = [ //247
    "ffd7d7",
    "d7afaf",
    "af8787",
    "875f5f",
    "5f0000",
    "870000",
    "af0000",
    "d70000",
    "ff0000",
    "ff5f5f",
    "d75f5f",
    "d78787",
    "ff8787",
    "ffafaf",
    "ffaf87",
    "ffaf5f",
    "ffaf00",
    "ff875f",
    "ff8700",
    "ff5f00",
    "d75f00",
    "af5f5f",
    "af5f00",
    "d78700",
    "d7875f",
    "af875f",
    "af8700",
    "875f00",
    "d7af87",
    "ffd7af",
    "ffd787",
    "ffd75f",
    "d7af00",
    "d7af5f",
    "ffd700",
    "ffff5f",
    "ffff00",
    "ffff87",
    "ffffaf",
    "ffffd7",
    "d7ff00",
    "afd75f",
    "d7d700",
    "d7d787",
    "d7d7af",
    "afaf87",
    "87875f",
    "5f5f00",
    "878700",
    "afaf00",
    "afaf5f",
    "d7d75f",
    "d7ff5f",
    "d7ff87",
    "87ff00",
    "afff00",
    "afff5f",
    "afd700",
    "87d700",
    "87af00",
    "5f8700",
    "87af5f",
    "5faf00",
    "afd787",
    "d7ffd7",
    "d7ffaf",
    "afffaf",
    "afff87",
    "5fff00",
    "5fd700",
    "87d75f",
    "5fd75f",
    "87ff5f",
    "5fff5f",
    "87ff87",
    "afd7af",
    "87d787",
    "87d7af",
    "87af87",
    "5f875f",
    "5faf5f",
    "005f00",
    "008700",
    "00af00",
    "00d700",
    "00ff00",
    "00ff5f",
    "5fff87",
    "00ff87",
    "87ffaf",
    "afffd7",
    "5fd787",
    "00d75f",
    "5faf87",
    "00af5f",
    "5fffaf",
    "00ffaf",
    "5fd7af",
    "00d787",
    "00875f",
    "00af87",
    "00d7af",
    "5fffd7",
    "87ffd7",
    "00ffd7",
    "d7ffff",
    "afd7d7",
    "87afaf",
    "5f8787",
    "5fafaf",
    "87d7d7",
    "5fd7d7",
    "5fffff",
    "00ffff",
    "87ffff",
    "afffff",
    "00d7d7",
    "00d7ff",
    "5fd7ff",
    "5fafd7",
    "00afd7",
    "00afff",
    "0087af",
    "00afaf",
    "008787",
    "005f5f",
    "005f87",
    "0087d7",
    "0087ff",
    "5fafff",
    "87afff",
    "5f87d7",
    "5f87ff",
    "005fd7",
    "005fff",
    "005faf",
    "5f87af",
    "87afd7",
    "afd7ff",
    "87d7ff",
    "d7d7ff",
    "afafd7",
    "8787af",
    "afafff",
    "8787d7",
    "8787ff",
    "5f5fff",
    "5f5fd7",
    "5f5faf",
    "5f5f87",
    "00005f",
    "000087",
    "0000af",
    "0000d7",
    "0000ff",
    "5f00ff",
    "5f00d7",
    "5f00af",
    "5f0087",
    "8700af",
    "8700d7",
    "8700ff",
    "af00ff",
    "af00d7",
    "d700ff",
    "d75fff",
    "d787ff",
    "ffafd7",
    "ffafff",
    "ffd7ff",
    "d7afff",
    "d7afd7",
    "af87af",
    "af87d7",
    "af87ff",
    "875fd7",
    "875faf",
    "875fff",
    "af5fff",
    "af5fd7",
    "af5faf",
    "d75fd7",
    "d787d7",
    "ff87ff",
    "ff5fff",
    "ff5fd7",
    "ff00ff",
    "ff00af",
    "ff00d7",
    "d700af",
    "d700d7",
    "af00af",
    "870087",
    "5f005f",
    "87005f",
    "af005f",
    "af0087",
    "d70087",
    "d7005f",
    "ff0087",
    "ff005f",
    "ff5f87",
    "d75f87",
    "d75faf",
    "ff5faf",
    "ff87af",
    "ff87d7",
    "d787af",
    "af5f87",
    "875f87",
    "000000",
    "080808",
    "121212",
    "1c1c1c",
    "262626",
    "303030",
    "3a3a3a",
    "444444",
    "4e4e4e",
    "585858",
    "5f5f5f",
    "626262",
    "6c6c6c",
    "767676",
    "808080",
    "878787",
    "8a8a8a",
    "949494",
    "9e9e9e",
    "a8a8a8",
    "afafaf",
    "b2b2b2",
    "bcbcbc",
    "c6c6c6",
    "d0d0d0",
    "d7d7d7",
    "dadada",
    "e4e4e4",
    "eeeeee",
    "ffffff",
    ]

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

    /* Given a color, compute a "border color" for it that can show it selected */
    $scope.border_color_for_color = function (color_str) {
        return adjust_lightness(color_str, function(lightness){
            var adjust = .5
            var new_lightness = lightness + adjust
            if (new_lightness > 1.0 || new_lightness < 0.0) {
                new_lightness -= 2 * adjust
            }
            return new_lightness
        })
    }

    $scope.text_color_for_color = function (color_str) {
        /* Use this function to make a color that contrasts well with the given color */
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

    $scope.changeSelectedColorScheme= function(newScheme) {
        $scope.selectedColorScheme = newScheme;
        if ($scope.selectedColorScheme.preferred_background) {
            $scope.terminalBackgroundColor = $scope.selectedColorScheme.preferred_background;
        }
        $scope.selectedColorSetting = 'command';
        $scope.colorArraysArray = $scope.getColorArraysArray();
        //TODO: Save button should be shown only when colors are changed
        $scope.showSaveButton = true;
    }

    $scope.changeTerminalBackgroundColor = function(color) {
        $scope.terminalBackgroundColor = color;
    }

    $scope.getColorArraysArray = function() {
        var result = null;
        if ($scope.selectedColorScheme.colors.length > 0)
            result = get_colors_as_nested_array($scope.selectedColorScheme.colors, 32);
        else
            result =  get_colors_as_nested_array($scope.term256Colors, 32);
        return result;
    }

    $scope.selectColorSetting = function(name) {
        $scope.selectedColorSetting = name;
    }

    $scope.changeSelectedTextColor = function(color) {
        $scope.selectedColorScheme[$scope.selectedColorSetting] = color;
    }

    var color_scheme_fish_default = {
        "name": "fish default",
        "colors": [],
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

    construct_color_scheme_mono('Mono Lace', 'white', false),
    construct_color_scheme_mono('Mono Smoke', 'black', true)
    ];

    $scope.sampleTerminalBackgroundColors = ['white', '#' + solarized.base3, '#300', '#003', '#' + solarized.base03, '#232323', 'black'];

    /* Array of FishColorSchemes */
    $scope.color_schemes = [color_scheme_fish_default, color_scheme_solarized_light, color_scheme_solarized_dark, color_scheme_tomorrow, color_scheme_tomorrow_night, color_scheme_tomorrow_night_bright];
    for (var i=0; i < additional_color_schemes.length; i++)
        $scope.color_schemes.push(additional_color_schemes[i])

    var supported_setting_names = ['autosuggestion', 'command', 'comment', 'end', 'error', 'param', 'quote', 'redirection'];

    $scope.changeSelectedColorScheme($scope.color_schemes[0]);
    $scope.color_settings = $scope.color_schemes.colors; 
    $scope.setTheme = function() {
        var settingNames = ["autosuggestion", "command", "param", "redirection", "comment", "error", "escape", "operator", "quote", "end"];
        for (name in settingNames) {
            var postData = "what=" + settingNames[name] + "&color=" + $scope.selectedColorScheme[settingNames[name]] + "&background_color=&bold=&underline=";
            $http.post("/set_color/", postData, { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config) {
                console.log(data);
            })
        }
    };




    range = function(start, end) {
        var result = [];
        for (var i = start; i <= end; i++) {
            result.push(i);
        }
        return result;
    };

    $scope.selectedColorConfig = null;
    $scope.itemsPerRow = range(0, 15);
    $scope.totalRows = range(0, $scope.term256Colors.length/$scope.itemsPerRow.length);
    $scope.selectedCell = null;
    $scope.target = "text";

    $scope.selectConfig = function(newSelection) {
        $scope.selectedColorConfig = newSelection;
        //console.log("Color :" + $scope.colorConfig[$scope.selectedColorConfig].color.toLowerCase() + $scope.term256Colors.indexOf($scope.colorConfig[$scope.selectedColorConfig].color.toLowerCase()));
        if ($scope.target == "background") {
            $scope.selectedCell = $scope.term256Colors.indexOf($scope.selectedColorConfig.background.toLowerCase());
        }
        else {
            $scope.selectedCell = $scope.term256Colors.indexOf($scope.selectedColorConfig.color.toLowerCase());
       } 
    }

    $scope.pickedColorCell = function(index) {
        console.log("color picked" + index +  " " + $scope.term256Colors[index]);
        $scope.selectedCell = index;

        if ($scope.target == "background") {
            $scope.selectedColorConfig.background = $scope.term256Colors[$scope.selectedCell];
        }
        else {
            $scope.selectedColorConfig.color = $scope.term256Colors[$scope.selectedCell];
        } 

        $scope.setColor();
    }

    $scope.pickedColorPickerTarget = function(target) {
        console.log("Picked " + target);
        $scope.target = target;
        // Update selection in color picker
        $scope.selectConfig($scope.selectedColorConfig);
    }

    $scope.setUnderline = function() {
        $scope.selectedColorConfig.underline = !$scope.selectedColorConfig.underline;
        $scope.setColor();
    }

    $scope.fetchColors = function() {
        $http.get("/colors/").success(function(data, status, headers, config) {
        $scope.colorConfigs = data;
        $scope.selectedColorConfig = data[0];
        $scope.selectedCell = $scope.term256Colors.indexOf($scope.selectedColorConfig.color);
    })};


    $scope.fetchColors();
});

fishconfig.controller("promptController", function($scope, $http) {
    $scope.selectedPrompt = null;

    $scope.fetchCurrentPrompt = function(currenttPrompt) {
        $http.get("/current_prompt/").success(function(data, status, headers, config) {
        currenttPrompt.function = data.function;
    })};

    $scope.fetchSamplePrompts= function() {
        $http.get("/sample_prompts/").success(function(data, status, headers, config) {
            $scope.samplePrompts = data;
            if ($scope.selectedPrompt == null) {
                $scope.selectPrompt($scope.samplePrompts[0]);
            }
        })};

    $scope.fetchSamplePrompt = function(selectedPrompt) {
        console.log("Fetcing sample prompt");
         $http.post("/get_sample_prompt/","what=" + encodeURIComponent(selectedPrompt.function), { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config)   {
            console.log("Data is " + JSON.stringify(data[0]));
            $scope.demoText= data[0].demo;
            $scope.demoTextFontSize = data[0].font_size;
            console.log("Demo text is " + $scope.demoText);
        })};

    $scope.selectPrompt = function(promptt) {
        $scope.selectedPrompt= promptt;
        if ($scope.selectedPrompt.name == "Current") {
            $scope.fetchCurrentPrompt($scope.selectedPrompt);
        }
        $scope.fetchSamplePrompt($scope.selectedPrompt);
    }

    $scope.setNewPrompt = function(selectedPrompt) {
        console.log("Set new prompt" + selectedPrompt);
        $http.post("/set_prompt/","what=" + encodeURIComponent(selectedPrompt.function), { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config){
            console.log("Data is " + JSON.stringify(data));
        })};

    $scope.fetchSamplePrompts();
});

fishconfig.controller("functionsController", function($scope, $http) {
    $scope.selectedFunction = null;
    $scope.functionDefinition = ""; 

    $scope.selectFunction = function(fun) {
        $scope.selectedFunction = fun;
        $scope.fetchFunctionDefinition($scope.selectedFunction);
    }

    $scope.fetchFunctions= function() {
        $http.get("/functions/").success(function(data, status, headers, config) {
            $scope.functions = data;
            $scope.selectFunction($scope.functions[0]);
    })};

    $scope.cleanupFishFunction = function (contents) {
        /* Replace leading tabs and groups of four spaces at the beginning of a line with two spaces. */
        lines = contents.split('\n')
        rx = /^[\t ]+/
        for (var i=0; i < lines.length; i++) {
            line = lines[i]
            /* Get leading tabs and spaces */
            whitespace_arr = rx.exec(line)
            if (whitespace_arr) {
                /* Replace four spaces with two spaces, and tabs with two spaces */
                var whitespace = whitespace_arr[0]
                new_whitespace = whitespace.replace(/(    )|(\t)/g, '  ')
                lines[i] = new_whitespace + line.slice(whitespace.length)
            }
        }
        return lines.join('\n')
    }

    $scope.fetchFunctionDefinition = function(name) {
         $http.post("/get_function/","what=" + name, { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config) {
            $scope.functionDefinition = $scope.cleanupFishFunction(data[0]);
    })};

    $scope.fetchFunctions();
});

fishconfig.controller("variablesController", function($scope, $http) {
    $scope.query = null;

    $scope.fetchVariables= function() {
        $http.get("/variables/").success(function(data, status, headers, config) {
        $scope.variables = data;
    })};

    $scope.fetchVariables();
});

fishconfig.controller("historyController", function($scope, $http, $timeout) {
    $scope.historyItems = [];
    $scope.historySize = 0;
    // Stores items which are yet to be added in history items
    $scope.remainingItems = [];

    // Populate history items in parts
    $scope.loadHistory = function() {
        if ($scope.remainingItems.length <= 0) {
            $scope.loadingText = "";
            return;
        }

        var to_load = $scope.remainingItems.splice(0, 100);
        for (i in to_load) {
            $scope.historyItems.push(to_load[i]);
        }

        $scope.loadingText = "Loading " + $scope.historyItems.length + "/" + $scope.historySize;
        $timeout($scope.loadHistory, 100);
    }

    // Get history from server
    $scope.fetchHistory = function() {
        $http.get("/history/").success(function(data, status, headers, config) {
        $scope.historySize = data.length;
        $scope.remainingItems = data;
        $timeout($scope.loadHistory, 100);
    })};

    $scope.deleteHistoryItem = function(item) {
        index = $scope.historyItems.indexOf(item);
        $http.post("/delete_history_item/","what=" + encodeURIComponent(item), { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config) {
        $scope.historyItems.splice(index, 1);
    })};

    $scope.fetchHistory();
});
