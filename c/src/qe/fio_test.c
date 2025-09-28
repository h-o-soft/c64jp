#include <stdio.h>

int main(void) {
    FILE *fp = fopen("test", "r");
    if (!fp) {
        puts("open failed");
    }
    return 0;
}
