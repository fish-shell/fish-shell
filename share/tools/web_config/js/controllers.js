controllers = angular.module("controllers", []);

controllers.controller("main", function ($scope, $location) {
    // substr(1) strips a leading slash
    $scope.currentTab = $location.path().substr(1) || "colors";
    $scope.changeView = function (view) {
        $location.path(view);
        $scope.currentTab = view;
    };
});

controllers.controller("colorsController", function ($scope, $http) {
    $scope.changeSelectedColorScheme = function (newScheme) {
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
    };

    $scope.changeTerminalBackgroundColor = function (color) {
        $scope.terminalBackgroundColor = color;
    };

    $scope.text_color_for_color = text_color_for_color;

    $scope.border_color_for_color = border_color_for_color;

    $scope.interpret_color = interpret_color;

    $scope.getColorArraysArray = function () {
        var result = null;
        if ($scope.selectedColorScheme.colors && $scope.selectedColorScheme.colors.length > 0)
            result = get_colors_as_nested_array($scope.selectedColorScheme.colors, 32);
        else result = get_colors_as_nested_array(term_256_colors, 32);
        return result;
    };

    $scope.beginCustomizationWithSetting = function (setting) {
        if (!$scope.customizationActive) {
            $scope.customizationActive = true;
            $scope.selectedColorSetting = setting;
            $scope.csEditingType = setting;
            $scope.csUserVisibleTitle = user_visible_title_for_setting_name(setting);
        }
    };

    $scope.selectColorSetting = function (name) {
        $scope.selectedColorSetting = name;
        $scope.csEditingType = $scope.customizationActive ? name : "";
        $scope.csUserVisibleTitle = user_visible_title_for_setting_name(name);
        $scope.beginCustomizationWithSetting(name);
    };

    $scope.toggleCustomizationActive = function () {
        if (!$scope.customizationActive) {
            $scope.beginCustomizationWithSetting($scope.selectedColorSetting || "command");
        } else {
            $scope.customizationActive = false;
            $scope.selectedColorSetting = "";
            $scope.csEditingType = "";
        }
    };

    $scope.changeSelectedTextColor = function (color) {
        $scope.selectedColorScheme[$scope.selectedColorSetting] = color;
        $scope.selectedColorScheme["colordata-" + $scope.selectedColorSetting].color = color;
        $scope.noteThemeChanged();
    };

    $scope.sampleTerminalBackgroundColors = [
        "white",
        "#" + solarized.base3,
        "#300",
        "#003",
        "#" + solarized.base03,
        "#232323",
        "#" + nord.nord0,
        "black",
    ];

    /* Array of FishColorSchemes */
    $scope.colorSchemes = [];

    isValidColor = function (col) {
        if (col == "normal") return true;
        var s = new Option().style;
        s.color = col;
        return !!s.color;
    };

    $scope.getThemes = function () {
        $http.get("colors/").then(function (arg) {
            for (var scheme of arg.data) {
                var currentScheme = { name: "Current", colors: [], preferred_background: "black" };
                currentScheme["name"] = scheme["theme"];
                if (scheme["name"]) currentScheme["name"] = scheme["name"];
                var data = scheme["colors"];
                if (scheme["preferred_background"]) {
                    if (isValidColor(scheme["preferred_background"])) {
                        currentScheme["preferred_background"] = scheme["preferred_background"];
                    }
                }
                if (scheme["url"]) currentScheme["url"] = scheme["url"];

                for (var i in data) {
                    currentScheme[data[i].name] = interpret_color(data[i].color).replace(/#/, "");
                    // HACK: For some reason the colors array is cleared later
                    // So we cheesily encode the actual objects as colordata-, so we can send them.
                    // TODO: We should switch to keeping the objects, and also displaying them
                    // with underlines and such.
                    currentScheme["colordata-" + data[i].name] = data[i];
                }
                $scope.colorSchemes.push(currentScheme);
            }
            $scope.changeSelectedColorScheme($scope.colorSchemes[0]);
        });
    };

    $scope.saveThemeButtonTitle = "Set Theme";

    $scope.noteThemeChanged = function () {
        $scope.saveThemeButtonTitle = "Set Theme";
    };

    $scope.setTheme = function () {
        var settingNames = [
            "normal",
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
            // Cheesy hardcoded variable names ahoy!
            // These are all the pager vars,
            // we should really just save all these in a dictionary.
            "fish_pager_color_background",
            "fish_pager_color_prefix",
            "fish_pager_color_progress",
            "fish_pager_color_completion",
            "fish_pager_color_description",
            "fish_pager_color_selected_background",
            "fish_pager_color_selected_prefix",
            "fish_pager_color_selected_completion",
            "fish_pager_color_selected_description",
            // TODO: Setting these to empty currently makes them weird. Figure out why!
            /*
                            "fish_pager_color_secondary_background",
                            "fish_pager_color_secondary_prefix",
                            "fish_pager_color_secondary_completion",
                            "fish_pager_color_secondary_description",
                            */
        ];
        var remaining = settingNames.length;
        var postdata = {
            theme: $scope.selectedColorScheme["name"],
            colors: [],
        };
        for (var name of settingNames) {
            var selected;
            var realname = "colordata-" + name;
            // Skip colors undefined in the current theme
            // js is dumb - the empty string is false,
            // but we want that to mean unsetting a var.
            if (
                !$scope.selectedColorScheme[realname] &&
                $scope.selectedColorScheme[realname] !== ""
            ) {
                continue;
            } else {
                selected = $scope.selectedColorScheme[realname];
            }
            postdata.colors.push({
                what: name,
                color: selected,
            });
        }
        $http
            .post("set_color/", postdata, { headers: { "Content-Type": "application/json" } })
            .then(function (arg) {
                if (arg.status == 200) {
                    $scope.saveThemeButtonTitle = "Theme Set!";
                }
            });
    };

    $scope.getThemes();
});
