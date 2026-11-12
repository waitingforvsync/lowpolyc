#!/usr/bin/env bash
#
# serve_site.sh
#
# Serves the wasm build's page (build/wasm/site) at http://127.0.0.1:8090 for trying it in a
# browser, using Python's own HTTP server. The site is static files, so nothing more is needed.
#
#   tools/serve_site.sh start  start the server in the background, or keep the one already running,
#                              and return once it answers
#   tools/serve_site.sh stop   stop it
#
# The server's process id is kept in build/wasm/server.pid, so stop and a second start know which
# server is ours. Its output goes to build/wasm/server.log.

set -u
cd "$(dirname "$0")/.."

port=8090
url="http://127.0.0.1:$port/"
pid_file=build/wasm/server.pid
log_file=build/wasm/server.log

# True if the server we started last is still running.
is_running() {
    [ -f "$pid_file" ] && kill -0 "$(cat "$pid_file")" 2>/dev/null
}

case "${1:-}" in
    start)
        if [ ! -d build/wasm/site ]; then
            echo "No build/wasm/site: build the wasm variant first." >&2
            exit 1
        fi
        if ! is_running; then
            # setsid puts the server in its own session, so it outlives the task's terminal
            setsid python3 -m http.server "$port" --bind 127.0.0.1 --directory build/wasm/site \
                >"$log_file" 2>&1 < /dev/null &
            echo $! > "$pid_file"
        fi

        # Wait up to five seconds for it to answer
        for _ in $(seq 50); do
            if curl -sf -o /dev/null "$url"; then
                echo "Serving $url"
                exit 0
            fi
            sleep 0.1
        done
        echo "The server did not start; see $log_file" >&2
        exit 1
        ;;

    stop)
        if is_running; then
            kill "$(cat "$pid_file")"
        fi
        rm -f "$pid_file"
        ;;

    *)
        echo "usage: $0 start|stop" >&2
        exit 2
        ;;
esac
