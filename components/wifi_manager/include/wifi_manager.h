#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// Función principal que evalúa si arranca en modo STA (con credenciales NVS) o modo AP (Portal Cautivo)
void wifi_init_sta_or_ap(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H


