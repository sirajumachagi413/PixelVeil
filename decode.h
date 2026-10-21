#ifndef DECODE_H
#define DECODE_H

#include <stdint.h>
#include <stdio.h>
#include "common.h"
#include "types.h"

#define MAX_DECODED_EXTENSION 4

typedef struct _DecodeInfo
{
    char *stego_image_fname;
    FILE *fptr_stego_image;
    char magic_string[20];

    char *output_fname;
    FILE *fptr_output;
    long extn_Size;
    char extn_output_file[MAX_DECODED_EXTENSION + 1];
    uint32_t size_output_file;
    char default_output_fname[32];
} DecodeInfo;

Status read_and_validate_decode_args(char *argv[], DecodeInfo *decInfo);
Status do_decoding(DecodeInfo *decInfo);
Status open_files_for_decode(DecodeInfo *decInfo);
Status skip_bmp_header(FILE *fptr_src_image, FILE *fptr_dest_image);
Status decode_magic_string(char *magic_string, DecodeInfo *decInfo);
Status decode_secret_file_extn(const char *file_extn, DecodeInfo *decInfo);
Status decode_secret_file_size(DecodeInfo *decInfo);
Status decode_secret_file_data(DecodeInfo *decInfo);
Status decode_image_to_data(char *data, int size, FILE *fptr_stego_image);
char decode_lsb_to_byte(char *image_buffer);
int decode_lsb_to_int(char *image_buffer);

#endif
