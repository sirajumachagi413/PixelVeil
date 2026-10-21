#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "encode.h"

static int same_file_name(const char *left, const char *right)
{
#ifdef _WIN32
    return _stricmp(left, right) == 0;
#else
    return strcmp(left, right) == 0;
#endif
}

OperationType check_operation_type(char *argv[])
{
    if (argv == NULL || argv[1] == NULL)
    {
        return e_unsupported;
    }
    if (strcmp(argv[1], "-e") == 0)
    {
        return e_encode;
    }
    if (strcmp(argv[1], "-d") == 0)
    {
        return e_decode;
    }
    return e_unsupported;
}

static int read_bmp_pixel_data(FILE *image, uint32_t *pixel_offset,
                               uint32_t *pixel_size)
{
    unsigned char header[14];
    long file_size;

    if (fseek(image, 0, SEEK_SET) != 0 ||
        fread(header, 1, sizeof(header), image) != sizeof(header) ||
        header[0] != 'B' || header[1] != 'M' ||
        fseek(image, 0, SEEK_END) != 0)
    {
        return 0;
    }

    file_size = ftell(image);
    *pixel_offset = (uint32_t)header[10] |
                    ((uint32_t)header[11] << 8) |
                    ((uint32_t)header[12] << 16) |
                    ((uint32_t)header[13] << 24);

    if (file_size < 0 || (unsigned long)file_size > UINT32_MAX ||
        *pixel_offset > (uint32_t)file_size || *pixel_offset < sizeof(header) ||
        fseek(image, (long)*pixel_offset, SEEK_SET) != 0)
    {
        return 0;
    }

    *pixel_size = (uint32_t)file_size - *pixel_offset;
    return 1;
}

static Status encode_uint32(uint32_t value, EncodeInfo *encInfo)
{
    unsigned char image_buffer[32];
    unsigned int bit;

    if (fread(image_buffer, 1, sizeof(image_buffer), encInfo->fptr_src_image) !=
            sizeof(image_buffer))
    {
        return e_failure;
    }

    for (bit = 0; bit < 32; bit++)
    {
        image_buffer[bit] = (unsigned char)((image_buffer[bit] & 0xfe) |
                                            ((value >> (31 - bit)) & 1));
    }

    return fwrite(image_buffer, 1, sizeof(image_buffer), encInfo->fptr_stego_image) ==
                   sizeof(image_buffer)
               ? e_success
               : e_failure;
}

uint get_image_size_for_bmp(FILE *fptr_image)
{
    uint32_t offset;
    uint32_t size;

    if (!read_bmp_pixel_data(fptr_image, &offset, &size))
    {
        return 0;
    }
    return size;
}

Status open_files(EncodeInfo *encInfo)
{
    encInfo->fptr_src_image = fopen(encInfo->src_image_fname, "rb");
    if (encInfo->fptr_src_image == NULL)
    {
        perror(encInfo->src_image_fname);
        return e_failure;
    }

    encInfo->fptr_secret = fopen(encInfo->secret_fname, "rb");
    if (encInfo->fptr_secret == NULL)
    {
        perror(encInfo->secret_fname);
        fclose(encInfo->fptr_src_image);
        encInfo->fptr_src_image = NULL;
        return e_failure;
    }

    if (same_file_name(encInfo->src_image_fname, encInfo->stego_image_fname) ||
        same_file_name(encInfo->secret_fname, encInfo->stego_image_fname))
    {
        fprintf(stderr, "Error: output file must differ from both input files\n");
        fclose(encInfo->fptr_src_image);
        fclose(encInfo->fptr_secret);
        encInfo->fptr_src_image = NULL;
        encInfo->fptr_secret = NULL;
        return e_failure;
    }

    return e_success;
}

Status read_and_validate_encode_args(char *argv[], EncodeInfo *encInfo)
{
    size_t image_length = strlen(argv[2]);
    size_t secret_length = strlen(argv[3]);
    const char *extension;

    if (image_length < 4 || strcmp(argv[2] + image_length - 4, ".bmp") != 0)
    {
        fprintf(stderr, "Error: source image must have a .bmp extension\n");
        return e_failure;
    }
    if (secret_length < 5 || strcmp(argv[3] + secret_length - 4, ".txt") != 0)
    {
        fprintf(stderr, "Error: secret file must have a .txt extension\n");
        return e_failure;
    }

    memset(encInfo, 0, sizeof(*encInfo));
    encInfo->src_image_fname = argv[2];
    encInfo->secret_fname = argv[3];
    encInfo->stego_image_fname = argv[4] != NULL ? argv[4] : "stego.bmp";

    extension = strrchr(encInfo->secret_fname, '.');
    if (extension == NULL || strlen(extension) >= sizeof(encInfo->extn_secret_file))
    {
        fprintf(stderr, "Error: unsupported secret file extension\n");
        return e_failure;
    }
    strcpy(encInfo->extn_secret_file, extension);
    return e_success;
}

Status do_encoding(EncodeInfo *encInfo)
{
    Status status = e_failure;
    int close_failed = 0;

    if (open_files(encInfo) != e_success)
    {
        return e_failure;
    }

    if (check_capacity(encInfo) != e_success)
    {
        goto cleanup;
    }
    encInfo->fptr_stego_image = fopen(encInfo->stego_image_fname, "wb");
    if (encInfo->fptr_stego_image == NULL)
    {
        perror(encInfo->stego_image_fname);
        goto cleanup;
    }

    if (copy_bmp_header(encInfo->fptr_src_image, encInfo->fptr_stego_image) == e_success &&
        encode_magic_string(MAGIC_STRING, encInfo) == e_success &&
        encode_secret_file_extn(encInfo->extn_secret_file, encInfo) == e_success &&
        encode_secret_file_size(encInfo->size_secret_file, encInfo) == e_success &&
        encode_secret_file_data(encInfo) == e_success &&
        copy_remaining_img_data(encInfo->fptr_src_image, encInfo->fptr_stego_image) == e_success)
    {
        status = e_success;
    }
    else
    {
        fprintf(stderr, "Error: failed while encoding image data\n");
    }

cleanup:
    if (encInfo->fptr_src_image != NULL && fclose(encInfo->fptr_src_image) != 0)
    {
        close_failed = 1;
    }
    if (encInfo->fptr_secret != NULL && fclose(encInfo->fptr_secret) != 0)
    {
        close_failed = 1;
    }
    if (encInfo->fptr_stego_image != NULL && fclose(encInfo->fptr_stego_image) != 0)
    {
        close_failed = 1;
    }
    if (close_failed)
    {
        fprintf(stderr, "Error: failed to close an encoding file\n");
        status = e_failure;
    }
    encInfo->fptr_src_image = NULL;
    encInfo->fptr_secret = NULL;
    encInfo->fptr_stego_image = NULL;
    return status;
}

Status check_capacity(EncodeInfo *encInfo)
{
    uint32_t offset;
    uint32_t pixel_size;
    unsigned long long required;
    unsigned long long secret_size;
    long end;
    size_t extension_size = strlen(encInfo->extn_secret_file);

    if (!read_bmp_pixel_data(encInfo->fptr_src_image, &offset, &pixel_size))
    {
        fprintf(stderr, "Error: source is not a supported BMP image\n");
        return e_failure;
    }

    if (fseek(encInfo->fptr_secret, 0, SEEK_END) != 0 ||
        (end = ftell(encInfo->fptr_secret)) < 0 ||
        fseek(encInfo->fptr_secret, 0, SEEK_SET) != 0 ||
        end == 0 || (unsigned long)end > UINT32_MAX)
    {
        fprintf(stderr, "Error: secret file must be non-empty and fit in 4 GiB\n");
        return e_failure;
    }
    secret_size = (unsigned long)end;
    required = (2 + 4 + extension_size + 4 + secret_size) * 8;
    if (required > pixel_size)
    {
        fprintf(stderr, "Error: secret file does not fit in the source image\n");
        return e_failure;
    }

    encInfo->image_capacity = pixel_size;
    encInfo->size_secret_file = end;
    return e_success;
}

uint get_file_size(FILE *fptr)
{
    long size;

    if (fseek(fptr, 0, SEEK_END) != 0 || (size = ftell(fptr)) < 0 ||
        fseek(fptr, 0, SEEK_SET) != 0)
    {
        return 0;
    }
    return (uint)size;
}

Status copy_bmp_header(FILE *fptr_src_image, FILE *fptr_dest_image)
{
    unsigned char header[14];
    uint32_t pixel_offset;
    uint32_t pixel_size;

    if (!read_bmp_pixel_data(fptr_src_image, &pixel_offset, &pixel_size) ||
        fseek(fptr_src_image, 0, SEEK_SET) != 0)
    {
        return e_failure;
    }

    while (pixel_offset > 0)
    {
        size_t chunk = pixel_offset < sizeof(header) ? pixel_offset : sizeof(header);
        if (fread(header, 1, chunk, fptr_src_image) != chunk ||
            fwrite(header, 1, chunk, fptr_dest_image) != chunk)
        {
            return e_failure;
        }
        pixel_offset -= (uint32_t)chunk;
    }
    return e_success;
}

Status encode_magic_string(const char *magic_string, EncodeInfo *encInfo)
{
    return encode_data_to_image((char *)magic_string, (int)strlen(magic_string),
                                encInfo->fptr_src_image, encInfo->fptr_stego_image);
}

Status encode_data_to_image(char *data, int size, FILE *fptr_src_image,
                            FILE *fptr_stego_image)
{
    unsigned char image_buffer[8];
    int index;

    if (size < 0)
    {
        return e_failure;
    }
    for (index = 0; index < size; index++)
    {
        if (fread(image_buffer, 1, sizeof(image_buffer), fptr_src_image) !=
            sizeof(image_buffer))
        {
            return e_failure;
        }
        encode_byte_to_lsb(data[index], (char *)image_buffer);
        if (fwrite(image_buffer, 1, sizeof(image_buffer), fptr_stego_image) !=
            sizeof(image_buffer))
        {
            return e_failure;
        }
    }
    return e_success;
}

Status encode_byte_to_lsb(char data, char *image_buffer)
{
    unsigned char value = (unsigned char)data;
    unsigned int bit;

    for (bit = 0; bit < 8; bit++)
    {
        image_buffer[bit] = (char)(((unsigned char)image_buffer[bit] & 0xfe) |
                                   ((value >> (7 - bit)) & 1));
    }
    return e_success;
}

Status encode_int_to_lsb(int data, char *image_buffer)
{
    uint32_t value = (uint32_t)data;
    unsigned int bit;

    for (bit = 0; bit < 32; bit++)
    {
        image_buffer[bit] = (char)(((unsigned char)image_buffer[bit] & 0xfe) |
                                   ((value >> (31 - bit)) & 1));
    }
    return e_success;
}

Status encode_secret_file_size(long file_size, EncodeInfo *encInfo)
{
    return encode_uint32((uint32_t)file_size, encInfo);
}

Status encode_secret_file_extn(const char *file_extn, EncodeInfo *encInfo)
{
    size_t extension_size = strlen(file_extn);

    if (extension_size == 0 || extension_size >= sizeof(encInfo->extn_secret_file) ||
        encode_uint32((uint32_t)extension_size, encInfo) != e_success ||
        encode_data_to_image((char *)file_extn, (int)extension_size,
                             encInfo->fptr_src_image, encInfo->fptr_stego_image) != e_success)
    {
        return e_failure;
    }
    return e_success;
}

Status encode_secret_file_data(EncodeInfo *encInfo)
{
    unsigned char image_buffer[8];
    int value;
    long encoded_size = 0;

    while ((value = fgetc(encInfo->fptr_secret)) != EOF)
    {
        if (fread(image_buffer, 1, sizeof(image_buffer), encInfo->fptr_src_image) !=
            sizeof(image_buffer))
        {
            return e_failure;
        }
        encode_byte_to_lsb((char)value, (char *)image_buffer);
        if (fwrite(image_buffer, 1, sizeof(image_buffer), encInfo->fptr_stego_image) !=
            sizeof(image_buffer))
        {
            return e_failure;
        }
        encoded_size++;
    }
    return !ferror(encInfo->fptr_secret) && encoded_size == encInfo->size_secret_file
               ? e_success
               : e_failure;
}

Status copy_remaining_img_data(FILE *fptr_src, FILE *fptr_dest)
{
    unsigned char buffer[4096];
    size_t count;

    while ((count = fread(buffer, 1, sizeof(buffer), fptr_src)) > 0)
    {
        if (fwrite(buffer, 1, count, fptr_dest) != count)
        {
            return e_failure;
        }
    }
    return ferror(fptr_src) ? e_failure : e_success;
}
