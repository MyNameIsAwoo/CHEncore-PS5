/*
 * Import stub for the PS5 system CommonDialog module. The SDK pin does not
 * ship this library's linker stub, but the native title imports this entry
 * from libSceCommonDialog.sprx at launch.
 */
__asm__(".global sceCommonDialogInitialize\n"
    ".type sceCommonDialogInitialize @function\n"
    "sceCommonDialogInitialize:\n");
