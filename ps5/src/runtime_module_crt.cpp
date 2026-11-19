// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
// Runs the engine in the bootstrap's main thread after the module loader has
// returned, preserving the normal native Vulkan/audio initialization path.
#include "runtime_module_api.h"
#include <cstddef>
using Initializer=void(*)();
extern "C" {
    int application_main(int,char**,char**) __asm__("main");
    void catchReturnFromMain(int);
    void ps5_fp_ieee();
    extern Initializer __preinit_array_start[] __attribute__((weak));
    extern Initializer __preinit_array_end[] __attribute__((weak));
    extern Initializer __init_array_start[] __attribute__((weak));
    extern Initializer __init_array_end[] __attribute__((weak));
}
namespace {
void initialize(Initializer* first,Initializer* last){if(first && last)while(first!=last)(*first++)();}
int run(int argc,char** argv){
    ps5_fp_ieee();
    const int status=application_main(argc,argv,nullptr);
    catchReturnFromMain(status);return status;
}
}
extern "C" int _start(size_t size,const void* arguments){
    if(size!=sizeof(EncoreRuntimeRequest) || !arguments)return -22;
    const auto& request=*static_cast<const EncoreRuntimeRequest*>(arguments);
    if(request.magic!=ENCORE_RUNTIME_MODULE_MAGIC || request.abi!=ENCORE_RUNTIME_MODULE_ABI ||
       request.bytes!=sizeof(request) || !request.output)return -22;
    initialize(__preinit_array_start,__preinit_array_end);
    initialize(__init_array_start,__init_array_end);
    *request.output={ENCORE_RUNTIME_MODULE_MAGIC,ENCORE_RUNTIME_MODULE_ABI,sizeof(EncoreRuntimeExports),run};
    return 0;
}
