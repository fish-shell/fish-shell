# Ported from https://github.com/nushell/nu_scripts/pull/577/files PR
function __fish_json_to_schema -d "Converts specific JSON to JSON schema" -a schema url json
    if string match -v -r -q -- '^[47]$' "$schema"
        echo "'http://json-schema.org/draft-0$schema/schema' is not supported yet." >&2
        return
    end

    if test -z "$url"
        echo "Documentation url can't be empty." >&2
        return
    end

    set schema "http://json-schema.org/draft-0$schema/schema"

    echo "$json" | jq 'def to_title: . | gsub("(?<x>[A-Z])"; " \(.x)") | ascii_downcase;
    def to_description: . | to_title + "\n" + $url;

    def to_singular:
        if . | test("^[aeiouy]"; "i") then
            . | sub("^(?<x>.*?)s\n"; "An \(.x)\n")
        else
            . | sub("^(?<x>.*?)s\n"; "A \(.x)\n")
        end;

    def wrap: {
        type: "object",
        properties: with_entries(
            if .value | type != "object" then
                {
                    key,
                    value: ({
                        title: .key | to_title,
                        description: .key | to_description,
                        type: (.value | type),

                    } +
                    if .key | test("width|height|size") then
                        { minimum: 0 }
                    else
                        {}
                    end +
                    if .value | type == "array" then
                        {
                            uniqueItems: true,
                            items: {
                                description: .key | to_description | to_singular
                            }
                        } *
                        if .value | length > 0 then
                            {
                                items: {
                                    type: .value[0] | type
                                }
                            }
                        else
                            {}
                        end
                    else
                        {}
                    end +
                    if .key | test("^([Mm]y|[Ss]ample|[Ee]xample)[A-Z]") then
                        { examples: [.value] }
                    else
                        { default: .value }
                    end)
                }
            else
                .value = {
                    title: .key | to_title,
                    description: .key | to_description
                } + (.value | wrap) + {
                    additionalProperties: false
                }
            end)
        };

    {
        "$schema": $schema,
        title: "config",
        description: "A config"
    } + (. | wrap)' --arg url "$url" --arg schema "$schema"
end
