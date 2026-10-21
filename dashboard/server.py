#!/usr/bin/env python3
"""Local ROS 2 Jazzy dashboard. No third-party web runtime required."""
import argparse
import json
import mimetypes
import secrets
import signal
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from core import Fleet

STATIC = Path(__file__).parent / 'static'


def json_safe(value):
    # ROS floating-point fields may legitimately contain NaN (unknown).
    import math
    if isinstance(value, float) and not math.isfinite(value):
        return None
    if isinstance(value, dict):
        return {key: json_safe(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [json_safe(item) for item in value]
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=8080)
    parser.add_argument('--preview', action='store_true', help='UI only, without ROS or motor commands')
    args = parser.parse_args()
    lock = threading.RLock()
    token = secrets.token_urlsafe(32)
    node = None
    catalog = []
    publishers, subscriptions = {}, {}

    def publish(robot, v, w):
        msg = Twist()
        msg.linear.x, msg.angular.z = v, w
        publishers[robot].publish(msg)

    fleet = Fleet(publish)
    if not args.preview:
        import rclpy
        from geometry_msgs.msg import Twist
        from sensor_msgs.msg import Imu
        from std_msgs.msg import Int64MultiArray, Float32, String
        from rclpy.qos import qos_profile_sensor_data
        from rosidl_runtime_py.convert import message_to_ordereddict
        from rclpy.signals import SignalHandlerOptions
        rclpy.init(signal_handler_options=SignalHandlerOptions.NO)
        node = rclpy.create_node('panopticon_dashboard')
        types = {'encoder_ticks': (Int64MultiArray, 'std_msgs/msg/Int64MultiArray'),
                 'imu/data_raw': (Imu, 'sensor_msgs/msg/Imu'),
                 'battery_voltage': (Float32, 'std_msgs/msg/Float32'),
                 'status': (String, 'std_msgs/msg/String')}

        def discover():
            nonlocal catalog
            with lock:
                catalog = [{'name': name, 'types': names} for name, names in node.get_topic_names_and_types()]
                for item in catalog:
                    for suffix, (cls, typename) in types.items():
                        name = item['name']
                        if not name.endswith('/' + suffix) or typename not in item['types'] or name in subscriptions:
                            continue
                        robot = name[:-len(suffix)-1]
                        # Firmware robots always have a namespace.
                        if not robot:
                            continue
                        if robot not in publishers:
                            publishers[robot] = node.create_publisher(Twist, robot + '/cmd_vel', qos_profile_sensor_data)
                        def receive(msg, robot=robot, suffix=suffix):
                            value = message_to_ordereddict(msg) if suffix == 'imu/data_raw' else msg.data
                            if suffix == 'encoder_ticks':
                                value = list(value)
                                if len(value) != 2:
                                    return
                            with lock:
                                fleet.receive(robot, suffix, value)
                        subscriptions[name] = node.create_subscription(cls, name, receive, qos_profile_sensor_data)
        node.create_timer(1.0, discover)
        def watchdog():
            with lock:
                fleet.tick()
        node.create_timer(.05, watchdog)
        def spin():
            from rclpy.executors import ExternalShutdownException
            try:
                rclpy.spin(node)
            except ExternalShutdownException:
                pass
        threading.Thread(target=spin, daemon=True).start()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def allowed(self):
            return self.headers.get('Host') in (f'localhost:{args.port}', f'127.0.0.1:{args.port}')

        def reply(self, status, body, content_type='application/json'):
            raw = body if isinstance(body, bytes) else json.dumps(json_safe(body), allow_nan=False).encode()
            self.send_response(status)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(raw)))
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Security-Policy', "default-src 'self'; style-src 'self'; script-src 'self'; frame-ancestors 'none'")
            self.end_headers()
            self.wfile.write(raw)

        def do_GET(self):
            if not self.allowed():
                return self.reply(403, {'error': 'Use localhost'})
            if self.path == '/api/state':
                with lock:
                    payload = {'robots': fleet.snapshot(), 'catalog': catalog, 'token': token, 'preview': args.preview}
                return self.reply(200, payload)
            name = {'/': 'index.html', '/app.js': 'app.js', '/style.css': 'style.css'}.get(self.path)
            if not name:
                return self.reply(404, {'error': 'Not found'})
            self.reply(200, (STATIC / name).read_bytes(), mimetypes.guess_type(name)[0] or 'text/plain')

        def do_POST(self):
            if not self.allowed() or self.headers.get('X-Control-Token') != token:
                return self.reply(403, {'error': 'Invalid control token'})
            if self.path != '/api/command' or args.preview:
                return self.reply(404, {'error': 'Control unavailable'})
            try:
                size = int(self.headers.get('Content-Length', 0))
                if not 0 < size < 2048:
                    raise ValueError('Invalid request size')
                data = json.loads(self.rfile.read(size))
                with lock:
                    fleet.command(data)
                self.reply(200, {'ok': True})
            except (ValueError, KeyError, TypeError, OverflowError) as error:
                self.reply(400, {'error': str(error)})

    def terminate(*_):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, terminate)
    server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    print(f'Panopticon: http://localhost:{args.port}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        with lock:
            fleet.stop()
        server.server_close()
        if node:
            rclpy.try_shutdown()


if __name__ == '__main__':
    main()
