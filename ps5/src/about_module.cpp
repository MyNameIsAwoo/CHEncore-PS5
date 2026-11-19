// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
// Application-owned metadata module. Encore does not request external rights
// or call entitlement exports. Supplying this module keeps the package writer
// from injecting a third-party module into the application.
volatile unsigned encoreAboutModuleRevision=1;
extern "C" int _start(unsigned long,const void*) {
    return encoreAboutModuleRevision==1?0:-22;
}
