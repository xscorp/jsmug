#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>


// Symbols for Base 4 representation system
unsigned char SYMBOLS[] = {0x09 , 0x0a , 0x0d , 0x20};


/*
Function to convert raw bytes to their 8 digit Base4 symbol notation
*/
void bytes_to_base4_symbols(unsigned char* input_bytes , size_t size_input_bytes , unsigned char* base4_string_buffer)
{
    size_t counter = 0;
    for (size_t i = 0; i < size_input_bytes; i++)
    {
        long quotient = input_bytes[i];
        for (int j = 7; j >= 0; j--)
        {
            base4_string_buffer[counter + j] = SYMBOLS[quotient % 4];
            quotient /= 4;
        }
        counter += 8;
    }
}


/*
Utility function to obtain index of an insiginficant byte from SYMBOLS array
*/
int _get_index(unsigned char byte)
{
    for (int i = 0; i < 4; i++)
    {
        if (SYMBOLS[i] == byte)
        {
            return i;
        }
    }
    return -1;
}


/*
Utility function to check if a byte is one of the insignificant bytes
*/
int _is_insignificant_whitespace_byte(unsigned char ch)
{
    for (int i = 0; i < sizeof(SYMBOLS); i++)
    {
        if (ch == SYMBOLS[i])
            return 1;
    }
    return 0;
}


/*
Function to convert 8-digit Base4 symbols back to raw bytes
*/
void base4_symbols_to_bytes(unsigned char* base4_string_buffer , size_t size_base4_string_buffer , unsigned char* byte_string_buffer , size_t* size_output_bytes_buffer)
{
    size_t n = size_base4_string_buffer / 8;
    for (size_t i = 0; i < n; i++)
    {
        unsigned char byte = 0;
        for (int j = 0; j < 8; j++)
        {
            byte = (byte << 2) | (_get_index(base4_string_buffer[i * 8 + j]));
        }
        byte_string_buffer[i] = byte;
    }
    *size_output_bytes_buffer = n;
}


/*
Function to construct JSON payload bytes using Base4 symbol bytes
*/
void construct_json_bytes(unsigned char* base4_string_buffer , size_t size_base4_string_buffer , unsigned char* json_bytes_buffer , size_t* size_json_bytes_buffer , size_t bytes_per_pair)
{
    unsigned char HEAD[] = "{\"data\":[";
    unsigned char TAIL[] =  "]}";
    unsigned char TEMPLATE[] = ",${$\"json\"$:$\"smuggled\"$}$";
    const size_t required_template_copies = ceil(size_base4_string_buffer / (float)(bytes_per_pair * 6)); //6 is the number of '$'

    unsigned char* document_buffer = (unsigned char *)malloc((*size_json_bytes_buffer) * sizeof(unsigned char));

    if (document_buffer == NULL)
    {
        puts("[!] Unable to allocate memory to store document buffer while constructing JSON\n");
        fflush(stdout);
    }

    size_t len_head = strlen((char *)HEAD);
    size_t len_tail = strlen((char *)TAIL);
    size_t len_template = strlen((char *)TEMPLATE);
    size_t offset = 0;

    memcpy(document_buffer + offset , HEAD , len_head);
    offset += len_head;
    
    size_t i;
    for (i = 0; i < required_template_copies; i++)
    {
        // If template payload is being appended for the first time, Dont put heading ","
        if (i == 0)
        {
            memcpy(document_buffer + offset , TEMPLATE + 1 , len_template - 1);
            offset += len_template - 1;
        }
        else
        {
            memcpy(document_buffer + offset , TEMPLATE , len_template);
            offset += len_template;
        }
    }
    memcpy(document_buffer + offset , TAIL , len_tail);
    offset += len_tail;

    size_t size_document_buffer = offset;

    size_t base4_buffer_offset = 0;
    size_t json_buffer_offset = 0;
    size_t document_buffer_offset = 0;
    size_t insignificant_bytes_written = 0;

    for (; document_buffer_offset < size_document_buffer; document_buffer_offset++)
    {
        unsigned char byte = document_buffer[document_buffer_offset];
        if (byte != '$')
        {
            json_bytes_buffer[json_buffer_offset] = byte;
            ++json_buffer_offset;
        }
        else
        {
            if (base4_buffer_offset < size_base4_string_buffer)
            {
                size_t iteration = 0;
                if (size_base4_string_buffer - base4_buffer_offset < bytes_per_pair)
                    iteration = size_base4_string_buffer - base4_buffer_offset;
                else
                    iteration = bytes_per_pair;

                for (size_t j = 0; j < iteration; j++)
                {
                    if (base4_string_buffer[base4_buffer_offset] == '\0')
                        continue;

                    json_bytes_buffer[json_buffer_offset] = base4_string_buffer[base4_buffer_offset];
                    ++insignificant_bytes_written;
                    ++json_buffer_offset;
                    ++base4_buffer_offset;
                }
            }
        }
    }

    *size_json_bytes_buffer = json_buffer_offset;

    free(document_buffer);

    printf("[+] Insignificant Bytes written: %lu\n" , insignificant_bytes_written);
    
}


/*
Function to deconstruct JSON payload bytes in order to retrieve insignificant bytes.
*/
void deconstruct_json_bytes(unsigned char* json_bytes_buffer , size_t size_json_bytes_buffer , unsigned char* output_base4_symbols_buffer , size_t* size_output_base4_symbols_buffer)
{
    size_t counter_output_bytes = 0;
    for (size_t i = 0; i < size_json_bytes_buffer; i++)
    {
        unsigned char byte = json_bytes_buffer[i];
        if (_is_insignificant_whitespace_byte(byte))
            output_base4_symbols_buffer[counter_output_bytes++] = byte;
    }
    *size_output_base4_symbols_buffer = counter_output_bytes;
}


/*
Master function which begins the process of generating JSON bytes out of raw bytes
*/
size_t encode(unsigned char* input_bytes_buffer , size_t size_input_bytes_buffer , unsigned char* output_bytes_buffer , size_t bytes_per_pair)
{
    /*This function receives pointers to input and output bytes buffer. It writes the JSON-payload on output_bytes_buffer
    and returns the size of written bytes on output_bytes_buffer.*/


    // Allocate buffer to temporarily store base4 symbols
    size_t size_base4_string_buffer = (size_input_bytes_buffer * 8) + 1;
    unsigned char* base4_string_buffer = (unsigned char *)malloc(size_base4_string_buffer);
    if (base4_string_buffer == NULL)
    {
        perror("[!] Error allocating memory to store base4 string!");
        return 0;
    }

    
    // Converting bytes to base4 string representation
    bytes_to_base4_symbols(input_bytes_buffer , size_input_bytes_buffer , base4_string_buffer);


    // construct_json_bytes will return bytes of JSON document and also updates size_output_bytes_buffer
    size_t size_output_bytes_buffer; 
    construct_json_bytes(base4_string_buffer , size_base4_string_buffer , output_bytes_buffer , &size_output_bytes_buffer , bytes_per_pair);

    // Free the allocated memory before returning
    free(base4_string_buffer);

    return size_output_bytes_buffer;
}


/*
Master function to decode JSON payload bytes back to raw bytes
*/
size_t decode(unsigned char* input_bytes_buffer , size_t size_input_bytes_buffer , unsigned char* output_bytes_buffer)
{
    /*This function receives pointers to input and output bytes buffer. It writes the decoded bytes on output_bytes_buffer
    and returns the size of written bytes on output_bytes_buffer.*/

    // Allocate memory to temprorily store obtained base4 symbol bytes
    size_t size_base4_symbols_buffer = 0;
    unsigned char* base4_symbols_buffer = (unsigned char *)malloc(size_input_bytes_buffer * sizeof(unsigned char));
    
    if (base4_symbols_buffer == NULL)
    {
        perror("[!] Error allocating memory to store base4 symbols obtained from JSON!");
        return 0;
    }

    deconstruct_json_bytes(input_bytes_buffer , size_input_bytes_buffer , base4_symbols_buffer , &size_base4_symbols_buffer);


    size_t size_output_bytes_buffer;
    base4_symbols_to_bytes(base4_symbols_buffer , size_base4_symbols_buffer , output_bytes_buffer , &size_output_bytes_buffer);


    // Free the temprorily allocated memory
    free(base4_symbols_buffer);

    return size_output_bytes_buffer;
}



int main(int argc , char *argv[])
{
    const char *choice = argv[1];

    if (strncmp(choice , "encode" , 6) == 0)
    {
        char *input_file_name = argv[2];
        char *output_file_name = argv[3];
        size_t bytes_per_pair;
        
        if (strlen(argv[4]))
            bytes_per_pair = (size_t) strtoul(argv[4] , NULL , 10);
        else
            bytes_per_pair = 20;

        size_t input_file_size;
        unsigned char *input_bytes_buffer;

        // Reading bytes from input file
        FILE *if_ptr;
        if_ptr = fopen(input_file_name , "rb");
        if (if_ptr == NULL)
        {
            perror("[!] Error opening input file!\n");
            return 1;
        }
        fseek(if_ptr , 0 , SEEK_END);
        input_file_size = ftell(if_ptr);
        rewind(if_ptr);

        // Allocate memory to store bytes from file
        input_bytes_buffer = (unsigned char *)malloc(input_file_size * sizeof(unsigned char));
        if (input_bytes_buffer == NULL)
        {
            perror("[!] Error allocating memory to store input bytes\n");
            fclose(if_ptr);
            return 1;
        }
        size_t size_input_bytes_buffer = fread(input_bytes_buffer , sizeof(unsigned char) , input_file_size , if_ptr);
        fclose(if_ptr);
        printf("[+] Bytes read from input file: %lu\n" , size_input_bytes_buffer);


        size_t size_base4_string_buffer = (size_input_bytes_buffer * 8) + 1;

        /* Next few lines of code calculates the maximum size the JSON payload can have. 
        This calculation is done to avoid unnecessarly allocating huge amount of memory to store JSON bytes.
        JSON bytes can be large in size, so instead of allocating a huge memory chunk in heap, calculation is done */
        
        size_t payload_bytes_per_template = 6 * bytes_per_pair; // 6 is the number of '$' characters in a single payload template

        // Number of payload copies required can be calculated by dividing the size of base4 payload with payload bytes required in one template
        size_t required_template_copies = ceil(size_base4_string_buffer / (float)payload_bytes_per_template);

        /* The size of JSON template is 25 bytes(without comma included). So, to effectively calculate max size,
        We subtract 6 (number of '$' bytes in a single payload template) from the size of single payload template and multiply it with required copies.
        Since we didn't took "," character in consideration which will come n-1 times between the payloads, Thats why we again add
        required_template_copies - 1. Ultimately we add 11 which is the size of HEAD and TAIL of the final JSON document */
        size_t maxsize_output_bytes_buffer = (25 - 6 + payload_bytes_per_template) * required_template_copies + (required_template_copies - 1) + 11;

        // This variable stores that actual size of json bytes buffer which is calculated by the construct_json_bytes function.
        size_t size_output_bytes_buffer = 0;

        unsigned char* output_bytes_buffer = (unsigned char *)malloc(maxsize_output_bytes_buffer * sizeof(unsigned char));
        
        if (output_bytes_buffer == NULL)
        {
            perror("[!] Error allocating memory to store json bytes buffer!");
            free(input_bytes_buffer);
            return 0;
        }

        // Call encode() to obtain JSON bytes
        size_output_bytes_buffer = encode(input_bytes_buffer , size_input_bytes_buffer , output_bytes_buffer , bytes_per_pair);


        // Write the JSON encoded payload to file
        FILE *ec_ptr = fopen(output_file_name , "wb");
        size_t ec_bytes_written = fwrite(output_bytes_buffer , sizeof(unsigned char) , size_output_bytes_buffer , ec_ptr);
        printf("[+] JSON encoded bytes written: %lu\n" , ec_bytes_written);
        fclose(ec_ptr);


        free(output_bytes_buffer);
        free(input_bytes_buffer);

    }

    else if (strncmp(choice , "decode" , 6) == 0)
    {
        char *input_file_name = argv[2];
        char *output_file_name = argv[3];

        size_t input_file_size;
        unsigned char *input_bytes_buffer;

        FILE *if_ptr;
        if_ptr = fopen(input_file_name , "rb");
        if (if_ptr == NULL)
        {
            perror("[!] Error opening input JSON file!\n");
            return 1;
        }
        fseek(if_ptr , 0 , SEEK_END);
        input_file_size = ftell(if_ptr);
        rewind(if_ptr);

        // Allocate memory to store bytes from input file
        input_bytes_buffer = (unsigned char *)malloc(input_file_size * sizeof(unsigned char));
        if (input_bytes_buffer == NULL)
        {
            perror("[!] Error allocating memory to store input JSON bytes\n");
            fclose(if_ptr);
            return 1;
        }
        size_t size_input_bytes_buffer = fread(input_bytes_buffer , sizeof(unsigned char) , input_file_size , if_ptr);
        fclose(if_ptr);
        printf("[+] Bytes read from Input file: %lu\n" , size_input_bytes_buffer);


        // Allocate memory to store retrieved bytes
        size_t maxsize_output_bytes_buffer = size_input_bytes_buffer / 8; // Approximate size of output bytes (don't wanna do the calc)
        unsigned char* output_bytes_buffer = (unsigned char *)malloc(maxsize_output_bytes_buffer);
        if (output_bytes_buffer == NULL)
        {
            perror("[!] Error allocating memory to store output bytes!\n");
            free(input_bytes_buffer);
            return 1;
        }

        size_t size_output_bytes_buffer = decode(input_bytes_buffer , size_input_bytes_buffer , output_bytes_buffer);

        // Write output bytes to output file
        FILE *of_ptr = fopen(output_file_name , "wb");
        size_t bytes_written = fwrite(output_bytes_buffer , sizeof(unsigned char) , size_output_bytes_buffer , of_ptr);
        printf("[+] Raw bytes written: %lu\n" , bytes_written);
        fclose(of_ptr);


        free(output_bytes_buffer);
        free(input_bytes_buffer);
    }

    else
    {
        printf("[!] Invalid choice! Please choose either [encode|decode]\n");
        return 1;
    }
    
    return 0;
}

