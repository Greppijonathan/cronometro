
/*
Implementar un cronómetro utilizando más de una tarea de FreeRTOS que muestre en pantalla el valor de la cuenta actual
con una resolución de décimas de segundo.El cronómetro debe iniciar y detener la cuenta al presionar un pulsador conectado
a la placa. Si está detenido, al presionar un segundo contador debe volver a cero.Mientras la cuenta está activa un led RGB
debe parpadear en verde y cuando está detenida debe permanecer en rojo.
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
#define TEC1_Pausa GPIO_NUM_4
#define TEC2_Reiniciar GPIO_NUM_6

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

// Variables globales para el control del tiempo
volatile bool pausaCronometro = false;
volatile bool reiniciarCuenta = false;

void ControlTiempo(void *parametros)
{
    bool estadoLed = false;
    while (1)
    {
        if (!pausaCronometro)
        {
            if (xSemaphoreTake(semaforo, portMAX_DELAY)) // Acceso al recurso compartido, para este caso los digitos del cronometro
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
                xSemaphoreGive(semaforo); // Liberar el recurso compartido
            }

            gpio_set_level(RGB_VERDE, estadoLed); // LED verde parpadeando mientras funciona
            estadoLed = !estadoLed;
            gpio_set_level(RGB_AZUL, 0);
            gpio_set_level(RGB_ROJO, 0);
        }
        else
        {
            if (reiniciarCuenta)
            {
                if (xSemaphoreTake(semaforo, portMAX_DELAY)) // Acceso al recurso nuevamente, por lo tando se debe volver a trabajar con el MUTEX
                {
                    // Reiniciar los valores del cronómetro
                    digitosActuales_t->unidades_segundos = 0;
                    digitosActuales_t->decenas_segundos = 0;
                    digitosActuales_t->unidades_minutos = 0;
                    digitosActuales_t->decenas_minutos = 0;
                    xSemaphoreGive(semaforo); // Se libera el recurso compartido
                }

                reiniciarCuenta = false; // Se limpia la bandera de reinicio
            }
            else // Si está en pausa pero no se reinició
            {
                gpio_set_level(RGB_ROJO, estadoLed); // Parpadeo de led rojo para indicar que se encuentra en pausa
                estadoLed = !estadoLed;
                gpio_set_level(RGB_VERDE, 0);
                gpio_set_level(RGB_AZUL, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void EscanearPulsadores(void *parametros)
{
    bool estadoAnteriorPulsadorTEC1_Pausa = true;
    bool estadoAnteriorPulsadorTEC2_Reiniciar = true;
    while (1)
    {
        bool estadoActualTEC1_Pausa = gpio_get_level(TEC1_Pausa);
        if (!estadoActualTEC1_Pausa && estadoAnteriorPulsadorTEC1_Pausa)
        {
            pausaCronometro = !pausaCronometro;
        }
        estadoAnteriorPulsadorTEC1_Pausa = estadoActualTEC1_Pausa;

        bool estadoActualTEC2_Reiniciar = gpio_get_level(TEC2_Reiniciar);
        if (!estadoActualTEC2_Reiniciar && estadoAnteriorPulsadorTEC2_Reiniciar)
        {
            if (pausaCronometro)
            {
                reiniciarCuenta = true;
            }
        }
        estadoAnteriorPulsadorTEC2_Reiniciar = estadoActualTEC2_Reiniciar;

        vTaskDelay(pdMS_TO_TICKS(50)); // Manejo del rebote de pulsadores
    }
}

void ActualizarPantalla(void *parametros)
{
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    panel_t horas = CrearPanel(30, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t minutos = CrearPanel(170, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);

    struct digitos_previos
    {
        int decenasAnteriorMinutos;
        int unidadesAnteriorMinutos;
        int decenasAnteriorSegundos;
        int unidadesAnteriorSegundos;
    } digitosPrevios = {-1, -1, -1, -1}; // Valores iniciales "invalidados", para no comparar con un valor existente antes

    // Logica para detectar el cambio en alguno de los digitos y solo actualizar la pantalla en el que hay cambio
    while (1)
    {
        if (xSemaphoreTake(semaforo, portMAX_DELAY))
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
        vTaskDelay(pdMS_TO_TICKS(50)); // Actualización de pantalla cada 1 ms
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
    gpio_set_direction(TEC1_Pausa, GPIO_MODE_INPUT);
    gpio_set_pull_mode(TEC1_Pausa, GPIO_PULLUP_ONLY); // Resistencia de pull-up

    gpio_set_direction(TEC2_Reiniciar, GPIO_MODE_INPUT);
    gpio_set_pull_mode(TEC2_Reiniciar, GPIO_PULLUP_ONLY); // Resistencia de pull-up

    // Crear el semáforo
    semaforo = xSemaphoreCreateMutex();

    // Crear las tareas
    xTaskCreate(ControlTiempo, "ControlTiempo", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(EscanearPulsadores, "EscanearPulsadores", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(ActualizarPantalla, "ActualizarPantalla", 4096, NULL, tskIDLE_PRIORITY + 2, NULL);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
