#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "decode.h"

static int same_file_name(const char *left, const char *right)
{
#ifdef _WIN32
    return _stricmp(left, right) == 0;
#else
    return strcmp(left, right) == 0;
#endif
}

static int seek_to_bmp_pixels(FILE *image)
{
    unsigned char header[14];
    uint32_t pixel_offset;
    long file_size;

    if (fseek(image, 0, SEEK_SET) != 0 ||
        fread(header, 1, sizeof(header), image) != sizeof(header) ||
        header[0] != 'B' || header[1] != 'M' ||
        fseek(image, 0, SEEK_END) != 0)
    {
        return 0;
    }

    file_size = ftell(image);
    pixel_offset = (uint32_t)header[10] |
                   ((uint32_t)header[11] << 8) |
                   ((uint32_t)header[12] << 16) |
                   ((uint32_t)header[13] << 24);
    return file_size >= 0 && pixel_offset >= sizeof(header) &&
           (unsigned long)pixel_offset <= (unsigned long)file_size &&
           fseek(image, (long)pixel_offset, SEEK_SET) == 0;
}

static int read_uint32(FILE *image, uint32_t *value)
{
    unsigned char buffer[32];
    uint32_t result = 0;
    unsigned int bit;

    if (fread(buffer, 1, sizeof(buffer), image) != sizeof(buffer))
    {
        return 0;
    }
    for (bit = 0; bit < 32; bit++)
    {
        result = (result << 1) | (buffer[bit] & 1);
    }
    *value = result;
    return 1;
}

Status read_and_validate_decode_args(char *argv[], DecodeInfo *decInfo)
{
    size_t image_length = strlen(argv[2]);

    if (image_length < 4 || strcmp(argv[2] + image_length - 4, ".bmp") != 0)
    {
        fprintf(stderr, "Error: stego image must have a .bmp extension\n");
        return e_failure;
    }

    memset(decInfo, 0, sizeof(*decInfo));
    decInfo->stego_image_fname = argv[2];
    decInfo->output_fname = argv[3];
    return e_success;
}

Status open_files_for_decode(DecodeInfo *decInfo)
{
    decInfo->fptr_stego_image = fopen(decInfo->stego_image_fname, "rb");
    if (decInfo->fptr_stego_image == NULL)
    {
        perror(decInfo->stego_image_fname);
        return e_failure;
    }
    if (!seek_to_bmp_pixels(decInfo->fptr_stego_image))
    {
        fprintf(stderr, "Error: input is not a supported BMP image\n");
        fclose(decInfo->fptr_stego_image);
        decInfo->fptr_stego_image = NULL;
        return e_failure;
    }
    return e_success;
}

Status do_decoding(DecodeInfo *decInfo)
{
    Status status = e_failure;
    size_t extension_size;
    int image_close_status;

    if (open_files_for_decode(decInfo) != e_success)
    {
        return e_failure;
    }

    if (decode_magic_string(MAGIC_STRING, decInfo) != e_success ||
        decode_secret_file_extn(decInfo->extn_output_file, decInfo) != e_success ||
        decode_secret_file_size(decInfo) != e_success)
    {
        goto cleanup;
    }

    if (decInfo->output_fname == NULL)
    {
        extension_size = strlen(decInfo->extn_output_file);
        memcpy(decInfo->default_output_fname, "decoded", 7);
        memcpy(decInfo->default_output_fname + 7, decInfo->extn_output_file,
               extension_size + 1);
        decInfo->output_fname = decInfo->default_output_fname;
    }
    if (same_file_name(decInfo->stego_image_fname, decInfo->output_fname))
    {
        fprintf(stderr, "Error: output file must differ from the stego image\n");
        goto cleanup;
    }

    decInfo->fptr_output = fopen(decInfo->output_fname, "wb");
    if (decInfo->fptr_output == NULL)
    {
        perror(decInfo->output_fname);
        goto cleanup;
    }
    if (decode_secret_file_data(decInfo) == e_success)
    {
        status = e_success;
    }
    if (fclose(decInfo->fptr_output) != 0)
    {
        fprintf(stderr, "Error: failed to close decoded output file\n");
        status = e_failure;
    }
    decInfo->fptr_output = NULL;

cleanup:
    image_close_status = fclose(decInfo->fptr_stego_image);
    decInfo->fptr_stego_image = NULL;
    if (image_close_status != 0)
    {
        fprintf(stderr, "Error: failed to close stego image\n");
        status = e_failure;
    }
    return status;
}

Status skip_bmp_header(FILE *fptr_src_image, FILE *fptr_dest_image)
{
    unsigned char header[14];
    uint32_t pixel_offset;

    if (fseek(fptr_src_image, 0, SEEK_SET) != 0 ||
        fread(header, 1, sizeof(header), fptr_src_image) != sizeof(header) ||
        header[0] != 'B' || header[1] != 'M')
    {
        return e_failure;
    }

    pixel_offset = (uint32_t)header[10] |
                   ((uint32_t)header[11] << 8) |
                   ((uint32_t)header[12] << 16) |
                   ((uint32_t)header[13] << 24);
    if (pixel_offset < sizeof(header) ||
        fseek(fptr_src_image, (long)pixel_offset, SEEK_SET) != 0)
    {
        return e_failure;
    }
    if (fptr_dest_image != NULL &&
        fseek(fptr_dest_image, (long)pixel_offset, SEEK_SET) != 0)
    {
        return e_failure;
    }
    return e_success;
}

Status decode_magic_string(char *magic_string, DecodeInfo *decInfo)
{
    size_t size = strlen(magic_string);

    if (size >= sizeof(decInfo->magic_string) ||
        decode_image_to_data(decInfo->magic_string, (int)size,
                             decInfo->fptr_stego_image) != e_success)
    {
        return e_failure;
    }
    decInfo->magic_string[size] = '\0';
    if (memcmp(decInfo->magic_string, magic_string, size) != 0)
    {
        fprintf(stderr, "Error: no supported hidden data found in image\n");
        return e_failure;
    }
    return e_success;
}

Status decode_secret_file_extn(const char *file_extn, DecodeInfo *decInfo)
{
    uint32_t extension_size;
    size_t index;

    if (!read_uint32(decInfo->fptr_stego_image, &extension_size) ||
        extension_size < 2 || extension_size > MAX_DECODED_EXTENSION ||
        decode_image_to_data(decInfo->extn_output_file, (int)extension_size,
                             decInfo->fptr_stego_image) != e_success)
    {
        fprintf(stderr, "Error: invalid or truncated embedded file extension\n");
        return e_failure;
    }
    decInfo->extn_output_file[extension_size] = '\0';
    if (decInfo->extn_output_file[0] != '.')
    {
        fprintf(stderr, "Error: invalid embedded file extension\n");
        return e_failure;
    }
    for (index = 1; index < extension_size; index++)
    {
        unsigned char character = (unsigned char)decInfo->extn_output_file[index];
        if (!((character >= 'a' && character <= 'z') ||
              (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9')))
        {
            fprintf(stderr, "Error: invalid embedded file extension\n");
            return e_failure;
        }
    }
    decInfo->extn_Size = extension_size;
    (void)file_extn;
    return e_success;
}

Status decode_secret_file_size(DecodeInfo *decInfo)
{
    uint32_t file_size;
    long data_start;
    long image_end;

    if (!read_uint32(decInfo->fptr_stego_image, &file_size))
    {
        fprintf(stderr, "Error: invalid or truncated embedded file size\n");
        return e_failure;
    }
    data_start = ftell(decInfo->fptr_stego_image);
    if (data_start < 0 || fseek(decInfo->fptr_stego_image, 0, SEEK_END) != 0 ||
        (image_end = ftell(decInfo->fptr_stego_image)) < data_start ||
        fseek(decInfo->fptr_stego_image, data_start, SEEK_SET) != 0 ||
        (uint64_t)file_size > (uint64_t)(image_end - data_start) / 8)
    {
        fprintf(stderr, "Error: embedded file size exceeds available image data\n");
        return e_failure;
    }
    decInfo->size_output_file = file_size;
    return e_success;
}

Status decode_secret_file_data(DecodeInfo *decInfo)
{
    unsigned char image_buffer[8];
    uint32_t remaining = decInfo->size_output_file;

    while (remaining > 0)
    {
        if (fread(image_buffer, 1, sizeof(image_buffer), decInfo->fptr_stego_image) !=
            sizeof(image_buffer))
        {
            fprintf(stderr, "Error: stego image ended before hidden data was complete\n");
            return e_failure;
        }
        {
            unsigned char value = (unsigned char)decode_lsb_to_byte((char *)image_buffer);
            if (fwrite(&value, 1, 1, decInfo->fptr_output) != 1)
            {
                fprintf(stderr, "Error: failed to write decoded data\n");
                return e_failure;
            }
        }
        remaining -= 1;
    }
    return e_success;
}

Status decode_image_to_data(char *data, int size, FILE *fptr_stego_image)
{
    unsigned char image_buffer[8];
    int index;

    if (size < 0)
    {
        return e_failure;
    }
    for (index = 0; index < size; index++)
    {
        if (fread(image_buffer, 1, sizeof(image_buffer), fptr_stego_image) !=
            sizeof(image_buffer))
        {
            return e_failure;
        }
        data[index] = decode_lsb_to_byte((char *)image_buffer);
    }
    return e_success;
}

char decode_lsb_to_byte(char *image_buffer)
{
    unsigned char value = 0;
    unsigned int bit;

    for (bit = 0; bit < 8; bit++)
    {
        value = (unsigned char)((value << 1) |
                                ((unsigned char)image_buffer[bit] & 1));
    }
    return (char)value;
}

int decode_lsb_to_int(char *image_buffer)
{
    uint32_t value = 0;
    unsigned int bit;

    for (bit = 0; bit < 32; bit++)
    {
        value = (value << 1) | ((unsigned char)image_buffer[bit] & 1);
    }
    return (int)value;
}
