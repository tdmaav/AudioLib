#include "AudioLib.h"
#include "AudioLibObjc.h"

/*
 * AudioLibSound
 */

@interface AudioLibSound()
@property (nonatomic, assign) AudioLib::Sound *sound;
-(id) initWith:(AudioLib::Sound*)sound;
@end

@implementation AudioLibSound
@synthesize volume = _volume;
@synthesize pan = _pan;

-(id) initWith:(AudioLib::Sound*)sound {
    _sound = sound;
    return self;
}

-(void)setVolume:(float)v {
    _volume = v;
    _sound->volume = v;
}
-(void)setPan:(float)v {
    _pan = v;
    _sound->pan = v;
}
-(float)volume { return _volume; }
-(float)pan { return _pan; }

-(void) play { _sound->play(); }
-(void) pause { _sound->pause(); }
-(void) stop { _sound->stop(); }
-(void) seek:(float)t_sec { _sound->seek(t_sec); }

-(float) getPositionSec { return _sound->getPositionSec(); }
-(float) getDuration { return _sound->getDuration(); }
-(BOOL) isPlaying { return _sound->isPlaying(); }

-(AudioLib::Sound*) getNativeHandle { return _sound; }

@end


/*
 * AudioLibManager
 */

@interface AudioLibManager()
@property (nonatomic, assign) AudioLib::Manager *manager;
@end

@implementation AudioLibManager

-(id) init {
    _manager = new AudioLib::Manager();
    return self;
}

-(void) dealloc {
    delete _manager;
    [super dealloc];
}

-(AudioLibSound*) load:(NSString*)path isLoop:(BOOL)is_loop {
    std::string cpath([path UTF8String]);
    int32_t err = AudioLib::AUDIOLIB_SUCCESS;
    auto audio = _manager->load(cpath,is_loop,&err);
    
    switch(err) {
        case AudioLib::AUDIOLIB_FILE_ERROR:
            [NSException raise:NSGenericException format:@"File error: %@", path];
            break;
        case AudioLib::AUDIOLIB_DECODE_ERROR:
            [NSException raise:NSInternalInconsistencyException format:@"Decode error: %@", path];
            break;
        case AudioLib::AUDIOLIB_WRONG_SAMPLE_RATE:
            [NSException raise:NSInternalInconsistencyException format:@"Wrong sample rate in %@", path];
            break;
        case AudioLib::AUDIOLIB_WRONG_CHANNEL_COUNT:
            [NSException raise:NSInternalInconsistencyException format:@"Wrong channel count in %@", path];
            break;
    }
    
    return [[AudioLibSound alloc] initWith:audio];
}

-(void) release:(AudioLibSound*)p {
    _manager->free([p getNativeHandle]);
    [p release];
}

@end


/*
 */

@implementation ObjC

+ (BOOL)catchException:(void(^)(void))tryBlock error:(__autoreleasing NSError **)error {
    @try {
        tryBlock();
        return YES;
    }
    @catch (NSException *exception) {
        *error = [[NSError alloc] initWithDomain:exception.description code:0 userInfo:exception.userInfo];
        return NO;
    }
}

@end
