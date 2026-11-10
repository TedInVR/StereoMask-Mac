#import <AppKit/AppKit.h>
// Make intentional launch active, including a build script launching the executable.
void activateMacApplication() {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    NSBundle *bundle = [NSBundle mainBundle];
    NSString *iconFile = [bundle objectForInfoDictionaryKey:@"CFBundleIconFile"];
    NSString *iconPath = iconFile.length ? [bundle pathForResource:iconFile ofType:nil] : nil;
    if (iconPath) {
        NSImage *icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
        if (icon) [NSApp setApplicationIconImage:icon];
#if !__has_feature(objc_arc)
        [icon release];
#endif
    }
    [NSApp activateIgnoringOtherApps:YES];
}
