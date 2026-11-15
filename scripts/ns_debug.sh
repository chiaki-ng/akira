#!/bin/bash
set -e

# This script is used to request the latest crash report from the NintendoSwitch homebrew FTP server
# Tested with: https://github.com/cathery/sys-ftpd
# Author: https://github.com/xfangfang

if [ $# -lt 2 ]; then
  echo -e "This script is used to request the latest crash report from the NintendoSwitch homebrew FTP server"
  echo -e "Usage: $0 <ftp_config/local> <elf_path> [log_path]"
  echo -e "Example 1: \n\t$0 ftp://user:passwd@192.168.1.3:5000 main.elf"
  echo -e "Example 2: \n\t$0 local main.elf 01693081214_ffffffffffffffff.log"
  exit 1
fi

operation="$1"
elf_path="$2"
log_file="$3"

case "$elf_path" in
  /*) elf_abs="$elf_path" ;;
  *)  elf_abs="$PWD/$elf_path" ;;
esac
elf_dir="$(cd "$(dirname "$elf_abs")" && pwd)"
elf_name="$(basename "$elf_abs")"

if [ "$operation" = "local" ]; then
  echo "===>    Load local log file: $log_file"
  crash_report="$(cat "$log_file")"
else
  echo "===>    Using FTP Config: $operation"
  ftp_server="$operation"
  reports_dir="atmosphere/crash_reports"

  echo "===>    Requesting crash report list from $ftp_server/$reports_dir"
  crash_report_path="$(curl -s "$ftp_server"/$reports_dir/ | awk '/\.log$/ {print $NF}' | sort -r | head -n 1)"
  if [ -z "$crash_report_path" ]; then
    echo "===>    No crash reports found (is sys-ftpd running?)"
    exit 1
  fi

  echo "===>    Requesting latest report: $ftp_server/$reports_dir/$crash_report_path"
  crash_report="$(curl -s "$ftp_server"/$reports_dir/"$crash_report_path")"

  saved_dir="$elf_dir/crash-reports"
  mkdir -p "$saved_dir"
  printf '%s\n' "$crash_report" > "$saved_dir/$crash_report_path"
  echo "===>    Saved raw report: $saved_dir/$crash_report_path"
  echo "===>    Re-run offline with: make crash LOG=$saved_dir/$crash_report_path"
fi

A2L=/opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line
if [ -x "$A2L" ]; then
  addr2line_batch() { "$A2L" -e "$elf_abs" -f -p -C -i -a "$@"; }
elif command -v aarch64-none-elf-addr2line >/dev/null 2>&1; then
  addr2line_batch() { aarch64-none-elf-addr2line -e "$elf_abs" -f -p -C -i -a "$@"; }
else
  addr2line_batch() {
    docker run --rm -v "$elf_dir:/work" devkitpro/devkita64 \
      "$A2L" -e "/work/$elf_name" -f -p -C -i -a "$@"
  }
fi

if printf '%s\n' "$crash_report" | grep -q ' crash report ('; then
  printf '%s\n' "$crash_report" | grep -v ' elf+0x'
  echo "===>    Symbolicated:"
  while IFS= read -r line; do
    label="$(printf '%s' "$line" | awk '{print $1}')"
    off="$(printf '%s' "$line" | grep -oE 'elf\+0x[0-9a-f]+' | cut -c5-)"
    printf '%-4s %s\n' "$label" "$(addr2line_batch "$off" | tr '\n' ' ')"
  done <<EOF
$(printf '%s\n' "$crash_report" | grep ' elf+0x')
EOF
  exit 0
fi

ascii_of_hex() {
  local hex="$1" out="" i byte dec
  for ((i=${#hex}-2; i>=0; i-=2)); do
    byte="${hex:$i:2}"
    dec=$((16#$byte))
    if [ "$dec" -ge 32 ] && [ "$dec" -lt 127 ]; then
      out="$out$(printf "\\$(printf '%03o' "$dec")")"
    else
      out="$out."
    fi
  done
  printf '%s' "$out"
}

describe_raw_addr() {
  local raw="$1" ascii
  raw="$(printf '%s' "$raw" | tr -d '\r' | tr 'A-F' 'a-f')"
  case "$raw" in *[!0-9a-f]*|"") return 0 ;; esac
  if [ $((16#${raw: -1} % 4)) -ne 0 ]; then
    echo "        ! not 4-byte aligned - on AArch64 an instruction fetch here"
    echo "          raises an Alignment Fault, i.e. a branch through a corrupted"
    echo "          function pointer rather than a genuine misaligned data access"
  fi
  ascii="$(ascii_of_hex "$raw")"
  case "$ascii" in
    *[!.]*[!.]*[!.]*[!.]*)
      echo "        ! decodes as ASCII (little-endian): \"$ascii\""
      echo "          a register holding text means this pointer was read from"
      echo "          freed/overwritten memory - look for a use-after-free"
      ;;
  esac
}

offset_of_line() {
  printf '%s' "$1" | sed -n 's/.*(\([^ ()]*\) *+ *\(0x[0-9a-fA-F]*\)).*/\2/p' | head -n 1
}
raw_of_line() {
  printf '%s' "$1" | awk '{for (i=1;i<=NF;i++) if ($i ~ /^[0-9a-fA-F]{8,16}$/) {print $i; exit}}'
}

report_register() {
  local label="$1" line off raw
  line="$(printf '%s\n' "$crash_report" | grep -E "^[[:space:]]*$label:" | head -n 1)"
  echo "===>    $label:"
  if [ -z "$line" ]; then
    echo "        (not present in report)"
    return 0
  fi
  echo "        $(printf '%s' "$line" | sed 's/^[[:space:]]*//')"
  off="$(offset_of_line "$line")"
  raw="$(raw_of_line "$line")"
  if [ -n "$off" ]; then
    addr2line_batch "$off" | sed 's/^/        /'
  else
    echo "        (outside any loaded module - not symbolicated)"
    describe_raw_addr "$raw"
  fi
}

echo "===>    Exception Info:"
exception_info="$(printf '%s\n' "$crash_report" | awk '/Exception Info:/,/Crashed Thread Info:/ {if (/Crashed Thread Info:/ && ++count>0) {exit} if (!/Exception Info:/) {print}}')"
echo "$exception_info"
printf '%s\n' "$exception_info" | while IFS= read -r info_line; do
  case "$info_line" in
    *Address*)
      info_raw="$(raw_of_line "$info_line")"
      if [ -n "$info_raw" ] && [ "$info_raw" != "0000000000000000" ]; then
        describe_raw_addr "$info_raw"
      fi
      ;;
  esac
done

report_register "PC"
report_register "LR"

echo "===>    Crashed Thread Info (verbatim):"
printf '%s\n' "$crash_report" | awk '/Crashed Thread Info:/,/Stack Trace:/ {if (/Stack Trace:/) {exit} print}'

echo "===>    Suspicious register values:"
found_suspicious=0
while IFS= read -r reg_line; do
  case "$reg_line" in
    *X\[*|*PC:*|*LR:*|*SP:*|*FP:*)
      reg_raw="$(raw_of_line "$reg_line")"
      [ -z "$reg_raw" ] && continue
      reg_ascii="$(ascii_of_hex "$(printf '%s' "$reg_raw" | tr 'A-F' 'a-f')")"
      case "$reg_ascii" in
        *[!.]*[!.]*[!.]*[!.]*)
          echo "        $(printf '%s' "$reg_line" | sed 's/^[[:space:]]*//')  ->  \"$reg_ascii\""
          found_suspicious=1
          ;;
      esac
      ;;
  esac
done <<EOF
$(printf '%s\n' "$crash_report" | awk '/Crashed Thread Info:/,/Stack Trace:/ {if (/Stack Trace:/) {exit} print}')
EOF
if [ "$found_suspicious" = 0 ]; then
  echo "        (none decode as ASCII text)"
fi

echo "===>    Stack info:"
stack_info="$(printf '%s\n' "$crash_report" | awk '/Stack Trace:/,/Stack Dump:/ {if (/Stack Dump:/) {exit} if (!/Stack Trace:/) {print}}')"
echo "$stack_info"
echo "        NOTE: Atmosphere builds this list by scanning the stack for values"
echo "        that look like code pointers, so it is not a true call chain."
echo "        Stale frames from earlier calls are common - treat entries that"
echo "        cannot belong (static init/destruction, exit, libc internals) as"
echo "        leftovers rather than evidence."

stack_offsets=""
while IFS= read -r stack_line; do
  [ -z "$stack_line" ] && continue
  off="$(offset_of_line "$stack_line")"
  [ -n "$off" ] && stack_offsets="$stack_offsets $off"
done <<EOF
$stack_info
EOF

if [ -n "$stack_offsets" ]; then
  # shellcheck disable=SC2086
  addr2line_batch $stack_offsets
else
  echo "        (no module-relative offsets to symbolicate)"
fi
