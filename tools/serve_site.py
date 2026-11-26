#!/usr/bin/env python3
#
# serve_site.py
#
# Serves one or more site directories as one, the first that has a file winning, with the headers
# that make a page cross-origin isolated, so it may use shared memory and so wasm threads.
# tools/serve_site.sh runs it.
#
#   python3 tools/serve_site.py <port> <dir> [<dir> ...]

import functools
import http.server
import os
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    """Serves from the first of roots that has the file, isolating every page."""

    def __init__(self, *args, roots, **kwargs):
        self.roots = roots
        super().__init__(*args, directory=roots[0], **kwargs)

    def translate_path(self, path):
        first = super().translate_path(path)
        relative = os.path.relpath(first, self.roots[0])

        for root in self.roots:
            candidate = os.path.join(root, relative)
            if os.path.exists(candidate):
                return candidate
        return first

    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()


def main():
    port = int(sys.argv[1])
    roots = [os.path.abspath(root) for root in sys.argv[2:] if os.path.isdir(root)]

    if not roots:
        sys.exit('serve_site.py: none of the site directories exist')

    handler = functools.partial(Handler, roots=roots)
    http.server.ThreadingHTTPServer(('127.0.0.1', port), handler).serve_forever()


if __name__ == '__main__':
    main()
