
/**
* crush.c => Driver for file archiver ops
*/


#include "can.h"

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


// ADD YOUR #defines HERE


typedef enum action {
    a_invalid,
    a_list,
    a_extract,
    a_create
} action_t;


void usage(char *myname);
action_t process_arguments(int argc, char *argv[], char **can_pathname,
                           char ***pathnames, int *compress_can);
void list_can(char *can_pathname);
void extract_can(char *can_pathname);
void create_can(char *can_pathname, char *pathnames[], int compress_can);
uint8_t crush_hash(uint8_t hash, uint8_t byte);


////////////////////////////////////////////////////////////////////////////////
void handle_error(char *error_desc);
////////////////////////////////////////////////////////////////////////////////


int main(int argc, char *argv[]) {
    char *can_pathname = NULL;
    char **pathnames = NULL;
    int compress_can = 0;
    action_t action = process_arguments(argc, argv, &can_pathname, &pathnames,
                                        &compress_can);

    switch (action) {
    case a_list:
        list_can(can_pathname);
        break;

    case a_extract:
        extract_can(can_pathname);
        break;

    case a_create:
        create_can(can_pathname, pathnames, compress_can);
        break;

    default:
        usage(argv[0]);
    }

    return 0;
}

// print a usage message and exit

void usage(char *myname) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\t%s -l <can-file>\n", myname);
    fprintf(stderr, "\t%s -x <can-file>\n", myname);
    fprintf(stderr, "\t%s [-z] -c <can-file> pathnames [...]\n", myname);
    exit(1);
}

// process command-line arguments
// check we have a valid set of arguments
// and return appropriate action
// *can_pathname set to pathname for canfile
// *pathname and *compress_can set for create action

action_t process_arguments(int argc, char *argv[], char **can_pathname,
                           char ***pathnames, int *compress_can) {
    extern char *optarg;
    extern int optind, optopt;
    int create_can_flag = 0;
    int extract_can_flag = 0;
    int list_can_flag = 0;
    int opt;
    while ((opt = getopt(argc, argv, ":l:c:x:z")) != -1) {
        switch (opt) {
        case 'c':
            create_can_flag++;
            *can_pathname = optarg;
            break;

        case 'x':
            extract_can_flag++;
            *can_pathname = optarg;
            break;

        case 'l':
            list_can_flag++;
            *can_pathname = optarg;
            break;

        case 'z':
            (*compress_can)++;
            break;

        default:
            return a_invalid;
        }
    }

    if (create_can_flag + extract_can_flag + list_can_flag != 1) {
        return a_invalid;
    }

    if (list_can_flag && argv[optind] == NULL) {
        return a_list;
    } else if (extract_can_flag && argv[optind] == NULL) {
        return a_extract;
    } else if (create_can_flag && argv[optind] != NULL) {
        *pathnames = &argv[optind];
        return a_create;
    }

    return a_invalid;
}


/**
* Print out the contents of a can
* provided all the cans are valid 
* as determined by their magic number.
*/
void list_can(char *can_pathname) {

    // if(is_compressed(can_pathname)) {
    //     read_compressed_can(can_pathname);
    //     return;
    // }

    FILE *input_stream = fopen(can_pathname, "r");

    if (!input_stream) 
        handle_error("File stream error");
    
    while (input_stream) { 
        CAN CAN = new_CAN();
        CAN = build_CAN(CAN, input_stream);

        printf("%06lo %5lu %s\n", CAN->mode, CAN->content_length, 
                read_CAN_path_name(CAN, input_stream));
        
        // Move to next CAN.
        fseek(input_stream, (long) CAN->content_length + CAN_HASH_BYTES, SEEK_CUR);        
    }

    fclose(input_stream); 
}


/**
* Extracts the contents of a can, writing
* extracted files to disk.
* 
* Performs error checking.
*/
void extract_can(char *can_pathname) {

    FILE *file_ptr = fopen(can_pathname, "r");

    if (!file_ptr)
        handle_error("File stream error");

    char *path_name = NULL;
    int hash_byte = 0;
    // Extract each CAN
    while (file_ptr) {
        CAN CAN = new_CAN();
        CAN = build_CAN(CAN, file_ptr);

        path_name = read_CAN_path_name(CAN, file_ptr);
        
        write_extracted_CAN(file_ptr, CAN, path_name);

        // Check hash integirity.
        hash_byte = fgetc(file_ptr);
        if (hash_byte != CAN->hash) 
            handle_error("can hash incorrect");
    }

    fclose(file_ptr);
}

// create can_pathname from NULL-terminated array pathnames
// compress with xz if compress_can non-zero (subset 4)

void create_can(char *can_pathname, char *pathnames[], int compress_can) {

    // Open can to write
    FILE *can_file = fopen(can_pathname, "w");

    if (!can_file) 
        handle_error("file stream error");

    // Split folder pathnames
    // to descend from file path root.
    char *split_hurstic = "/";
    char *adjusted_path = NULL;
    

    for (int p = 0; pathnames[p]; p++) {
        
        char *on_going_path = malloc(CAN_MAX_PATHNAME_LENGTH * sizeof(char));
        char *pre_path = malloc(CAN_MAX_PATHNAME_LENGTH * sizeof(char));
        char *goal_path = malloc(CAN_MAX_PATHNAME_LENGTH * sizeof(char));

        strcpy(pre_path, pathnames[p]);
        strcpy(goal_path, pathnames[p]);

        // Add directories prior to the one we wish to 
        // traverse or the file we want to add directly.
        adjusted_path = strtok(pre_path, split_hurstic);
        if (adjusted_path)
            strcpy(on_going_path, adjusted_path);

        while (adjusted_path) {
            add_file(can_file, on_going_path);
            strcat(on_going_path, "/");
            adjusted_path = strtok(NULL, split_hurstic);
            if (adjusted_path) 
                strcat(on_going_path, adjusted_path);
        }

        add_dir(can_file, goal_path);

        free(on_going_path);
        free(pre_path);
    }

    // Flush can file.
    fclose(can_file);

    // compress the can if 
    // -z option is supplied.
}


//                                  HELPERS
////////////////////////////////////////////////////////////////////////////////


/**
* prints an error msg to standerr.
*/
void handle_error(char *error_desc){
    fprintf(stderr, "ERROR: %s\n", error_desc);
    exit(1);
}


// Lookup table for a simple Pearson hash

const uint8_t crush_hash_table[256] = {
    241, 18,  181, 164, 92,  237, 100, 216, 183, 107, 2,   12,  43,  246, 90,
    143, 251, 49,  228, 134, 215, 20,  193, 172, 140, 227, 148, 118, 57,  72,
    119, 174, 78,  14,  97,  3,   208, 252, 11,  195, 31,  28,  121, 206, 149,
    23,  83,  154, 223, 109, 89,  10,  178, 243, 42,  194, 221, 131, 212, 94,
    205, 240, 161, 7,   62,  214, 222, 219, 1,   84,  95,  58,  103, 60,  33,
    111, 188, 218, 186, 166, 146, 189, 201, 155, 68,  145, 44,  163, 69,  196,
    115, 231, 61,  157, 165, 213, 139, 112, 173, 191, 142, 88,  106, 250, 8,
    127, 26,  126, 0,   96,  52,  182, 113, 38,  242, 48,  204, 160, 15,  54,
    158, 192, 81,  125, 245, 239, 101, 17,  136, 110, 24,  53,  132, 117, 102,
    153, 226, 4,   203, 199, 16,  249, 211, 167, 55,  255, 254, 116, 122, 13,
    236, 93,  144, 86,  59,  76,  150, 162, 207, 77,  176, 32,  124, 171, 29,
    45,  30,  67,  184, 51,  22,  105, 170, 253, 180, 187, 130, 156, 98,  159,
    220, 40,  133, 135, 114, 147, 75,  73,  210, 21,  129, 39,  138, 91,  41,
    235, 47,  185, 9,   82,  64,  87,  244, 50,  74,  233, 175, 247, 120, 6,
    169, 85,  66,  104, 80,  71,  230, 152, 225, 34,  248, 198, 63,  168, 179,
    141, 137, 5,   19,  79,  232, 128, 202, 46,  70,  37,  209, 217, 123, 27,
    177, 25,  56,  65,  229, 36,  197, 234, 108, 35,  151, 238, 200, 224, 99,
    190
};

// Given the current hash value and a byte
// crush_hash returns the new hash value
uint8_t crush_hash(uint8_t hash, uint8_t byte) {
    return crush_hash_table[hash ^ byte];
}
