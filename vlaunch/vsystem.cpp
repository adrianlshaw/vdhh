//
//  vsystem.h
//  vlaunch
//
//  Created by Boris Remizov on 02/10/16.
//  Copyright © 2016 Veertu. All rights reserved.
//

#include "vlaunch.h"
#include "vmsg.h"
#include "vobj.h"
#include "log.h"
#include <string>
#include <sstream>
#include <vector>
#include <sys/un.h>
#include <sys/socket.h>
#include <ServiceManagement/ServiceManagement.h>

#ifndef VLAUNCH_DEF_SOCKET_PATH
#define VLAUNCH_DEF_SOCKET_PATH "/var/tmp/com.veertu.vlaunch.socket"
#endif

namespace {

void split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (getline(ss, item, delim)) {
        if (item.size() > 0)
            elems.push_back(item);
    }
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return std::move(elems);
}

int vlaunch_install_helper(const char* tool) {

    // request elevating priviledges to bless the helper
    AuthorizationRef auth = NULL;

    // try to create authorization from external representation first
    AuthorizationItem items[] = {
        {kSMRightBlessPrivilegedHelper, 0, NULL, 0}
    };
    AuthorizationRights rights = {
        sizeof(items) / sizeof(*items),
        items
    };

    OSStatus result = AuthorizationCreate(&rights, NULL,
        kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed |
        kAuthorizationFlagPreAuthorize, &auth);
    if (noErr != result) {
        ERR("Failed to authorize helper tool with error %d", result);
        errno = EAUTH;
        return -1;
    }

    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, tool, kCFStringEncodingUTF8);
    CFErrorRef err = NULL;
#if 0
    // remove possibly corrupted previous tool
    // this looks doesn't work as expected - all helper artifacts remain on their places
    Boolean result = SMJobRemove(kSMDomainSystemLaunchd, name, auth, kCFBooleanTrue, &err);
#endif
    // start new tool
    Boolean res = SMJobBless(kSMDomainSystemLaunchd, name, auth, &err);
    CFRelease(name);
    
    return res ? 0 : -1;
}

int vlaunch_connect(const char* path) {

    int fd = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (-1 == fd)
        return -1;

    struct sockaddr_un addr = {
        sizeof(struct sockaddr_un),
        AF_LOCAL,
        VLAUNCH_DEF_SOCKET_PATH
    };

    if (-1 == connect(fd, (struct sockaddr*)&addr, sizeof(addr))) {
        close(fd);
        return -1;
    }
    
    return fd;
}

} // namespace

extern "C" {

int vlaunchfd[2] = {-1, -1};
const char* vlaunchd = NULL;
const char* vlaunchd_socket = VLAUNCH_DEF_SOCKET_PATH;

int vsystem(const char* command, int wait) {
    if (-1 == vlaunchfd[1]) {
        // no communucation pipe, connect to vlaunchd
        int fd = vlaunch_connect(vlaunchd_socket);
        if (-1 == fd && EACCES == errno && NULL != vlaunchd) {
            vlaunch_install_helper(vlaunchd);
            fd = vlaunch_connect(vlaunchd_socket);
        }
        if (-1 == fd)
            return -1;
        vlaunchfd[0] = fd;
        vlaunchfd[1] = fd;
    }

    // run the command
    vobj_t msg = vobj_create();
    vobj_set_llong(msg, VLAUNCH_KEY_CMD, VLAUNCH_CMD_LAUNCH);

    size_t cmdlen = strlen(command);

    // infer arguments from string,
    auto args = split(command, ' ');
    const char* path = args[0].c_str();
    vobj_set_str(msg, VLAUNCH_KEY_PATH, path);
    if (args.size() > 1) {
        vobj_t argv = vobj_create();
        for (auto const& arg : args)
            vobj_add_str(argv, arg.c_str());
        vobj_set_obj(msg, VLAUNCH_KEY_ARGV, argv);
        vobj_dispose(argv);
    }

    vobj_set_llong(msg, "wait", wait);

    if (vmsg_write(vlaunchfd[1], msg) <= 0) {
        vobj_dispose(msg);
        return -1;
    }

    // get status
    int status = 0;
    if (-1 != vlaunchfd[0]) {
        status = vmsg_read(vlaunchfd[0], msg) > 0 ?
            (int)vobj_get_llong(msg, "status") : 127;
    }

    vobj_dispose(msg);
    
    return status;
}

int vregt(const char* name, const char* path) {
    if (-1 == vlaunchfd[1]) {
        // no communucation pipe, connect to vlaunchd
        int fd = vlaunch_connect(vlaunchd_socket);
        if (-1 == fd && EACCES == errno && NULL != vlaunchd) {
            vlaunch_install_helper(vlaunchd);
            fd = vlaunch_connect(vlaunchd_socket);
        }
        if (-1 == fd)
            return -1;
        vlaunchfd[0] = fd;
        vlaunchfd[1] = fd;
    }

    // run the command
    vobj_t msg = vobj_create();
    vobj_set_llong(msg, VLAUNCH_KEY_CMD, VLAUNCH_CMD_REGT);
    vobj_set_str(msg, VLAUNCH_KEY_NAME, name);
    vobj_set_str(msg, VLAUNCH_KEY_PATH, path);

    if (vmsg_write(vlaunchfd[1], msg) <= 0) {
        vobj_dispose(msg);
        return -1;
    }

    // get status
    int status = 0;
    if (-1 != vlaunchfd[0]) {
        status = vmsg_read(vlaunchfd[0], msg) > 0 ?
        (int)vobj_get_llong(msg, "status") : 127;
    }

    vobj_dispose(msg);

    return status;
}

} // extern "C"
