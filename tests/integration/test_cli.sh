set -eu

executable=$1
output_directory=$2

rm -f "$output_directory"/*

expect_usage() {
    name=$1
    shift
    set +e
    "$executable" "$@" >"$output_directory/$name.stdout" 2>"$output_directory/$name.stderr"
    status=$?
    set -e
    test "$status" -eq 2
    test ! -s "$output_directory/$name.stdout"
    grep -q "^npbc: " "$output_directory/$name.stderr"
    grep -q "Try 'npbc --help'." "$output_directory/$name.stderr"
}

printf 'P4\n2 2\n\100\200' >"$output_directory/p1.expected"
printf 'P5\n2 1\n15\n\000\017' >"$output_directory/p2.expected"
printf 'P6\n1 1\n255\n\001\002\003' >"$output_directory/p3.expected"
cp tests/fixtures/netpbm/p4.pbm "$output_directory/p4.expected"
cp tests/fixtures/netpbm/p5.pgm "$output_directory/p5.expected"
cp tests/fixtures/netpbm/p6.ppm "$output_directory/p6.expected"

"$executable" --help >"$output_directory/help.stdout" 2>"$output_directory/help.stderr"
test ! -s "$output_directory/help.stderr"
grep -q "npbc compress --algorithm rle" "$output_directory/help.stdout"
grep -q "npbc verify INPUT" "$output_directory/help.stdout"

expect_usage no-arguments
expect_usage unsupported unsupported
expect_usage help-extra --help extra
expect_usage compress-missing compress tests/fixtures/netpbm/p1.pbm "$output_directory/missing.npbc"
expect_usage compress-unknown-algorithm compress --algorithm unknown tests/fixtures/netpbm/p1.pbm "$output_directory/unknown.npbc"
expect_usage compress-misplaced compress rle tests/fixtures/netpbm/p1.pbm "$output_directory/misplaced.npbc"
expect_usage compress-unknown-option compress --codec rle tests/fixtures/netpbm/p1.pbm "$output_directory/unknown-option.npbc"
expect_usage compress-repeated compress --algorithm rle --algorithm huffman tests/fixtures/netpbm/p1.pbm "$output_directory/repeated.npbc"
expect_usage compress-extra compress --algorithm rle tests/fixtures/netpbm/p1.pbm "$output_directory/extra.npbc" extra
expect_usage decompress-missing decompress tests/fixtures/netpbm/p1.pbm
expect_usage decompress-extra decompress input output extra
expect_usage info-missing info
expect_usage info-extra info input extra
expect_usage verify-missing verify
expect_usage verify-extra verify input extra
test ! -e "$output_directory/missing.npbc"
test ! -e "$output_directory/unknown.npbc"
test ! -e "$output_directory/misplaced.npbc"
test ! -e "$output_directory/unknown-option.npbc"
test ! -e "$output_directory/repeated.npbc"
test ! -e "$output_directory/extra.npbc"

for algorithm in rle huffman
do
    for fixture in p1.pbm p2.pgm p3.ppm p4.pbm p5.pgm p6.ppm
    do
        stem=${fixture%.*}
        container="$output_directory/$stem-$algorithm.npbc"
        restored="$output_directory/$stem-$algorithm.restored"
        statistics="$output_directory/$stem-$algorithm.compress.stdout"
        errors="$output_directory/$stem-$algorithm.compress.stderr"
        input_bytes=$(wc -c <"tests/fixtures/netpbm/$fixture" | tr -d ' ')

        "$executable" compress --algorithm "$algorithm" "tests/fixtures/netpbm/$fixture" "$container" >"$statistics" 2>"$errors"
        output_bytes=$(wc -c <"$container" | tr -d ' ')
        expected_ratio=$(awk -v output="$output_bytes" -v input="$input_bytes" 'BEGIN { printf "%.4f", output / input }')
        test ! -s "$errors"
        test -s "$container"
        grep -q "^algorithm: $algorithm$" "$statistics"
        grep -q "^input-bytes: $input_bytes$" "$statistics"
        grep -q "^output-bytes: $output_bytes$" "$statistics"
        grep -q "^ratio: $expected_ratio$" "$statistics"
        test "$output_bytes" -gt "$input_bytes"

        if test "$algorithm" = rle
        then
            test "$(od -An -tu1 -j5 -N1 "$container" | tr -d ' ')" = "1"
        else
            test "$(od -An -tu1 -j5 -N1 "$container" | tr -d ' ')" = "2"
        fi

        "$executable" info "$container" >"$output_directory/$stem-$algorithm.info.stdout" 2>"$output_directory/$stem-$algorithm.info.stderr"
        raster_bytes=$(sed -n 's/^raster-bytes: //p' "$output_directory/$stem-$algorithm.info.stdout")
        payload_bytes=$(sed -n 's/^payload-bytes: //p' "$output_directory/$stem-$algorithm.info.stdout")
        expected_info_ratio=$(awk -v payload="$payload_bytes" -v raster="$raster_bytes" 'BEGIN { printf "%.4f", payload / raster }')
        test ! -s "$output_directory/$stem-$algorithm.info.stderr"
        grep -q "^compatibility: 1$" "$output_directory/$stem-$algorithm.info.stdout"
        grep -q "algorithm: $algorithm" "$output_directory/$stem-$algorithm.info.stdout"
        grep -q "^image: " "$output_directory/$stem-$algorithm.info.stdout"
        grep -q "^width: " "$output_directory/$stem-$algorithm.info.stdout"
        grep -q "^height: " "$output_directory/$stem-$algorithm.info.stdout"
        grep -q "^max-value: " "$output_directory/$stem-$algorithm.info.stdout"
        grep -q "^raster-bytes: " "$output_directory/$stem-$algorithm.info.stdout"
        grep -q "^payload-bytes: " "$output_directory/$stem-$algorithm.info.stdout"
        grep -q "^ratio: $expected_info_ratio$" "$output_directory/$stem-$algorithm.info.stdout"
        grep -q "ratio-basis: payload/raster" "$output_directory/$stem-$algorithm.info.stdout"

        "$executable" verify "$container" >"$output_directory/$stem-$algorithm.verify.stdout" 2>"$output_directory/$stem-$algorithm.verify.stderr"
        test ! -s "$output_directory/$stem-$algorithm.verify.stderr"
        test "$(cat "$output_directory/$stem-$algorithm.verify.stdout")" = "valid"

        "$executable" decompress "$container" "$restored" >"$output_directory/$stem-$algorithm.decompress.stdout" 2>"$output_directory/$stem-$algorithm.decompress.stderr"
        test ! -s "$output_directory/$stem-$algorithm.decompress.stdout"
        test ! -s "$output_directory/$stem-$algorithm.decompress.stderr"
        test -s "$restored"
        cmp "$output_directory/$stem.expected" "$restored"
    done
done

"$executable" compress -a rle tests/fixtures/netpbm/p1.pbm "$output_directory/short-option.npbc" >"$output_directory/short-option.stdout" 2>"$output_directory/short-option.stderr"
test ! -s "$output_directory/short-option.stderr"
grep -q "algorithm: rle" "$output_directory/short-option.stdout"

"$executable" compress -a huffman tests/fixtures/netpbm/p1.pbm "$output_directory/short-option-huffman.npbc" >"$output_directory/short-option-huffman.stdout" 2>"$output_directory/short-option-huffman.stderr"
test ! -s "$output_directory/short-option-huffman.stderr"
grep -q "algorithm: huffman" "$output_directory/short-option-huffman.stdout"

printf 'existing\n' >"$output_directory/existing.npbc"
set +e
"$executable" compress --algorithm rle tests/fixtures/netpbm/p1.pbm "$output_directory/existing.npbc" >"$output_directory/existing.stdout" 2>"$output_directory/existing.stderr"
status=$?
set -e
test "$status" -eq 4
test "$(cat "$output_directory/existing.npbc")" = "existing"
grep -q "output file already exists" "$output_directory/existing.stderr"

printf 'existing restored\n' >"$output_directory/existing.restored"
set +e
"$executable" decompress "$output_directory/p1-rle.npbc" "$output_directory/existing.restored" >"$output_directory/existing-decompress.stdout" 2>"$output_directory/existing-decompress.stderr"
status=$?
set -e
test "$status" -eq 4
test "$(cat "$output_directory/existing.restored")" = "existing restored"
grep -q "output file already exists" "$output_directory/existing-decompress.stderr"

printf 'bad\n' >"$output_directory/bad.pbm"
set +e
"$executable" compress --algorithm rle "$output_directory/bad.pbm" "$output_directory/bad-output.npbc" >"$output_directory/bad.stdout" 2>"$output_directory/bad.stderr"
status=$?
set -e
test "$status" -eq 3
test ! -e "$output_directory/bad-output.npbc"

printf 'P4\n4294967295 4294967295\n' >"$output_directory/huge.pbm"
set +e
"$executable" compress --algorithm rle "$output_directory/huge.pbm" "$output_directory/huge.npbc" >"$output_directory/huge.stdout" 2>"$output_directory/huge.stderr"
status=$?
set -e
test "$status" -eq 6
test ! -e "$output_directory/huge.npbc"

set +e
"$executable" verify "$output_directory/not-found.npbc" >"$output_directory/not-found.stdout" 2>"$output_directory/not-found.stderr"
status=$?
set -e
test "$status" -eq 4

cp "$output_directory/p1-huffman.npbc" "$output_directory/corrupt.npbc"
printf '\000\000\000\000' | dd of="$output_directory/corrupt.npbc" bs=1 seek=40 conv=notrunc status=none
set +e
"$executable" verify "$output_directory/corrupt.npbc" >"$output_directory/corrupt.stdout" 2>"$output_directory/corrupt.stderr"
status=$?
set -e
test "$status" -eq 5
test ! -s "$output_directory/corrupt.stdout"
grep -q "integrity verification" "$output_directory/corrupt.stderr"

set +e
"$executable" decompress "$output_directory/corrupt.npbc" "$output_directory/corrupt.restored" >"$output_directory/corrupt-decompress.stdout" 2>"$output_directory/corrupt-decompress.stderr"
status=$?
set -e
test "$status" -eq 5
test ! -e "$output_directory/corrupt.restored"

head -c 43 "$output_directory/p1-rle.npbc" >"$output_directory/truncated.npbc"
set +e
"$executable" decompress "$output_directory/truncated.npbc" "$output_directory/truncated.restored" >"$output_directory/truncated.stdout" 2>"$output_directory/truncated.stderr"
status=$?
set -e
test "$status" -eq 3
test ! -e "$output_directory/truncated.restored"

set +e
"$executable" compress --algorithm rle tests/fixtures/netpbm/p1.pbm "$output_directory/missing-directory/output.npbc" >"$output_directory/create.stdout" 2>"$output_directory/create.stderr"
status=$?
set -e
test "$status" -eq 4

iteration=0
while test "$iteration" -lt 100
do
    set +e
    "$executable" decompress "$output_directory/corrupt.npbc" "$output_directory/repeated-failure.restored" >/dev/null 2>/dev/null
    status=$?
    set -e
    test "$status" -eq 5
    test ! -e "$output_directory/repeated-failure.restored"

    set +e
    "$executable" compress --algorithm rle "$output_directory/bad.pbm" "$output_directory/repeated-failure.npbc" >/dev/null 2>/dev/null
    status=$?
    set -e
    test "$status" -eq 3
    test ! -e "$output_directory/repeated-failure.npbc"
    iteration=$((iteration + 1))
done

test -z "$(find "$output_directory" -maxdepth 1 -name '*.tmp.*' -print)"
