// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/diagnostics.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
int main(){
    char directory[]="/tmp/ch-diagnostics-XXXXXX";assert(mkdtemp(directory));
    assert(rhythm::startDiagnostics(directory,"test-build"));
    rhythm::logDiagnostic("loader error=%d; line\nbreak",17);
    std::thread a([]{for(int i=0;i<4000;++i)rhythm::logDiagnostic("worker A %d",i);});
    std::thread b([]{for(int i=0;i<4000;++i)rhythm::logDiagnostic("worker B %d",i);});a.join();b.join();
    rhythm::stopDiagnostics();
    const std::filesystem::path path=std::string(directory)+"/encore-test.log";
    std::ifstream file(path);std::string text((std::istreambuf_iterator<char>(file)),{});
    assert(text.find("SESSION START")!=std::string::npos && text.find("loader error=17; line break")!=std::string::npos);
    assert(std::filesystem::file_size(path)<530000);
    assert(rhythm::startDiagnostics(directory,"restart-test"));rhythm::stopDiagnostics();
    std::filesystem::remove_all(directory);
    std::puts("PASS background diagnostics concurrent producers, bounded storage, newline sanitation, flush and restart");
}
