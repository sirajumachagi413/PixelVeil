#include <stdio.h>
#include <string.h>
#include "decode.h"
#include "encode.h"
#include "types.h"

static void print_usage(const char *program)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s -e <image.bmp> <secret.txt> [output.bmp]\n"
            "  %s -d <stego.bmp> [output-file]\n",
            program, program);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return e_failure;
    }

    if (strcmp(argv[1], "-e") == 0)
    {
        EncodeInfo encInfo;
        if (argc < 4 || argc > 5)
        {
            print_usage(argv[0]);
            return e_failure;
        }
        if (read_and_validate_encode_args(argv, &encInfo) != e_success ||
            do_encoding(&encInfo) != e_success)
        {
            return e_failure;
        }
        printf("Encoding completed successfully: %s\n", encInfo.stego_image_fname);
        return e_success;
    }

    if (strcmp(argv[1], "-d") == 0)
    {
        DecodeInfo decInfo;
        if (argc < 3 || argc > 4)
        {
            print_usage(argv[0]);
            return e_failure;
        }
        if (read_and_validate_decode_args(argv, &decInfo) != e_success ||
            do_decoding(&decInfo) != e_success)
        {
            return e_failure;
        }
        printf("Decoding completed successfully: %s\n", decInfo.output_fname);
        return e_success;
    }

    print_usage(argv[0]);
    return e_failure;
}
