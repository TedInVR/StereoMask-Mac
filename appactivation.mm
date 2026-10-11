#import <AppKit/AppKit.h>
// Make intentional launch active, including a build script launching the executable.
void activateMacApplication() {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
}
