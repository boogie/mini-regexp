#!/bin/sh
# Standard code-size measurement for the mini-regexp research.
# usage: measure.sh FILE.c [extra cflags...]
# Prints .text/.data/.bss for Cortex-M4 (Thumb-2, nRF52), Cortex-M0+ (Thumb-1),
# and x86-64 (clang), all at -Os, plus per-function breakdown for Cortex-M4.
# "text" in the object file includes .rodata (tables), which is what we want:
# flash footprint = text + data.
set -e
f="$1"; shift
COMMON="-Os -std=gnu99 -DNDEBUG -ffreestanding -fno-stack-protector -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-pic -w"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
armver=$(arm-none-eabi-gcc -dumpfullversion)
clangver=$(clang -dumpversion)
case $(uname -s) in
  Darwin) x86target=x86_64-apple-macos12 ;;
  *) x86target=x86_64-linux-gnu ;;
esac
echo "== $f"
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -fstack-usage $COMMON "$@" -c "$f" -o "$tmp/m4.o"
printf 'cortex-m4  (thumb2, gcc %s): ' "$armver"; arm-none-eabi-size "$tmp/m4.o" | tail -1 | awk '{printf "text=%d data=%d bss=%d\n",$1,$2,$3}'
arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb $COMMON "$@" -c "$f" -o "$tmp/m0.o"
printf 'cortex-m0+ (thumb1, gcc %s): ' "$armver"; arm-none-eabi-size "$tmp/m0.o" | tail -1 | awk '{printf "text=%d data=%d bss=%d\n",$1,$2,$3}'
clang --target="$x86target" $COMMON "$@" -c "$f" -o "$tmp/x86.o"
printf 'x86-64     (clang %s):         ' "$clangver"; size "$tmp/x86.o" | tail -1 | awk '{printf "text=%d data=%d\n",$1,$2}'
echo "-- cortex-m4 per-function (bytes):"
arm-none-eabi-size -A "$tmp/m4.o" | awk '$1 ~ /^\.(text|rodata)/ && $2>0 {printf "  %6d  %s\n",$2,$1}' | sort -rn
echo "-- cortex-m4 stack frames (bytes):"
sed 's/.*://' "$tmp/m4.su"
echo "-- cortex-m4 undefined symbols:"
arm-none-eabi-nm -u "$tmp/m4.o"
