# Netpbm Image Compressor

`npbc` is a portable C17 command-line tool for compressing Netpbm images into
`.npbc` files and restoring them without changing their pixel data.

It supports:

- `P1` and `P4` bitmaps
- `P2` and `P5` grayscale images
- `P3` and `P6` RGB images
- Run-length encoding
- Canonical Huffman coding
- CRC-32 integrity verification

Compression preserves image type, dimensions, maximum sample value, and every
pixel sample. Comments and source formatting are not preserved. Decompression
produces canonical binary `P4`, `P5`, or `P6` output.

## Build

The project requires a C17 compiler and GNU Make.

```sh
make
```

The executable is created at `build/npbc`.

Run the test and verification suites with:

```sh
make test
make check
make sanitize
```

`make sanitize` uses AddressSanitizer and UndefinedBehaviorSanitizer when they
are available.

## Install

Install to `/usr/local/bin`:

```sh
sudo make install
```

For a local installation:

```sh
make install PREFIX="$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
```

Uninstall using the same configuration:

```sh
make uninstall PREFIX="$HOME/.local"
```

`PREFIX`, `BINDIR`, and `DESTDIR` can be overridden for custom or staged
installations.

## Usage

An algorithm must be selected explicitly when compressing:

```text
npbc compress --algorithm rle INPUT OUTPUT
npbc compress --algorithm huffman INPUT OUTPUT
npbc decompress INPUT OUTPUT
npbc info INPUT
npbc verify INPUT
npbc --help
```

`-a` can be used instead of `--algorithm`.

Existing output files are not overwritten.

## Examples

The `examples/` directory includes `colors.ppm`, a small RGB image.

Compress it with canonical Huffman coding:

```sh
npbc compress --algorithm huffman examples/colors.ppm examples/colors.npbc
```

Display information about the compressed file:

```sh
npbc info examples/colors.npbc
```

Verify its integrity:

```sh
npbc verify examples/colors.npbc
```

Restore the image:

```sh
npbc decompress examples/colors.npbc examples/colors-restored.ppm
```

RLE generally works best with long repeated byte sequences. Huffman coding is
better suited to uneven byte frequencies. Neither algorithm guarantees that
the compressed file will be smaller. Generated example outputs are ignored by
Git.

## Limitations

- Only Netpbm `P1` through `P6` are supported.
- Source comments, whitespace, and ASCII formatting are not preserved.
- Standard input and standard output are not supported as image streams.
- There is no lossy mode, encryption, multithreading, or automatic algorithm
  selection.
- The maximum canonical raster size is 1 GiB.
- CRC-32 detects corruption but does not authenticate files.

## License

Licensed under the [MIT License](LICENSE).
