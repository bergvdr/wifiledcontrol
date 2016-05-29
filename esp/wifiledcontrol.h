/*
 * Helper file
 */

/*
 * Convert received string into a number
 */
uint32_t charstr2int(uint8_t *charstr, size_t length, uint8_t base = 10) {
	uint32_t result = 0;
    if (base == 16) {
        for(uint8_t pos =0; pos < length; pos++) {
            char c = *charstr;
            if (c >= '0' && c <= '9') {
                result *= 16;
                result += c - '0';
            }
   			else if (c >= 'A' && c <= 'F' || c>= 'a' && c <= 'f') {
                result *= 16;
                result += c - '0';
			}
            charstr++;
        }
	} else {
        for(uint8_t pos = 0; pos < length; pos++) {
            char c = *charstr;
            if (c >= '0' && c <= '9') {
                result *= base;
                result += c - '0';
            }
            charstr++;
        }
	}
    return result;
}
