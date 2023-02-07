document.addEventListener("alpine:init", () => {
    window.Alpine.data("prompt", () => ({
        selectedPrompt: null,
        showSaveButton: true,
        samplePrompts: [],
        savePromptButtonTitle: "Set Prompt",
        // where is this initialized in the original code?
        terminalBackgroundColor: "black",

        async fetchSamplePrompts() {
            this.samplePrompts = await (await fetch("sample_prompts/")).json();

            if (this.selectedPrompt === null) {
                this.selectPrompt(this.samplePrompts[0]);
            }
        },

        selectPrompt(prompt) {
            this.selectedPrompt = prompt;
            this.savePromptButtonTitle = "Set Prompt";
        },

        setNewPrompt(selectedPrompt) {
            $http
                .post("set_prompt/", { fish_prompt: selectedPrompt.function })
                .then(function (arg) {
                    // Update attributes of current prompt and select it
                    this.samplePrompts[0].demo = selectedPrompt.demo;
                    this.samplePrompts[0].right = selectedPrompt.right;
                    this.samplePrompts[0].function = selectedPrompt.function;
                    this.samplePrompts[0].font_size = selectedPrompt.font_size;
                    this.selectedPrompt = this.samplePrompts[0];

                    // Note that we set it
                    this.savePromptButtonTitle = "Prompt Set!";
                });
        },

        setPrompt() {
            if (this.selectedPrompt) {
                this.setNewPrompt(this.selectedPrompt);
            }
        },

        async init() {
            await this.fetchSamplePrompts();
        },
    }));

    window.Alpine.data("functions", () => ({
        functions: [],
        selectedFunction: null,
        functionDefinition: "",

        async selectFunction(fun) {
            this.selectedFunction = fun;
            await this.fetchFunctionDefinition(this.selectedFunction);
        },

        cleanupFishFunction(contents) {
            /* Replace leading tabs and groups of four spaces at the beginning of a line with two spaces. */
            var lines = contents ? contents.split("\n") : [];
            var rx = /^[\t ]+/;
            for (var i = 0; i < lines.length; i++) {
                var line = lines[i];
                /* Get leading tabs and spaces */
                var whitespace_arr = rx.exec(line);
                if (whitespace_arr) {
                    /* Replace four spaces with two spaces, and tabs with two spaces */
                    var whitespace = whitespace_arr[0];
                    var new_whitespace = whitespace.replace(/(    )|(\t)/g, "  ");
                    lines[i] = new_whitespace + line.slice(whitespace.length);
                }
            }
            return lines.join("\n");
        },

        async fetchFunctionDefinition(name) {
            let resp = await fetch("get_function/", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                // TODO are functions guaranteed to newer contain symbols which require escaping?
                body: `what=${name}`,
            });
            this.functionDefinition = (await resp.json())[0];
        },

        async init() {
            this.functions = await (await fetch("functions/")).json();
            this.selectFunction(this.functions[0]);
        },
    }));

    window.Alpine.data("variables", () => ({
        vars: [],
        query: "",
        async init() {
            this.vars = await (await fetch("variables/")).json();
        },
        get filteredVars() {
            return this.vars.filter(
                (v) => v.name.includes(this.query) || v.value.includes(this.query)
            );
        },
    }));

    window.Alpine.data("history", () => ({
        // All history items, as strings.
        allItems: [],
        // How many items per page.
        itemsPerPage: 50,
        // Filtered items grouped into pages. A list of lists. Never empty.
        filteredItems: [],
        // The current page index (0 based).
        currentPage: 0,
        // The filter query.
        query: "",
        // The items which are selected.
        selectedItems: [],

        // Filter all of our items.
        // This could also be a getter though they are not cached
        // and would be recalculated on each page change.
        filterAndGroup() {
            this.filteredItems = this.allItems.filter((item) => item.includes(this.query));
        },

        get currentPageDescription() {
            if (!this.filteredItems.length) {
                return "None";
            }
            return `${this.currentPage + 1} / ${Math.ceil(
                this.filteredItems.length / this.itemsPerPage
            )}`;
        },

        prevPage() {
            this.currentPage = Math.max(this.currentPage - 1, 0);
        },

        nextPage() {
            this.currentPage = Math.min(this.currentPage + 1, this.filteredItems.length - 1);
        },

        selectItem(item) {
            var index = this.selectedItems.indexOf(item);
            if (index >= 0) {
                // Deselect.
                this.selectedItems.splice(index, 1);
            } else {
                this.selectedItems.push(item);
            }
        },

        async deleteHistoryItem(item) {
            await fetch("delete_history_item/", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: `what=${encodeURIComponent(item)}`,
            });
            let index = this.allItems.indexOf(item);
            this.allItems.splice(index, 1);
            this.filterAndGroup();
        },

        async init() {
            this.allItems = await (await fetch("history/")).json();
            // TODO this should not be necessary as long as we use $watch below
            this.filterAndGroup();

            this.$watch("query", () => {
                this.filterAndGroup();
                this.currentPage = 0;
            });
        },
    }));

    window.Alpine.data("bindings", () => ({
        bindings: [],
        query: "",
        async init() {
            this.bindings = await (await fetch("bindings/")).json();
        },
        get filteredBindings() {
            return this.bindings.filter(
                (b) =>
                    b.command.includes(this.query) ||
                    b.bindings.some((variety) =>
                        variety.readable_binding.toLowerCase().includes(this.query.toLowerCase())
                    )
            );
        },
    }));
});
