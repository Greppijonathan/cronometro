#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "ili9341.h"
#include "digitos.h"
#include "stdio.h"
#include "driver/gpio.h"

// Definiciones para el manejo de los digitos de la pantalla tft
#define DIGITO_ANCHO 60
#define DIGITO_ALTO 100
#define DIGITO_ENCENDIDO ILI9341_BLUE2
#define DIGITO_APAGADO 0x3800
#define DIGITO_FONDO ILI9341_BLACK

// Pines del led RGB
#define RGB_ROJO GPIO_NUM_1
#define RGB_VERDE GPIO_NUM_8
#define RGB_AZUL GPIO_NUM_10

// Estructura para el manejo de los digitos del cronometro, en este caso es el recurso compartido que se debe cuidar al usar semaforos
struct digitos_cronometro
{
    int unidades_segundos;
    int decenas_segundos;
    int unidades_minutos;
    int decenas_minutos;
} *digitosActuales_t;

// Se decalara el semaforo
SemaphoreHandle_t semaforo;

void ControlTiempo(void *parametros)
{
    while (1)
    {

        if (xSemaphoreTake(semaforo, portMAX_DELAY)) // Si el semaforo indica que el recurso compartido esta disponible, se accede a modificar los digitos
        {
            digitosActuales_t
                ->unidades_segundos = (digitosActuales_t
                                           ->unidades_segundos +
                                       1) %
                                      10;
            if (digitosActuales_t
                    ->unidades_segundos == 0)
            {
                digitosActuales_t
                    ->decenas_segundos = (digitosActuales_t
                                              ->decenas_segundos +
                                          1) %
                                         6;
                if (digitosActuales_t
                        ->decenas_segundos == 0)
                {
                    digitosActuales_t
                        ->unidades_minutos = (digitosActuales_t
                                                  ->unidades_minutos +
                                              1) %
                                             10;
                    if (digitosActuales_t
                            ->unidades_minutos == 0)
                    {
                        digitosActuales_t
                            ->decenas_minutos = (digitosActuales_t
                                                     ->decenas_minutos +
                                                 1) %
                                                6;
                    }
                }
            }
            xSemaphoreGive(semaforo); // Se libera el recurso, se lo indica en el semaforo
        }
        // Cada un segundo ejecutamos el incremento de las variables del cronometro
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void ActualizarPantalla(void *parametros)
{
    // Se inicializa la pantalla TFT
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    // Se crean los paneles-dígitos para mostrar los segmentos prendidos
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

    // En esta seccion se muestran en pantalla los digitos y se verifica si hubo un cambio para poder optimizar la actualizacion
    while (1)
    {
        if (xSemaphoreTake(semaforo, portMAX_DELAY)) // Se accede al recurso compartido cuando está disponible
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

void app_main(void)
{
    // Se inicializan los digitos del cronometro
    digitosActuales_t = malloc(sizeof(struct digitos_cronometro));
    digitosActuales_t
        ->unidades_segundos = 0;
    digitosActuales_t
        ->decenas_segundos = 0;
    digitosActuales_t
        ->unidades_minutos = 0;
    digitosActuales_t
        ->decenas_minutos = 0;

    // Se declaran como salida los pines del led RGB
    gpio_set_direction(RGB_ROJO, GPIO_MODE_OUTPUT);
    gpio_set_direction(RGB_VERDE, GPIO_MODE_OUTPUT);
    gpio_set_direction(RGB_AZUL, GPIO_MODE_OUTPUT);

        gpio_set_level(RGB_ROJO, 0);
    gpio_set_level(RGB_VERDE, 0);
    gpio_set_level(RGB_AZUL, 1);

    // Creamos el semaforo
    semaforo = xSemaphoreCreateMutex();

    xTaskCreate(ControlTiempo, "ControlTiempo", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(ActualizarPantalla, "ActualizarPantalla", 4096, NULL, tskIDLE_PRIORITY + 2, NULL);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
