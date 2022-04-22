#import <Foundation/Foundation.h>

@interface AudioLibSound : NSObject
@property (nonatomic) float volume;
@property (nonatomic) float pan;
-(void) play;
-(void) pause;
-(void) seek:(float)t_sec;

-(float) getPositionSec;
-(float) getDuration;
-(BOOL) isPlaying;
@end

@interface AudioLibManager : NSObject
-(AudioLibSound*) load:(NSString*)path isLoop:(BOOL)is_loop;
-(void) free:(AudioLibSound*)p;
@end
