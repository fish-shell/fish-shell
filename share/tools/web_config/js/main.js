// TODO double check all defaults for nullability etc
document.addEventListener("alpine:init", () => {
    window.Alpine.data("colors", () => ({
        // TODO make null
        selectedColorScheme: {
            by_color_theme: {
                unknown: {
                    colors: {
                    },
                    colordata: {
                    },
                },
            },
        },
        terminalColorTheme: "dark",
        actualTerminalBackgroundColor: "black",
        overriddenTerminalBackgroundColor: undefined,
        showSaveButton: false,
        csUserVisibleTitle: "",
        selectedColorSetting: false,

        text_color_for_color: window.text_color_for_color,

        currentColorScheme() {
            let scheme = this.selectedColorScheme.by_color_theme;
            if (scheme[this.terminalColorTheme] !== undefined) {
                return scheme[this.terminalColorTheme];
            }
            return scheme['unknown'];
        },

        changedSelectedColorSchemeOrColorTheme() {
            this.selectedColorScheme.name_with_color_theme =
                color_scheme_respects_color_theme(this.selectedColorScheme)
                ? `${this.selectedColorScheme.name} (${this.terminalColorTheme} color theme)`
                : this.selectedColorScheme.name;
        },

        changeSelectedColorScheme(newScheme) {
            this.terminalColorTheme = newScheme.terminal_color_theme ||
                (newScheme.by_color_theme.light !== undefined ? "light" : "dark");
            this.selectedColorScheme = newScheme;
            let preferred_background = this.currentColorScheme().preferred_background;
            if (preferred_background) {
                this.actualTerminalBackgroundColor = preferred_background;
            }
            this.overriddenTerminalBackgroundColor = undefined;
            this.changedSelectedColorSchemeOrColorTheme();
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
            if (this.currentColorScheme().colors &&
                this.currentColorScheme().colors.length > 0)
                result = get_colors_as_nested_array(this.currentColorScheme().colors, 32);
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
                this.beginCustomizationWithSetting(this.selectedColorSetting || "fish_color_command");
            } else {
                this.customizationActive = false;
                this.selectedColorSetting = "";
                this.csEditingType = "";
            }
        },

        changeSelectedTextColor(color) {
            this.currentColorScheme().colors[this.selectedColorSetting] = color;
            this.currentColorScheme().colordata[this.selectedColorSetting].color = color;
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

        saveThemeButtonTitle: "Set Theme",

        noteThemeChanged() {
            this.saveThemeButtonTitle = "Set Theme";
        },

        async setTheme() {
            var postdata = {
                theme: this.selectedColorScheme["name"],
                colors: [],
            };
            for (let [name, value] of Object.entries(this.currentColorScheme().colordata)) {
                postdata.colors.push({
                    what: name,
                    color: value,
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
                    by_color_theme: {
                        'unknown': {
                            colors: {},
                            colordata: {},
                        }
                    },
                };
                let name = scheme["theme"];
                if (scheme["name"]) name = scheme["name"];
                currentScheme["name"] = name;
                currentScheme["name_with_color_theme"] = name;
                currentScheme["name_with_color_themes"] = name +
                    (color_scheme_respects_color_theme(scheme) ? " (light/dark)" : "");
                if (scheme["url"]) currentScheme["url"] = scheme["url"];
                var by_theme = scheme["by_color_theme"];
                for (const [color_theme, data] of Object.entries(by_theme)) {
                    let outdata = currentScheme.by_color_theme[color_theme] = {
                        colors: {},
                        colordata: {},
                        preferred_background: "black",
                    };
                    let preferredBackground = data["preferred_background"];
                    if (preferredBackground) {
                        outdata.preferred_background =
                            preferredBackground.match(/^([0-9a-fA-F]{3}|[0-9a-fA-F]{6})$/)
                            ? "#"+preferredBackground : preferredBackground;
                    }
                    let colors = data.colors;
                    for (var i in colors) {
                        outdata.colors[colors[i].name] = interpret_color(colors[i].color).replace(/#/, "");
                        // HACK: For some reason the colors array is cleared later
                        // So we cheesily encode the actual objects in colordata, so we can send them.
                        // TODO: We should switch to keeping the objects, and also displaying them
                        // with underlines and such.
                        outdata.colordata[colors[i].name] = colors[i];
                    }
                }
                this.colorSchemes.push(currentScheme);
            }
            this.changeSelectedColorScheme(JSON.parse(JSON.stringify(this.colorSchemes[0])));

            this.$watch('overriddenTerminalBackgroundColor', (newValue) => {
                if (newValue === undefined)
                    return;
                // see sampleTerminalBackgroundColors
                const isLight = newValue === 'white' || newValue === "#"+solarized.base3;
                this.terminalColorTheme = isLight ? 'light' : 'dark';
                this.actualTerminalBackgroundColor = newValue;
                this.changedSelectedColorSchemeOrColorTheme();
            });
        },
    }));

    window.Alpine.data("prompt", () => ({
        selectedPrompt: null,
        showSaveButton: true,
        samplePrompts: [],
        savePromptButtonTitle: "Set Prompt",
        // where is this initialized in the original code?
        actualTerminalBackgroundColor: "black",
        overriddenTerminalBackgroundColor: undefined,

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

function color_scheme_respects_color_theme(colorScheme) {
    return colorScheme.by_color_theme.light !== undefined &&
        colorScheme.by_color_theme.dark !== undefined;
}

function default_color_scheme(colorScheme) {
    if (colorScheme.by_color_theme.light !== undefined)
        return colorScheme.by_color_theme.light;
    else if (colorScheme.by_color_theme.dark !== undefined)
        return colorScheme.by_color_theme.dark;
    return colorScheme.by_color_theme.unknown;
}
