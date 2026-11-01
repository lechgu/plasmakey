#!/bin/sh
# Session helpers used by the Makefile.
#
#   write-env QML_DIR ENV_FILE   write Plasma's per-user env script so plasmashell
#                                finds the QML module in QML_DIR at every login
#   reload QML_DIR ENV_FILE      apply that to the running session and restart
#                                plasmashell (adds QML_DIR to plasmashell's
#                                environment if ENV_FILE exists, removes it if not)
set -eu

write_env() {
    qml_dir=$1
    env_file=$2
    mkdir -p "$(dirname "$env_file")"
    cat >"$env_file" <<EOF
# Installed by plasmakey (Sticky Keys tray indicator).
# Removed by 'make uninstall'. Plasma sources this at login so plasmashell can
# load the indicator's QML module from your home directory.
case ":\${QML_IMPORT_PATH:-}:" in
    *":$qml_dir:"*) ;;
    *) export QML_IMPORT_PATH="$qml_dir\${QML_IMPORT_PATH:+:\$QML_IMPORT_PATH}" ;;
esac
EOF
    echo "Wrote $env_file"
}

# Prints LIST (colon-separated) with DIR added or removed.
path_add() {
    case ":$1:" in
        *":$2:"*) printf '%s' "$1" ;;
        *) printf '%s' "$2${1:+:$1}" ;;
    esac
}
path_remove() {
    printf '%s' "$1" | tr ':' '\n' | grep -vxF "$2" | paste -sd: - || true
}

reload() {
    qml_dir=$1
    env_file=$2

    if systemctl --user is-active --quiet plasma-plasmashell.service 2>/dev/null; then
        current=$(systemctl --user show-environment | sed -n 's/^QML_IMPORT_PATH=//p')
        if [ -f "$env_file" ]; then
            new=$(path_add "$current" "$qml_dir")
        else
            new=$(path_remove "$current" "$qml_dir")
        fi
        if [ -n "$new" ]; then
            systemctl --user set-environment QML_IMPORT_PATH="$new"
        else
            systemctl --user unset-environment QML_IMPORT_PATH
        fi
        echo "Restarting plasmashell (systemd user service)..."
        systemctl --user restart plasma-plasmashell.service
    elif pgrep -u "$(id -u)" -x plasmashell >/dev/null 2>&1; then
        current=${QML_IMPORT_PATH:-}
        if [ -f "$env_file" ]; then
            QML_IMPORT_PATH=$(path_add "$current" "$qml_dir")
        else
            QML_IMPORT_PATH=$(path_remove "$current" "$qml_dir")
        fi
        export QML_IMPORT_PATH
        echo "Restarting plasmashell..."
        kquitapp6 plasmashell >/dev/null 2>&1 || true
        kstart plasmashell >/dev/null 2>&1
    else
        echo "plasmashell is not running. Log out and back in to load the indicator."
        return 0
    fi
    echo "Done. If the indicator is not in the tray, see README.md, 'Show it in the system tray'."
}

case "${1:-}" in
    write-env) [ $# -eq 3 ] || exit 2; write_env "$2" "$3" ;;
    reload) [ $# -eq 3 ] || exit 2; reload "$2" "$3" ;;
    *) echo "usage: $0 write-env QML_DIR ENV_FILE | reload QML_DIR ENV_FILE" >&2; exit 2 ;;
esac
