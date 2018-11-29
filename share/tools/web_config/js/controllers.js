controllers = angular.module("controllers", []);

controllers.controller("main", function($scope, $location) {
    // substr(1) strips a leading slash
    $scope.currentTab = $location.path().substr(1) || "colors";
    $scope.changeView = function(view) {
        $location.path(view);
        $scope.currentTab = view;
    }

})

controllers.controller("colorsController", function($scope, $http) {
    $scope.changeSelectedColorScheme= function(newScheme) {
        $scope.selectedColorScheme = angular.copy(newScheme);
        if ($scope.selectedColorScheme.preferred_background) {
            $scope.terminalBackgroundColor = $scope.selectedColorScheme.preferred_background;
        }
        $scope.selectedColorSetting = false;
		$scope.customizationActive = false;
		$scope.csEditingType = false;
        $scope.colorArraysArray = $scope.getColorArraysArray();
        //TODO: Save button should be shown only when colors are changed
        $scope.showSaveButton = true;

        $scope.noteThemeChanged();
    }

    $scope.changeTerminalBackgroundColor = function(color) {
        $scope.terminalBackgroundColor = color;
    }

    $scope.text_color_for_color = text_color_for_color;

    $scope.border_color_for_color = border_color_for_color;

    $scope.getColorArraysArray = function() {
        var result = null;
        if ( $scope.selectedColorScheme.colors && $scope.selectedColorScheme.colors.length > 0)
            result = get_colors_as_nested_array($scope.selectedColorScheme.colors, 32);
        else
            result =  get_colors_as_nested_array(term_256_colors, 32);
        return result;
    }

    $scope.beginCustomizationWithSetting = function(setting) {
    	if (! $scope.customizationActive) {
	    	$scope.customizationActive = true;
			$scope.selectedColorSetting = setting;
			$scope.csEditingType = setting;
			$scope.csUserVisibleTitle = user_visible_title_for_setting_name(setting);
    	}
    }

    $scope.selectColorSetting = function(name) {
    	$scope.selectedColorSetting = name;
    	$scope.csEditingType = $scope.customizationActive ? name : '';
    	$scope.csUserVisibleTitle = user_visible_title_for_setting_name(name);
    	$scope.beginCustomizationWithSetting(name);
    }

    $scope.toggleCustomizationActive = function() {
	    if (! $scope.customizationActive) {
	    	$scope.beginCustomizationWithSetting($scope.selectedColorSetting || 'command');
	    } else {
	    	$scope.customizationActive = false;
			$scope.selectedColorSetting = '';
			$scope.csEditingType = '';
	    }
    }

    $scope.changeSelectedTextColor = function(color) {
        $scope.selectedColorScheme[$scope.selectedColorSetting] = color;
        $scope.noteThemeChanged();
    }

    $scope.sampleTerminalBackgroundColors = ['white', '#' + solarized.base3, '#300', '#003', '#' + solarized.base03, '#232323', 'black'];

    /* Array of FishColorSchemes */
    $scope.colorSchemes = [color_scheme_fish_default, color_scheme_solarized_light, color_scheme_solarized_dark, color_scheme_tomorrow, color_scheme_tomorrow_night, color_scheme_tomorrow_night_bright];
    for (var i=0; i < additional_color_schemes.length; i++)
        $scope.colorSchemes.push(additional_color_schemes[i])


    $scope.getCurrentTheme = function() {
        $http.get("colors/").success(function(data, status, headers, config) {
           var currentScheme = { "name": "Current", "colors":[], "preferred_background": "" };
            for (var i in data) {
                currentScheme[data[i].name] = data[i].color;
            }
            $scope.colorSchemes.splice(0, 0, currentScheme);
            $scope.changeSelectedColorScheme(currentScheme);
     })};

	$scope.saveThemeButtonTitle = "Set Theme";

	$scope.noteThemeChanged = function() {
		$scope.saveThemeButtonTitle = "Set Theme";
	}

    $scope.setTheme = function() {
        var settingNames = ["normal",
                            "command",
                            "quote",
                            "redirection",
                            "end",
                            "error",
                            "param",
                            "comment",
                            "match",
                            "selection",
                            "search_match",
                            "history_current",
                            "operator",
                            "escape",
                            "cwd",
                            "cwd_root",
                            "valid_path",
                            "autosuggestion",
                            "user",
                            "host",
                            "cancel",
                            "fish_pager_color_completion",
                            "fish_pager_color_description",
                            "fish_pager_color_prefix",
                            "fish_pager_color_progress"
                           ];
        var remaining = settingNames.length;
        for (name of settingNames) {
            // Skip colors undefined in the current theme
            if (!$scope.selectedColorScheme[name]) {
            	remaining -= 1;
                continue;
            }
            var postData = "what=" + name + "&color=" + $scope.selectedColorScheme[name] + "&background_color=&bold=&underline=";
            $http.post("set_color/", postData, { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config) {
            	if (status == 200) {
            		remaining -= 1;
            		if (remaining == 0) {
            			/* All styles set! */
            			$scope.saveThemeButtonTitle = "Theme Set!";
            		}
            	}
            })
        }
    };

    $scope.getCurrentTheme();
});

controllers.controller("promptController", function($scope, $http) {
    $scope.selectedPrompt = null;
    $scope.showSaveButton = true;
    $scope.savePromptButtonTitle = "Set Prompt";

    $scope.fetchSamplePrompts= function() {
        $http.get("sample_prompts/").success(function(data, status, headers, config) {
            $scope.samplePrompts = data;
            $scope.samplePromptsArrayArray = get_colors_as_nested_array($scope.samplePrompts, 1);

            if ($scope.selectedPrompt == null) {
                $scope.selectPrompt($scope.samplePrompts[0]);
            }
        })};

    $scope.selectPrompt = function(prompt) {
        $scope.selectedPrompt= prompt;

        $scope.savePromptButtonTitle = "Set Prompt";
    }

    $scope.setNewPrompt = function(selectedPrompt) {
        $http.post("set_prompt/", {'fish_prompt': selectedPrompt.function,}).success(function(data, status, headers, config){

            // Update attributes of current prompt and select it
            $scope.samplePrompts[0].demo = selectedPrompt.demo;
            $scope.samplePrompts[0].function = selectedPrompt.function;
            $scope.samplePrompts[0].font_size = selectedPrompt.font_size;
            $scope.selectedPrompt = $scope.samplePrompts[0];

            // Note that we set it
            $scope.savePromptButtonTitle = "Prompt Set!";
        })};

    $scope.fetchSamplePrompts();

    $scope.setPrompt = function() {
        if ($scope.selectedPrompt) {
            $scope.setNewPrompt($scope.selectedPrompt);
        }
    }
});

controllers.controller("functionsController", function($scope, $http) {
    $scope.selectedFunction = null;
    $scope.functionDefinition = "";

    $scope.selectFunction = function(fun) {
        $scope.selectedFunction = fun;
        $scope.fetchFunctionDefinition($scope.selectedFunction);
    }

    $scope.fetchFunctions= function() {
        $http.get("functions/").success(function(data, status, headers, config) {
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
         $http.post("get_function/","what=" + name, { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config) {
            $scope.functionDefinition = $scope.cleanupFishFunction(data[0]);
    })};

    $scope.fetchFunctions();
});

controllers.controller("variablesController", function($scope, $http) {
    $scope.query = null;

    $scope.fetchVariables= function() {
        $http.get("variables/").success(function(data, status, headers, config) {
        $scope.variables = data;
    })};

    $scope.fetchVariables();
});

controllers.controller("historyController", function($scope, $http, $timeout) {
    // All history items, as strings.
    $scope.allItems = [];
    // How many items per page.
    $scope.itemsPerPage = 50;
    // Filtered items grouped into pages. A list of lists. Never empty.
    $scope.filteredItemPages = [[]];
    // The current page index (0 based).
    $scope.currentPage = 0;
    // The filter query.
    $scope.query = "";
    // The items which are selected.
    $scope.selectedItems = [];

    // Filter all of our items, and group them into pages.
    $scope.filterAndGroup = function () {
        // Filter items.
        var filteredItems;
        if ($scope.query && $scope.query.length > 0) {
            filteredItems = $scope.allItems.filter(function(item) {
                return item.indexOf($scope.query) >= 0;
            });
        } else {
            filteredItems = $scope.allItems;
        }

        // Group them by pages. Ensure our pages are never empty.
        var pages = [];
        for (var start = 0; start < filteredItems.length; start += $scope.itemsPerPage) {
            var end = Math.min(start + $scope.itemsPerPage, filteredItems.length);
            pages.push(filteredItems.slice(start, end));
        }
        if (pages.length == 0) {
            pages.push([]);
        }
        $scope.filteredItemPages = pages;
    };

    $scope.currentPageDescription = function() {
        if ($scope.filteredItemPages[0].length === 0) {
            return "None";
        }
        return ($scope.currentPage + 1) + " / " + $scope.filteredItemPages.length;
    };

    $scope.prevPage = function () {
        $scope.currentPage = Math.max($scope.currentPage - 1, 0);
    };
    
    $scope.nextPage = function () {
        $scope.currentPage = Math.min($scope.currentPage + 1,
                                      $scope.filteredItemPages.length - 1);
    };

    $scope.selectItem = function(item) {
        var index = $scope.selectedItems.indexOf(item);
        if (index >= 0) {
            // Deselect.
            $scope.selectedItems.splice(index,1);
        }
        else {
            $scope.selectedItems.push(item);
        }
    }

    // Get history from server
    $scope.fetchHistory = function() {
        $http.get("history/").success(function(data, status, headers, config) {
            $scope.allItems = data;
            $scope.filterAndGroup();
        });
    };

    $scope.deleteHistoryItem = function(item) {
        index = $scope.allItems.indexOf(item);
        $http.post("delete_history_item/","what=" + encodeURIComponent(item), { headers: {'Content-Type': 'application/x-www-form-urlencoded'} }).success(function(data, status, headers, config) {
        $scope.allItems.splice(index, 1);
        $scope.filterAndGroup();
    })};

    var queryInputTimeout = null;
    $scope.$watch("queryInput", function() {
        if (queryInputTimeout){
            $timeout.cancel(queryInputTimeout);
        }

        queryInputTimeout = $timeout(function() {
            $scope.query = $scope.queryInput;
            $scope.filterAndGroup();
            $scope.currentPage = 0;
        }, 1000);
    });

    $scope.fetchHistory();
});

controllers.controller("bindingsController", function($scope, $http) {
    $scope.bindings = [];
    $scope.fetchBindings = function() {
        $http.get("bindings/").success(function(data, status, headers, config) {
            $scope.bindings = data;
    })};

    $scope.fetchBindings();
});

controllers.controller("abbreviationsController", function($scope, $http) {
    $scope.abbreviations = [];
    $scope.addBlank = function() {
        // Add blank entry if it is missing
        hasBlank = {hasBlank: false}
        angular.forEach($scope.abbreviations, function(value, key) {
            if (value.phrase === "" && value.word === "") {
                this.hasBlank = true;
            }
        }, hasBlank);
        if (! hasBlank.hasBlank) {
            $scope.abbreviations.push({phrase: "", word: "", editable: true})
        }
    }
    $scope.fetchAbbreviations = function() {
        $http.get("abbreviations/").success(function(data, status, headers, config) {
            $scope.abbreviations = data;
            $scope.addBlank();
    })};

    $scope.editAbbreviation = function(abbreviation) {
        abbreviation.editable = true;
    }

    $scope.saveAbbreviation = function(abbreviation) {
        if (abbreviation.word && abbreviation.phrase) {
            $http.post("save_abbreviation/", abbreviation).success(function(data, status, headers, config) {
                abbreviation.editable = false;
                $scope.addBlank();
            });
        }
    };

    $scope.removeAbbreviation = function(abbreviation) {
        if (abbreviation.word) {
            $http.post("remove_abbreviation/", abbreviation).success(function(data, status, headers, config) {
                $scope.abbreviations.splice($scope.abbreviations.indexOf(abbreviation), 1);
                $scope.addBlank();
            });
        }
    };

    $scope.fetchAbbreviations();
});
