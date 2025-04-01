/*
Requisitos mínimos: Implementar un cronómetro utilizando más de una tarea de FreeRTOS. El
cronómetro debe iniciar y detener la cuenta al presionar la tecla TEC1. Si está detenido, al
presionar la tecla TEC2 debe volver a cero. Mientras la cuenta está activa el red RGB debe
parpadear en verde y cuando está detenida debe permanecer en rojo. Mientras la cuenta está
activa se debe congelar el valor parcial con la tecla TEC3, es decir que se mantiene el valor que
se muestra en pantalla fijo pero la cuenta continúa activa. En este estado, al presionar
nuevamente la tecla TEC3 se recupera el valor actual de la cuenta en vivo.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "ili9341.h"
#include "digitos.h"
#include "stdio.h"
#include "driver/gpio.h"

// Definiciones para el manejo de los dígitos de la pantalla TFT
#define DIGITO_ANCHO 60
#define DIGITO_ALTO 100
#define DIGITO_ENCENDIDO ILI9341_BLUE2
#define DIGITO_APAGADO 0x3800
#define DIGITO_FONDO ILI9341_BLACK

// Pines del LED RGB
#define RGB_ROJO GPIO_NUM_1
#define RGB_VERDE GPIO_NUM_8
#define RGB_AZUL GPIO_NUM_10

// Pines conectados a pulsadores
#define TEC1 GPIO_NUM_4
// #define TEC2 GPIO_NUM_4

// Estructura para el manejo de los dígitos del cronómetro
struct digitos_cronometro
{
    int unidades_segundos;
    int decenas_segundos;
    int unidades_minutos;
    int decenas_minutos;
} *digitosActuales_t;

volatile int estadoVerde = 0;
// Declaración del semáforo
SemaphoreHandle_t semaforo;

// Variable global para pausar o reanudar cronómetro
volatile bool pausaCronometro = false;
// Variable global para pausar o reanaudar cuenta
volatile bool reiniciarCuenta = false;

void ControlTiempo(void *parametros)
{
    bool estadoLed = false; // Control del parpadeo del LED
    while (1)
    {
        if (!pausaCronometro) // Si el cronómetro no está en pausa
        {
            if (xSemaphoreTake(semaforo, portMAX_DELAY)) // Acceso al recurso compartido
            {
                // Actualizar los dígitos del cronómetro
                digitosActuales_t->unidades_segundos = (digitosActuales_t->unidades_segundos + 1) % 10;
                if (digitosActuales_t->unidades_segundos == 0)
                {
                    digitosActuales_t->decenas_segundos = (digitosActuales_t->decenas_segundos + 1) % 6;
                    if (digitosActuales_t->decenas_segundos == 0)
                    {
                        digitosActuales_t->unidades_minutos = (digitosActuales_t->unidades_minutos + 1) % 10;
                        if (digitosActuales_t->unidades_minutos == 0)
                        {
                            digitosActuales_t->decenas_minutos = (digitosActuales_t->decenas_minutos + 1) % 6;
                        }
                    }
                }
                xSemaphoreGive(semaforo); // Liberar el recurso
            }

            gpio_set_level(RGB_VERDE, estadoLed);
            estadoLed = !estadoLed;
            gpio_set_level(RGB_AZUL, 0);
            gpio_set_level(RGB_ROJO, 0);
        }
        // Parpadeo LED azul mientras está en funcionamiento
        if (pausaCronometro)
        {
            gpio_set_level(RGB_AZUL, estadoLed);
            estadoLed = !estadoLed;
            gpio_set_level(RGB_VERDE, 0);
            gpio_set_level(RGB_ROJO, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void ActualizarPantalla(void *parametros)
{
    // Se inicializa la pantalla TFT
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    // Se crean los paneles-dígitos
    panel_t horas = CrearPanel(30, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t minutos = CrearPanel(170, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);

    // Declaración e inicialización de estructura para valores previos
    struct digitos_previos
    {
        int decenasAnteriorMinutos;
        int unidadesAnteriorMinutos;
        int decenasAnteriorSegundos;
        int unidadesAnteriorSegundos;
    } digitosPrevios = {-1, -1, -1, -1}; // Valores iniciales "invalidados"

    while (1)
    {
        if (xSemaphoreTake(semaforo, portMAX_DELAY)) // Acceso al recurso compartido, la estructura de los digitos en este caso
        {
            if (digitosActuales_t->decenas_minutos != digitosPrevios.decenasAnteriorMinutos)
            {
                DibujarDigito(horas, 0, digitosActuales_t->decenas_minutos);
                digitosPrevios.decenasAnteriorMinutos = digitosActuales_t->decenas_minutos;
            }
            if (digitosActuales_t->unidades_minutos != digitosPrevios.unidadesAnteriorMinutos)
            {
                DibujarDigito(horas, 1, digitosActuales_t->unidades_minutos);
                digitosPrevios.unidadesAnteriorMinutos = digitosActuales_t->unidades_minutos;
            }
            if (digitosActuales_t->decenas_segundos != digitosPrevios.decenasAnteriorSegundos)
            {
                DibujarDigito(minutos, 0, digitosActuales_t->decenas_segundos);
                digitosPrevios.decenasAnteriorSegundos = digitosActuales_t->decenas_segundos;
            }
            if (digitosActuales_t->unidades_segundos != digitosPrevios.unidadesAnteriorSegundos)
            {
                DibujarDigito(minutos, 1, digitosActuales_t->unidades_segundos);
                digitosPrevios.unidadesAnteriorSegundos = digitosActuales_t->unidades_segundos;
            }
            ILI9341DrawFilledCircle(160, 90, 5, DIGITO_ENCENDIDO);
            ILI9341DrawFilledCircle(160, 130, 5, DIGITO_ENCENDIDO);
            xSemaphoreGive(semaforo); // Liberar el semáforo
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Actualización de pantalla cada 100 ms
    }
}

// Tarea para controlar el pulsador
void EscanearPulsadores(void *parametros)
{
    bool estadoAnteriorPulsadorTEC1 = true; // Estado anterior del pulsador TEC1
    bool estadoAnteriorPulsadorTEC2 = true; // Estado anterior del pulsador TEC2
    while (1)
    {
        // Escaneo del pulsador TEC1
        bool estadoActualTEC1 = gpio_get_level(TEC1);        // Leer el estado actual de TEC1
        if (!estadoActualTEC1 && estadoAnteriorPulsadorTEC1) // Detecta flanco de bajada
        {
            pausaCronometro = !pausaCronometro; // Alternar pausa del cronómetro
        }
        estadoAnteriorPulsadorTEC1 = estadoActualTEC1; // Actualizar el estado anterior para TEC1

        // Escaneo del pulsador TEC2
        /*       bool estadoActualTEC2 = gpio_get_level(TEC2);        // Leer el estado actual de TEC2
               if (!estadoActualTEC2 && estadoAnteriorPulsadorTEC2) // Detecta flanco de bajada
               {
                   reiniciarCuenta = !reiniciarCuenta; // Alternar reinicio de cuenta
               }
               estadoAnteriorPulsadorTEC2 = estadoActualTEC2; // Actualizar el estado anterior para TEC2

               // Retraso para debounce*/
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    // Se inicializan los dígitos del cronómetro
    digitosActuales_t = malloc(sizeof(struct digitos_cronometro));
    digitosActuales_t->unidades_segundos = 0;
    digitosActuales_t->decenas_segundos = 0;
    digitosActuales_t->unidades_minutos = 0;
    digitosActuales_t->decenas_minutos = 0;

    // Configuración de los pines del LED RGB
    gpio_set_direction(RGB_ROJO, GPIO_MODE_OUTPUT);
    gpio_set_direction(RGB_VERDE, GPIO_MODE_OUTPUT);
    gpio_set_direction(RGB_AZUL, GPIO_MODE_OUTPUT);
    gpio_set_level(RGB_ROJO, 0);
    gpio_set_level(RGB_VERDE, 0);
    gpio_set_level(RGB_AZUL, 0);

    // Configuración de los pines de los pulsadores
    gpio_set_direction(TEC1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(TEC1, GPIO_PULLUP_ONLY); // Resistencia de pull-up

    //   gpio_set_direction(TEC2, GPIO_MODE_INPUT);
    //   gpio_set_pull_mode(TEC2, GPIO_PULLUP_ONLY); // Resistencia de pull-up

    // Crear el semáforo
    semaforo = xSemaphoreCreateMutex();

    // Crear las tareas
    xTaskCreate(ControlTiempo, "ControlTiempo", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(ActualizarPantalla, "ActualizarPantalla", 4096, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(EscanearPulsadores, "LecturaPulsadores", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Evitar saturar la CPU
    }
}
