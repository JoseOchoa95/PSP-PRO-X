"""Isolated native TLS tests using production transport.c and Mbed TLS config.

Run inside the pinned Docker image with --network none. A test-only socket shim
maps GitHub hosts to a loopback TLS server; no endpoint override is compiled
into the PSP executable. Private keys exist only in ignored test output.
"""
from pathlib import Path
import contextlib
import os
import http.server
import ssl
import subprocess
import tarfile
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'artifacts/phase3/https-native'


def run(*args, **kwargs):
    return subprocess.run(args, check=True, stdout=subprocess.DEVNULL,
                          stderr=subprocess.PIPE, **kwargs)


def prepare():
    WORK.mkdir(parents=True, exist_ok=True)
    tls = WORK / 'mbedtls'
    tls.mkdir(exist_ok=True)
    with tarfile.open(ROOT / 'firmware/Updater/vendor/mbedtls-3.6.7.tar.bz2') as archive:
        # Verified upstream release; reject links/traversal even in test inputs.
        for entry in archive:
            path = Path(entry.name)
            if path.is_absolute() or '..' in path.parts or entry.issym() or entry.islnk():
                raise RuntimeError('Unsafe TLS source archive')
            relative = Path(*path.parts[1:])
            destination = tls / relative
            if entry.isdir():
                destination.mkdir(parents=True, exist_ok=True)
            elif entry.isfile():
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(archive.extractfile(entry).read())
    for name in ('ca', 'other'):
        run('openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-sha256',
            '-subj', '/CN=PRO-X TEST ONLY '+name, '-days', '2',
            '-keyout', str(WORK / (name+'.key')), '-out', str(WORK / (name+'.pem')))
    run('openssl', 'req', '-new', '-newkey', 'rsa:2048', '-nodes',
        '-subj', '/CN=api.github.com', '-keyout', str(WORK / 'server.key'),
        '-out', str(WORK / 'server.csr'))
    (WORK / 'extensions').write_text('subjectAltName=DNS:api.github.com\nbasicConstraints=CA:FALSE\n'
                                   'keyUsage=digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\n')
    for name, days in [('server', '1'), ('expired', '0')]:
        run('openssl', 'x509', '-req', '-in', str(WORK / 'server.csr'),
            '-CA', str(WORK / 'ca.pem'), '-CAkey', str(WORK / 'ca.key'),
            '-CAcreateserial', '-days', days, '-sha256', '-extfile', str(WORK / 'extensions'),
            '-out', str(WORK / (name+'.pem')))
    # Modern ECDSA chain: P-384 CA/SHA-384 and P-256 leaf, matching the class
    # of GitHub chain that the original SHA-256-only fixtures did not cover.
    run('openssl', 'ecparam', '-name', 'secp384r1', '-genkey', '-noout', '-out', str(WORK/'ca384.key'))
    run('openssl', 'req', '-x509', '-new', '-key', str(WORK/'ca384.key'), '-sha384', '-days', '2',
        '-subj', '/CN=PRO-X TEST ONLY SHA384 CA', '-out', str(WORK/'ca384.pem'))
    run('openssl', 'ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', str(WORK/'server384.key'))
    run('openssl', 'req', '-new', '-key', str(WORK/'server384.key'), '-subj', '/CN=api.github.com',
        '-out', str(WORK/'server384.csr'))
    run('openssl', 'x509', '-req', '-in', str(WORK/'server384.csr'), '-CA', str(WORK/'ca384.pem'),
        '-CAkey', str(WORK/'ca384.key'), '-CAcreateserial', '-days', '1', '-sha384',
        '-extfile', str(WORK/'extensions'), '-out', str(WORK/'server384.pem'))
    # subprocess/make shell quoting: protect the literal C string once.
    flags = '-Os -DMBEDTLS_CONFIG_FILE=\'"prox_mbedtls_config.h"\' -I'+str(ROOT / 'firmware/Updater')
    log = WORK / 'compile.log'
    with log.open('wb') as output:
        result = subprocess.run(['make', '-C', str(tls / 'library'), '-j4', 'CC=cc', 'AR=ar',
                                 'CFLAGS='+flags, 'all'], stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(log.read_text()[-6000:])
    run('cc', '-std=c99', '-DPROX_HTTPS_NATIVE', '-DMBEDTLS_CONFIG_FILE="prox_mbedtls_config.h"',
        '-I'+str(ROOT / 'tests/native'), '-I'+str(ROOT / 'firmware/Updater'),
        '-I'+str(ROOT / 'firmware/Common'), '-I'+str(tls / 'include'),
        str(ROOT / 'tests/native/test_update_transport.c'), str(ROOT / 'firmware/Updater/transport.c'),
        str(ROOT / 'firmware/Updater/policy.c'), str(ROOT / 'firmware/Updater/connect_retry.c'), '-L'+str(tls / 'library'),
        '-lmbedtls', '-lmbedx509', '-lmbedcrypto', '-o', str(WORK / 'transport-test'))
    run('cc', '-std=c99', '-DMBEDTLS_CONFIG_FILE="prox_mbedtls_config.h"',
        '-I'+str(ROOT/'firmware/Updater'), '-I'+str(tls/'include'),
        str(ROOT/'tests/native/test_tls_chain.c'), '-L'+str(tls/'library'),
        '-lmbedx509', '-lmbedcrypto', '-o', str(WORK/'chain-test'))


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *args):
        pass

    def do_GET(self):
        path = self.path.rsplit('/', 1)[-1]
        if path == 'redirect':
            self.send_response(302); self.send_header('Location', 'http://github.com/unsafe')
            self.send_header('Content-Length', '0'); self.end_headers()
        elif path == 'https_redirect':
            self.send_response(302)
            self.send_header('Location', 'https://api.github.com/repos/JoseOchoa95/PSP-PRO-X/ok')
            self.send_header('Content-Length', '0'); self.end_headers()
        elif path == 'loop':
            self.send_response(302)
            self.send_header('Location', 'https://api.github.com/repos/JoseOchoa95/PSP-PRO-X/loop')
            self.send_header('Content-Length', '0'); self.end_headers()
        elif path == 'chunked':
            self.send_response(200); self.send_header('Transfer-Encoding', 'chunked'); self.end_headers()
            self.wfile.write(b'2\r\nok\r\n0\r\n\r\n')
        elif path == 'rate':
            self.send_response(429); self.send_header('Retry-After', '60')
            self.send_header('X-RateLimit-Remaining', '0'); self.send_header('X-RateLimit-Reset', '123')
            self.send_header('Content-Length', '0'); self.end_headers()
        elif path == 'missing':
            self.send_response(404); self.send_header('Content-Length', '0'); self.end_headers()
        elif path == 'github_long_header':
            self.send_response(302)
            name = 'Content-Security-Policy'
            # Opaque synthetic value with the authenticated GitHub observation's
            # exact wire-line length (4175). No signed redirect query is stored.
            self.send_header(name, 'x'*(4175-len(name)-4))
            self.send_header('Location', 'https://api.github.com/repos/JoseOchoa95/PSP-PRO-X/ok')
            self.send_header('Content-Length', '0'); self.end_headers()
        elif path == 'header_line_limit':
            self.send_response(200); self.send_header('X-Padding', 'x'*9000)
            self.send_header('Content-Length', '0'); self.end_headers()
        elif path == 'headers_total_limit':
            self.send_response(200)
            for i in range(3):
                self.send_header('X-Padding-'+str(i), 'x'*6000)
            self.send_header('Content-Length', '0'); self.end_headers()
        elif path == 'location_limit':
            self.send_response(302)
            self.send_header('Location', 'https://api.github.com/repos/JoseOchoa95/PSP-PRO-X/'+'x'*2500)
            self.send_header('Content-Length', '0'); self.end_headers()
        else:
            self.send_response(200)
            self.send_header('Content-Length', '999999' if path == 'oversize' else '20' if path == 'truncated' else '2')
            self.send_header('Connection', 'close'); self.end_headers()
            self.wfile.write(b'ok')
            self.close_connection = True


@contextlib.contextmanager
def server(certificate='server'):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    key = 'server384.key' if certificate == 'server384' else 'server.key'
    context.load_cert_chain(str(WORK / (certificate+'.pem')), str(WORK / key))
    httpd = http.server.HTTPServer(('127.0.0.1', 0), Handler)
    httpd.socket = context.wrap_socket(httpd.socket, server_side=True)
    thread = threading.Thread(target=httpd.serve_forever, daemon=True); thread.start()
    try:
        yield httpd.server_port
    finally:
        httpd.shutdown(); httpd.server_close(); thread.join()


@unittest.skipUnless(os.environ.get('PROX_RUN_TLS_TESTS') == '1', 'requires isolated native TLS test environment')
class HttpsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        prepare()

    def request(self, suffix='ok', host='api.github.com', ca='ca', certificate='server', cancel='0', dns_failures=0, tcp_failures=0):
        with server(certificate) as port:
            path = ('/repos/JoseOchoa95/PSP-PRO-X/' if host == 'api.github.com' else
                    '/JoseOchoa95/PSP-PRO-X/releases/download/v0.3.0/')
            result = subprocess.run([str(WORK / 'transport-test'), str(port),
                'https://'+host+path+suffix,
                str(WORK / (ca+'.pem')), '32', cancel], capture_output=True, text=True, timeout=15,
                env=dict(os.environ, PROX_TEST_DNS_FAILURES=str(dns_failures), PROX_TEST_TCP_FAILURES=str(tcp_failures)))
        (WORK / (self._testMethodName+'.log')).write_text(result.stdout+result.stderr)
        return result.returncode, result.stdout

    def test_valid_chain_hostname_and_body(self):
        rc, out = self.request(); self.assertEqual(rc, 0, out)
        self.assertIn('verify_flags=0', out); self.assertIn('status=200 bytes=2 sink=2', out)

    def test_sha384_ecdsa_chain(self):
        rc, out = self.request(ca='ca384', certificate='server384'); self.assertEqual(rc, 0, out)
        self.assertIn('verify_flags=0', out); self.assertIn('status=200 bytes=2 sink=2', out)

    def test_sha384_wrong_hostname(self):
        rc, out = self.request(host='github.com', ca='ca384', certificate='server384')
        self.assertNotEqual(rc, 0, out); self.assertIn('TLS certificate flags=', out)
        self.assertIn('sink=0', out)

    def test_pinned_ca_bundle_complete(self):
        result = subprocess.run([str(WORK/'chain-test'), str(ROOT/'firmware/Updater/cacert.pem')],
                                capture_output=True, text=True, timeout=15)
        (WORK/(self._testMethodName+'.log')).write_text(result.stdout+result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertIn('CA skipped=0 parsed=118', result.stdout)

    def test_wrong_hostname(self):
        rc, out = self.request(host='github.com'); self.assertNotEqual(rc, 0, out)
        self.assertIn('HTTPS FAIL', out)
        self.assertIn('sink=0', out)

    def test_untrusted_chain(self):
        rc, out = self.request(ca='other'); self.assertNotEqual(rc, 0, out)
        self.assertIn('sink=0', out)

    def test_expired_certificate(self):
        rc, out = self.request(certificate='expired'); self.assertNotEqual(rc, 0, out)
        self.assertIn('sink=0', out)

    def test_http_redirect_rejected(self):
        rc, out = self.request('redirect'); self.assertNotEqual(rc, 0, out)
        self.assertIn('sink=0', out)

    def test_https_redirect_validated(self):
        rc, out = self.request('https_redirect'); self.assertEqual(rc, 0, out)
        self.assertIn('hop=1', out); self.assertIn('status=200 bytes=2 sink=2', out)

    def test_github_long_header_redirect(self):
        rc, out = self.request('github_long_header'); self.assertEqual(rc, 0, out)
        self.assertIn('hop=1', out); self.assertIn('status=200 bytes=2 sink=2', out)

    def test_header_line_limit_preserved(self):
        rc, out = self.request('header_line_limit'); self.assertNotEqual(rc, 0, out)
        self.assertIn('sink=0', out); self.assertIn('HTTP response headers rejected', out)

    def test_total_headers_limit_preserved(self):
        rc, out = self.request('headers_total_limit'); self.assertNotEqual(rc, 0, out)
        self.assertIn('sink=0', out); self.assertIn('16384-byte total limit', out)

    def test_location_limit_preserved(self):
        rc, out = self.request('location_limit'); self.assertNotEqual(rc, 0, out)
        self.assertIn('sink=0', out)

    def test_redirect_limit(self):
        rc, out = self.request('loop'); self.assertNotEqual(rc, 0, out)
        self.assertIn('hop=4', out); self.assertNotIn('hop=5', out)

    def test_truncated_body(self):
        rc, out = self.request('truncated'); self.assertNotEqual(rc, 0, out)

    def test_oversize_before_body_write(self):
        rc, out = self.request('oversize'); self.assertNotEqual(rc, 0, out)
        self.assertIn('sink=0', out)

    def test_chunked_response(self):
        rc, out = self.request('chunked'); self.assertEqual(rc, 0, out)
        self.assertIn('status=200 bytes=2 sink=2', out)

    def test_404_is_not_successful_update_decision(self):
        rc, out = self.request('missing'); self.assertEqual(rc, 0, out)
        self.assertIn('status=404 bytes=0 sink=0', out)

    def test_rate_limit_no_retry(self):
        rc, out = self.request('rate'); self.assertEqual(rc, 0, out)
        self.assertIn('retry-after=60', out); self.assertIn('status=429 bytes=0 sink=0', out)
        self.assertIn('CONNECT dns=1 tcp=1', out)

    def test_dns_recovery_fifth_attempt_authenticated(self):
        rc, out = self.request(dns_failures=4); self.assertEqual(rc, 0, out)
        self.assertIn('CONNECT dns=5 tcp=1', out); self.assertIn('verify_flags=0', out)
        self.assertIn('Connection attempt 5/5', out)

    def test_dns_exhaustion_no_tls_or_body(self):
        rc, out = self.request(dns_failures=99); self.assertNotEqual(rc, 0, out)
        self.assertIn('CONNECT dns=5 tcp=0', out); self.assertIn('sink=0', out)
        self.assertNotIn('TLS PASS', out)

    def test_tcp_recovery_and_cleanup(self):
        rc, out = self.request(tcp_failures=2); self.assertEqual(rc, 0, out)
        self.assertIn('CONNECT dns=3 tcp=3', out); self.assertIn('verify_flags=0', out)

    def test_retry_then_bad_certificate_stops(self):
        rc, out = self.request(dns_failures=1, ca='other'); self.assertNotEqual(rc, 0, out)
        self.assertIn('CONNECT dns=2 tcp=1', out); self.assertIn('sink=0', out)

    def test_redirect_shares_retry_budget(self):
        rc, out = self.request('https_redirect', tcp_failures=4); self.assertEqual(rc, 0, out)
        self.assertIn('CONNECT dns=6 tcp=6', out); self.assertIn('session retries used=4/4', out)

    def test_cancel_before_network(self):
        rc, out = self.request(cancel='2'); self.assertNotEqual(rc, 0, out)
        self.assertIn('sink=0', out)

    def test_cancel_during_download(self):
        rc, out = self.request(cancel='1'); self.assertNotEqual(rc, 0, out)
        self.assertIn('cancelled=1', out)


if __name__ == '__main__':
    unittest.main(verbosity=2)
