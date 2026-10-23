// The iOS app: UIKit with a CAMetalLayer view, no storyboard or scene
// manifest (the build is clang and a plist, as the family's). The game runs
// on its own thread (ios_game_main, host_ios.c), owning its loop as on every
// platform; the main thread publishes the screen, the taps and the
// foreground state (plat_ios.h), and its display link ticks the clock the
// game thread waits on. Only the game thread uses Metal.
#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include "plat_ios.h"

void ios_metal_attach(CAMetalLayer *layer);

@interface OTMetalView : UIView
@property (nonatomic) BOOL started;
@end

@implementation OTMetalView

+ (Class)layerClass { return [CAMetalLayer class]; }

- (void)didMoveToWindow {
    [super didMoveToWindow];
    if (!self.window || self.started) return;
    self.started = YES;
    self.multipleTouchEnabled = NO;
    ios_metal_attach((CAMetalLayer *)self.layer);
    [self updateDrawableSize];
    CADisplayLink *link = [CADisplayLink displayLinkWithTarget:self selector:@selector(onFrame:)];
    link.preferredFramesPerSecond = 60;
    [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    NSThread *t = [[NSThread alloc] initWithBlock:^{ ios_game_main(); }];
    t.stackSize = 16 * 1024 * 1024;                 // the engine's longjmp-based flow keeps deep frames
    t.name = @"opentrail.game";
    [t start];
}

- (void)layoutSubviews { [super layoutSubviews]; [self updateDrawableSize]; }
- (void)safeAreaInsetsDidChange { [super safeAreaInsetsDidChange]; [self updateDrawableSize]; }

// The drawable in device pixels, and the safe rectangle the game draws in.
- (void)updateDrawableSize {
    CAMetalLayer *layer = (CAMetalLayer *)self.layer;
    CGFloat scale = self.window ? self.window.screen.scale : [UIScreen mainScreen].scale;
    self.contentScaleFactor = scale;
    layer.contentsScale = scale;
    CGSize full = CGSizeMake(MAX(1, self.bounds.size.width * scale), MAX(1, self.bounds.size.height * scale));
    layer.drawableSize = full;
    UIEdgeInsets in = self.safeAreaInsets;
    int sx = (int)(in.left * scale), sy = (int)(in.top * scale);
    int sw = (int)full.width - sx - (int)(in.right * scale), sh = (int)full.height - sy - (int)(in.bottom * scale);
    plat_ios_set_screen((int)full.width, (int)full.height, sx, sy, sw, sh);
}

- (void)onFrame:(CADisplayLink *)link { (void)link; plat_ios_tick(); }

// A tap is a touch's beginning, in device pixels.
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    for (UITouch *t in touches) {
        CGPoint p = [t locationInView:self];
        plat_ios_push_tap((float)(p.x * self.contentScaleFactor), (float)(p.y * self.contentScaleFactor));
    }
}

@end

@interface OTViewController : UIViewController
@end

@implementation OTViewController
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (BOOL)shouldAutorotate { return YES; }
- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeAll; }
@end

@interface OTAppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation OTAppDelegate

- (UIInterfaceOrientationMask)application:(UIApplication *)application supportedInterfaceOrientationsForWindow:(UIWindow *)window {
    (void)application; (void)window;
    return UIInterfaceOrientationMaskLandscape;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    (void)application; (void)options;
    CGRect bounds = [UIScreen mainScreen].bounds;
    self.window = [[UIWindow alloc] initWithFrame:bounds];
    OTViewController *vc = [[OTViewController alloc] init];
    vc.view = [[OTMetalView alloc] initWithFrame:bounds];
    self.window.rootViewController = vc;
    [self.window makeKeyAndVisible];
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:nil
                usingBlock:^(NSNotification *n) { (void)n; plat_ios_set_active(true); }];
    [nc addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:nil
                usingBlock:^(NSNotification *n) { (void)n; plat_ios_set_active(false); }];
    return YES;
}

@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([OTAppDelegate class]));
    }
}
