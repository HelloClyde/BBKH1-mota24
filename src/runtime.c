#include <stddef.h>
/* Freestanding compiler helpers; the firmware C runtime is not linked. */
void *memset(void *destination,int value,size_t size){
    unsigned char *out=destination;
    while(size--)*out++=(unsigned char)value;
    return destination;
}
