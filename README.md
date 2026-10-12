# todo

- [ ] reservoir sensor - onboard to publish level, ota to parse, determine stale time difference and act accordingly
- [ ] moisture level to publish level
- [ ] app config to receive ota updates
- [ ] OTA flash/version bumps
- [ ] OTA logs
- [ ] setup multiple nodes and test sync

## Bugs

acd1 oneshot read error

```bash
D (10446) transport: Event dispatched from event loop base=li, event_id=%
I (10446) transport: MQTT_EVENT_PUBLISHED, msg_id=20710
E (10676) adc_oneshot: adc_oneshot_new_unit(99): adc1 is already in use
E (10676) analog_read: failed to init oneshot unit: ESP_ERR_NOT_FOUND
E (10676) channel_control: failed to read moisture sensor: ESP_ERR_NOT_FOUND
```
