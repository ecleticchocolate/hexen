#include "compiler.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

void* Extern_Resolve(const char* name) {
    // We clear any existing dlerror so we can check it reliably
    dlerror();
    
    // RTLD_DEFAULT allows us to find any symbol exported by the host executable
    // or loaded libraries (like libc).
    void* addr = dlsym(RTLD_DEFAULT, name);
    
    const char* err = dlerror();
    if (err != NULL || addr == NULL) {
        if (g_aot_mode) {
            return NULL; // Allowed in AOT mode, the linker will resolve it later.
        }
        fprintf(stderr, "Link Error: Unresolved external function '%s'\n", name);
        if (err) {
            fprintf(stderr, "  dlerror: %s\n", err);
        }
        exit(1);
    }
    
    return addr;
}
