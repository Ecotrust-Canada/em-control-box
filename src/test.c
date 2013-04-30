#include <stdio.h>
#include <string.h>

void main(void) {
char BUF[32] = "5V";
float f;
char c1 = '\0';

sscanf(BUF, "%fV%c", &f, &c1);
printf("f: %f, c1: '%c', c2: %c\n", f, c1, c1);
if(c1) printf("Yes\n");
else printf("No\n");

}
