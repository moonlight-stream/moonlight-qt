"""Loopback TLS fixture. Requires Python's cryptography package."""
import datetime
import http.server
import pathlib
import ssl
import sys
import time
from urllib.parse import urlsplit

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID

folder = pathlib.Path(sys.argv[1])
for who in ('client', 'host'):
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, who)])
    now = datetime.datetime.now(datetime.timezone.utc)
    cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name)
            .public_key(key.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(now - datetime.timedelta(days=1))
            .not_valid_after(now + datetime.timedelta(days=1)).sign(key, hashes.SHA256()))
    (folder / (who + '.crt')).write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    (folder / (who + '.pem')).write_bytes(key.private_bytes(
        serialization.Encoding.PEM, serialization.PrivateFormat.TraditionalOpenSSL,
        serialization.NoEncryption()))


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        mode = (folder / 'mode').read_text()
        path = urlsplit(self.path).path
        assert self.connection.getpeercert()
        if path == '/cancel':
            if mode == 'slow':
                time.sleep(32)
            status = 403 if mode == 'denied' else 200
            payload = '' if mode == 'missing' else '<cancel>%s</cancel>' % ('0' if mode == 'refused' else '1')
        elif path == '/serverinfo':
            status = 200
            busy = mode == 'busy'
            payload = ('<appversion>7.1.0.0</appversion><state>MJOLNIR_SERVER_%s</state>'
                       '<currentgame>%s</currentgame>') % ('BUSY' if busy else 'FREE', 42 if busy else 0)
        else:
            self.send_error(404)
            return
        body = ('<root status_code="%s" status_message="test">%s</root>' % (status, payload)).encode()
        try:
            self.send_response(200)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass  # The old client aborts the slow request before the response.


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.load_cert_chain(folder / 'host.crt', folder / 'host.pem')
ctx.verify_mode = ssl.CERT_REQUIRED
ctx.load_verify_locations(cafile=folder / 'client.crt')
server.socket = ctx.wrap_socket(server.socket, server_side=True)
print(server.server_port, flush=True)
server.serve_forever()
