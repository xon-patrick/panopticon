"""Telemetry and command lease logic, independent of ROS for testing."""
import math
import time


class Fleet:
    def __init__(self, publish, clock=time.monotonic):
        self.publish = publish
        self.clock = clock
        self.robots = {}
        self.active = None
        self.sequence = {}

    def receive(self, robot, topic, value):
        now = self.clock()
        state = self.robots.setdefault(robot, {'topics': {}, 'yaw': 0.0})
        old = state['topics'].get(topic)
        dt = now - old['time'] if old else 0
        entry = {'value': value, 'time': now, 'hz': 1 / dt if dt > 0 else 0}
        if topic == 'encoder_ticks':
            entry['rate'] = [(value[i] - old['value'][i]) / dt for i in range(2)] if 0 < dt < .5 else None
        if topic == 'imu/data_raw' and 0 < dt < .2:
            state['yaw'] += value['angular_velocity']['z'] * dt
        state['topics'][topic] = entry

    def online(self, robot):
        entry = self.robots.get(robot, {}).get('topics', {}).get('encoder_ticks')
        return bool(entry and self.clock() - entry['time'] < 1)

    def stop(self):
        if self.active:
            self.publish(self.active['robot'], 0.0, 0.0)
            self.active = None

    def command(self, data):
        if not isinstance(data, dict):
            raise ValueError('Expected a command object')
        robot, client, seq = data['robot'], data['client'], data['seq']
        v, w = float(data['linear']), float(data['angular'])
        if not isinstance(robot, str) or not isinstance(client, str) or len(client) > 80:
            raise ValueError('Invalid identity')
        if type(seq) is not int or not all(map(math.isfinite, (v, w))) or abs(v) > .15 or abs(w) > .8:
            raise ValueError('Invalid command')
        key = (client, robot)
        if seq <= self.sequence.get(key, (-1, 0))[0]:
            raise ValueError('Old command')
        self.sequence[key] = (seq, self.clock())
        if v == 0 and w == 0:
            if self.active and self.active['robot'] == robot:
                self.stop()
            return
        if not self.online(robot):
            raise ValueError('Robot telemetry is offline')
        if self.active and self.active['client'] != client:
            raise ValueError('Teleoperation is in use in another window')
        if self.active and self.active['robot'] != robot:
            self.stop()
        self.active = {'robot': robot, 'client': client, 'until': self.clock() + .25, 'v': v, 'w': w}
        self.publish(robot, v, w)

    def tick(self):
        if self.active and (self.clock() >= self.active['until'] or not self.online(self.active['robot'])):
            self.stop()
        self.sequence = {k: v for k, v in self.sequence.items() if self.clock() - v[1] < 3600}

    def snapshot(self):
        now = self.clock()
        return {name: {'online': self.online(name), 'yaw': state['yaw'],
                       'controlling': bool(self.active and self.active['robot'] == name),
                       'topics': {topic: {**entry, 'age': now - entry['time']} for topic, entry in state['topics'].items()}}
                for name, state in self.robots.items()}
