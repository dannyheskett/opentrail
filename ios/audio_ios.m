// The sound on iOS: an AVAudioEngine source node that pulls the game's mixed
// sound (the speaker, modern mode's music) from core.c's ring, 44.1 kHz
// stereo; the engine converts to the device's rate. Ambient: it mixes with
// other audio and goes quiet with the ring/silent switch. Restarted after an
// interruption (a call) and on coming back to the foreground.
#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#include "plat_ios.h"
#include "core.h"

static AVAudioEngine *s_engine;

static void start_engine(void) {
    if (!s_engine || s_engine.isRunning) return;
    [[AVAudioSession sharedInstance] setActive:YES error:nil];
    NSError *err = nil;
    if (![s_engine startAndReturnError:&err]) NSLog(@"opentrail: audio engine: %@", err);
}

void ios_audio_start(void) {
    dispatch_sync(dispatch_get_main_queue(), ^{
        AVAudioSession *session = [AVAudioSession sharedInstance];
        [session setCategory:AVAudioSessionCategoryAmbient error:nil];
        s_engine = [[AVAudioEngine alloc] init];
        AVAudioFormat *fmt = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                              sampleRate:CORE_AUDIO_RATE channels:2 interleaved:NO];
        AVAudioSourceNode *src = [[AVAudioSourceNode alloc] initWithFormat:fmt
            renderBlock:^OSStatus(BOOL *silence, const AudioTimeStamp *ts, AVAudioFrameCount frames, AudioBufferList *abl) {
                (void)ts;
                static int16_t pcm[2 * 4096];
                float *l = abl->mBuffers[0].mData;
                float *r = abl->mNumberBuffers > 1 ? abl->mBuffers[1].mData : NULL;
                AVAudioFrameCount done = 0;
                while (done < frames) {
                    int n = (int)(frames - done > 4096 ? 4096 : frames - done);
                    core_audio_read(pcm, n);
                    for (int i = 0; i < n; i++) {
                        l[done + i] = pcm[2 * i] / 32768.0f;
                        if (r) r[done + i] = pcm[2 * i + 1] / 32768.0f;
                    }
                    done += (AVAudioFrameCount)n;
                }
                *silence = NO;
                return noErr;
            }];
        [s_engine attachNode:src];
        [s_engine connect:src to:s_engine.mainMixerNode format:fmt];
        start_engine();
        NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
        [nc addObserverForName:AVAudioSessionInterruptionNotification object:nil queue:nil usingBlock:^(NSNotification *n) {
            NSNumber *type = n.userInfo[AVAudioSessionInterruptionTypeKey];
            if (type.unsignedIntegerValue == AVAudioSessionInterruptionTypeEnded) start_engine();
        }];
        [nc addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:nil usingBlock:^(NSNotification *n) {
            (void)n; start_engine();
        }];
        [nc addObserverForName:AVAudioEngineConfigurationChangeNotification object:s_engine queue:nil usingBlock:^(NSNotification *n) {
            (void)n; start_engine();                // a route change (headphones): the engine stops itself
        }];
    });
}
