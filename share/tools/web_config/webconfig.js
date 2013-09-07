webconfig = angular.module("webconfig", []);

webconfig.filter("filterVariable", function() {
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

webconfig.config(
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

webconfig.controller("main", function($scope, $location) {
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

webconfig.controller("colorsController", function($scope, $http) {
    $scope.term256Colors = [ "ffd7d7", "d7afaf", "af8787", "875f5f", "5f0000", "870000", "af0000", "d70000", "ff0000", "ff5f5f", "d75f5f", "d78787", "ff8787", "ffafaf", "ffaf87", "ffaf5f", "ffaf00", "ff875f", "ff8700", "ff5f00", "d75f00", "af5f5f", "af5f00", "d78700", "d7875f", "af875f", "af8700", "875f00", "d7af87", "ffd7af", "ffd787", "ffd75f", "d7af00", "d7af5f", "ffd700", "ffff5f", "ffff00", "ffff87", "ffffaf", "ffffd7", "d7ff00", "afd75f", "d7d700", "d7d787", "d7d7af", "afaf87", "87875f", "5f5f00", "878700", "afaf00", "afaf5f", "d7d75f", "d7ff5f", "d7ff87", "87ff00", "afff00", "afff5f", "afd700", "87d700", "87af00", "5f8700", "87af5f", "5faf00", "afd787", "d7ffd7", "d7ffaf", "afffaf", "afff87", "5fff00", "5fd700", "87d75f", "5fd75f", "87ff5f", "5fff5f", "87ff87", "afd7af", "87d787", "87d7af", "87af87", "5f875f", "5faf5f", "005f00", "008700", "00af00", "00d700", "00ff00", "00ff5f", "5fff87", "00ff87", "87ffaf", "afffd7", "5fd787", "00d75f", "5faf87", "00af5f", "5fffaf", "00ffaf", "5fd7af", "00d787", "00875f", "00af87", "00d7af", "5fffd7", "87ffd7", "00ffd7", "d7ffff", "afd7d7", "87afaf", "5f8787", "5fafaf", "87d7d7", "5fd7d7", "5fffff", "00ffff", "87ffff", "afffff", "00d7d7", "00d7ff", "5fd7ff", "5fafd7", "00afd7", "00afff", "0087af", "00afaf", "008787", "005f5f", "005f87", "0087d7", "0087ff", "5fafff", "87afff", "5f87d7", "5f87ff", "005fd7", "005fff", "005faf", "5f87af", "87afd7", "afd7ff", "87d7ff", "d7d7ff", "afafd7", "8787af", "afafff", "8787d7", "8787ff", "5f5fff", "5f5fd7", "5f5faf", "5f5f87", "00005f", "000087", "0000af", "0000d7", "0000ff", "5f00ff", "5f00d7", "5f00af", "5f0087", "8700af", "8700d7", "8700ff", "af00ff", "af00d7", "d700ff", "d75fff", "d787ff", "ffafd7", "ffafff", "ffd7ff", "d7afff", "d7afd7", "af87af", "af87d7", "af87ff", "875fd7", "875faf", "875fff", "af5fff", "af5fd7", "af5faf", "d75fd7", "d787d7", "ff87ff", "ff5fff", "ff5fd7", "ff00ff", "ff00af", "ff00d7", "d700af", "d700d7", "af00af", "870087", "5f005f", "87005f", "af005f", "af0087", "d70087", "d7005f", "ff0087", "ff005f", "ff5f87", "d75f87", "d75faf", "ff5faf", "ff87af", "ff87d7", "d787af", "af5f87", "875f87", "000000", "080808", "121212", "1c1c1c", "262626", "303030", "3a3a3a", "444444", "4e4e4e", "585858", "5f5f5f", "626262", "6c6c6c", "767676", "808080", "878787", "8a8a8a", "949494", "9e9e9e", "a8a8a8", "afafaf", "b2b2b2", "bcbcbc", "c6c6c6", "d0d0d0", "d7d7d7", "dadada", "e4e4e4", "eeeeee", "ffffff", ];

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
    }
    $scope.pickedColorPickerTarget = function(target) {
        console.log("Picked " + target);
        $scope.target = target;
        // Update selection in color picker
        $scope.selectConfig($scope.selectedColorConfig);
    }

    $scope.fetchColors = function() {
        $http.get("/colors/").success(function(data, status, headers, config) {
        $scope.colorConfigs = data;
        $scope.selectedColorConfig = data[0];
        $scope.selectedCell = $scope.term256Colors.indexOf($scope.selectedColorConfig.color);
    })};
    $scope.fetchColors();
});

webconfig.controller("promptController", function($scope, $http) {
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

webconfig.controller("functionsController", function($scope, $http) {

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

webconfig.controller("variablesController", function($scope, $http) {
    $scope.query = null;
    $scope.fetchVariables= function() {
        $http.get("/variables/").success(function(data, status, headers, config) {
        $scope.variables = data;
    })};

    $scope.fetchVariables();
});

webconfig.controller("historyController", function($scope, $http) {
    $scope.fetchHistory = function() {
        $http.get("/history/").success(function(data, status, headers, config) {
        $scope.historyItems = data;
    })};

    $scope.deleteHistoryItem = function(item) {
        index = $scope.historyItems.indexOf(item);
        $http.post("/delete_history_item/","what=" + encodeURIComponent(item), { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config) {
        $scope.historyItems.splice(index, 1);
    })};

    $scope.fetchHistory();
});
