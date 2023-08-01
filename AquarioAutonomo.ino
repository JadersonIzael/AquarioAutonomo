#include <NTPClient.h>
#include <ESP32_Servo.h>
#include <analogWrite.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// termostato
#define pinoAquecedor 21
#define pinoVentilador 16

// luminaria
#define pinoIluminacao1 23
#define pinoIluminacao2 19

// motor submerso e dimmer
#define pinoMotorSubmerso 22
#define pinoMotorSubmersoZeroCross 5
#define pinoZeroCross 5
#define pinoDimmer 22
#define iMin 8
#define iMax 80
#define periodo 8333

#define pinoAlimentacao 26

// Temperatura
OneWire oneWire(17);
DallasTemperature DS18B20(&oneWire);
Servo servo;

// dimmer
String hora;
char horaAtual[2];
// char* horaDesejada;

int vazao = 10;
volatile int intensidadeVazao = 100;
int botao1 = 0;
int botao2 = 0;
int botao3 = 0;
int botao4 = 0;

float temperaturaMedida;
float temperatura = 25.00;
float margemErroTemperatura = 0.25;

int iluminacao = 0;
int inicioIluminacao = 9;
int terminoIluminacao = 18;
String tipoIluminacao = "MANUAL";
int botaoIluminacao1 = 0;
int botaoIluminacao2 = 0;
int botaoIluminacao3 = 0;
int botaoIluminacao4 = 0;

int horaIluminacao = 0;
int minutoIluminacao = 0;
int duracaoIluminacao = 5;

int minServo = 500;
int maxServo = 2400;

int horaAlimentacao = 0;
int numeroDeAlimentacao = 1;
int alimentarAutomaticamente = 1;
int alimentado = 1;

const char *ssid = "2G_FRED";
const char *password = "cabritavoadora";

IPAddress ip(192, 168, 0, 20);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiUDP udp;
NTPClient ntp(udp, "gps.ntp.br", -10800, 60000);
WiFiServer server(80);

void setup()
{
    Serial.begin(115200);
    DS18B20.begin();

    pinMode(pinoDimmer, OUTPUT);
    pinMode(pinoZeroCross, INPUT);
    attachInterrupt(digitalPinToInterrupt(pinoZeroCross), sinalZC, RISING);

    pinMode(pinoMotorSubmerso, OUTPUT);
    pinMode(pinoMotorSubmersoZeroCross, INPUT);
    pinMode(pinoIluminacao1, OUTPUT);
    pinMode(pinoIluminacao2, OUTPUT);
    pinMode(pinoAquecedor, OUTPUT);
    pinMode(pinoVentilador, OUTPUT);

    digitalWrite(pinoIluminacao1, HIGH);
    digitalWrite(pinoIluminacao2, HIGH);
    digitalWrite(pinoAquecedor, HIGH);
    digitalWrite(pinoVentilador, HIGH);

    servo.attach(pinoAlimentacao, minServo, maxServo);

    WiFi.config(ip, gateway, subnet);
    WiFi.begin(ssid, password);
    ntp.begin();

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    server.begin();
}

void loop()
{

    DS18B20.requestTemperatures();
    temperaturaMedida = DS18B20.getTempCByIndex(0);

    // atualiza o horario
    ntp.forceUpdate();
    hora = ntp.getFormattedTime();
    horaAtual[0] = hora[0];
    horaAtual[1] = hora[1];

    if (horaDesejada[0] == horaAtual[0] && horaDesejada[1] == horaAtual[1] && alimentado == 0)
    {
        servo.write(100);
        for (int pos = 100; pos >= 0; pos -= 1)
        {
            servo.write(pos);
            delay(20);
        }
        alimentado = 1;
    }

    // ----------------termostato---------------------
    // pinoAquecedor == LOW -> aquecedor ativo, relé conectado a saida normalmente aberta
    // pinoVentilador == HIGH -> ventilador ativo, relé conectado a saida normalmente fechada
    if (temperaturaMedida <= temperatura)
    {
        digitalWrite(pinoAquecedor, LOW);
        digitalWrite(pinoVentilador, LOW);
    }

    if (temperaturaMedida >= temperatura + margemErroTemperatura)
    {
        digitalWrite(pinoAquecedor, HIGH);
        digitalWrite(pinoVentilador, HIGH);
    }

    if (temperaturaMedida == temperatura + margemErroTemperatura)
    {
        digitalWrite(pinoAquecedor, HIGH);
        digitalWrite(pinoVentilador, LOW);
    }

    //---------------------------------------
    // LED -> LOW indica que o LED em questao esta ativo
    if (tipoIluminacao == "MANUAL")
    {
        if (iluminacao == 0)
        {
            digitalWrite(pinoIluminacao2, HIGH);
            digitalWrite(pinoIluminacao1, HIGH);
        }

        if (iluminacao == 1)
        {
            digitalWrite(pinoIluminacao2, LOW);
            digitalWrite(pinoIluminacao1, HIGH);
        }

        if (iluminacao == 2)
        {
            digitalWrite(pinoIluminacao2, HIGH);
            digitalWrite(pinoIluminacao1, LOW);
        }

        if (iluminacao == 3)
        {
            digitalWrite(pinoIluminacao2, LOW);
            digitalWrite(pinoIluminacao1, LOW);
        }
    }
    else
    {
        if (inicioIluminacao == horaAtual || inicioIluminacao > horaAtual && horaAtual < terminoIluminacao)
        {
            if (iluminacao == 0)
            {
                digitalWrite(pinoIluminacao2, HIGH);
                digitalWrite(pinoIluminacao1, HIGH);
            }

            if (iluminacao == 1)
            {
                digitalWrite(pinoIluminacao2, LOW);
                digitalWrite(pinoIluminacao1, HIGH);
            }

            if (iluminacao == 2)
            {
                digitalWrite(pinoIluminacao2, HIGH);
                digitalWrite(pinoIluminacao1, LOW);
            }

            if (iluminacao == 3)
            {
                digitalWrite(pinoIluminacao2, LOW);
                digitalWrite(pinoIluminacao1, LOW);
            }
        }
        else if (horaAtual == terminoIluminacao || inicioIluminacao < horaAtual && horaAtual > terminoIluminacao)
        {
            digitalWrite(pinoIluminacao2, HIGH);
            digitalWrite(pinoIluminacao1, HIGH);
        }
    }

    //------------------------------------------------
    // Escuta o client
    WiFiClient client = server.available();

    if (client)
    {
        String currentLine = "";
        while (client.connected())
        {
            if (client.available())
            {
                char c = client.read();
                Serial.write(c);
                if (c == '\n')
                {
                    if (currentLine.length() == 0)
                    {
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println();

                        // the content of the HTTP response follows the header:
                        client.print("<head>");
                        client.print("<meta charset=\"utf-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no\"/>");
                        client.print("</head>");

                        // inicio do body
                        client.print("<body>");
                        client.print("<div style = 'background:#222222'>");
                        client.print("<h1  style='color:#fff9ff; text-align:center; font-size: 4vh;'>Aquário FHO</h1>");

                        // Vazão
                        client.print("<div style = 'color:#fff9ff; text-align:center; font-size:4vh'>");
                        client.print("Vazão");
                        client.print("<br>");
                        client.print("<div style = 'color:#2b364a; text-align:center; font-size:4vh'>");
                        if (botao1 == 1)
                        {
                            client.print("<a href='/A1'><button style = 'background-color: #d8f171; border-radius: 10%; border-color: #2b364a;  color: #000000;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 1 </button></a>  ");
                        }
                        else
                        {
                            client.print("<a href='/A1'><button style = 'background-color: #d4f7dc; border-radius: 10%; border-color: #2b364a;  color: #2b364a;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 1 </button></a>  ");
                        }
                        if (botao2 == 1)
                        {
                            client.print("<a href='/A2'><button style = 'background-color: #d8f171; border-radius: 10%; border-color: #2b364a;  color: #000000;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 2 </button></a>  ");
                        }
                        else
                        {
                            client.print("<a href='/A2'><button style = 'background-color: #d4f7dc; border-radius: 10%; border-color: #2b364a;  color: #2b364a;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 2 </button></a>  ");
                        }
                        if (botao3 == 1)
                        {
                            client.print("<a href='/A3'><button style = 'background-color: #d8f171; border-radius: 10%; border-color: #2b364a;  color: #000000;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 3 </button></a>  ");
                        }
                        else
                        {
                            client.print("<a href='/A3'><button style = 'background-color: #d4f7dc; border-radius: 10%; border-color: #2b364a;  color: #2b364a;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 3 </button></a>  ");
                        }
                        if (botao4 == 1)
                        {
                            client.print("<a href='/A4'><button style = 'background-color: #d8f171; border-radius: 10%; border-color: #2b364a;  color: #000000;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 4 </button></a>  ");
                        }
                        else
                        {
                            client.print("<a href='/A4'><button style = 'background-color: #d4f7dc; border-radius: 10%; border-color: #2b364a;  color: #2b364a;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 4 </button></a>  ");
                        }
                        if (botao5 == 1)
                        {
                            client.print("<a href='/A5'><button style = 'background-color: #d8f171; border-radius: 10%; border-color: #2b364a;  color: #000000;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 5 </button></a>  ");
                        }
                        else
                        {
                            client.print("<a href='/A5'><button style = 'background-color: #d4f7dc; border-radius: 10%; border-color: #2b364a;  color: #2b364a;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 5 </button></a>  ");
                        }
                        client.print("</div>");
                        client.print("<br>");

                        // Temperatura
                        client.print("<div style = 'color:#fff9ff; text-align:center; font-size:4vh'>");
                        client.print("Temperatura [ °C ]");
                        client.print("<br>");
                        client.print("<a href=\"/C\"><button style = 'background-color: #222222; border: none;  color: #fff9ff;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'>-</button></a>");
                        client.print(int(temperatura));
                        client.print("<a href=\"/D\"><button style = 'background-color: #222222; border: none;  color: #fff9ff;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'>+</button></a> <br>");
                        client.print("<br>");
                        client.print("</div>");

                        // Iluminação
                        client.print("<div style = 'color:#fff9ff; text-align:center; font-size:4vh'>");
                        client.print("Modo de iluminação");
                        client.print("<br>");
                        client.print("<br>");
                        client.print(" <a href=\"/E\"><button style = 'background-color: #222222;  border-radius: 10%; border-color: #fff9ff;  color: #fff9ff;  padding: 8px 8px; text-decoration: none; display: inline-block; font-size: 4vh;'> ");
                        client.print(tipoIluminacao);
                        client.print(" </button></a>");
                        client.print("<br>");
                        client.print("<br>");
                        client.print("Leds ativos");
                        client.print("<br>");
                        if (botaoIluminacao1 == 1)
                        {
                            client.print("<a href='/IM1'><button style = 'background-color: #d8f171; border-radius: 10%; border-color: #2b364a;  color: #000000;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 0 </button></a>  ");
                        }
                        else
                        {
                            client.print("<a href='/IM1'><button style = 'background-color: #d4f7dc; border-radius: 10%; border-color: #2b364a;  color: #2b364a;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 0 </button></a>  ");
                        }
                        if (botaoIluminacao2 == 1)
                        {
                            client.print("<a href='/IM2'><button style = 'background-color: #d8f171; border-radius: 10%; border-color: #2b364a;  color: #000000;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 1 </button></a>  ");
                        }
                        else
                        {
                            client.print("<a href='/IM2'><button style = 'background-color: #d4f7dc; border-radius: 10%; border-color: #2b364a;  color: #2b364a;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 1 </button></a>  ");
                        }
                        if (botaoIluminacao3 == 1)
                        {
                            client.print("<a href='/IM3'><button style = 'background-color: #d8f171; border-radius: 10%; border-color: #2b364a;  color: #000000;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 2 </button></a>  ");
                        }
                        else
                        {
                            client.print("<a href='/IM3'><button style = 'background-color: #d4f7dc; border-radius: 10%; border-color: #2b364a;  color: #2b364a;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 2 </button></a>  ");
                        }
                        if (botaoIluminacao4 == 1)
                        {
                            client.print("<a href='/IM4'><button style = 'background-color: #d8f171; border-radius: 10%; border-color: #2b364a;  color: #000000;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 3 </button></a>  ");
                        }
                        else
                        {
                            client.print("<a href='/IM4'><button style = 'background-color: #d4f7dc; border-radius: 10%; border-color: #2b364a;  color: #2b364a;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'> 3 </button></a>  ");
                        }
                        client.print("</div>");
                        client.print("<br>");

                        if (tipoIluminacao == "AUTOMÁTICO")
                        {
                            client.print("<div style = 'color:#fff9ff; text-align:center; font-size:4vh'>");
                            client.print("Horário de início");
                            client.print("<br>");
                            client.print(" <a href=\"/F1\"><button style = 'background-color: #222222; border: none;  color: #fff9ff;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'>-</button></a> ");
                            client.print(String(inicioIluminacao));
                            client.print(" <a href=\"/G1\"><button style = 'background-color: #222222; border: none;  color: #fff9ff;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'>+</button></a>");

                            client.print("<br>");
                            client.print("Horário de término");
                            client.print("<br>");
                            client.print(" <a href=\"/F2\"><button style = 'background-color: #222222; border: none;  color: #fff9ff;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'>-</button></a> ");
                            client.print(String(terminoIluminacao));
                            client.print(" <a href=\"/G2\"><button style = 'background-color: #222222; border: none;  color: #fff9ff;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'>+</button></a>");
                            client.print("</div>");
                        }

                        client.print("<br>");

                        // Alimentação
                        client.print("<div style = 'color:#fff9ff; text-align:center; font-size:4vh'>");
                        client.print(" <a href=\"/H\"><button  style = 'background-color: #222222;  border-radius: 10%; border-color: #fff9ff;  color: #fff9ff;  padding: 8px 8px; text-decoration: none; display: inline-block; font-size: 4vh;'>Alimentar Agora</button></a>");
                        client.print("<br>");
                        client.print("<br>");
                        client.print(" <a href=\"/I\"><button  style = 'background-color: #222222;  border-radius: 10%; border-color: #fff9ff;  color: #fff9ff;  padding: 8px 8px; text-decoration: none; display: inline-block; font-size: 4vh;'>Programar alimentação</button></a>");
                        client.print("<br>");
                        client.print("<br>");
                        client.print("</div>");

                        if (alimentarAutomaticamente == -1)
                        {
                            client.print("<div style = 'color:#fff9ff; text-align:center; font-size:4vh'>");
                            client.print("Horário de alimentação");
                            client.print("<br>");
                            client.print(" <a href=\"/J\"><button style = 'background-color: #222222; border: none;  color: #fff9ff;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'>-</button></a> ");
                            client.print(int(horaAlimentacao));
                            client.print(" <a href=\"/K\"><button style = 'background-color: #222222; border: none;  color: #fff9ff;  padding: 10px 10px; text-decoration: none; display: inline-block; font-size: 4vh;'>+</button></a>");
                            client.print("</div>");
                            client.print("<br>");
                        }

                        client.print("</div>");
                        client.print("</body>");
                        
                        client.println();
                        
                        break;
                    }

                    else
                    { 
                        currentLine = "";
                    }
                }

                else if (c != '\r')
                {                     
                    currentLine += c;
                }

                //--------------------------vazão-----------------------------------------

                if (currentLine.endsWith("GET /A1"))
                {
                    if (botao1 == 0)
                    {
                        botao1 = 1;
                        intensidadeVazao = 0;
                    }
                    else
                    {
                        botao1 = 0;
                    }
                }
                else if (currentLine.endsWith("GET /A2"))
                {
                    if (botao2 == 0)
                    {
                        botao2 = 1;
                        intensidadeVazao = 26;
                    }
                    else
                    {
                        botao2 = 0;
                    }
                }
                else if (currentLine.endsWith("GET /A3"))
                {
                    if (botao3 == 0)
                    {
                        botao3 = 1;
                        intensidadeVazao = 44;
                    }
                    else
                    {
                        botao3 = 0;
                    }
                }
                else if (currentLine.endsWith("GET /A4"))
                {
                    if (botao4 == 0)
                    {
                        botao4 = 1;
                        intensidadeVazao = 62;
                    }
                    else
                    {
                        botao4 = 0;
                    }
                }
                else if (currentLine.endsWith("GET /A5"))
                {
                    if (botao4 == 0)
                    {
                        botao4 = 1;
                    }
                    else
                    {
                        botao4 = 0;
                        intensidadeVazao = 80;
                    }
                }

                //-----------------------temperatura----------------------------------------------

                if (currentLine.endsWith("GET /C"))
                {
                    if (temperatura > 15)
                        temperatura -= 1;
                }
                if (currentLine.endsWith("GET /D"))
                {
                    if (temperatura < 30)
                        temperatura += 1;
                }
                //------------------------iluminação----------------------------------------------
                if (currentLine.endsWith("GET /E"))
                {
                    if (tipoIluminacao == "MANUAL")
                    {
                        tipoIluminacao = "AUTOMÁTICO";
                    }
                    else if (tipoIluminacao == "AUTOMÁTICO")
                    {
                        tipoIluminacao = "MANUAL";
                    }
                }
                if (currentLine.endsWith("GET /IM1"))
                {
                    iluminacao = 0;
                }
                if (currentLine.endsWith("GET /IM2"))
                {
                    iluminacao = 1;
                }
                if (currentLine.endsWith("GET /IM3"))
                {
                    iluminacao = 2;
                }
                if (currentLine.endsWith("GET /IM4"))
                {
                    iluminacao = 3;
                }

                if (currentLine.endsWith("GET /F1"))
                {
                    if (inicioIluminacao > 0)
                        inicioIluminacao -= 1;
                    else if (inicioIluminacao == 0)
                        inicioIluminacao = terminoIluminacao - 1;
                }
                if (currentLine.endsWith("GET /G1"))
                {
                    if (inicioIluminacao < 23 && terminoIluminacao > (inicioIluminacao + 1))
                        inicioIluminacao += 1;
                    else if (inicioIluminacao == terminoIluminacao)
                        inicioIluminacao = 0;
                }

                if (currentLine.endsWith("GET /F2"))
                {
                    if (terminoIluminacao > (inicioIluminacao + 1))
                        terminoIluminacao -= 1;
                }
                if (currentLine.endsWith("GET /G2"))
                {
                    if (terminoIluminacao < 23)
                        terminoIluminacao += 1;
                    else if (terminoIluminacao == 23)
                        terminoIluminacao = (inicioIluminacao + 1);
                }

                //--------------------Controle do servo motor---------------
                if (currentLine.endsWith("GET /H"))
                {
                    // fazer o motor da uma volta
                    servo.write(100);
                    for (int pos = 100; pos >= 0; pos -= 1)
                    { // sweep from 180 degrees to 0 degrees
                        servo.write(pos);
                        delay(20);
                    }
                }
                //---------------------alimentação automática---------------
                if (currentLine.endsWith("GET /I"))
                {
                    alimentarAutomaticamente *= -1;
                }

                if (currentLine.endsWith("GET /J"))
                {
                    if (horaAlimentacao > 0)
                        horaAlimentacao -= 1;
                    alimentado = 0;
                }
                if (currentLine.endsWith("GET /K"))
                {
                    if (horaAlimentacao < 23)
                        horaAlimentacao += 1;
                    alimentado = 0;
                }
            }
        }
    }
    
    client.stop();
    Serial.println("Client Disconnected.");
}

int converteHora(char hora){
  int retorno;
  if(hora == '0') retorno = 0;
  else if(hora == '1') retorno = 1;
  else if(hora == '2') retorno = 2;
  else if(hora == '3') retorno = 3;
  else if(hora == '4') retorno = 4;
  else if(hora == '5') retorno = 5;
  else if(hora == '6') retorno = 6;
  else if(hora == '7') retorno = 7;
  else if(hora == '8') retorno = 8;
  else if(hora == '9') retorno = 9;
  return retorno;
}
void sinalZC() {
  if ( intensidade < iMin) return;
  if ( intensidade > iMax) intensidade = iMax;
  int delayInt = periodo - (intensidade * (periodo / 100) );
  digitalWrite(pinoDimmer, LOW);
  delayMicroseconds(delayInt);
  digitalWrite(pinoDimmer, HIGH);
}