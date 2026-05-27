#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$ROOT"

count=0
while IFS= read -r -d '' file; do
    echo "$file"
    clang-format -i --style=file "$file"
    count=$((count + 1))
done < <(find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0)

echo "Formatted $count file(s) under src/"
