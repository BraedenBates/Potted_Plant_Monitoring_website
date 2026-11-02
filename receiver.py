from http.server import BaseHTTPRequestHandler, HTTPServer
import json, datetime

class H(BaseHTTPRequestHandler):
    def do_POST(self):
        body = self.rfile.read(int(self.headers["Content-Length"]))
        d = json.loads(body)[0]
        t = datetime.datetime.now().strftime("%H:%M:%S")
        print(f"{t}  pH={d['ph']}  moisture={d['moisture']}  temp={d['temp']}", flush=True)
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b"ok")

    def log_message(self, *args):
        pass

HTTPServer(("0.0.0.0", 8080), H).serve_forever()