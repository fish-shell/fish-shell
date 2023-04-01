#import <Foundation/Foundation.h>
#import <CoreServices/CoreServices.h>
#import <Carbon/Carbon.h>
#import <stdlib.h>
#import <stdio.h>
#import <unistd.h>
#import <errno.h>
#import <sys/types.h>
#import <sys/stat.h>


// The path to the command file, which we'll delete on death (if it exists)
static char s_command_path[PATH_MAX];

static void die(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fputc('\n', stderr);

    if (s_command_path[0] != '\0') {
        unlink(s_command_path);
    }

    exit(EXIT_FAILURE);
}

static void launch_ghoti_with_applescript(NSString *ghoti_binary_path)
{
    // load the script from a resource by fetching its URL from within our bundle
    NSString *path = [[NSBundle mainBundle] pathForResource:@"launch_ghoti" ofType:@"scpt"];
    if (! path) die("Couldn't get path to launch_ghoti.scpt");

    NSURL *url = [NSURL fileURLWithPath:path isDirectory:NO];
    if (! url) die("Couldn't get URL to launch_ghoti.scpt");

    NSDictionary *errors = nil;
    NSAppleScript *appleScript = [[NSAppleScript alloc] initWithContentsOfURL:url error:&errors];
    if (! appleScript) die("Couldn't load AppleScript");

    // create the first parameter
    NSAppleEventDescriptor *firstParameter =
            [NSAppleEventDescriptor descriptorWithString:ghoti_binary_path];

    // create and populate the list of parameters (in our case just one)
    NSAppleEventDescriptor *parameters = [NSAppleEventDescriptor listDescriptor];
    [parameters insertDescriptor:firstParameter atIndex:1];

    // create the AppleEvent target
    ProcessSerialNumber psn = {0, kCurrentProcess};
    NSAppleEventDescriptor *target =
    [NSAppleEventDescriptor
            descriptorWithDescriptorType:typeProcessSerialNumber
            bytes:&psn
            length:sizeof(ProcessSerialNumber)];

    // create an NSAppleEventDescriptor with the script's method name to call,
    // this is used for the script statement: "on show_message(user_message)"
    // Note that the routine name must be in lower case.
    NSAppleEventDescriptor *handler = [NSAppleEventDescriptor descriptorWithString:
            [@"launch_ghoti" lowercaseString]];

    // create the event for an AppleScript subroutine,
    // set the method name and the list of parameters
    NSAppleEventDescriptor *event =
            [NSAppleEventDescriptor appleEventWithEventClass:kASAppleScriptSuite
                    eventID:kASSubroutineEvent
                    targetDescriptor:target
                    returnID:kAutoGenerateReturnID
    transactionID:kAnyTransactionID];
    [event setParamDescriptor:handler forKeyword:keyASSubroutineName];
    [event setParamDescriptor:parameters forKeyword:keyDirectObject];

    // call the event in AppleScript
    if (![appleScript executeAppleEvent:event error:&errors])
    {
        // report any errors from 'errors'
        NSLog(@"Oops: %@", errors);
    }

    [appleScript release];
}

/* This approach asks Terminal to open a script that we control */
int main(void) {

    @autoreleasepool {
        /* Get the ghoti executable. Make sure it's absolute. */
        NSURL *ghoti_executable = [[NSBundle mainBundle] URLForResource:@"ghoti" withExtension:@""
                                                          subdirectory:@"base/usr/local/bin"];
        if (! ghoti_executable)
            die("Could not find ghoti executable in bundle");

        launch_ghoti_with_applescript([ghoti_executable path]);
    }

    /* If we succeeded, it will clean itself up */
    return 0;
}
