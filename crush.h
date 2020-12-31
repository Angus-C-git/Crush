#ifndef crush_H
#define crush_H

/**
* Wrapper for implementation in crush.c
*/
uint8_t crush_hash(uint8_t hash, uint8_t byte);

void handle_error(char *error_desc);

#endif