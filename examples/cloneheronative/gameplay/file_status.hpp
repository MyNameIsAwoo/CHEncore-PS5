// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace rhythm::fs {
// Native PS5 titles can read stat/fstat while lstat is denied with EPERM.
// Do not fall back to stat, which follows symlinks. Open the final entry with
// O_NOFOLLOW and inspect its handle instead; nonblocking prevents FIFO hangs.
inline int statusNoFollow(const char* path,struct stat* status) noexcept {
    if(lstat(path,status)==0)return 0;
    if(errno!=EPERM && errno!=ENOSYS && errno!=EACCES)return -1;
    const int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0){
        if(errno==ELOOP){std::memset(status,0,sizeof(*status));status->st_mode=S_IFLNK;return 0;}
        return -1;
    }
    const int result=fstat(fd,status),saved=errno;close(fd);errno=saved;return result;
}
}
