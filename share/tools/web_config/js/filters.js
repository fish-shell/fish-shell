filters = angular.module("filters", []);

filters.filter("filterVariable", function() {
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

filters.filter("filterBinding", function() {
    return function(bindings, query) {
        var result = []
        if (bindings == undefined) return result;
        if (query == null) { return bindings};

        for(i=0; i<bindings.length; ++i) {
            binding = bindings[i];
            if (binding.command.indexOf(query) != -1 || binding.readable_binding.toLowerCase().indexOf(query.toLowerCase()) != -1) {
                result.push(binding);
            }
        }

        return result;
    }
});
