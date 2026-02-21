# RUN: %fish -N %s

set -f FISH_ENV_TEST_2 def
set -gx FISH_ENV_TEST_2 abc
env | string match FISH_ENV_TEST_\*
# No output

set -ef FISH_ENV_TEST_2
env | string match FISH_ENV_TEST_\*
# CHECK: FISH_ENV_TEST_2=abc

set -e FISH_ENV_TEST_2
env | string match FISH_ENV_TEST_\*
# No output
