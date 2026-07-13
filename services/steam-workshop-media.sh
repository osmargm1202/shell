#!/usr/bin/env bash
set -euo pipefail

mode=${1:-}

canonical_root() {
    local root
    root=$(realpath -e -- "$1")
    [[ -d "$root" ]] || return 1
    printf '%s' "$root"
}

contained_regular_file() {
    local root=$1 source=$2 canonical
    [[ ! -L "$source" && -f "$source" ]] || return 1
    canonical=$(realpath -e -- "$source") || return 1
    [[ "$source" == "$canonical" ]] || return 1
    [[ "$canonical" == "$root"/* ]] || return 1
    printf '%s' "$canonical"
}

case "$mode" in
    discover)
        root=$(canonical_root "${2:?item root required}")
        preference=${3:-all}
        case "$preference" in
            all) names=( -iname '*.mp4' -o -iname '*.webm' -o -iname '*.gif' -o -iname '*.jpg' -o -iname '*.jpeg' -o -iname '*.png' ) ;;
            video) names=( -iname '*.mp4' -o -iname '*.webm' ) ;;
            gif) names=( -iname '*.gif' ) ;;
            image) names=( -iname '*.jpg' -o -iname '*.jpeg' -o -iname '*.png' ) ;;
            *) exit 2 ;;
        esac
        while IFS= read -r -d '' candidate; do
            canonical=$(contained_regular_file "$root" "$candidate") || continue
            size=$(stat -c '%s' -- "$canonical") || continue
            encoded=$(printf '%s' "$canonical" | base64 -w0)
            printf '%s\t%s\n' "$size" "$encoded"
        done < <(find -P "$root" -type f \( "${names[@]}" \) -print0)
        ;;
    safe-copy)
        root=$(canonical_root "${2:?item root required}")
        source=$(contained_regular_file "$root" "${3:?source required}") || exit 3
        destination=${4:?destination required}
        destination_dir=$(realpath -e -- "$(dirname -- "$destination")")
        destination_name=$(basename -- "$destination")
        temporary=$(mktemp --tmpdir="$destination_dir" ".${destination_name}.XXXXXX")
        trap 'rm -f -- "$temporary"' EXIT
        cp -- "$source" "$temporary"
        mv -fT -- "$temporary" "$destination"
        trap - EXIT
        ;;
    *)
        exit 2
        ;;
esac
