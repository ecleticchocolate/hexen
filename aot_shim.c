#include <stdio.h>
#include <stdint.h>

extern int64_t torrent_main(void);

int main(void) {
    int64_t r = torrent_main();
    fprintf(stderr, "Found main! running... offset=0\n");
    printf("= %lld\n", (long long)r);
    return 0;
}
