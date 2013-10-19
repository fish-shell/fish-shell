fishconfig = angular.module("fishconfig", ["filters", "controllers"]);

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
        .when("/bindings", {
            controller: "bindingsController",
            templateUrl: "partials/bindings.html"
        })
        .otherwise({
            redirectTo: "/colors"
        })
    }]);
