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
        .when("/abbreviations", {
            controller: "abbreviationsController",
            templateUrl: "partials/abbreviations.html"
        })
        .otherwise({
            redirectTo: "/colors"
        })
    }]);

/* Inspired from http://blog.tomaka17.com/2012/12/random-tricks-when-using-angularjs/ */
fishconfig.config(function($httpProvider, $compileProvider) {
    var global_error_element = null;

    var showMessage = function(content) {
        global_error_element.text(content);
    };

    $httpProvider.responseInterceptors.push(function($q) {
        return function(promise) {
            return promise.then(function(successResponse) {
                showMessage('');
                return successResponse;
            }, function(errorResponse) {
                switch (errorResponse.status) {
                    case 0:
                        showMessage("The request received an error. Perhaps the server has shut down.");
                        break;
                   case 500:
                        showMessage('Server internal error: ' + errorResponse.data);
                        break;
                    default:
                        showMessage('Error ' + errorResponse.status + ': ' + errorResponse.data);
                }
                return $q.reject(errorResponse);
            });
        };
    });

    $compileProvider.directive('errorMessage', function() {
        return {
            link: function(scope, element, attrs) { global_error_element = element; }
        };
    });
});
