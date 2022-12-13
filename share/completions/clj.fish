# ht Bob B@Clojurians Slack
# https://raw.githubusercontent.com/bobisageek/bb-scripts/main/clj-alils.clj
#
# Babashka code for listing all aliases and tools available from the current context
# based on :config-files from `clojure -Sdescribe`

set -l bb_helper '
(require \'[babashka.fs :as fs])

(def config (->> (with-out-str (babashka.tasks/clojure "-Sdescribe"))
                 clojure.edn/read-string))

(defn configs []
  (reverse (:config-files config)))

(defn aliases-from-config [path]
  (->> (clojure.edn/read-string (slurp path))
       :aliases
       keys
       (map name)
       sort))

(defn installed-tools
  []
  (as-> config $
    (:config-dir $)
    (str $ "/tools/")
    (fs/match $ "regex:.*\\\\.edn")
    (map fs/file-name $)
    (map #(fs/strip-ext % {:ext "edn"}) $)))

(defn aliases []
  (->> (configs)
       (mapcat aliases-from-config)
       dedupe))

(run! println
      (case (first *command-line-args*)
        "tools" (installed-tools)
        (aliases)))'

function __fish_clj_aliases -V bb_helper
    command -q bb; or return
    bb -e "$bb_helper"
end

function __fish_clj_tools -V bb_helper
    command -q bb; or return
    bb -e "$bb_helper" tools
end

complete -c clj -s X -x -r -k -a "(__fish_complete_list : __fish_clj_aliases)" -d "Use concatenated aliases to modify classpath or supply exec fn/args"
complete -c clj -s A -x -r -k -a "(__fish_complete_list : __fish_clj_aliases)" -d "Use concatenated aliases to modify classpath"
complete -c clj -s M -x -r -k -a "(__fish_complete_list : __fish_clj_aliases)" -d "Use concatenated aliases to modify classpath or supply main opts"
complete -c clj -s T -x -r -k -a "(__fish_complete_list : __fish_clj_tools)" -d "Invoke tool by name or via aliases ala -X"

complete -c clj -f -o Sdeps -r -d "Deps data to use as the last deps file to be merged"
complete -c clj -f -o Spath -d "Compute classpath and echo to stdout only"
complete -c clj -f -o Spom -d "Generate (or update) pom.xml with deps and paths"
complete -c clj -f -o Stree -d "Print dependency tree"
complete -c clj -f -o Scp -r -d "Do NOT compute or cache classpath, use this one instead"
complete -c clj -f -o Srepro -d "Ignore the ~/.clojure/deps.edn config file"
complete -c clj -f -o Sforce -d "Force recomputation of the classpath (don't use the cache)"
complete -c clj -f -o Sverbose -d "Print important path info to console"
complete -c clj -f -o Sdescribe -d "Print environment and command parsing info as data"
complete -c clj -f -o Sthreads -d "Set specific number of download threads"
complete -c clj -f -o Strace -d "Write a trace.edn file that traces deps expansion"

complete -c clj -k -f -s h -l help -d "Show help"
complete -c clj -f -s P -d "Prepare deps - download libs, cache classpath, but don't exec"
complete -c clj -s J -d "Pass opt through in java_opts, ex: -J-Xmx512m"
complete -c clj -s i -l init -r -d "Load a file or resource"
complete -c clj -f -s e -l eval -r -d "Eval exprs in string; print non-nil values"
complete -c clj -f -l report -d "Report uncaught exception to \"file\" (default), \"stderr\", or \"none\""
complete -c clj -f -o version -l version -d "Print the version to stdout and exit"
complete -c clj -f -s m -l main -d "Call the -main function from namespace w/args"
complete -c clj -f -s r -l repl -d "Run a repl"
complete -c clj -r -d "Run a script from a file or resource"
