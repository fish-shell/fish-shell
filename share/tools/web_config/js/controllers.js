controllers = angular.module("controllers", []);

controllers.controller("main", function($scope, $location) {
    $scope.currentTab = "colors";

    $scope.changeView = function(view) {
        $location.path(view);
        $scope.currentTab = view;
    }

})

controllers.controller("colorsController", function($scope, $http) {
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

    $scope.text_color_for_color = function(color) {
        return text_color_for_color(color);
    }

    $scope.getColorArraysArray = function() {
        var result = null;
        if ( $scope.selectedColorScheme.colors && $scope.selectedColorScheme.colors.length > 0)
            result = get_colors_as_nested_array($scope.selectedColorScheme.colors, 32);
        else
            result =  get_colors_as_nested_array(term_256_colors, 32);
        return result;
    }

    $scope.selectColorSetting = function(name) {
        $scope.selectedColorSetting = name;
    }

    $scope.changeSelectedTextColor = function(color) {
        $scope.selectedColorScheme[$scope.selectedColorSetting] = color;
    }

    $scope.sampleTerminalBackgroundColors = ['white', '#' + solarized.base3, '#300', '#003', '#' + solarized.base03, '#232323', 'black'];

    /* Array of FishColorSchemes */
    $scope.colorSchemes = [color_scheme_fish_default, color_scheme_solarized_light, color_scheme_solarized_dark, color_scheme_tomorrow, color_scheme_tomorrow_night, color_scheme_tomorrow_night_bright];
    for (var i=0; i < additional_color_schemes.length; i++)
        $scope.colorSchemes.push(additional_color_schemes[i])


    $scope.getCurrentTheme = function() {
        $http.get("/colors/").success(function(data, status, headers, config) {
           var currentScheme = { "name": "Current", "colors":[], "preferred_background": "" };
            for (var i in data) {
                currentScheme[data[i].name] = data[i].color;
            }
            $scope.colorSchemes.splice(0, 0, currentScheme);
            $scope.changeSelectedColorScheme(currentScheme);
     })};

    $scope.setTheme = function() {
        var settingNames = ["autosuggestion", "command", "param", "redirection", "comment", "error", "quote", "end"];
        for (name in settingNames) {
            var postData = "what=" + settingNames[name] + "&color=" + $scope.selectedColorScheme[settingNames[name]] + "&background_color=&bold=&underline=";
            $http.post("/set_color/", postData, { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config) {
            })
        }
    };

    $scope.getCurrentTheme();
});

controllers.controller("promptController", function($scope, $http) {
    $scope.selectedPrompt = null;

    $scope.fetchSamplePrompts= function() {
        $http.get("/sample_prompts/").success(function(data, status, headers, config) {
            $scope.samplePrompts = data;
            $scope.samplePromptsArrayArray = get_colors_as_nested_array($scope.samplePrompts, 1);

            if ($scope.selectedPrompt == null) {
                $scope.selectPrompt($scope.samplePrompts[0]);
            }
        })};

    $scope.selectPrompt = function(promptt) {
        $scope.selectedPrompt= promptt;
    }

    $scope.setNewPrompt = function(selectedPrompt) {
        $http.post("/set_prompt/","what=" + encodeURIComponent(selectedPrompt.function), { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config){

            // Update attributes of current prompt and select it
            $scope.samplePrompts[0].demo = selectedPrompt.demo;
            $scope.samplePrompts[0].function = selectedPrompt.function;
            $scope.samplePrompts[0].font_size = selectedPrompt.font_size;
            $scope.selectedPrompt = $scope.samplePrompts[0];
        })};

    $scope.fetchSamplePrompts();
});

controllers.controller("functionsController", function($scope, $http) {
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

controllers.controller("variablesController", function($scope, $http) {
    $scope.query = null;

    $scope.fetchVariables= function() {
        $http.get("/variables/").success(function(data, status, headers, config) {
        $scope.variables = data;
    })};

    $scope.fetchVariables();
});

controllers.controller("historyController", function($scope, $http, $timeout) {
    $scope.historyItems = [];
    $scope.historySize = 0;
    // Stores items which are yet to be added in history items
    $scope.remainingItems = [];
    $scope.selectedItems = [];

    // Populate history items in parts
    $scope.loadHistory = function() {
        if ($scope.remainingItems.length <= 0) {
            $scope.loadingText = "";
            return;
        }

        var toLoad = $scope.remainingItems.splice(0, 100);
        for (i in toLoad) {
            $scope.historyItems.push(toLoad[i]);
        }

        $scope.loadingText = "Loading " + $scope.historyItems.length + "/" + $scope.historySize;
        $timeout($scope.loadHistory, 100);
    }

    $scope.selectItem = function(item) {
        var index = $scope.selectedItems.indexOf(item);
        if ( index >= 0) {
            $scope.selectedItems.splice(index,1);
        }
        else {
            $scope.selectedItems.push(item);
        }
    }
    // Get history from server
    $scope.fetchHistory = function() {
        $http.get("/history/").success(function(data, status, headers, config) {
        $scope.historySize = data.length;
        $scope.remainingItems = data;

        /* Call this function 10 times/second */
        $timeout($scope.loadHistory, 100);
    })};

    $scope.deleteHistoryItem = function(item) {
        index = $scope.historyItems.indexOf(item);
        $http.post("/delete_history_item/","what=" + encodeURIComponent(item), { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config) {
        $scope.historyItems.splice(index, 1);
    })};

    var queryInputTimeout = null;
    $scope.$watch("queryInput", function() {
        if (queryInputTimeout){
            $timeout.cancel(queryInputTimeout);
        }

        queryInputTimeout = $timeout(function() {
            $scope.query = $scope.queryInput;
        }, 1000);
    });

    $scope.fetchHistory();
});

controllers.controller("bindingsController", function($scope, $http) {
    $scope.bindings = [];
    $scope.fetchBindings = function() {
        $http.get("/bindings/").success(function(data, status, headers, config) {
            $scope.bindings = data;
    })};

    $scope.fetchBindings();
});
