#include "expand.h"

/// \param input the string to tilde expand
void expand_tilde(wcstring &input, const environment_t &vars) {
    // Avoid needless COW behavior by ensuring we use const at.
    const wcstring &tmp = input;
    if (!tmp.empty() && tmp.at(0) == L'~') {
        input.at(0) = HOME_DIRECTORY;
        input = *expand_home_directory(input, vars);
    }
}
