#import <Foundation/Foundation.h>

@interface AudioLibSound : NSObject
@property (nonatomic) float volume;
@property (nonatomic) float pan;
-(void) play;
-(void) pause;
-(void) stop;
-(void) seek:(float)t_sec;

-(float) getPositionSec;
-(float) getDuration;
-(BOOL) isPlaying;
@end

@interface AudioLibManager : NSObject
-(AudioLibSound*) load:(NSString*)path isLoop:(BOOL)is_loop;
-(void) release:(AudioLibSound*)p;
@end

@interface ObjC : NSObject
+ (BOOL)catchException:(void(^)(void))tryBlock error:(__autoreleasing NSError **)error;
@end
