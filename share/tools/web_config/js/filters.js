filters = angular.module("filters", []);

filters.filter("filterVariable", function() {
    return function(variables, query) {
        var result = []
        if (variables == undefined) return result;
        if (query == null) { return variables };

        for(var i=0; i<variables.length; ++i) {
            var variable = variables[i];
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

        for(var i=0; i<bindings.length; ++i) {
            var binding = bindings[i];
            if (binding.command.indexOf(query) != -1) {
                result.push(binding);
                continue;
            }
            var varieties = binding.bindings;
            for (var j=0; j<varieties.length; ++j) {
                if (varieties[j].readable_binding.toLowerCase().indexOf(query.toLowerCase()) != -1) {
                    result.push(binding);
                    break;
                }
            }
        }

        return result;
    }
});
