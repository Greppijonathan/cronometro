/*
gpio_set_direction(LED_RGB, GPIO_MODE_OUTPUT);
gpio_set_level(LED_RGB, 0);
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ili9341.h"
#include "digitos.h"
#include "stdio.h"
#include "driver/gpio.h"

#define DIGITO_ANCHO 60
#define DIGITO_ALTO 100
#define DIGITO_ENCENDIDO ILI9341_BLUE2
#define DIGITO_APAGADO 0x3800
#define DIGITO_FONDO ILI9341_BLACK

// Estructura para el manejo de los digitos del cronometro

struct digitos_cronometro
{
    int unidades_segundos;
    int decenas_segundos;
    int unidades_minutos;
    int decenas_minutos;
} *digitos_t;

void tft(void)
{
    // Inicialización de las variables del cronometro
    digitos_t = malloc(sizeof(struct digitos_cronometro));
    digitos_t->unidades_segundos = 0;
    digitos_t->decenas_segundos = 0;
    digitos_t->unidades_minutos = 0;
    digitos_t->decenas_minutos = 0;

    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    // Configuración inicial de los paneles
    panel_t horas = CrearPanel(30, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t minutos = CrearPanel(170, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);

    while (1)
    {
        // Rtina para el control del tiempo (segundos/minutos)
        digitos_t->unidades_segundos = (digitos_t->unidades_segundos + 1) % 10;
        if (digitos_t->unidades_segundos == 0)
        {
            digitos_t->decenas_segundos = (digitos_t->decenas_segundos + 1) % 6;
            if (digitos_t->decenas_segundos == 0)
            {
                digitos_t->unidades_minutos = (digitos_t->unidades_minutos + 1) % 10;
                if (digitos_t->unidades_minutos == 0)
                {
                    digitos_t->decenas_minutos = (digitos_t->decenas_minutos + 1) % 6;
                }
            }
        }

        // Se dibujan los digitos en pantalla, con el punto separador
        DibujarDigito(horas, 0, digitos_t->decenas_minutos);
        DibujarDigito(horas, 1, digitos_t->unidades_minutos);

        ILI9341DrawFilledCircle(160, 90, 5, DIGITO_ENCENDIDO);
        ILI9341DrawFilledCircle(160, 130, 5, DIGITO_ENCENDIDO);

        DibujarDigito(minutos, 0, digitos_t->decenas_segundos);
        DibujarDigito(minutos, 1, digitos_t->unidades_segundos);

        // Pausa para controlar un tiempo de 1 segundo en la rutina del tiempo
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    // Crear la tarea con tamaño de pila de 4096 bytes, otro valor no funcionaba
    xTaskCreate(tft, "Manejo_Display_tft", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Retraso para no saturar la CPU
    }
}