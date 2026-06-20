set -eu

checker=$1
output_directory=$2
clean_file=$output_directory/clean.c
forbidden_file=$output_directory/forbidden.c

printf '%s\n' 'const char *value = "https://example.test/a//b";' >"$clean_file"
printf '%s\n' 'int value = 0; // forbidden' >"$forbidden_file"

"$checker" "$clean_file"

set +e
"$checker" "$forbidden_file" >"$output_directory/check.stdout" 2>"$output_directory/check.stderr"
status=$?
set -e

test "$status" -ne 0
grep -q "forbidden comment" "$output_directory/check.stderr"
