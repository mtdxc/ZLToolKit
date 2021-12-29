/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
#import "Socket.h"
#include "Util/logger.h"

#if defined (OS_IPHONE)
#import <Foundation/Foundation.h>
#endif //OS_IPHONE

namespace toolkit {

#if defined (OS_IPHONE)
bool SockNum::setSocketOfIOS(int sock){
    CFReadStreamRef readRef, writeRef;
    CFStreamCreatePairWithSocket(NULL, (CFSocketNativeHandle)sock, &readRef, &writeRef);
    if ((readRef == NULL) || (readRef == NULL))
    {
        WarnL<<"Unable to create read and write stream...";
        if (readRef)
        {
            CFReadStreamClose(readRef);
            CFRelease(readRef);
        }
        if (writeRef)
        {
            CFWriteStreamClose(writeRef);
            CFRelease(writeRef);
        }
        return false;
    }
    
    CFReadStreamSetProperty(readRef, kCFStreamPropertyShouldCloseNativeSocket, kCFBooleanFalse);
    CFWriteStreamSetProperty(writeRef, kCFStreamPropertyShouldCloseNativeSocket, kCFBooleanFalse);

    Boolean r1 = CFReadStreamSetProperty(readRef, kCFStreamNetworkServiceType, kCFStreamNetworkServiceTypeVoIP);
    Boolean r2 = CFWriteStreamSetProperty(writeRef, kCFStreamNetworkServiceType, kCFStreamNetworkServiceTypeVoIP);
    
    if (!r1 || !r2)
    {
        return false;
    }
    
    CFStreamStatus readStatus = CFReadStreamGetStatus(readRef);
    CFStreamStatus writeStatus = CFWriteStreamGetStatus(writeRef);
    
    if ((readStatus == kCFStreamStatusNotOpen) || (writeStatus == kCFStreamStatusNotOpen))
    {
        BOOL r1 = CFReadStreamOpen(readRef);
        BOOL r2 = CFWriteStreamOpen(writeRef);
        
        if (!r1 || !r2)
        {
            WarnL<<"Error in CFStreamOpen";
            return false;
        }
    }
    
    readStream = readRef;
    writeStream = writeRef;
    //NSLog(@"setSocketOfIOS:%d",sock);
    return true;
}
void SockNum::unsetSocketOfIOS(int sock){
    //NSLog(@"unsetSocketOfIOS:%d",sock);
    if (readStream) {
        CFReadStreamClose((CFReadStreamRef)readStream);
        readStream=NULL;
    }
    if (writeStream) {
        CFWriteStreamClose((CFWriteStreamRef)writeStream);
        writeStream=NULL;
    }
}
#endif //OS_IPHONE



}  // namespace toolkit
