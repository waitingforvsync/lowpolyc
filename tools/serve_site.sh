#!/usr/bin/env bash
#
# serve_site.sh
#
# Serves the wasm builds' page at http://127.0.0.1:8090 for trying it in a browser: build/wasm/site
# and build/wasm-threads/site laid one over the other, as CI lays them for GitHub Pages, so the
# page has both lowpoly.wasm and lowpoly-threads.wasm if both are built. Unlike GitHub Pages, it
# sends the headers that make the page cross-origin isolated, so the page runs the threaded build.
# tools/serve_site.py is the server.
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
        if [ ! -d build/wasm/site ] && [ ! -d build/wasm-threads/site ]; then
            echo "No build/wasm/site or build/wasm-threads/site: build a wasm variant first." >&2
            exit 1
        fi
        mkdir -p build/wasm
        if ! is_running; then
            # setsid puts the server in its own session, so it outlives the task's terminal
            setsid python3 tools/serve_site.py "$port" build/wasm/site build/wasm-threads/site \
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
