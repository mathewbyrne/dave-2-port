#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#include <mach/mach_time.h>

#include <stdlib.h>
#include <string.h>

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
    NSString *chars = [event charactersIgnoringModifiers];
    if ([chars length] == 0) {
        return;
    }

    unichar c = [chars characterAtIndex:0];
    if (c == 27) {
        game_input.toggle_pause = 1;
    } else if (c == 13 || c == 3 || c == ' ') {
        game_input.next_scene = 1;
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

        while (g_running) {
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

            if (!g_backbuffer.memory || g_backbuffer.width != width || g_backbuffer.height != height) {
                macos_resize_backbuffer(&g_backbuffer, width, height);
            }

            uint64_t counter_now = mach_absolute_time();
            float dt = macos_dt_seconds(last_counter, counter_now, timebase);
            last_counter = counter_now;

            game_tick(&game_state, &game_input, dt, (uint32_t *)g_backbuffer.memory, g_backbuffer.width,
                      g_backbuffer.height);
            game_input.toggle_pause = 0;
            game_input.next_scene   = 0;

            [view setNeedsDisplay:YES];
            [view displayIfNeeded];
            [NSApp updateWindows];
        }

        free(g_backbuffer.memory);
        g_backbuffer.memory = 0;
    }

    return 0;
}
