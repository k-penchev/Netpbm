set -eu

executable=$1
repository=$3
output_directory="$repository/$2"
executable_path="$repository/$executable"

rm -rf "$output_directory"
mkdir -p "$output_directory"

stage="$output_directory/stage"
local_prefix="$output_directory/local"
custom_stage="$output_directory/custom-stage"
outside="$output_directory/outside"
build_copy="$output_directory/npbc.build-copy"

restore_build_program() {
    if test -e "$build_copy"
    then
        mv "$build_copy" "$executable_path"
    fi
}

trap restore_build_program EXIT HUP INT TERM

mkdir -p "$outside"

make -s install DESTDIR="$stage"
test -x "$stage/usr/local/bin/npbc"
test "$(find "$stage" -type f | wc -l | tr -d ' ')" -eq 1
test "$(find "$stage" -type f -name npbc | wc -l | tr -d ' ')" -eq 1

(
    cd "$outside"
    PATH="$stage/usr/local/bin:$PATH" npbc --help >stage-help.stdout
)
grep -q "npbc compress --algorithm rle" "$outside/stage-help.stdout"

printf 'unrelated\n' >"$stage/usr/local/bin/unrelated"
make -s uninstall DESTDIR="$stage"
test ! -e "$stage/usr/local/bin/npbc"
test "$(cat "$stage/usr/local/bin/unrelated")" = "unrelated"
test -d "$stage/usr/local/bin"
make -s uninstall DESTDIR="$stage"
test -e "$stage/usr/local/bin/unrelated"

make -s install PREFIX="$local_prefix"
test -x "$local_prefix/bin/npbc"
(
    cd "$outside"
    PATH="$local_prefix/bin:$PATH" npbc --help >local-help.stdout
)
grep -q "npbc verify INPUT" "$outside/local-help.stdout"

mv "$executable_path" "$build_copy"
(
    cd "$outside"
    PATH="$local_prefix/bin:$PATH" npbc --help >independent-help.stdout
)
restore_build_program
grep -q "npbc decompress INPUT OUTPUT" "$outside/independent-help.stdout"

make -s install PREFIX="$local_prefix"
test -x "$local_prefix/bin/npbc"
make -s uninstall PREFIX="$local_prefix"
test ! -e "$local_prefix/bin/npbc"
test -d "$local_prefix/bin"

make -s install DESTDIR="$custom_stage" BINDIR=/opt/npbc-tools
test -x "$custom_stage/opt/npbc-tools/npbc"
make -s uninstall DESTDIR="$custom_stage" BINDIR=/opt/npbc-tools
test ! -e "$custom_stage/opt/npbc-tools/npbc"
test -d "$custom_stage/opt/npbc-tools"

make -s -n install | grep -q "/usr/local/bin/npbc"
