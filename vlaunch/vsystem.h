//
//  vsystem.h
//  vlaunch
//
//  Created by Boris Remizov on 02/10/16.
//  Copyright © 2016 Veertu. All rights reserved.
//

#ifndef vsystem_h
#define vsystem_h

/// \brief communication descriptors [rd,wr] for vlaunch system
extern int vlaunchfd[2];

/// \brief path to privileged helper tool to be installed on request
///        if needed, the tool remains persistent after system restart
extern const char* vlaunchd;

/// \brief path to socket to communicate with vlaunchd tool
extern const char* vlaunchd_socket;

/// \brief launch external command via vlaunch system
int vsystem(const char* command, int wait);

/// \brief register tool for execution via short name
int vregt(const char* name, const char* path);

#endif /* vsystem_h */
