deno completions fish | source

# complete deno task
set searchForDenoFilesCode '
const denoFile = ["deno.json", "deno.jsonc", "package.json"];
for (const file of denoFile) {
  try {
    Deno.statSync(file);
    // file exists
    if (file === "package.json") {
      console.log(
        Object.keys(JSON.parse(Deno.readTextFileSync(file)).scripts).join("\n"),
      );
    } else {
      console.log(
        Object.keys(JSON.parse(Deno.readTextFileSync(file)).tasks).join("\n"),
      );
    }
    break;
  } catch {
  }
}
'
complete -f -c deno -n "__fish_seen_subcommand_from task" -a "(deno eval '$searchForDenoFilesCode')"
