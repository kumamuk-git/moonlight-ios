//
//  TemporarySettings.m
//  Moonlight
//
//  Created by Cameron Gutman on 12/1/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "TemporarySettings.h"
#import "OnScreenControls.h"
#import <Security/Security.h>

@implementation TemporarySettings

+ (uint16_t)sonoBusPort {
    NSInteger port = [NSUserDefaults.standardUserDefaults integerForKey:@"sonoBusPort"];
    return (port > 0 && port <= UINT16_MAX) ? (uint16_t)port : 0;
}

+ (void)setSonoBusPort:(uint16_t)port {
    [NSUserDefaults.standardUserDefaults setInteger:port forKey:@"sonoBusPort"];
    [NSUserDefaults.standardUserDefaults setBool:NO forKey:@"sonoBusUseGroup"];
}

+ (BOOL)usesSonoBusGroup {
    return [NSUserDefaults.standardUserDefaults boolForKey:@"sonoBusUseGroup"];
}

+ (uint16_t)sonoBusLocalPort {
    NSInteger port = [NSUserDefaults.standardUserDefaults integerForKey:@"sonoBusLocalPort"];
    return (port > 1023 && port <= UINT16_MAX) ? (uint16_t)port : 0;
}

+ (void)setSonoBusLocalPort:(uint16_t)port {
    [NSUserDefaults.standardUserDefaults setInteger:port forKey:@"sonoBusLocalPort"];
}

+ (NSString *)sonoBusParticipantName {
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    NSString *name = [defaults stringForKey:@"sonoBusParticipantName"];
    if (!name.length) {
        NSString *uuid = NSUUID.UUID.UUIDString;
        name = [@"Moonlight-" stringByAppendingString:[uuid substringFromIndex:uuid.length - 12]];
        [defaults setObject:name forKey:@"sonoBusParticipantName"];
    }
    return name;
}

static NSDictionary *sonoBusKeychainQuery(void) {
    return @{(__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
             (__bridge id)kSecAttrService: @"Moonlight.SonoBus",
             (__bridge id)kSecAttrAccount: @"Private Group"};
}

+ (NSURLCredential *)sonoBusGroupCredential {
    NSMutableDictionary *query = [sonoBusKeychainQuery() mutableCopy];
    query[(__bridge id)kSecReturnAttributes] = @YES;
    query[(__bridge id)kSecReturnData] = @YES;
    CFTypeRef item = NULL;
    if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &item) != errSecSuccess) {
        return nil;
    }
    NSDictionary *attributes = CFBridgingRelease(item);
    NSString *name = attributes[(__bridge id)kSecAttrLabel];
    NSString *password = [[NSString alloc] initWithData:attributes[(__bridge id)kSecValueData] encoding:NSUTF8StringEncoding];
    return name.length && password ? [NSURLCredential credentialWithUser:name password:password
                                                             persistence:NSURLCredentialPersistenceNone] : nil;
}

+ (BOOL)useSonoBusGroup:(NSString *)name password:(NSString *)password {
    NSDictionary *query = sonoBusKeychainQuery();
    NSDictionary *values = @{(__bridge id)kSecAttrLabel: name,
                             (__bridge id)kSecValueData: [password dataUsingEncoding:NSUTF8StringEncoding],
                             (__bridge id)kSecAttrAccessible: (__bridge id)kSecAttrAccessibleWhenUnlockedThisDeviceOnly};
    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)values);
    if (status == errSecItemNotFound) {
        NSMutableDictionary *item = [query mutableCopy];
        [item addEntriesFromDictionary:values];
        status = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    }
    if (status == errSecSuccess) {
        [NSUserDefaults.standardUserDefaults setBool:YES forKey:@"sonoBusUseGroup"];
    }
    return status == errSecSuccess;
}

- (id) initFromSettings:(Settings*)settings {
    self = [self init];
    
    self.parent = settings;
    
#if TARGET_OS_TV
    // Apply default values from our Root.plist
    NSString* settingsBundle = [[NSBundle mainBundle] pathForResource:@"Settings" ofType:@"bundle"];
    NSDictionary* settingsData = [NSDictionary dictionaryWithContentsOfFile:[settingsBundle stringByAppendingPathComponent:@"Root.plist"]];
    NSArray* preferences = [settingsData objectForKey:@"PreferenceSpecifiers"];
    NSMutableDictionary* defaultsToRegister = [[NSMutableDictionary alloc] initWithCapacity:[preferences count]];
    for (NSDictionary* prefSpecification in preferences) {
        NSString* key = [prefSpecification objectForKey:@"Key"];
        if (key != nil) {
            [defaultsToRegister setObject:[prefSpecification objectForKey:@"DefaultValue"] forKey:key];
        }
    }
    [[NSUserDefaults standardUserDefaults] registerDefaults:defaultsToRegister];
    
    self.bitrate = [NSNumber numberWithInteger:[[NSUserDefaults standardUserDefaults] integerForKey:@"bitrate"]];
    assert([self.bitrate intValue] != 0);
    self.framerate = [NSNumber numberWithInteger:[[NSUserDefaults standardUserDefaults] integerForKey:@"framerate"]];
    assert([self.framerate intValue] != 0);
    self.audioConfig = [NSNumber numberWithInteger:[[NSUserDefaults standardUserDefaults] integerForKey:@"audioConfig"]];
    assert([self.audioConfig intValue] != 0);
    self.preferredCodec = (typeof(self.preferredCodec))[[NSUserDefaults standardUserDefaults] integerForKey:@"preferredCodec"];
    self.useFramePacing = [[NSUserDefaults standardUserDefaults] integerForKey:@"useFramePacing"] != 0;
    self.playAudioOnPC = [[NSUserDefaults standardUserDefaults] boolForKey:@"audioOnPC"];
    self.enableHdr = [[NSUserDefaults standardUserDefaults] boolForKey:@"enableHdr"];
    self.optimizeGames = [[NSUserDefaults standardUserDefaults] boolForKey:@"optimizeGames"];
    self.multiController = [[NSUserDefaults standardUserDefaults] boolForKey:@"multipleControllers"];
    self.swapABXYButtons = [[NSUserDefaults standardUserDefaults] boolForKey:@"swapABXYButtons"];
    self.btMouseSupport = [[NSUserDefaults standardUserDefaults] boolForKey:@"btMouseSupport"];
    self.statsOverlay = [[NSUserDefaults standardUserDefaults] boolForKey:@"statsOverlay"];
    
    NSInteger _screenSize = [[NSUserDefaults standardUserDefaults] integerForKey:@"streamResolution"];
    switch (_screenSize) {
        case 0:
            self.height = [NSNumber numberWithInteger:720];
            self.width = [NSNumber numberWithInteger:1280];
            break;
        case 1:
            self.height = [NSNumber numberWithInteger:1080];
            self.width = [NSNumber numberWithInteger:1920];
            break;
        case 2:
            self.height = [NSNumber numberWithInteger:2160];
            self.width = [NSNumber numberWithInteger:3840];
            break;
        case 3:
            self.height = [NSNumber numberWithInteger:1440];
            self.width = [NSNumber numberWithInteger:2560];
            break;
        default:
            abort();
    }
    self.onscreenControls = [NSNumber numberWithInteger:OnScreenControlsLevelOff];
#else
    self.bitrate = settings.bitrate;
    self.framerate = settings.framerate;
    self.height = settings.height;
    self.width = settings.width;
    self.audioConfig = settings.audioConfig;
    self.preferredCodec = settings.preferredCodec;
    self.useFramePacing = settings.useFramePacing;
    self.playAudioOnPC = settings.playAudioOnPC;
    self.enableHdr = settings.enableHdr;
    self.optimizeGames = settings.optimizeGames;
    self.multiController = settings.multiController;
    self.swapABXYButtons = settings.swapABXYButtons;
    self.onscreenControls = settings.onscreenControls;
    self.btMouseSupport = settings.btMouseSupport;
    self.absoluteTouchMode = settings.absoluteTouchMode;
    self.statsOverlay = settings.statsOverlay;
#endif
    self.uniqueId = settings.uniqueId;
    
    return self;
}

@end
