// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "../ps5/src/startup_log.hpp"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/wait.h>
int main() {
    char folder[]="/tmp/encore-startup-XXXXXX";assert(mkdtemp(folder));
    const std::string path=std::string(folder)+"/startup.log";
    encore_startup::path=path.c_str();
    const pid_t child=fork();assert(child>=0);
    if(!child){encore_startup::enabled=true;encore_startup::checkpoint("before graphics");_exit(0);}
    int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    auto read=[&](){std::ifstream f(path);return std::string(std::istreambuf_iterator<char>(f),{});};
    assert(read().find("before graphics")!=std::string::npos);
    encore_startup::enabled=true;encore_startup::finish();const auto completed=read();
    assert(completed.find("startup complete")!=std::string::npos);
    encore_startup::checkpoint("must not write during gameplay");assert(read()==completed);
    (void)std::remove(path.c_str());(void)rmdir(folder);
    std::puts("PASS startup checkpoints survive abrupt process exit and stop before gameplay");
}
