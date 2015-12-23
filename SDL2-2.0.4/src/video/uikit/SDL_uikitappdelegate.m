/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_UIKIT

#include "../SDL_sysvideo.h"
#include "SDL_assert.h"
#include "SDL_hints.h"
#include "SDL_system.h"
#include "SDL_main.h"

#import "SDL_uikitappdelegate.h"
#import "SDL_uikitmodes.h"
#import "SDL_uikitwindow.h"

#include "../../events/SDL_events_c.h"
#include "SDL_third.h"

#ifdef main
#undef main
#endif

#define SDL_APNS        0

#if SDL_IPHONE_QQ
#import <TencentOAuth.h>
#endif

#if SDL_IPHONE_WX
#import "WXApi.h"
#endif

extern SDL_LoginCallback login_callback;
extern void* login_param;
extern NSString* login_secretkey1;
extern NSString* login_secretkey2; // to WeChat, it is appid.
extern SDL_SendCallback send_callback;
extern void* send_param;

static int forward_argc;
static char **forward_argv;
static int exit_status;
NSString* apns_token = @"";

#if SDL_IPHONE_WX
@interface SDLUIKitDelegate ()<WXApiDelegate>

@property (strong, nonatomic) NSString *access_token;
@property (strong, nonatomic) NSString *openid;
@end
#endif

int main(int argc, char **argv)
{
    int i;

    /* store arguments */
    forward_argc = argc;
    forward_argv = (char **)malloc((argc+1) * sizeof(char *));
    for (i = 0; i < argc; i++) {
        forward_argv[i] = malloc( (strlen(argv[i])+1) * sizeof(char));
        strcpy(forward_argv[i], argv[i]);
    }
    forward_argv[i] = NULL;

    /* Give over control to run loop, SDLUIKitDelegate will handle most things from here */
    @autoreleasepool {
        UIApplicationMain(argc, argv, nil, [SDLUIKitDelegate getAppDelegateClassName]);
    }

    /* free the memory we used to hold copies of argc and argv */
    for (i = 0; i < forward_argc; i++) {
        free(forward_argv[i]);
    }
    free(forward_argv);

    return exit_status;
}

static void
SDL_IdleTimerDisabledChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    BOOL disable = (hint && *hint != '0');
    [UIApplication sharedApplication].idleTimerDisabled = disable;
}

/* Load a launch image using the old UILaunchImageFile-era naming rules. */
static UIImage *
SDL_LoadLaunchImageNamed(NSString *name, int screenh)
{
    UIInterfaceOrientation curorient = [UIApplication sharedApplication].statusBarOrientation;
    UIUserInterfaceIdiom idiom = [UIDevice currentDevice].userInterfaceIdiom;
    UIImage *image = nil;

    if (idiom == UIUserInterfaceIdiomPhone && screenh == 568) {
        /* The image name for the iPhone 5 uses its height as a suffix. */
        image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-568h", name]];
    } else if (idiom == UIUserInterfaceIdiomPad) {
        /* iPad apps can launch in any orientation. */
        if (UIInterfaceOrientationIsLandscape(curorient)) {
            if (curorient == UIInterfaceOrientationLandscapeLeft) {
                image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-LandscapeLeft", name]];
            } else {
                image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-LandscapeRight", name]];
            }
            if (!image) {
                image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-Landscape", name]];
            }
        } else {
            if (curorient == UIInterfaceOrientationPortraitUpsideDown) {
                image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-PortraitUpsideDown", name]];
            }
            if (!image) {
                image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-Portrait", name]];
            }
        }
    }

    if (!image) {
        image = [UIImage imageNamed:name];
    }

    return image;
}

@implementation SDLLaunchScreenController

- (instancetype)init
{
    if (!(self = [super initWithNibName:nil bundle:nil])) {
        return nil;
    }

    NSBundle *bundle = [NSBundle mainBundle];
    NSString *screenname = [bundle objectForInfoDictionaryKey:@"UILaunchStoryboardName"];
    BOOL atleastiOS8 = UIKit_IsSystemVersionAtLeast(8.0);

    /* Launch screens were added in iOS 8. Otherwise we use launch images. */
    if (screenname && atleastiOS8) {
        @try {
            self.view = [bundle loadNibNamed:screenname owner:self options:nil][0];
        }
        @catch (NSException *exception) {
            /* If a launch screen name is specified but it fails to load, iOS
             * displays a blank screen rather than falling back to an image. */
            return nil;
        }
    }

    if (!self.view) {
        NSArray *launchimages = [bundle objectForInfoDictionaryKey:@"UILaunchImages"];
        UIInterfaceOrientation curorient = [UIApplication sharedApplication].statusBarOrientation;
        NSString *imagename = nil;
        UIImage *image = nil;

        int screenw = (int)([UIScreen mainScreen].bounds.size.width + 0.5);
        int screenh = (int)([UIScreen mainScreen].bounds.size.height + 0.5);

        /* We always want portrait-oriented size, to match UILaunchImageSize. */
        if (screenw > screenh) {
            int width = screenw;
            screenw = screenh;
            screenh = width;
        }

        /* Xcode 5 introduced a dictionary of launch images in Info.plist. */
        if (launchimages) {
            for (NSDictionary *dict in launchimages) {
                UIInterfaceOrientationMask orientmask = UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown;
                NSString *minversion   = dict[@"UILaunchImageMinimumOSVersion"];
                NSString *sizestring   = dict[@"UILaunchImageSize"];
                NSString *orientstring = dict[@"UILaunchImageOrientation"];

                /* Ignore this image if the current version is too low. */
                if (minversion && !UIKit_IsSystemVersionAtLeast(minversion.doubleValue)) {
                    continue;
                }

                /* Ignore this image if the size doesn't match. */
                if (sizestring) {
                    CGSize size = CGSizeFromString(sizestring);
                    if ((int)(size.width + 0.5) != screenw || (int)(size.height + 0.5) != screenh) {
                        continue;
                    }
                }

                if (orientstring) {
                    if ([orientstring isEqualToString:@"PortraitUpsideDown"]) {
                        orientmask = UIInterfaceOrientationMaskPortraitUpsideDown;
                    } else if ([orientstring isEqualToString:@"Landscape"]) {
                        orientmask = UIInterfaceOrientationMaskLandscape;
                    } else if ([orientstring isEqualToString:@"LandscapeLeft"]) {
                        orientmask = UIInterfaceOrientationMaskLandscapeLeft;
                    } else if ([orientstring isEqualToString:@"LandscapeRight"]) {
                        orientmask = UIInterfaceOrientationMaskLandscapeRight;
                    }
                }

                /* Ignore this image if the orientation doesn't match. */
                if ((orientmask & (1 << curorient)) == 0) {
                    continue;
                }

                imagename = dict[@"UILaunchImageName"];
            }

            if (imagename) {
                image = [UIImage imageNamed:imagename];
            }
        } else {
            imagename = [bundle objectForInfoDictionaryKey:@"UILaunchImageFile"];

            if (imagename) {
                image = SDL_LoadLaunchImageNamed(imagename, screenh);
            }

            if (!image) {
                image = SDL_LoadLaunchImageNamed(@"Default", screenh);
            }
        }

        if (image) {
            UIImageView *view = [[UIImageView alloc] initWithFrame:[UIScreen mainScreen].bounds];
            UIImageOrientation imageorient = UIImageOrientationUp;

            /* Bugs observed / workaround tested in iOS 8.3, 7.1, and 6.1. */
            if (UIInterfaceOrientationIsLandscape(curorient)) {
                if (atleastiOS8 && image.size.width < image.size.height) {
                    /* On iOS 8, portrait launch images displayed in forced-
                     * landscape mode (e.g. a standard Default.png on an iPhone
                     * when Info.plist only supports landscape orientations) need
                     * to be rotated to display in the expected orientation. */
                    if (curorient == UIInterfaceOrientationLandscapeLeft) {
                        imageorient = UIImageOrientationRight;
                    } else if (curorient == UIInterfaceOrientationLandscapeRight) {
                        imageorient = UIImageOrientationLeft;
                    }
                } else if (!atleastiOS8 && image.size.width > image.size.height) {
                    /* On iOS 7 and below, landscape launch images displayed in
                     * landscape mode (e.g. landscape iPad launch images) need
                     * to be rotated to display in the expected orientation. */
                    if (curorient == UIInterfaceOrientationLandscapeLeft) {
                        imageorient = UIImageOrientationLeft;
                    } else if (curorient == UIInterfaceOrientationLandscapeRight) {
                        imageorient = UIImageOrientationRight;
                    }
                }
            }

            /* Create the properly oriented image. */
            view.image = [[UIImage alloc] initWithCGImage:image.CGImage scale:image.scale orientation:imageorient];

            self.view = view;
        }
    }

    return self;
}

- (void)loadView
{
    /* Do nothing. */
}

- (BOOL)shouldAutorotate
{
    /* If YES, the launch image will be incorrectly rotated in some cases. */
    return NO;
}

- (NSUInteger)supportedInterfaceOrientations
{
    /* We keep the supported orientations unrestricted to avoid the case where
     * there are no common orientations between the ones set in Info.plist and
     * the ones set here (it will cause an exception in that case.) */
    return UIInterfaceOrientationMaskAll;
}

@end

@implementation SDLUIKitDelegate {
    UIWindow *launchWindow;
}

/* convenience method */
+ (id)sharedAppDelegate
{
    /* the delegate is set in UIApplicationMain(), which is guaranteed to be
     * called before this method */
    return [UIApplication sharedApplication].delegate;
}

+ (NSString *)getAppDelegateClassName
{
    /* subclassing notice: when you subclass this appdelegate, make sure to add
     * a category to override this method and return the actual name of the
     * delegate */
    return @"SDLUIKitDelegate";
}

- (void)hideLaunchScreen
{
    UIWindow *window = launchWindow;

    if (!window || window.hidden) {
        return;
    }

    launchWindow = nil;

    /* Do a nice animated fade-out (roughly matches the real launch behavior.) */
    [UIView animateWithDuration:0.2 animations:^{
        window.alpha = 0.0;
    } completion:^(BOOL finished) {
        window.hidden = YES;
    }];
}

- (void)postFinishLaunch
{
    /* Hide the launch screen the next time the run loop is run. SDL apps will
     * have a chance to load resources while the launch screen is still up. */
    [self performSelector:@selector(hideLaunchScreen) withObject:nil afterDelay:0.0];

    /* run the user's application, passing argc and argv */
    SDL_iPhoneSetEventPump(SDL_TRUE);
    exit_status = SDL_main(forward_argc, forward_argv);
    SDL_iPhoneSetEventPump(SDL_FALSE);

    if (launchWindow) {
        launchWindow.hidden = YES;
        launchWindow = nil;
    }

    /* exit, passing the return status from the user's application */
    /* We don't actually exit to support applications that do setup in their
     * main function and then allow the Cocoa event loop to run. */
    exit(exit_status);
}

#if SDL_IPHONE_WX
extern void wechat_registerapp();
#endif

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
#if SDL_APNS
    if ([application respondsToSelector:@selector(isRegisteredForRemoteNotifications)]) {
        // IOS8
        // create UIUserNotificationSettings, and set types
        UIUserNotificationSettings *notiSettings = [UIUserNotificationSettings settingsForTypes:(UIUserNotificationTypeBadge | UIUserNotificationTypeAlert | UIRemoteNotificationTypeSound) categories:nil];
        
        [application registerUserNotificationSettings:notiSettings];
        [application registerForRemoteNotifications];
        
    } else { // ios7
        [application registerForRemoteNotificationTypes:(UIRemoteNotificationTypeBadge | UIRemoteNotificationTypeSound | UIRemoteNotificationTypeAlert)];
    }
#endif
    
    NSBundle *bundle = [NSBundle mainBundle];
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    
#if SDL_IPHONE_LAUNCHSCREEN
    /* The normal launch screen is displayed until didFinishLaunching returns,
     * but SDL_main is called after that happens and there may be a noticeable
     * delay between the start of SDL_main and when the first real frame is
     * displayed (e.g. if resources are loaded before SDL_GL_SwapWindow is
     * called), so we show the launch screen programmatically until the first
     * time events are pumped. */
    UIViewController *viewcontroller = [[SDLLaunchScreenController alloc] init];
    
    if (viewcontroller.view) {
        launchWindow = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];

        /* We don't want the launch window immediately hidden when a real SDL
         * window is shown - we fade it out ourselves when we're ready. */
        launchWindow.windowLevel = UIWindowLevelNormal + 1.0;

        /* Show the window but don't make it key. Events should always go to
         * other windows when possible. */
        launchWindow.hidden = NO;

        launchWindow.rootViewController = viewcontroller;
    }
#endif

    /* Set working directory to resource path */
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[bundle resourcePath]];

    /* register a callback for the idletimer hint */
    SDL_AddHintCallback(SDL_HINT_IDLE_TIMER_DISABLED,
                        SDL_IdleTimerDisabledChanged, NULL);

    SDL_SetMainReady();
    [self performSelector:@selector(postFinishLaunch) withObject:nil afterDelay:0.0];

#if SDL_IPHONE_WX
    wechat_registerapp();
#endif

    return YES;
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    SDL_SendAppEvent(SDL_APP_TERMINATING);
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
    SDL_SendAppEvent(SDL_APP_LOWMEMORY);
}

- (void)application:(UIApplication *)application didChangeStatusBarOrientation:(UIInterfaceOrientation)oldStatusBarOrientation
{
    BOOL isLandscape = UIInterfaceOrientationIsLandscape(application.statusBarOrientation);
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this && _this->num_displays > 0) {
        SDL_DisplayMode *desktopmode = &_this->displays[0].desktop_mode;
        SDL_DisplayMode *currentmode = &_this->displays[0].current_mode;

        /* The desktop display mode should be kept in sync with the screen
         * orientation so that updating a window's fullscreen state to
         * SDL_WINDOW_FULLSCREEN_DESKTOP keeps the window dimensions in the
         * correct orientation. */
        if (isLandscape != (desktopmode->w > desktopmode->h)) {
            int height = desktopmode->w;
            desktopmode->w = desktopmode->h;
            desktopmode->h = height;
        }

        /* Same deal with the current mode + SDL_GetCurrentDisplayMode. */
        if (isLandscape != (currentmode->w > currentmode->h)) {
            int height = currentmode->w;
            currentmode->w = currentmode->h;
            currentmode->h = height;
        }
    }
}

- (void)applicationWillResignActive:(UIApplication*)application
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (_this) {
        SDL_Window *window;
        for (window = _this->windows; window != nil; window = window->next) {
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_FOCUS_LOST, 0, 0);
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
        }
    }
    SDL_SendAppEvent(SDL_APP_WILLENTERBACKGROUND);
}

- (void)applicationDidEnterBackground:(UIApplication*)application
{
    SDL_SendAppEvent(SDL_APP_DIDENTERBACKGROUND);
}

- (void)applicationWillEnterForeground:(UIApplication*)application
{
    SDL_SendAppEvent(SDL_APP_WILLENTERFOREGROUND);
}

- (void)applicationDidBecomeActive:(UIApplication*)application
{
    SDL_SendAppEvent(SDL_APP_DIDENTERFOREGROUND);

    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (_this) {
        SDL_Window *window;
        for (window = _this->windows; window != nil; window = window->next) {
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESTORED, 0, 0);
        }
    }
}

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
    NSURL *fileURL = url.filePathURL;
    if (fileURL != nil) {
        SDL_SendDropFile([fileURL.path UTF8String]);
    } else {
        SDL_SendDropFile([url.absoluteString UTF8String]);
    }

    return [self application:application handleOpenURL:url];
}

-(BOOL)application:(UIApplication *)application handleOpenURL:(NSURL *)url
{
#if SDL_IPHONE_WX
    if ([WXApi handleOpenURL:url delegate:self]) {
        return YES;
    }
#endif
#if SDL_IPHONE_QQ
    if ([TencentOAuth HandleOpenURL:url]) {
        return YES;
    }
#endif
    return NO;
}

#if SDL_IPHONE_WX
-(void)onResp:(BaseResp*)resp
{
    if ([resp isKindOfClass:[SendMessageToWXResp class]]) {
        /*
         ERR_OK = 0
         ERR_USER_CANCEL = -2
         */
        if (send_callback) {
            send_callback(SDL_APP_WeChat, resp.errCode, send_param);
        }

    } else if ([resp isKindOfClass:[SendAuthResp class]]) {
		/*
		errCode
			ERR_OK = 0 (login sucessfully)
			ERR_AUTH_DENIED = -4
			ERR_USER_CANCEL = -2
		code 
			use it to get access_token. valid only errCode==0
		*/
		SendAuthResp *aresp = (SendAuthResp *)resp;    
		if (aresp.errCode == 0) { // user agreen
			NSLog(@"errCode = %d", aresp.errCode);
			NSLog(@"code = %@", aresp.code);
        
			// Get access_token
			// format: https://api.weixin.qq.com/sns/oauth2/access_token?appid=APPID&secret=SECRET&code=CODE&grant_type=authorization_code
			NSString *url =[NSString stringWithFormat:@"https://api.weixin.qq.com/sns/oauth2/access_token?appid=%@&secret=%@&code=%@&grant_type=authorization_code", login_secretkey2, login_secretkey1, aresp.code];
			dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
				NSURL *zoneUrl = [NSURL URLWithString:url];
				NSString *zoneStr = [NSString stringWithContentsOfURL:zoneUrl encoding:NSUTF8StringEncoding error:nil];
				NSData *data = [zoneStr dataUsingEncoding:NSUTF8StringEncoding];
				dispatch_async(dispatch_get_main_queue(), ^{
					if (data) {
						NSDictionary *dic = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:nil];
						_openid = [dic objectForKey:@"openid"]; // initial
						_access_token = [dic objectForKey:@"access_token"];
						// NSLog(@"openid = %@", _openid);
						// NSLog(@"access = %@", [dic objectForKey:@"access_token"]);
						NSLog(@"dic = %@", dic);
						[self getUserInfo]; // get user information
					}
				});
			});
		} else if (aresp.errCode == -2) {
			NSLog(@"User cancel login");
		} else if (aresp.errCode == -4) {
			NSLog(@"User denied login");
		} else {
			NSLog(@"errCode = %d", aresp.errCode);
			NSLog(@"code = %@", aresp.code);
		}
	}
}

// get user information
- (void)getUserInfo {
    NSString *url = [NSString stringWithFormat:@"https://api.weixin.qq.com/sns/userinfo?access_token=%@&openid=%@", self.access_token, self.openid];
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSURL *zoneUrl = [NSURL URLWithString:url];
        NSString *zoneStr = [NSString stringWithContentsOfURL:zoneUrl encoding:NSUTF8StringEncoding error:nil];
        NSData *data = [zoneStr dataUsingEncoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (data) {
                NSDictionary *dic = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:nil];
                NSLog(@"openid = %@", [dic objectForKey:@"openid"]);
                NSLog(@"nickname = %@", [dic objectForKey:@"nickname"]);
                NSLog(@"sex = %@", [dic objectForKey:@"sex"]);
                NSLog(@"country = %@", [dic objectForKey:@"country"]);
                NSLog(@"province = %@", [dic objectForKey:@"province"]);
                NSLog(@"city = %@", [dic objectForKey:@"city"]);
                NSLog(@"headimgurl = %@", [dic objectForKey:@"headimgurl"]);
                NSLog(@"unionid = %@", [dic objectForKey:@"unionid"]);
                NSLog(@"privilege = %@", [dic objectForKey:@"privilege"]);
                
                SDLUIKitDelegate *appdelegate = (SDLUIKitDelegate *)[[UIApplication sharedApplication] delegate];
                [[NSNotificationCenter defaultCenter] postNotificationName:@"Note" object:nil]; // send notification

				NSString* name = [dic objectForKey:@"openid"];
                NSString* nick = [dic objectForKey:@"nickname"]; // nick
				NSString* avatarurl = [dic objectForKey:@"headimgurl"]; // avatar url
                if (login_callback) {
                    login_callback(SDL_APP_WeChat, name.UTF8String, nick.UTF8String, avatarurl.UTF8String, login_param);
                }
            }
        });
    });
}

//  Refresh access_token valid time. After get code, request below URL to get refresh_token https://api.weixin.qq.com/sns/oauth2/refresh_token?appid=APPID&grant_type=refresh_token&refresh_token=REFRESH_TOKEN
- (void)refresh {
//    NSString *url =[NSString stringWithFormat:@"https://api.weixin.qq.com/sns/oauth2/refresh_token?appid=%@&grant_type=refresh_token&refresh_token=%@", APP_ID, [dic objectForKey:@"access_token"]];
//    
//    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
//        NSURL *zoneUrl = [NSURL URLWithString:url];
//        NSString *zoneStr = [NSString stringWithContentsOfURL:zoneUrl encoding:NSUTF8StringEncoding error:nil];
//        NSData *data = [zoneStr dataUsingEncoding:NSUTF8StringEncoding];
//        dispatch_async(dispatch_get_main_queue(), ^{
//            if (data) {
//                NSDictionary *dic = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:nil];
//                NSLog(@"dic = %@", dic);
//            }
//        });
//    });
}

#endif

#if SDL_APNS
- (void)application:(UIApplication *)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)pToken
{
    // NSLog(@"---Token--%@", pToken);
    const unsigned char* src = [pToken bytes];
    size_t size = [pToken length];
    char* c_str = SDL_malloc(size * 2 + 1);
    for (size_t at = 0; at < size; at ++) {
        int tmp = (src[at] & 0xf0) >> 4;
        if (tmp < 0xa) {
            c_str[at * 2] = tmp + '0';
        } else {
            c_str[at * 2] = tmp - 0xa + 'a';
        }
        
        tmp = src[at] & 0x0f;
        if (tmp < 0xa) {
            c_str[at * 2 + 1] = tmp + '0';
        } else {
            c_str[at * 2 + 1] = tmp - 0xa + 'a';
        }
    }
    c_str[size * 2] = '\0';
    apns_token = [NSString stringWithUTF8String:c_str];
    SDL_free(c_str);
    c_str = NULL;
}

- (void)application:(UIApplication *)application didReceiveRemoteNotification:(NSDictionary *)userInfo
{
/*
    NSLog(@"userInfo == %@",userInfo);

    NSString *message = [[userInfo objectForKey:@"aps"] objectForKey:@"alert"];
    
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"Tip" message:message delegate:self cancelButtonTitle:@"Cancel" otherButtonTitles:@"OK", nil];
    
    [alert show];
*/
}

- (void)application:(UIApplication *)application didFailToRegisterForRemoteNotificationsWithError:(NSError *)error{
    
    NSLog(@"Regist fail%@",error);
}
#endif

@end

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
