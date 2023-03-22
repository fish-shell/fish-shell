deno completions fish | source

# complete deno task
set searchForDenoFilesCode '
// order matters
const denoFile = ["deno.json", "deno.jsonc", "package.json"];
for (const file of denoFile) {
  try {
    Deno.statSync(file);
    // file exists
    const props = file === "package.json" ? "scripts" : "tasks";
    console.log(
      Object.keys(JSON.parse(Deno.readTextFileSync(file))[props]).join("\n"),
    );
    break;
  } catch {}
}
'
complete -f -c deno -n "__fish_seen_subcommand_from task" -n "__fish_is_nth_token 2" -a "(deno eval '$searchForDenoFilesCode')"
