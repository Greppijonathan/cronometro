/* Cronometro en pantalla tft*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ili9341.h"
#include "digitos.h"

#define DIGITO_ANCHO 60
#define DIGITO_ALTO 100
#define DIGITO_ENCENDIDO ILI9341_BLUE2
#define DIGITO_APAGADO 0x3800
#define DIGITO_FONDO ILI9341_BLACK

void tft(void)
{
    // Inicializaci del LCD
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    // Configuración inicial de los paneles
    panel_t horas = CrearPanel(30, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t minutos = CrearPanel(170, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);

    // Dibujar las horas
    while (1)
    {
        DibujarDigito(horas, 0, 1);
        DibujarDigito(horas, 1, 4);

        // Dibujar los puntos separadores
        ILI9341DrawFilledCircle(160, 90, 5, DIGITO_ENCENDIDO);
        ILI9341DrawFilledCircle(160, 130, 5, DIGITO_ENCENDIDO);

        // Dibujar los minutos
        DibujarDigito(minutos, 0, 1);
        DibujarDigito(minutos, 1, 9);

        // Pausa para evitar redibujado constante
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    // Crear la tarea con tamaño de pila suficiente
    xTaskCreate(tft, "Manejo_Display_tft", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Retraso para no saturar la CPU
    }
}