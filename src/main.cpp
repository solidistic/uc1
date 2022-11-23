#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <math.h>
#include <time.h>

enum ledStates
{
  YELLOW,
  GREEN,
  RED
};
volatile int ledState = RED;
volatile int roundPassed = 0;
volatile int timeForInput = 0;
volatile int lastmillis = 0;
const int LCD_COLS = 8;
const int LCD_ROWS = 2;

// Tulosta serial monitoriin jännitteen arvo
// analogisesta pinnistä A5
const bool printVoltage = true;

/**
 * LCD kytkennät
 *
 * Arduino | LCD
 * D12     | 4 (RS)
 * D11     | 6 (E)
 * D5      | 11 (DB4)
 * D4      | 12 (DB5)
 * D3      | 13 (DB6)
 * D2      | 14 (DB7)
 *
 * */
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

void print(char str[]);
void handleLedState();
void setTimeForUserInput();
uint16_t ADCAnalogRead(uint8_t ch);

void setup()
{
  Serial.begin(9600);

  /*
   * Kaikki Port D (digitaaliset pinnit 0-7 arvoon "output")
   *   - 1 = output
   *   - 0 = input
   *
   * Port D kaksi ensmimmäistä bittiä kuuluu RX & TX pinneille, pidetään nämä aina arvossa 0.
   */
  DDRD |= 0b11111100;

  // Portin B pinnit
  DDRB |= 0b11111000;

  cli();                // keskeytykset pois päältä säädön ajaksi
  PCICR |= 0b00000001;  // Keskeytykset päälle porttiin B
  PCMSK0 |= 0b00000001; // PB0 (digitaalinen pinni 8) kuuntelemaan keskeytyksiä
  EICRA |= 0b00001100;  // Keskeytykset päälle keskeytys pinneille INT 1 (bitit 3 & 2) ja INT  (bitit 1 & 0) nousevalla reunalla
  sei();                // keskeytykset takaisin päälle uusilla konffeilla

  // LCD setup
  lcd.begin(8, 2);

  // Alustetaan ajastimet
  srand(time(NULL));
  setTimeForUserInput();
}

void loop()
{
  if (printVoltage) {
    int bitVal = ADCAnalogRead(5);
    double percentage = (double)bitVal / 1023.0;
    double voltage = (double)percentage * 5.0;

    Serial.println("----------");
    Serial.println(bitVal);
    Serial.println(analogRead(A5));
    Serial.println(percentage);
    Serial.println(voltage);

    char buffer[16];
    dtostrf(voltage, 4, 2, buffer);
    print(buffer);
    Serial.println(buffer);

    delay(200);
  } else {
    if (ledState == RED)
    {
      if (timeForInput == 0)
      {
        ledState = YELLOW;
      }
      else
      {
        print("Wait for it...");
        timeForInput--;
      }
    }
    else if (ledState == YELLOW)
    {
      print("GO GO GO!");
      roundPassed++;

      // too long, throw to result screen
      if (roundPassed >= 200)
      {
        ledState = GREEN;
      }
    }
    else
    {
      char buffer[16];
      sprintf(buffer, "%d rounds", roundPassed);
      print(buffer);
    }

    handleLedState();
  
    delay(20);
  }
}

uint16_t ADCAnalogRead(uint8_t ch)
{
  // Alimmat bitit arvoon 0
  ADMUX = (ADMUX & 0xF8) | ch;

  // Aloitetaan luku, asetetaan ADSC bitti 1
  ADCSRA |= (1 << ADSC);

  // Odotetaan, kunnes ADCS on palannut arvoon 0
  while (ADCSRA & (1 << ADSC));

  // Palautetaan saatu arvo
  // Vaihtoehtoisesti: ADCL | (ADCH << 8);
  return (ADC);
}

/**
 * Kirjoita teksti LCD näytölle
 *
 * Alla LCD näytön kursorin paikat, kun kirjoitetaan näytölle
 *
 *      (0, 0)              (0, 1)
 * | X X X X X X X X | X X X X X X X X |
 */
void print(char str[])
{
  lcd.clear();

  if (strlen(str) > 16)
  {
    lcd.print("Error");
    return;
  }

  double rounds = ceil((double)(strlen(str) / (double)LCD_COLS));

  for (unsigned int j = 0; j < rounds; j++)
  {
    unsigned int ri = j * LCD_COLS;
    char temp[LCD_COLS];
    lcd.setCursor(0, j);

    for (unsigned int i = 0; i < LCD_COLS; i++)
    {
      if (strlen(str) > (ri + i))
      {
        temp[i] = str[ri + i];
      }
      else
      {
        temp[i] = '\0';
      }
    }

    lcd.print(temp);
  }
}

/**
 * Asetetaan arvot pinneille
 */
void handleLedState()
{
  // Pinnit 6 ja 7 HIGH -- Keltainen
  if (ledState == YELLOW)
    PORTD = 0b11000000;

  // Pinni 6 LOW ja 7 HIGH -- Vihreä
  if (ledState == GREEN)
    PORTD = 0b10000000;

  // Pinni 6 HIGH ja 7 LOW -- Punainen
  if (ledState == RED)
    PORTD = 0b01000000;
}

/**
 * Nollaa ajastimet, ja aloittaa pelin alusta
 */
void setTimeForUserInput()
{
  timeForInput = (rand() % 300) + 60;
  roundPassed = 0;
}

ISR(PCINT0_vect)
{
  int diff = millis() - lastmillis;

  // Ajoittain diff on suuri negatiivinen numero, tällöin oletetaan sen olevan 0
  // Korjataan painikkeen toimivuutta lisäämällä hieman viivettä siihen, milloin painallukset
  // hyväksytään. Luultavasti liittyvää englannin kieliseen termiin "debouncing"
  if ((diff < 0 ? 0 : diff) < 500)
    return;

  lastmillis = millis();

  // odotetaan ajastimen menevän nollaan, jonka jälkeen hyväksytään
  // käyttäjän painallus, josta lasketaan aika
  if (ledState == RED && timeForInput == 0)
    ledState = YELLOW;

  // Vaihdetaan tulosnäkymään
  else if (ledState == YELLOW)
    ledState = GREEN;

  // Aloitetaan alusta, nollataan arvot ja arvotaan uusi arvo odotusajalle
  else if (ledState == GREEN)
  {
    setTimeForUserInput();
    ledState = RED;
  }
}