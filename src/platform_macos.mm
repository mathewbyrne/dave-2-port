#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <CoreGraphics/CoreGraphics.h>
#include <mach/mach_time.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app.cpp"

typedef struct {
    void *memory;
    int   width;
    int   height;
    int   pitch;
} macos_backbuffer_t;

static macos_backbuffer_t g_backbuffer = {0};
static int                g_running    = 1;
static game_state_t       game_state;
static game_input_t       game_input;

static void macos_resize_backbuffer(macos_backbuffer_t *buffer, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    size_t new_pitch = (size_t)width * 4u;
    size_t new_size  = new_pitch * (size_t)height;
    void  *new_mem   = realloc(buffer->memory, new_size);
    if (!new_mem) {
        return;
    }

    buffer->memory = new_mem;
    buffer->width  = width;
    buffer->height = height;
    buffer->pitch  = (int)new_pitch;
}

@interface DaveView : NSView
@end

@implementation DaveView

- (void)setButton:(game_button_t *)button isDown:(BOOL)isDown {
    if (!button) {
        return;
    }

    if (isDown && !button->is_down) {
        button->pressed_this_frame = 1;
    }
    button->is_down = isDown ? 1 : 0;
}

- (BOOL)handleAppQuitShortcut:(NSEvent *)event {
    NSEventModifierFlags mods = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;
    if ((mods & NSEventModifierFlagCommand) == 0) {
        return NO;
    }

    NSString *chars = [[event charactersIgnoringModifiers] lowercaseString];
    if ([chars length] == 0) {
        return NO;
    }

    if ([chars characterAtIndex:0] == 'q') {
        g_running = 0;
        return YES;
    }

    return NO;
}

- (void)updateMovementKey:(unichar)c isDown:(BOOL)isDown {
    if (c == NSLeftArrowFunctionKey) {
        game_input.direction_keys.move_left = isDown ? 1 : 0;
    } else if (c == NSRightArrowFunctionKey) {
        game_input.direction_keys.move_right = isDown ? 1 : 0;
    } else if (c == NSUpArrowFunctionKey) {
        game_input.direction_keys.move_up = isDown ? 1 : 0;
    } else if (c == NSDownArrowFunctionKey) {
        game_input.direction_keys.move_down = isDown ? 1 : 0;
    } else {
        return;
    }
}

- (void)triggerMenuKey:(unichar)c {
    if (c == NSF1FunctionKey) {
        game_input.menu_help = 1;
    } else if (c == NSF2FunctionKey) {
        game_input.menu_sound_toggle = 1;
    } else if (c == NSF3FunctionKey) {
        game_input.menu_keyboard_config = 1;
    } else if (c == NSF5FunctionKey) {
        game_input.menu_reset_game = 1;
    } else if (c == 27) {
        game_input.menu_quit_game = 1;
        g_running                 = 0;
    } else if (c == NSTabCharacter) {
        game_input.menu_status = 1;
    }
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    [[self window] makeFirstResponder:self];
}

- (void)keyDown:(NSEvent *)event {
    if ([self handleAppQuitShortcut:event]) {
        return;
    }

    NSString *chars = [event charactersIgnoringModifiers];
    if ([chars length] == 0) {
        return;
    }

    unichar c = [chars characterAtIndex:0];
    [self updateMovementKey:c isDown:YES];
    [self triggerMenuKey:c];
}

- (BOOL)performKeyEquivalent:(NSEvent *)event {
    if ([self handleAppQuitShortcut:event]) {
        return YES;
    }
    return [super performKeyEquivalent:event];
}

- (void)keyUp:(NSEvent *)event {
    NSString *chars = [event charactersIgnoringModifiers];
    if ([chars length] == 0) {
        return;
    }

    unichar c = [chars characterAtIndex:0];
    [self updateMovementKey:c isDown:NO];
}

- (void)flagsChanged:(NSEvent *)event {
    NSEventModifierFlags mods = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;

    switch ([event keyCode]) {
    case kVK_Control:
    case kVK_RightControl:
        [self setButton:&game_input.action_1 isDown:((mods & NSEventModifierFlagControl) != 0)];
        break;

    case kVK_Option:
    case kVK_RightOption:
        [self setButton:&game_input.action_2 isDown:((mods & NSEventModifierFlagOption) != 0)];
        break;

    default:
        [super flagsChanged:event];
        break;
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;

    if (!g_backbuffer.memory || g_backbuffer.width <= 0 || g_backbuffer.height <= 0) {
        return;
    }

    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    if (!context) {
        return;
    }

    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    if (!colorspace) {
        return;
    }

    CGDataProviderRef provider =
        CGDataProviderCreateWithData(0, g_backbuffer.memory, (size_t)g_backbuffer.pitch * (size_t)g_backbuffer.height, 0);
    if (!provider) {
        CGColorSpaceRelease(colorspace);
        return;
    }

    CGImageRef image =
        CGImageCreate((size_t)g_backbuffer.width, (size_t)g_backbuffer.height, 8, 32,
                      (size_t)g_backbuffer.pitch, colorspace,
                      kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst, provider, 0, false,
                      kCGRenderingIntentDefault);

    if (image) {
        CGContextSetInterpolationQuality(context, kCGInterpolationNone);
        CGRect bounds = [self bounds];
        CGContextSaveGState(context);
        CGContextTranslateCTM(context, 0.0, bounds.size.height);
        CGContextScaleCTM(context, 1.0, -1.0);
        CGContextDrawImage(context, bounds, image);
        CGContextRestoreGState(context);
        CGImageRelease(image);
    }

    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorspace);
}

@end

@interface DaveWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation DaveWindowDelegate

- (BOOL)windowShouldClose:(id)sender {
    (void)sender;
    g_running = 0;
    return YES;
}

@end

static float macos_dt_seconds(uint64_t start, uint64_t end, mach_timebase_info_data_t tb) {
    uint64_t elapsed = end - start;
    double   nanos = ((double)elapsed * (double)tb.numer) / (double)tb.denom;
    return (float)(nanos / 1000000000.0);
}

static void macos_sleep_seconds(double seconds) {
    if (seconds <= 0.0) {
        return;
    }

    struct timespec req = {0, 0};
    req.tv_sec          = (time_t)seconds;
    req.tv_nsec         = (long)((seconds - (double)req.tv_sec) * 1000000000.0);
    nanosleep(&req, 0);
}

int main(int argc, const char **argv) {
    (void)argc;
    (void)argv;

    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect window_frame = NSMakeRect(0, 0, 960, 600);
        NSWindow *window = [[NSWindow alloc]
            initWithContentRect:window_frame
                      styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                 NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                        backing:NSBackingStoreBuffered
                          defer:NO];

        DaveView *view = [[DaveView alloc] initWithFrame:[[window contentView] bounds]];
        [view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
        [window setContentView:view];
        [window setTitle:@"Dangerous Dave 2 (Port)"];
        [window makeKeyAndOrderFront:nil];

        DaveWindowDelegate *window_delegate = [[DaveWindowDelegate alloc] init];
        [window setDelegate:window_delegate];

        [NSApp activateIgnoringOtherApps:YES];

        game_init(&game_state);

        mach_timebase_info_data_t timebase = {0};
        mach_timebase_info(&timebase);
        uint64_t last_counter = mach_absolute_time();
        const double target_frame_seconds = 1.0 / 70.0;

        while (g_running) {
            @autoreleasepool {
                for (;;) {
                    NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                        untilDate:[NSDate distantPast]
                                                           inMode:NSDefaultRunLoopMode
                                                          dequeue:YES];
                    if (!event) {
                        break;
                    }
                    [NSApp sendEvent:event];
                }

                NSRect  bounds = [view bounds];
                CGFloat scale  = [window backingScaleFactor];
                int     width  = (int)(bounds.size.width * scale);
                int     height = (int)(bounds.size.height * scale);
                if (width <= 0 || height <= 0) {
                    width  = 320;
                    height = 200;
                }

                if (!g_backbuffer.memory || g_backbuffer.width != width ||
                    g_backbuffer.height != height) {
                    macos_resize_backbuffer(&g_backbuffer, width, height);
                }

                uint64_t counter_now = mach_absolute_time();
                float dt             = macos_dt_seconds(last_counter, counter_now, timebase);
                last_counter         = counter_now;

                game_tick(&game_state, &game_input, dt, (uint32_t *)g_backbuffer.memory,
                          g_backbuffer.width, g_backbuffer.height);
                if (game_input.menu_reset_game) {
                    game_init(&game_state);
                }
                game_input.action_1.pressed_this_frame = 0;
                game_input.action_2.pressed_this_frame = 0;
                game_input.menu_help                   = 0;
                game_input.menu_sound_toggle           = 0;
                game_input.menu_keyboard_config        = 0;
                game_input.menu_reset_game             = 0;
                game_input.menu_quit_game              = 0;
                game_input.menu_status                 = 0;

                [view setNeedsDisplay:YES];
                [view displayIfNeeded];
                [NSApp updateWindows];

                uint64_t frame_end   = mach_absolute_time();
                double   frame_time  = (double)macos_dt_seconds(counter_now, frame_end, timebase);
                double   sleep_time  = target_frame_seconds - frame_time;
                macos_sleep_seconds(sleep_time);
            }
        }

        free(g_backbuffer.memory);
        g_backbuffer.memory = 0;
    }

    return 0;
}
