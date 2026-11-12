#!/usr/bin/env bash
# Chạy test cấp câu qua vgo-cli (có nạp data/*.txt). Chạy từ gốc repo sau `make`.
# Đối số: [file ca] [cli] — vd `bash test/run-cases.sh test/vni-cases.txt ./vgo-vni-cli`
set -u
cases="${1:-test/vikey-cases.txt}"
cli="${2:-./vgo-cli}"
pass=0; fail=0
while IFS='|' read -r id keys want; do
  [[ -z "$id" || "$id" == \#* ]] && continue
  got="$("$cli" "$keys")"
  if [[ "$got" == "$want" ]]; then
    pass=$((pass+1))
  else
    fail=$((fail+1))
    printf 'FAIL %-4s %-30s -> %-25s (want: %s)\n' "$id" "$keys" "$got" "$want"
  fi
done < "$cases"
printf '\n%d passed, %d failed\n' "$pass" "$fail"
[[ $fail -eq 0 ]]
