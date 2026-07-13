#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

canonical_directory() {
    local input=$1
    local -n output=$2
    IFS= read -r -d '' output < <(realpath -ze -- "$input") || return 1
    [[ -d "$output" ]]
}

contained_regular_file() {
    local root=$1 source=$2
    local -n output=$3
    [[ ! -L "$source" && -f "$source" ]] || return 1
    IFS= read -r -d '' output < <(realpath -ze -- "$source") || return 1
    [[ "$source" == "$output" ]] || return 1
    [[ "$output" == "$root"/* ]]
}

media_rank() {
    case "${1,,}" in
        *.mp4|*.webm) printf '0' ;;
        *.gif) printf '1' ;;
        *.jpg|*.jpeg|*.png) printf '2' ;;
        *) return 1 ;;
    esac
}

media_extension() {
    case "${1,,}" in
        *.mp4) printf 'mp4' ;;
        *.webm) printf 'webm' ;;
        *.gif) printf 'gif' ;;
        *.jpg) printf 'jpg' ;;
        *.jpeg) printf 'jpeg' ;;
        *.png) printf 'png' ;;
        *) return 1 ;;
    esac
}

install_media() {
    local root preference walls_dir item_id
    canonical_directory "${1:?item root required}" root || return 3
    preference=${2:-all}
    canonical_directory "${3:?wallpaper directory required}" walls_dir || return 3
    item_id=${4:?item ID required}
    [[ "$item_id" =~ ^[0-9]+$ ]] || return 2

    local -a names
    case "$preference" in
        all) names=( -iname '*.mp4' -o -iname '*.webm' -o -iname '*.gif' -o -iname '*.jpg' -o -iname '*.jpeg' -o -iname '*.png' ) ;;
        video) names=( -iname '*.mp4' -o -iname '*.webm' ) ;;
        gif) names=( -iname '*.gif' ) ;;
        image) names=( -iname '*.jpg' -o -iname '*.jpeg' -o -iname '*.png' ) ;;
        *) return 2 ;;
    esac

    local candidate canonical rank size
    local best='' best_rank=99 best_size=-1
    while IFS= read -r -d '' candidate; do
        contained_regular_file "$root" "$candidate" canonical || continue
        rank=$(media_rank "$canonical") || continue
        size=$(stat -c '%s' -- "$canonical") || continue
        if (( rank < best_rank || (rank == best_rank && size > best_size) )) \
                || { (( rank == best_rank && size == best_size )) && [[ -n "$best" && "$canonical" < "$best" ]]; }; then
            best=$canonical
            best_rank=$rank
            best_size=$size
        fi
    done < <(find -P "$root" -type f \( "${names[@]}" \) -print0)

    if [[ -z "$best" ]]; then
        printf 'NONE\n'
        return 4
    fi

    local verified extension destination destination_name temporary
    contained_regular_file "$root" "$best" verified || return 3
    extension=$(media_extension "$verified") || return 3
    destination_name="steam-${item_id}.${extension}"
    destination="${walls_dir}/${destination_name}"
    temporary=$(mktemp --tmpdir="$walls_dir" ".${destination_name}.XXXXXX")
    trap 'rm -f -- "$temporary"' RETURN
    cp -- "$verified" "$temporary"
    mv -fT -- "$temporary" "$destination"
    trap - RETURN
    printf 'OK\t%s\n' "$destination"
}

case "${1:-}" in
    install) install_media "${2:-}" "${3:-}" "${4:-}" "${5:-}" ;;
    *) exit 2 ;;
esac
