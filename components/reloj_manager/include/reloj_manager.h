#ifndef RELOJ_MANAGER_H
#define RELOJ_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa la zona horaria, el servicio SNTP y crea la tarea para actualizar la pantalla
void reloj_manager_init(void);

// Obtiene la fecha y hora actual formateada (Ej: "2026-11-11 12:50:00")
void reloj_manager_get_datetime_str(char *dest, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // RELOJ_MANAGER_H



