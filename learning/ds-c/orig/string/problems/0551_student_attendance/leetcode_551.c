#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

bool checkRecord(const char * s){
    int abesnt_count = 0;
    int late_count = 0;

    for (int i = 0; i < strlen(s); i++) {
        if (s[i] == 'A') {
            abesnt_count++;
            if (abesnt_count >= 2) {
                return false;
            }
        }
        
        if (s[i] == 'L') {
            late_count++;
            if (late_count >= 3) {
                return false;
            }
        } else {
            late_count = 0;
        }
    }

    return  true;
}

int main()
{
    const char *s = "PPALLP";
    bool res = checkRecord(s);

    printf("res is: %d.\n", res);
    return 0;
}