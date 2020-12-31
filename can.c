#include "can.h"
#include "crush.h"

// the first byte of every CAN has this value
#define CAN_MAGIC_NUMBER          0x42

// number of bytes in fixed-length CAN fields
#define CAN_MAGIC_NUMBER_BYTES    1
#define CAN_MODE_LENGTH_BYTES     3
#define CAN_PATHNAME_LENGTH_BYTES 2
#define CAN_CONTENT_LENGTH_BYTES  6
#define CAN_HASH_BYTES            1

// maximum number of bytes in variable-length CAN fields
#define CAN_MAX_PATHNAME_LENGTH   65535
#define CAN_MAX_CONTENT_LENGTH    281474976710655

/////////////////////// Function Prototypes /////////////////////////////////////
// static uint8_t calculate_prelim_hash(CAN CAN);
// static int is_dir(FILE *file_ptr);
// static CAN create_CAN_header(FILE *CAN);
static struct stat get_stat(char *file_path);
static void traverse_dir(FILE *can_file, char *file_path);
static void write_file(FILE *can_file, char *file);
static uint8_t write_magic(FILE *can, uint8_t hash);
static uint8_t write_mode(FILE *can, uint8_t hash, struct stat);
static uint8_t write_pathname_length(FILE *can, uint8_t hash, char *path_name);
static uint8_t write_content_length(FILE *can, uint8_t hash, struct stat); 
static uint8_t write_pathname(FILE *can, uint8_t hash, char *path_name);
static uint8_t write_contents(FILE *can, uint8_t hash, char *file_to_write);
/////////////////////////////////////////////////////////////////////////////////

/**
* Creates a new empty CAN
* struct and returns it for use
* in building a CAN.
*/
CAN new_CAN(void) {
    CAN CAN = malloc(sizeof(*CAN));

    return CAN;
}


/**
* Builds the header components of a CAN
* given a file_ptr and an emptye CAN.
*/
CAN build_CAN(CAN CAN, FILE *file_ptr) {

    int component = 0;
    int byte = 0;
    int hash = 0;
    for (int byte_count = 0; byte_count <= 4; byte_count++) {

        switch (component) {
            case 0:
                byte = fgetc(file_ptr);
                // If byte is eof
                // exit instead.
                if (byte == EOF) 
                    exit(0);
                CAN->magic_number = byte;
                if (CAN->magic_number != CAN_MAGIC_NUMBER) 
                    handle_error("Magic byte of CAN incorrect");
                CAN->hash = crush_hash(hash, byte);
                component = 1;
                break;
            case 1:
                CAN->mode = get_CAN_mode(file_ptr, CAN);
                component = 2;
                break;
            case 2:
                CAN->path_length = get_path_length(file_ptr, CAN);
                component = 3;
                break;
            case 3:
                CAN->content_length = get_content_length(file_ptr, CAN);
                component = 4;
                break;
            case 4:
                break;
            default:
                handle_error("Hit an unhandeled case");
                break;
        }
    }

    return CAN;
}


/**
* Reads the bytes of a CAN pathname
* and returns the read number of bytes
* as a string.
*/
char *read_CAN_path_name(CAN CAN, FILE *file_ptr) {
    char *path_name = NULL; 
    path_name = calloc(CAN_MAX_PATHNAME_LENGTH, sizeof(char));

    for (int count = 0; count < CAN->path_length; count++) {
        path_name[count] = fgetc(file_ptr);
        CAN->hash = crush_hash(CAN->hash, path_name[count]);
    }

    return path_name;
}


/**
* Writes the contents of an extracted 
* CAN to disk given a file pointer 
* and a file name with the permissions
* defined in mode.
*/
void write_extracted_CAN(FILE *file_ptr, CAN CAN, char *file_name) {
    mode_t mode = CAN->mode;
    
    // Check if it already exists.
    if (S_ISDIR(mode)) {
        // Create a dir to check for
        // existance of dir.
        DIR *dir = opendir(file_name);
        // if the dir exists return otherwise
        // create it.
        if (dir) {
            return;
        } else if (ENOENT == errno) { // from stackoverflow
            printf("Creating directory: %s\n", file_name);
            
            if (mkdir(file_name, mode) != 0) 
                handle_error("Failed to make directory");

            return;
        } else {
            handle_error("Failed to open dir");
        }
    }

    printf("Extracting: %s\n", file_name);
    if(access(file_name, R_OK ) != -1 ) // From stack overflow
        handle_error(strcat(file_name, " Permission denied"));
    
    // file doesn't exist.
    FILE *new_file = fopen(file_name, "w");
    
    int crt_byte = 0;
    for (int byte = 0; byte < CAN->content_length; byte++) {
        crt_byte = fgetc(file_ptr);
        fputc(crt_byte, new_file);
        CAN->hash = crush_hash(CAN->hash, crt_byte);
    }

    if (chmod(file_name, mode) != 0) 
        handle_error("Failed to change permissions");
    
    fclose(new_file);
}


/**
* Gets the mode/permisions associated
* with a given CAN and returns
* it as a long. Computes updated hash and stores
* it in the CAN hash feild for use
* in error checking.
*/
long get_CAN_mode(FILE *file_ptr, CAN CAN) {
    long mode = 0;
    int byte;
    int sub = 2;
    int mode_byte = 0;
    for (byte = 0; byte < CAN_MODE_LENGTH_BYTES; byte++, sub--) {
        mode_byte = fgetc(file_ptr);
        mode |= (mode_byte << (sub * 8)); 
        CAN->hash = crush_hash(CAN->hash, mode_byte);
    }

    return mode;
}


/**
* Retrives a given CANs path name
* length header feild and returns it as
* a int. Computes updated hash and stores
* it in the CAN hash feild for use
* in error checking.
*/
int get_path_length(FILE *file_ptr, CAN CAN) {

    int path_length = 0;
    int byte, sub;
    int path_bytes = 0;
    for (byte = 0, sub = 1; byte < CAN_PATHNAME_LENGTH_BYTES; byte++, sub--) {
        path_bytes = fgetc(file_ptr);
        path_length |= (path_bytes << (sub * 8)); 
        CAN->hash = crush_hash(CAN->hash, path_bytes);
    }

    return path_length;
}


/**
* Retrives the content length feild
* of a CAN header and returns 
* it as a 64 bit int. Also updates the
* CANs hash value for using in
* error checking.
*/
uint64_t get_content_length(FILE *file_ptr, CAN CAN) {
    uint64_t content_length = 0;
    int byte;
    int sub;
    uint64_t fp_byte = 0;
    for (byte = 0, sub = 5; byte < CAN_CONTENT_LENGTH_BYTES; byte++, sub--) {
        fp_byte = fgetc(file_ptr);
        content_length |= (fp_byte << (sub * 8)); 
        CAN->hash = crush_hash(CAN->hash, fp_byte);
    }

    return content_length;
}


///////////////////////////////////////////////////////////////////////////////////


/**
* A wrapper function which is used to 
* handel adding of directories to can.
* Assists in creating a simple interface
* to CAN.c for use in the main 
* program.
*/
void add_dir(FILE *can_file, char *file_path) {

    struct stat file_stat = get_stat(file_path);

    if (S_ISDIR(file_stat.st_mode)) {
        traverse_dir(can_file, file_path);    
    }
}


/**
* Retrive the stats struct given a file path
* and return it.
*/
static struct stat get_stat(char *file_path) {

    struct stat s;
    if (stat(file_path, &s) != 0) {
        handle_error("failed to get struct stats");
    }

    return s;
}


/**
* Recursively traverses down a direcotry to discovery
* subdirectories and files to add to a given can.
*/
static void traverse_dir(FILE *can_file, char *file_path) {
    char running_path[CAN_MAX_PATHNAME_LENGTH];
    struct dirent *dir;
    struct stat s = get_stat(file_path);

    DIR *dir_ptr = opendir(file_path);
    
    if (!dir_ptr)
        handle_error("couldn't open dir");
    
    if (file_path[strlen(file_path) -1 ] != '/')
        strcat(file_path, "/");

    while ( (dir = readdir(dir_ptr)) ) {
        char *current = dir->d_name;

        // if file is hiddel file or last directory
        // dont add and skip to next file.
        if (strcmp(current, ".") == 0 || strcmp(current, "..") == 0)
            continue;

        strcpy(running_path, file_path);
        strcat(running_path, current);
        printf("Adding: %s\n", running_path);
        
        s = get_stat(running_path);
        write_file(can_file, running_path);
        
        // if the file is a subdirectory
        // explore it.
        if (S_ISDIR(s.st_mode)) {
            strcat(running_path, "/");
            traverse_dir(can_file, running_path);
        } 
    }

    closedir(dir_ptr);
}


/**
* A wrapper function which is used to
* add files to the given can. Allows
* the CAN interface to be simplistic.
*/
void add_file(FILE *can_file, char *dir_path) {
    printf("Adding: %s\n", dir_path);
    write_file(can_file, dir_path);
}


/**
* Writes a supplied file along with the files 
* CAN header to a can file. Acts as a 
* wrapper for numerous subroutines which build
* out the header and body of a CAN.
*/
static void write_file(FILE *can_file, char *file) {

    struct stat file_stat = get_stat(file);

    uint8_t hash = 0;
    
    hash = write_magic(can_file, hash);
    hash = write_mode(can_file, hash, file_stat);
    hash = write_pathname_length(can_file, hash, file);

    hash = write_content_length(can_file, hash, file_stat);
    
    hash = write_pathname(can_file, hash, file);
    
    // Don't write dir contents which 
    // should always be 0 in size.
    if (!S_ISDIR(file_stat.st_mode))
        hash = write_contents(can_file, hash, file); 

    // Add the final hash for
    // the CAN.
    fputc(hash, can_file);
}


/**
* Writes the magic number of a CAN, additionally
* returning the updated hash for error checking in
* extraction subroutines.
*/
static uint8_t write_magic(FILE *can, uint8_t hash) {
    fputc(CAN_MAGIC_NUMBER, can);
    return crush_hash(hash, 0x42);
}


/**
* Writes the mode bytes for a given file to a supplied can. Computes
* the updated hash and returns for error checking in extraction
* subroutines.
*/
static uint8_t write_mode(FILE *can, uint8_t hash, struct stat s) {
    int mode_byte = 0;
    
    long mode = s.st_mode;
    int sub = 2;

    for (int byte = 0; byte < CAN_MODE_LENGTH_BYTES; byte++, sub--) {
        mode_byte = mode >> (sub * 8);
        fputc(mode_byte, can);
        hash = crush_hash(hash, mode_byte);
    }

    return hash;
}


/**
* Writes the path_length header feild for a supplied
* CAN to a given can. Computes the updated 
* hash and returns it for error checking in 
* extraction subroutines.
*/
static uint8_t write_pathname_length(FILE *can, uint8_t hash, char *path_name) {
    int sub, byte;
    int path_length = strlen(path_name);
    int path_byte = 0;
    
    for (byte = 0, sub = 1; byte < CAN_PATHNAME_LENGTH_BYTES; byte++, sub--) {
        path_byte = path_length >> (sub * 8);
        fputc(path_byte, can);
        hash = crush_hash(hash, path_byte);
    }  
    return hash;
}


/**
* Writes the content_length header feild for a supplied
* CAN. Computes the updated hash and returns it for
* error checking in extraction subroutines.
*/
static uint8_t write_content_length(FILE *can, uint8_t hash, struct stat s) {
    
    // Fill directory bytes with 0s.
    if (S_ISDIR(s.st_mode)) {
        for (int d = 0; d < CAN_CONTENT_LENGTH_BYTES; d++) {
            fputc(0, can);
            hash = crush_hash(hash, 0);
        }
        return hash;
    }

    // Write content length as specified 
    // in stat.st_size.
    uint64_t content_length = s.st_size;
    int byte;
    int sub;
    uint64_t ct_byte = 0;
    for (byte = 0, sub = 5; byte < CAN_CONTENT_LENGTH_BYTES; byte++, sub--) {
        ct_byte = content_length >> (sub * 8);
        fputc(ct_byte, can);
        hash = crush_hash(hash, ct_byte);
    }    
    
    return hash;
}


/**
* Writes the pathname for a given CAN being
* added to a can. Advances the can file pointer foward
* in order to maintain canbies archive format. 
*
* Also returns the updated hash to reflect the added 
* content bytes.
*/
static uint8_t write_pathname(FILE *can, uint8_t hash, char *path_name) {
    
    for (int c = 0; c < strlen(path_name); c++) {
        fputc(path_name[c], can);
        hash = crush_hash(hash, path_name[c]);
    }
    
    return hash;
}


/**
* Writes the contents of a file to be added
* to a given can file stream. Computes the 
* updated hash for use in error checking 
* in later extraction subroutines.
*/
static uint8_t write_contents(FILE *can, uint8_t hash, char *file_to_write) {
    int c;
    FILE *input_stream = fopen(file_to_write, "r");

    if (!input_stream)
        handle_error("Failed to open file steam");

    while ( (c = fgetc(input_stream)) != EOF ) {
        fputc(c, can);
        hash = crush_hash(hash, c);
    }

    fclose(input_stream);
    return hash;
}
