#!/usr/bin/env python3

##
## Localhost-only server for debugging WASM. This is not suitable for public
## serving.
##

from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

class WithHeaders(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "localhost")
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

httpd = ThreadingHTTPServer( ('127.0.0.1', 8000), WithHeaders)
httpd.serve_forever()
