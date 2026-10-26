#ifndef MQTT_CLIENT_WRAPPER_H
#define MQTT_CLIENT_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

void mqtt_app_start(void);
void mqtt_publish_sensor_data(float temp, float hum);

#ifdef __cplusplus
}
#endif

#endif // MQTT_CLIENT_WRAPPER_H
