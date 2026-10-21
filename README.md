# BMP Steganography

A small C project exploring least-significant-bit (LSB) steganography: storing
text data in the pixel bytes of a BMP image.

## Status

The command-line program supports encoding a `.txt` file into a BMP image and
decoding it back. It stores a marker, the secret-file extension and size, and
the secret bytes in the least-significant bits of the image data.

## Build

Build the program with GCC:

```sh
gcc -std=c11 -Wall -Wextra -pedantic test_encode.c encode.c decode.c -o steganography
```

## Command-line usage

Encode a text file:

```text
./steganography -e <image.bmp> <secret.txt> [output.bmp]
```

If the output path is omitted, the program writes `stego.bmp`.

Decode it (the output file defaults to `decoded.txt`):

```sh
./steganography -d stego.bmp [output-file]
```

For example:

```sh
./steganography -e beautiful.bmp secret.txt stego.bmp
./steganography -d stego.bmp recovered.txt
```

The source must be a BMP image with enough pixel-data capacity. The secret input
is required to be a non-empty `.txt` file. The output BMP path must differ from
both input paths.

## Source files

- `test_encode.c` — command-line entry point and operation selection.
- `encode.c`, `encode.h` — encoding logic and its data structures.
- `decode.c`, `decode.h` — reads the embedded metadata and restores the secret.
- `common.h`, `types.h` — shared constants and status/operation types.

## Sample files

- `beautiful.bmp` — sample source image.
- `secret.txt` — sample text input.
- `stego.bmp` — example output filename (created by the encode command).
