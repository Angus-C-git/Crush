#ifndef CAN_H
#define CAN_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

/**
* Stores the header like 
* components of a CAN
* for use throughout the 
* program.
*/
struct CAN_Struct {
    int magic_number;
    long mode;
    int path_length;
    uint64_t content_length;
    uint8_t hash;
    long next_CAN;
};

typedef struct CAN_Struct *CAN;



/**
* Creates a new empty CAN
* struct and returns it.
*/
CAN new_CAN(void);


/**
* Builds the header components of a CAN
* given a file_ptr and an emptye CAN.
*/
CAN build_CAN(CAN CAN, FILE *file_ptr);


/**
*  TODO: Some of these might become static if not
*   needed in the main program.
*/


/**
* Reads the bytes of a CAN pathname
* and returns the read number of bytes
* as a string.
*/
char *read_CAN_path_name(CAN CAN, FILE *file_ptr);


/**
* Writes the contents of an extracted 
* CAN to disk given a file pointer 
* and a file name.
*/
void write_extracted_CAN(FILE *file_ptr, CAN canbete, char *file_name);

/**
* Gets the mode/permisions associates
* with a given CAN and returns
* it as a long.
*/
long get_CAN_mode(FILE *file_ptr, CAN CAN);


int get_path_length(FILE *file_ptr, CAN CAN);

/**
* Retrives the content length feild 
* of a CAN header and returns 
* it as a 64 bit int.
*/
uint64_t get_content_length(FILE *file_ptr, CAN CAN);


/**
* Writes a CAN to a given can
* file pointer. 
*/
void add_file(FILE *can_file, char *file_path);


/**
* Adds a directory to the supplied can
* file.
*/
void add_dir(FILE *can_file, char *file_path);



#endif