#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

uint32_t x2i(uint8_t *hexstring) {
	uint32_t x = 0;
	for(uint8_t pos =0; pos<6; pos++) {
		char c = *hexstring;
		if (c >= '0' && c <= '9') {
		  x *= 16;
		  x += c - '0';
		}
		else if (c >= 'A' && c <= 'F') {
		  x *= 16;
		  x += (c - 'A') + 10;
		} else {
		  printf("hiero");
		  return 0;
		}
		hexstring++;
	}
	return x;
}

int main() {
    uint8_t mytest[7] = {'s','F','F','1', '0', 'A', '3'};
    uint8_t *pointtest = mytest;
    uint32_t resultnr = 0;
    uint32_t ard;
    int rc;

    printf("Output: %d | %c | %s\n", mytest[0], mytest[0], pointtest);

    
    pointtest++;
    if(sscanf(pointtest, "%x", &resultnr) == 1) {
        printf("Result: %d\n", resultnr);
    }
    
	resultnr = 0;
    resultnr = strtoul(pointtest, NULL, 16);
    printf("Result: %d\n", resultnr);

	resultnr = 0;
	resultnr = x2i(pointtest);
    printf("Result: %d\n", resultnr);

    return 0;
}
