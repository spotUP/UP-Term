import http.server, time
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        time.sleep(30)
        try:
            self.send_response(404); self.end_headers()
        except Exception: pass
    def log_message(self,*a): pass
http.server.ThreadingHTTPServer(('127.0.0.1',53986),H).serve_forever()
