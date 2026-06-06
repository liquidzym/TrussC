#include "TCXIOSBridgeSupport.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMotion/CoreMotion.h>
#import <UIKit/UIKit.h>

#include <atomic>

@interface TCXIOSAudioObserver : NSObject
@end

@implementation TCXIOSAudioObserver

- (instancetype)init {
    self = [super init];
    if (self) {
        NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
        [center addObserver:self selector:@selector(handleInterruption:)
                       name:AVAudioSessionInterruptionNotification object:nil];
        [center addObserver:self selector:@selector(handleRouteChange:)
                       name:AVAudioSessionRouteChangeNotification object:nil];
    }
    return self;
}

- (void)handleInterruption:(NSNotification*)notification {
    NSDictionary* info = notification.userInfo;
    NSInteger typeValue = [info[AVAudioSessionInterruptionTypeKey] integerValue];
    NSInteger optionValue = [info[AVAudioSessionInterruptionOptionKey] integerValue];
    tcx::ios::AudioInterruption interruption;
    interruption.type = (typeValue == AVAudioSessionInterruptionTypeEnded)
        ? tcx::ios::AudioInterruptionType::Ended
        : tcx::ios::AudioInterruptionType::Began;
    interruption.shouldResume = (optionValue & AVAudioSessionInterruptionOptionShouldResume) != 0;
    tcx::ios::detail::dispatchAudioInterruption(interruption);
}

- (void)handleRouteChange:(NSNotification*)notification {
    NSDictionary* info = notification.userInfo;
    NSInteger reasonValue = [info[AVAudioSessionRouteChangeReasonKey] integerValue];
    NSString* reason = @"unknown";
    switch (reasonValue) {
        case AVAudioSessionRouteChangeReasonNewDeviceAvailable: reason = @"new device available"; break;
        case AVAudioSessionRouteChangeReasonOldDeviceUnavailable: reason = @"old device unavailable"; break;
        case AVAudioSessionRouteChangeReasonCategoryChange: reason = @"category change"; break;
        case AVAudioSessionRouteChangeReasonOverride: reason = @"override"; break;
        case AVAudioSessionRouteChangeReasonWakeFromSleep: reason = @"wake from sleep"; break;
        case AVAudioSessionRouteChangeReasonNoSuitableRouteForCategory: reason = @"no suitable route"; break;
        case AVAudioSessionRouteChangeReasonRouteConfigurationChange: reason = @"route configuration change"; break;
        default: break;
    }
    tcx::ios::detail::dispatchAudioRouteChange({TCXIOSStr(reason), tcx::ios::detail::platformCurrentAudioRoute()});
}

@end



namespace tcx::ios::detail {

namespace {

NSString* ns(const std::string& value) { return TCXIOSNs(value); }
std::string str(NSString* value) { return TCXIOSStr(value); }
Error nativeError(NSError* error, const std::string& fallback) { return TCXIOSNativeError(error, fallback); }

template <typename T>
void finish(Completion<T> done, Result<T> result) {
    TCXIOSFinish(std::move(done), std::move(result));
}

void finishVoid(Completion<void> done, Result<void> result) {
    TCXIOSFinishVoid(std::move(done), std::move(result));
}

UIViewController* presenter() { return TCXIOSPresenter(); }
UIWindow* activeWindow() { return TCXIOSActiveWindow(); }

} // namespace

NSString* audioCategory(AudioCategory category) {
    switch (category) {
        case AudioCategory::Ambient: return AVAudioSessionCategoryAmbient;
        case AudioCategory::SoloAmbient: return AVAudioSessionCategorySoloAmbient;
        case AudioCategory::Playback: return AVAudioSessionCategoryPlayback;
        case AudioCategory::Record: return AVAudioSessionCategoryRecord;
        case AudioCategory::PlayAndRecord: return AVAudioSessionCategoryPlayAndRecord;
        case AudioCategory::MultiRoute: return AVAudioSessionCategoryMultiRoute;
    }
    return AVAudioSessionCategoryPlayback;
}

NSString* audioMode(AudioMode mode) {
    switch (mode) {
        case AudioMode::Default: return AVAudioSessionModeDefault;
        case AudioMode::VoiceChat: return AVAudioSessionModeVoiceChat;
        case AudioMode::VideoRecording: return AVAudioSessionModeVideoRecording;
        case AudioMode::Measurement: return AVAudioSessionModeMeasurement;
    }
    return AVAudioSessionModeDefault;
}

AVAudioSessionCategoryOptions audioOptions(const AudioSessionConfig& config) {
    AVAudioSessionCategoryOptions options = 0;
    if (config.mixWithOthers) options |= AVAudioSessionCategoryOptionMixWithOthers;
    if (config.allowBluetooth || config.allowBluetoothHFP) options |= AVAudioSessionCategoryOptionAllowBluetoothHFP;
    if (config.allowBluetoothA2DP) options |= AVAudioSessionCategoryOptionAllowBluetoothA2DP;
    if (config.defaultToSpeaker) options |= AVAudioSessionCategoryOptionDefaultToSpeaker;
    return options;
}


CMMotionManager* motionManager() {
    static CMMotionManager* manager = [[CMMotionManager alloc] init];
    return manager;
}

std::atomic_bool gMotionRunning{false};



TCXIOSAudioObserver* TCXIOSAudioObserverInstance() {
    static TCXIOSAudioObserver* observer = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        observer = [[TCXIOSAudioObserver alloc] init];
    });
    return observer;
}

void platformSetAudioCategory(const AudioSessionConfig& config, Completion<void> done) {
    TCXIOSAudioObserverInstance();
    NSError* error = nil;
    AVAudioSession* session = [AVAudioSession sharedInstance];
    if (config.preferredSampleRate > 0.0 &&
        ![session setPreferredSampleRate:config.preferredSampleRate error:&error]) {
        finishVoid(std::move(done), Result<void>::failure(nativeError(error, "Audio session preferred sample rate failed.")));
        return;
    }
    if (config.preferredIOBufferDuration > 0.0 &&
        ![session setPreferredIOBufferDuration:config.preferredIOBufferDuration error:&error]) {
        finishVoid(std::move(done), Result<void>::failure(nativeError(error, "Audio session preferred IO buffer duration failed.")));
        return;
    }
    BOOL ok = [session setCategory:audioCategory(config.category)
                              mode:audioMode(config.mode)
                           options:audioOptions(config)
                             error:&error];
    if (ok) {
        finishVoid(std::move(done), Result<void>::success());
    } else {
        finishVoid(std::move(done), Result<void>::failure(nativeError(error, "Audio session category failed.")));
    }
}

void platformSetAudioActive(bool active, Completion<void> done) {
    TCXIOSAudioObserverInstance();
    NSError* error = nil;
    BOOL ok = [[AVAudioSession sharedInstance] setActive:active error:&error];
    if (ok) {
        finishVoid(std::move(done), Result<void>::success());
    } else {
        finishVoid(std::move(done), Result<void>::failure(nativeError(error, "Audio session activation failed.")));
    }
}

void platformOverrideAudioOutputToSpeaker(bool enabled, Completion<void> done) {
    TCXIOSAudioObserverInstance();
    NSError* error = nil;
    AVAudioSessionPortOverride override = enabled
        ? AVAudioSessionPortOverrideSpeaker
        : AVAudioSessionPortOverrideNone;
    BOOL ok = [[AVAudioSession sharedInstance] overrideOutputAudioPort:override error:&error];
    if (ok) {
        finishVoid(std::move(done), Result<void>::success());
    } else {
        finishVoid(std::move(done), Result<void>::failure(nativeError(error, "Audio output route override failed.")));
    }
}

AudioRoute platformCurrentAudioRoute() {
    TCXIOSAudioObserverInstance();
    AudioRoute route;
    AVAudioSessionRouteDescription* nativeRoute = [[AVAudioSession sharedInstance] currentRoute];
    for (AVAudioSessionPortDescription* input in nativeRoute.inputs) {
        route.inputs.push_back(str(input.portName));
    }
    for (AVAudioSessionPortDescription* output in nativeRoute.outputs) {
        route.outputs.push_back(str(output.portName));
    }
    return route;
}


bool platformHapticImpact(HapticImpactStyle style) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIImpactFeedbackStyle nativeStyle = UIImpactFeedbackStyleMedium;
        switch (style) {
            case HapticImpactStyle::Light: nativeStyle = UIImpactFeedbackStyleLight; break;
            case HapticImpactStyle::Medium: nativeStyle = UIImpactFeedbackStyleMedium; break;
            case HapticImpactStyle::Heavy: nativeStyle = UIImpactFeedbackStyleHeavy; break;
            case HapticImpactStyle::Soft: nativeStyle = UIImpactFeedbackStyleSoft; break;
            case HapticImpactStyle::Rigid: nativeStyle = UIImpactFeedbackStyleRigid; break;
        }
        UIImpactFeedbackGenerator* generator = [[UIImpactFeedbackGenerator alloc] initWithStyle:nativeStyle];
        [generator prepare];
        [generator impactOccurred];
    });
    return true;
}

bool platformHapticSelection() {
    dispatch_async(dispatch_get_main_queue(), ^{
        UISelectionFeedbackGenerator* generator = [[UISelectionFeedbackGenerator alloc] init];
        [generator prepare];
        [generator selectionChanged];
    });
    return true;
}

bool platformHapticNotification(HapticNotificationType type) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UINotificationFeedbackType nativeType = UINotificationFeedbackTypeSuccess;
        switch (type) {
            case HapticNotificationType::Success: nativeType = UINotificationFeedbackTypeSuccess; break;
            case HapticNotificationType::Warning: nativeType = UINotificationFeedbackTypeWarning; break;
            case HapticNotificationType::Error: nativeType = UINotificationFeedbackTypeError; break;
        }
        UINotificationFeedbackGenerator* generator = [[UINotificationFeedbackGenerator alloc] init];
        [generator prepare];
        [generator notificationOccurred:nativeType];
    });
    return true;
}

void platformStartMotion(const MotionConfig& config, Completion<void> done) {
    CMMotionManager* manager = motionManager();
    manager.deviceMotionUpdateInterval = config.updateIntervalSeconds;
    manager.accelerometerUpdateInterval = config.updateIntervalSeconds;
    manager.gyroUpdateInterval = config.updateIntervalSeconds;

    bool started = false;
    if (config.useDeviceMotion && manager.deviceMotionAvailable) {
        [manager startDeviceMotionUpdates];
        started = true;
    }
    if (manager.accelerometerAvailable) {
        [manager startAccelerometerUpdates];
        started = true;
    }
    if (manager.gyroAvailable) {
        [manager startGyroUpdates];
        started = true;
    }

    gMotionRunning.store(started);
    if (started) {
        finishVoid(std::move(done), Result<void>::success());
    } else {
        finishVoid(std::move(done), Result<void>::failure({ErrorCode::Unavailable, "Motion sensors are unavailable.", 0}));
    }
}

void platformStopMotion() {
    CMMotionManager* manager = motionManager();
    [manager stopDeviceMotionUpdates];
    [manager stopAccelerometerUpdates];
    [manager stopGyroUpdates];
    gMotionRunning.store(false);
}

bool platformMotionIsRunning() {
    return gMotionRunning.load();
}

bool platformLatestMotion(MotionSample& out) {
    CMMotionManager* manager = motionManager();
    bool hasAny = false;
    if (manager.deviceMotion) {
        CMDeviceMotion* motion = manager.deviceMotion;
        out.gravity = {motion.gravity.x, motion.gravity.y, motion.gravity.z};
        out.userAcceleration = {motion.userAcceleration.x, motion.userAcceleration.y, motion.userAcceleration.z};
        out.rotationRate = {motion.rotationRate.x, motion.rotationRate.y, motion.rotationRate.z};
        out.attitudeRoll = motion.attitude.roll;
        out.attitudePitch = motion.attitude.pitch;
        out.attitudeYaw = motion.attitude.yaw;
        out.timestampSeconds = motion.timestamp;
        out.hasDeviceMotion = true;
        hasAny = true;
    }
    if (manager.accelerometerData) {
        CMAccelerometerData* accel = manager.accelerometerData;
        out.acceleration = {accel.acceleration.x, accel.acceleration.y, accel.acceleration.z};
        if (!hasAny) out.timestampSeconds = accel.timestamp;
        hasAny = true;
    }
    if (manager.gyroData && !out.hasDeviceMotion) {
        CMGyroData* gyro = manager.gyroData;
        out.rotationRate = {gyro.rotationRate.x, gyro.rotationRate.y, gyro.rotationRate.z};
        if (!hasAny) out.timestampSeconds = gyro.timestamp;
        hasAny = true;
    }
    return hasAny;
}


} // namespace tcx::ios::detail
