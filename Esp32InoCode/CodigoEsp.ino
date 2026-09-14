#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// ---------------------------------------------------------
// Linhas que serão mostradas no OLED
// ---------------------------------------------------------

char linha1[32] = "CPU 0%";
char linha2[32] = "RAM 0%";
char linha3[32] = "GPU 0%";
char linha4[32] = "FPS ---";


// ---------------------------------------------------------
// Buffer da Serial
// ---------------------------------------------------------

char inputBuffer[64];
byte bufferIdx = 0;

bool dadosAlterados = false;


// ---------------------------------------------------------
// Atualiza o OLED
// ---------------------------------------------------------

void atualizarDisplay()
{
    display.clearDisplay();

    // Tamanho 1 permite textos maiores nas 4 linhas
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println(linha1);

    display.setCursor(0, 16);
    display.println(linha2);

    display.setCursor(0, 32);
    display.println(linha3);

    display.setCursor(0, 48);
    display.println(linha4);

    display.display();
}


// ---------------------------------------------------------
// Processa uma linha recebida pela Serial
// ---------------------------------------------------------

void processarLinha(char* linha)
{
    if (strncmp(linha, "LINHA1:", 7) == 0)
    {
        strncpy(linha1, linha + 7, sizeof(linha1) - 1);
        linha1[sizeof(linha1) - 1] = '\0';

        dadosAlterados = true;
    }

    else if (strncmp(linha, "LINHA2:", 7) == 0)
    {
        strncpy(linha2, linha + 7, sizeof(linha2) - 1);
        linha2[sizeof(linha2) - 1] = '\0';

        dadosAlterados = true;
    }

    else if (strncmp(linha, "LINHA3:", 7) == 0)
    {
        strncpy(linha3, linha + 7, sizeof(linha3) - 1);
        linha3[sizeof(linha3) - 1] = '\0';

        dadosAlterados = true;
    }

    else if (strncmp(linha, "LINHA4:", 7) == 0)
    {
        strncpy(linha4, linha + 7, sizeof(linha4) - 1);
        linha4[sizeof(linha4) - 1] = '\0';

        dadosAlterados = true;
    }
}


// ---------------------------------------------------------
// Setup
// ---------------------------------------------------------

void setup()
{
    Wire.setClock(400000);

    Serial.begin(115200);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        while (true);
    }

    display.clearDisplay();

    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println("PC MONITOR");

    display.setCursor(0, 16);
    display.println("Aguardando...");

    display.display();
}


// ---------------------------------------------------------
// Loop
// ---------------------------------------------------------

void loop()
{
    // Recebe os dados da Serial sem bloquear
    while (Serial.available() > 0)
    {
        char c = Serial.read();

        if (c == '\n' || c == '\r')
        {
            if (bufferIdx > 0)
            {
                inputBuffer[bufferIdx] = '\0';

                processarLinha(inputBuffer);

                bufferIdx = 0;
            }
        }
        else
        {
            if (bufferIdx < sizeof(inputBuffer) - 1)
            {
                inputBuffer[bufferIdx++] = c;
            }
        }
    }


    // Atualiza somente quando algum dado mudou
    if (dadosAlterados)
    {
        atualizarDisplay();

        dadosAlterados = false;
    }
}