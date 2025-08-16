# Older versions of delta do not have a --generate-completion option and will complain
# to stderr but emit nothing to stdout, making it safe (but a no-op) to source.
__fish_cache_sourced_completions delta --generate-completion fish 2>/dev/null
or delta --generate-completion fish 2>/dev/null | source
