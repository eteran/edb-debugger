#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." || exit 1

EXCLUDED_DIRS="lib/gdtoa-desktop src/qhexview/"

EXCLUDE_PATH_ARGS=()
for dir in $EXCLUDED_DIRS; do
	dir="${dir%/}"
	EXCLUDE_PATH_ARGS+=( -path "$dir" -o -path "$dir/*" -o )
done

# Remove trailing "-o" so the prune expression is valid.
if [ ${#EXCLUDE_PATH_ARGS[@]} -gt 0 ]; then
	unset 'EXCLUDE_PATH_ARGS[${#EXCLUDE_PATH_ARGS[@]}-1]'
	FILES=$(find include src plugins lib \
		\( "${EXCLUDE_PATH_ARGS[@]}" \) -prune -o \
		-type f \( -name "*.cpp" -o -name "*.h" \) -print)
else
	FILES=$(find include src plugins lib -type f \( -name "*.cpp" -o -name "*.h" \))
fi

echo $FILES | xargs clang-format -i
