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
            if (binding.command.indexOf(query) != -1) {
                result.push(binding);
                continue;
            }
            varieties = binding.bindings;
            for (j=0; j<varieties.length; ++j) {
                if (varieties[j].readable_binding.toLowerCase().indexOf(query.toLowerCase()) != -1) {
                    result.push(binding);
                    break;
                }
            }
        }

        return result;
    }
});

filters.filter("filterAbbreviations", function() {
    return function(abbreviations, query) {
        var result = []
        if (abbreviations == undefined) return result;
        if (query == null) { return abbreviations};

        for(i=0; i<abbreviations.length; ++i) {
            abbr = abbreviations[i];
            if (abbr.word.toLowerCase().indexOf(query) != -1 || abbr.phrase.toLowerCase().indexOf(query.toLowerCase()) != -1) {
                result.push(abbr);
            }
        }

        return result;
    }
});
