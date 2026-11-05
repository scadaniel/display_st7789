#ifndef RELOJ_MANAGER_H
#define RELOJ_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa la zona horaria, el servicio SNTP y crea la tarea para actualizar la pantalla
void reloj_manager_init(void);

#ifdef __cplusplus
}
#endif

#endif // RELOJ_MANAGER_H