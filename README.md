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

The `examples/` directory contains two grayscale images designed to highlight
the strengths of each compression algorithm.

### RLE example

`rle-blocks.pgm` contains large solid regions, so its canonical raster has long
runs of identical bytes.

Compress it:

```sh
npbc compress --algorithm rle examples/rle-blocks.pgm examples/rle-blocks.npbc
```

Inspect the result:

```sh
npbc info examples/rle-blocks.npbc
```

Verify it:

```sh
npbc verify examples/rle-blocks.npbc
```

Restore it:

```sh
npbc decompress examples/rle-blocks.npbc examples/rle-blocks-restored.pgm
```

### Huffman example

`huffman-pattern.pgm` uses a small set of byte values with uneven frequencies,
but avoids long repeated runs. This favors Huffman coding over RLE.

Compress it:

```sh
npbc compress --algorithm huffman \
    examples/huffman-pattern.pgm examples/huffman-pattern.npbc
```

Inspect the result:

```sh
npbc info examples/huffman-pattern.npbc
```

Verify it:

```sh
npbc verify examples/huffman-pattern.npbc
```

Restore it:

```sh
npbc decompress \
    examples/huffman-pattern.npbc examples/huffman-pattern-restored.pgm
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
