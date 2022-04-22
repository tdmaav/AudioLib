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

-(void) dealloc {
    delete _sound;
    [super dealloc];
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
-(void) seek:(float)t_sec { _sound->seek(t_sec); }

-(float) getPositionSec { return _sound->getPositionSec(); }
-(float) getDuration { return _sound->getDuration(); }
-(BOOL) isPlaying { return _sound->isPlaying(); }

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
    auto audio = _manager->load(cpath,is_loop);
    return [[AudioLibSound alloc] initWith:audio];
}

-(void) free:(AudioLibSound*)p {
    
}

@end
