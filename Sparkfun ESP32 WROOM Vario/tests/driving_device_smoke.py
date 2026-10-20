"""Stationary IMU/SD check on an explicitly selected, already-flashed device.
Usage: python driving_device_smoke.py http://192.168.50.99 <artifact-directory>
Device must be parked/still with its configured forward axis near horizontal.
Starts, re-zeros, and stops a practice session. Never injects sensor readings.
"""
import csv
import io
import json
from pathlib import Path
import sys
import time
import urllib.parse
import urllib.request


def main():
    base, output_arg = sys.argv[1:]
    output = Path(output_arg)
    output.mkdir(parents=True, exist_ok=True)
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def request(path, post=False):
        req = urllib.request.Request(base.rstrip('/') + path, data=b'' if post else None)
        with opener.open(req, timeout=8) as response:
            return response.read().decode()

    def get_json(path):
        return json.loads(request(path))

    before = get_json('/api/settings')
    assert before['fw_version'] == '1.6.0', before.get('fw_version')
    assert not before['gps_enabled'], 'Disable GPS before this independence check'
    assert not get_json('/api/driving')['active'], 'A practice session is already active'
    samples = []
    started = False
    try:
        request('/api/driving/start', post=True)
        started = True
        deadline = time.monotonic() + 35
        valid_samples = 0
        zero_requested = False
        while time.monotonic() < deadline:
            time.sleep(1)
            state = get_json('/api/driving')
            state['host_monotonic'] = time.monotonic()
            samples.append(state)
            assert state['active'] and state['logging'], state
            assert state['source'] == 'imu' and state['sample_hz'] == 120
            assert state['imu_faults'] == 0 and state['dropped_log_rows'] == 0, state
            assert not get_json('/api/settings')['gps_enabled'], 'Practice enabled GPS'
            if state['calibrated'] and state['accel_mph_s'] is not None:
                assert abs(state['accel_mph_s']) < 1, state
                assert state['fast_accels'] == 0 and state['hard_brakes'] == 0, state
                valid_samples += 1
            if len(samples) == 1 or valid_samples == 1:
                print(json.dumps(state), flush=True)
            if valid_samples >= 5 and not zero_requested:
                request('/api/driving/calibrate', post=True)
                zero_requested = True
                valid_samples = 0
            if zero_requested and valid_samples >= 5:
                break
        assert zero_requested and valid_samples >= 5, 'Calibration/measurements did not complete'
    finally:
        if started:
            request('/api/driving/stop', post=True)
            time.sleep(1)
        (output / 'device-driving-samples.json').write_text(json.dumps(samples, indent=2))

    after = get_json('/api/driving')
    assert not after['active'], 'Practice did not stop'
    settings = get_json('/api/settings')
    assert settings['gps_enabled'] == before['gps_enabled'], 'GPS setting changed'
    assert settings['imu_enabled'] == before['imu_enabled'], 'IMU setting was not restored'
    path = after['log_path']
    text = request('/file?path=' + urllib.parse.quote(path, safe=''))
    rows = list(csv.DictReader(io.StringIO(text)))
    (output / Path(path).name).write_text(text, newline='')
    assert rows[0]['record'] == 'session_start'
    assert rows[-1]['record'] == 'session_end'
    assert all(None not in row and None not in row.values() for row in rows), 'CSV shape mismatch'
    assert all(r['source'] == 'imu' and r['fw_version'] == '1.6.0' for r in rows)
    assert sum(r['record'] == 'calibrated' for r in rows) == 2, 'Re-zero did not complete'
    measurements = [r for r in rows if r['record'] == 'sample']
    assert len(measurements) >= 70, len(measurements)
    for row in measurements:
        assert int(row['sample_hz']) == 120
        assert abs(float(row['accel_mph_s'])) < 1
        assert abs(float(row['linear_g']) * (9.80665 / .44704) - float(row['accel_mph_s'])) < .003
    elapsed = samples[-1]['host_monotonic'] - samples[0]['host_monotonic']
    hz = (samples[-1]['samples'] - samples[0]['samples']) / elapsed
    assert 110 <= hz <= 130, f'Unexpected actual IMU rate: {hz:.1f} Hz'
    print(f'PASS: {hz:.1f} Hz live IMU, GPS off throughout, two calibrations, no false alerts.')
    print(f'PASS: {len(measurements)} SD samples, session end, settings restored. Log: {path}')


if __name__ == '__main__':
    main()
