// The library for various signal related issues.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#include <unistd.h>

#include <csignal>
#include <cwchar>
#include <mutex>

#include "common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "global_safety.h"
#include "reader.h"
#include "signals.h"
#include "termsize.h"
#include "topic_monitor.h"
#include "wutil.h"  // IWYU pragma: keep

extern "C" {
void get_signals_with_handlers_ffi(sigset_t *set);
}
void get_signals_with_handlers(sigset_t *set) { get_signals_with_handlers_ffi(set); }

sigchecker_t::sigchecker_t(topic_t signal) : topic_(signal) {
    // Call check() to update our generation.
    check();
}

bool sigchecker_t::check() {
    auto &tm = topic_monitor_principal();
    generation_t gen = tm.generation_for_topic(topic_);
    bool changed = this->gen_ != gen;
    this->gen_ = gen;
    return changed;
}

void sigchecker_t::wait() const {
    auto &tm = topic_monitor_principal();
    generation_list_t gens = invalid_generations();
    gens.at_mut(topic_) = this->gen_;
    tm.check(&gens, true /* wait */);
}
