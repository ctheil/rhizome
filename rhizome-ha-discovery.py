#!/usr/bin/env python3
"""Publish Home Assistant MQTT discovery configs for Rhizome channels.

One HA device per channel, state from rhizome/<group>/channel-state/<ch>.

    pip install paho-mqtt
    python rhizome_ha_discovery.py --host 192.168.1.10 --group main-room --channels 0 1
    python rhizome_ha_discovery.py ... --remove     # clears the retained configs
"""
import argparse
import json
import sys
import threading
import paho.mqtt.client as mqtt

DISC = "homeassistant"

# Index == firmware enum value (FAULT_NONE = 0, ...). Keep in sync with firmware.
FAULTS = ["none", "unknown", "reservoir_dry", "reservoir_fault",
          "pump_without_effect", "sensor_out_of_range"]

# Same rule for the reservoir level enum (RESERVOIR_UNKNOWN = 0, ...).
RESERVOIR = ["unknown", "ok", "stale", "empty", "fault"]


def channel_entities(group, ch):
    state_topic = f"rhizome/{group}/channel-state/{ch}"
    dev_id = f"rhizome_{group}_ch{ch}"
    device = {
        "identifiers": [dev_id],
        "name": f"Rhizome {group} ch{ch}",
        "manufacturer": "Rhizome",
        "model": "Plant channel",
    }

    def ent(comp, key, **kw):
        cfg = {
            "name": kw.pop("name"),
            "unique_id": f"{dev_id}_{key}",
            "state_topic": state_topic,
            "device": device,
            **kw,
        }
        return f"{DISC}/{comp}/{dev_id}/{key}/config", cfg

    yield ent("sensor", "dryness", name="Soil dryness",
              value_template="{{ value_json.moisture_sensor_dry_percentage }}",
              unit_of_measurement="%", state_class="measurement",
              icon="mdi:water-percent")
    yield ent("sensor", "pre_water_dryness", name="Pre-water dryness",
              value_template="{{ value_json.pre_water_dry_percentage }}",
              unit_of_measurement="%", state_class="measurement",
              entity_category="diagnostic")
    yield ent("sensor", "raw", name="Moisture raw ADC",
              value_template="{{ value_json.moisture_sensor_raw_reading }}",
              state_class="measurement", entity_category="diagnostic",
              enabled_by_default=False)
    yield ent("binary_sensor", "pump", name="Pump",
              value_template="{{ value_json.pump_state }}",
              payload_on="1", payload_off="0",
              device_class="running")
    yield ent("binary_sensor", "fault", name="Fault",
              value_template="{{ 1 if value_json.pump_fault | int(0) != 0 else 0 }}",
              payload_on="1", payload_off="0",
              device_class="problem", entity_category="diagnostic",
              json_attributes_topic=state_topic,
              json_attributes_template=(
                  "{{ {'pump_fault': value_json.pump_fault,"
                  " 'error': value_json.error} | tojson }}"))
    yield ent("sensor", "error", name="Error",
              value_template="{{ value_json.error | default('', true) }}",
              icon="mdi:alert-circle-outline", entity_category="diagnostic")
    yield ent("sensor", "pump_fault", name="Pump fault",
              device_class="enum", options=FAULTS,
              value_template=("{{ " + repr(FAULTS) + "[value_json.pump_fault | int(1)]"
                              " | default('unknown', true) }}"),
              icon="mdi:alert-octagon-outline", entity_category="diagnostic")
    yield ent("binary_sensor", "override", name="Manual override",
              value_template="{{ value_json.override_active }}",
              payload_on="1", payload_off="0")
    yield ent("sensor", "override_ttl", name="Override TTL",
              value_template="{{ value_json.override_expiry_ms }}",
              device_class="duration", unit_of_measurement="ms",
              entity_category="diagnostic")
    yield ent("sensor", "profile", name="Profile",
              value_template="{{ value_json.profile_name }}",
              icon="mdi:sprout", entity_category="diagnostic")
    yield ent("sensor", "start_time", name="Start time (ms since boot)",
              value_template="{{ value_json.start_time_ms }}",
              entity_category="diagnostic", enabled_by_default=False)


def reservoir_entities(group, topic):
    dev_id = f"rhizome_{group}"
    device = {"identifiers": [dev_id], "name": f"Rhizome {group}",
              "manufacturer": "Rhizome"}

    def ent(comp, key, **kw):
        cfg = {
            "name": kw.pop("name"),
            "unique_id": f"{dev_id}_{key}",
            "state_topic": topic,
            "device": device,
            **kw,
        }
        return f"{DISC}/{comp}/{dev_id}/{key}/config", cfg

    yield ent("sensor", "reservoir", name="Reservoir",
              device_class="enum", options=RESERVOIR,
              value_template=("{{ " + repr(RESERVOIR) + "[value_json.level | int(0)]"
                              " | default('unknown', true) }}"),
              icon="mdi:water-pump",
              json_attributes_topic=topic,
              json_attributes_template=(
                  "{{ {'level': value_json.level,"
                  " 'timestamp': value_json.timestamp} | tojson }}"))
    yield ent("sensor", "reservoir_raw", name="Reservoir raw reading",
              value_template="{{ value_json.raw_reading }}",
              state_class="measurement", entity_category="diagnostic")
    # Anything but OK blocks pumping (fail-safe), so treat it as a problem.
    yield ent("binary_sensor", "reservoir_problem", name="Reservoir problem",
              value_template="{{ 0 if value_json.level | int(0) == 1 else 1 }}",
              payload_on="1", payload_off="0", device_class="problem")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--reservoir-topic",
                   help="full MQTT topic the reservoir level is published on")
    p.add_argument("--host", required=True)
    p.add_argument("--port", type=int, default=1883)
    p.add_argument("--user")
    p.add_argument("--password")
    p.add_argument("--group", required=True)
    p.add_argument("--channels", nargs="+", required=True)
    p.add_argument("--remove", action="store_true")
    a = p.parse_args()

    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if a.user:
        c.username_pw_set(a.user, a.password)

    connected = threading.Event()
    result = {}

    def on_connect(client, userdata, flags, reason_code, properties):
        result["rc"] = reason_code
        connected.set()

    c.on_connect = on_connect
    try:
        c.connect(a.host, a.port)
    except OSError as e:
        sys.exit(f"can't reach {a.host}:{a.port}: {e}")
    c.loop_start()
    if not connected.wait(10):
        sys.exit("no CONNACK from broker within 10s")
    if result["rc"].is_failure:
        sys.exit(f"broker refused connection: {result['rc']} "
                 "(auth required? pass --user/--password)")

    items = []
    if a.reservoir_topic:
        items.extend(reservoir_entities(a.group, a.reservoir_topic))
    for ch in a.channels:
        items.extend(channel_entities(a.group, ch))

    for topic, cfg in items:
        payload = "" if a.remove else json.dumps(cfg)
        info = c.publish(topic, payload, qos=1, retain=True)
        info.wait_for_publish(timeout=5)
        if not info.is_published():
            sys.exit(f"publish timed out: {topic}")
    print(f"{'removed' if a.remove else 'published'} {len(items)} configs")
    c.loop_stop()
    c.disconnect()


if __name__ == "__main__":
    main()