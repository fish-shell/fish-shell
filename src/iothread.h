// Handles IO that may hang.
#ifndef FISH_IOTHREAD_H
#define FISH_IOTHREAD_H

/// Runs a command on a thread.
///
/// \param handler The function to execute on a background thread. Accepts an arbitrary context
/// pointer, and returns an int, which is passed to the completionCallback.
/// \param completionCallback The function to execute on the main thread once the background thread
/// is complete. Accepts an int (the return value of handler) and the context.
/// \param context A arbitary context pointer to pass to the handler and completion callback.
/// \return A sequence number, currently not very useful.
int iothread_perform_base(int (*handler)(void *), void (*completionCallback)(void *, int),
                          void *context);

/// Gets the fd on which to listen for completion callbacks.
///
/// \return A file descriptor on which to listen for completion callbacks.
int iothread_port(void);

/// Services one iothread competion callback.
void iothread_service_completion(void);

/// Waits for all iothreads to terminate.
void iothread_drain_all(void);

/// Performs a function on the main thread, blocking until it completes.
int iothread_perform_on_main_base(int (*handler)(void *), void *context);

/// Helper templates.
template <typename T>
int iothread_perform(int (*handler)(T *), void (*completionCallback)(T *, int), T *context) {
    return iothread_perform_base((int (*)(void *))handler,
                                 (void (*)(void *, int))completionCallback,
                                 static_cast<void *>(context));
}

// Variant that takes no completion callback.
template <typename T>
int iothread_perform(int (*handler)(T *), T *context) {
    return iothread_perform_base((int (*)(void *))handler, (void (*)(void *, int))0,
                                 static_cast<void *>(context));
}

template <typename T>
int iothread_perform_on_main(int (*handler)(T *), T *context) {
    return iothread_perform_on_main_base((int (*)(void *))handler, (void *)(context));
}

#endif
