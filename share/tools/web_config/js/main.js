// TODO double check all defaults for nullability etc
document.addEventListener("alpine:init", () => {
    window.Alpine.data("colors", () => ({
        // TODO make null
        selectedColorScheme: {},
        terminalBackgroundColor: "black",
        showSaveButton: false,
        csUserVisibleTitle: "",
        selectedColorSetting: false,

        text_color_for_color: window.text_color_for_color,

        changeSelectedColorScheme(newScheme) {
            console.log(newScheme);
            // TODO find out if angular.copy is deep copy
            this.selectedColorScheme = { ...newScheme };
            if (this.selectedColorScheme.preferred_background) {
                this.terminalBackgroundColor = this.selectedColorScheme.preferred_background;
            }
            this.selectedColorSetting = false;
            this.customizationActive = false;
            this.csEditingType = false;
            //TODO: Save button should be shown only when colors are changed
            this.showSaveButton = true;

            this.noteThemeChanged();
        },

        text_color_for_color: text_color_for_color,

        border_color_for_color: border_color_for_color,

        interpret_color: interpret_color,

        get colorArraysArray() {
            var result = null;
            if (this.selectedColorScheme.colors && this.selectedColorScheme.colors.length > 0)
                result = get_colors_as_nested_array(this.selectedColorScheme.colors, 32);
            else result = get_colors_as_nested_array(term_256_colors, 32);
            return result;
        },

        customizationActive: false,
        csEditingType: false,
        beginCustomizationWithSetting(setting) {
            if (!this.customizationActive) {
                this.customizationActive = true;
                this.selectedColorSetting = setting;
                this.csEditingType = setting;
                this.csUserVisibleTitle = user_visible_title_for_setting_name(setting);
            }
        },

        selectColorSetting(name) {
            this.selectedColorSetting = name;
            this.csEditingType = this.customizationActive ? name : "";
            this.csUserVisibleTitle = user_visible_title_for_setting_name(name);
            this.beginCustomizationWithSetting(name);
        },

        toggleCustomizationActive() {
            if (!this.customizationActive) {
                this.beginCustomizationWithSetting(this.selectedColorSetting || "command");
            } else {
                this.customizationActive = false;
                this.selectedColorSetting = "";
                this.csEditingType = "";
            }
        },

        changeSelectedTextColor(color) {
            this.selectedColorScheme[this.selectedColorSetting] = color;
            this.selectedColorScheme["colordata-" + this.selectedColorSetting].color = color;
            this.noteThemeChanged();
        },

        sampleTerminalBackgroundColors: [
            "white",
            "#" + solarized.base3,
            "#300",
            "#003",
            "#" + solarized.base03,
            "#232323",
            "#" + nord.nord0,
            "black",
        ],

        /* Array of FishColorSchemes */
        colorSchemes: [],
        isValidColor(col) {
            if (col == "normal") return true;
            var s = new Option().style;
            s.color = col;
            return !!s.color;
        },

        saveThemeButtonTitle: "Set Theme",

        noteThemeChanged() {
            this.saveThemeButtonTitle = "Set Theme";
        },

        async setTheme() {
            var settingNames = [
                "normal",
                "command",
                "keyword",
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
                "option",
                "valid_path",
                "autosuggestion",
                "user",
                "host",
                "host_remote",
                "history_current",
                "status",
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
                theme: this.selectedColorScheme["name"],
                colors: [],
            };
            for (var name of settingNames) {
                var selected;
                var realname = "colordata-" + name;
                // Skip colors undefined in the current theme
                // js is dumb - the empty string is false,
                // but we want that to mean unsetting a var.
                if (
                    !this.selectedColorScheme[realname] &&
                    this.selectedColorScheme[realname] !== ""
                ) {
                    continue;
                } else {
                    selected = this.selectedColorScheme[realname];
                }
                postdata.colors.push({
                    what: name,
                    color: selected,
                });
            }
            let resp = await fetch("set_color/", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(postdata),
            });
            if (resp.ok) {
                this.saveThemeButtonTitle = "Theme Set!";
            }
        },

        async init() {
            let schemes = await (await fetch("colors/")).json();

            for (var scheme of schemes) {
                var currentScheme = {
                    name: "Current",
                    colors: [],
                    preferred_background: "black",
                };
                currentScheme["name"] = scheme["theme"];
                if (scheme["name"]) currentScheme["name"] = scheme["name"];
                var data = scheme["colors"];
                if (scheme["preferred_background"]) {
                    if (this.isValidColor(scheme["preferred_background"])) {
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
                this.colorSchemes.push(currentScheme);
            }
            this.changeSelectedColorScheme(this.colorSchemes[0]);
        },
    }));

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

        async setNewPrompt(selectedPrompt) {
            let postdata = { fish_prompt: selectedPrompt.function };
            let resp = await fetch("set_prompt/", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(postdata)
            });
            if (resp.ok) {
                // Update attributes of current prompt and select it
                this.samplePrompts[0].demo = selectedPrompt.demo;
                this.samplePrompts[0].right = selectedPrompt.right;
                this.samplePrompts[0].function = selectedPrompt.function;
                this.samplePrompts[0].font_size = selectedPrompt.font_size;
                this.selectedPrompt = this.samplePrompts[0];

                // Note that we set it
                this.savePromptButtonTitle = "Prompt Set!";
            };
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
