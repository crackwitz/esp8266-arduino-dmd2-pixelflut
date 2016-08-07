/*
  Game of Life display

  Simulates Conway's Game of Life
  https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life
 */

#include <util/atomic.h>
#include <SPI.h>
#include <DMD2.h>
#include <fonts/SystemFont5x7.h>
#include <fonts/Arial_Black_16.h>

#define USART_BAUDRATE 115200
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 8UL))) - 1) // U2X:8
#define RECVTIMEOUT 500

uint8_t rxbuf[255];
uint8_t rxlen = 0;
uint8_t newdata = 0; // indicates processing needed. incremented by RX ISR, decremented by processing.
uint32_t lastrecv = 0;

uint8_t *const cmdbuf = rxbuf + 1;
uint8_t cmdlen = 0;

bool do_gameoflife = true;
bool autonoise = true;
uint8_t noise_amount = 0;

#define NOISEHIST 8
uint16_t noisehist[NOISEHIST] = {0};
#define NOISEINC 2
#define NOISEDEC 1

// How many displays do you have?
const int WIDTH = 2;
const int HEIGHT = 1;

SPIDMD dmd(WIDTH,HEIGHT);

void init_cells(uint8_t value) {
  for(int x = 0; x < dmd.width; x++) {
    for(int y = 0; y < dmd.height; y++) {
      dmd.setPixel(x, y, value ? GRAPHICS_ON : GRAPHICS_OFF);
    }
  }
}

void iterate_gameoflife()
{
  uint16_t curcount = 0;
  
  // Store the current generation by copying the current DMD frame contents
  DMDFrame current_generation(dmd);

  // noise
  for (uint8_t k = noise_amount; k > 0; k -= 1)
    current_generation.setPixel(random(dmd.width), random(dmd.height), GRAPHICS_ON);

  // Update next generation of every pixel
  bool change = false;
  for(int x = 0; x < dmd.width; x++) {
    for(int y = 0; y < dmd.height; y++) {
      bool state = current_generation.getPixel(x,y);
      int live_neighbours = 0;

      // Count how many live neighbours we have in the current generation
      for(int nx = x - 1; nx < x + 2; nx++) {
        for(int ny = y - 1; ny < y + 2; ny++) {
          if(nx == x && ny == y)
            continue;
          if(current_generation.getPixel(nx,ny))
            live_neighbours++;
        }
      }

      // Update pixel count for the next generation
      if(state && (live_neighbours < 2 || live_neighbours > 3)) {
        state = false;
        change = true;
      }
      else if(!state && (live_neighbours == 3)) {
        state = true;
        change = true;
      }
      curcount += state;
      dmd.setPixel(x,y,state ? GRAPHICS_ON : GRAPHICS_OFF);
    }
  }

  if (autonoise)
  {
    bool match = false;
    for (uint8_t i = 0; i < NOISEHIST; i += 1)
      if (noisehist[i] == curcount)
        match = true;

    if (match)
    {
      if (noise_amount <= 0xff - NOISEINC)
      {
        noise_amount += NOISEINC;
      }
      else
      {
        noise_amount = 0xff;
      }
    }
    else
    {
      if (noise_amount >= NOISEDEC)
      {
        noise_amount -= NOISEDEC;
      }
      else
      {
        noise_amount = 0;
      }
    }
  }

  for (uint8_t i = 0; i < NOISEHIST-1; i += 1)
    noisehist[i] = noisehist[i+1];
  noisehist[NOISEHIST-1] = curcount;
}

///////////////////////////////////////////////////

void command_dutycycle()
{
  if (cmdlen != 2) return;
  dmd.setBrightness(cmdbuf[1]);
}

void command_pixelflut()
{
  if (cmdlen != 4) return;
  const uint8_t x = cmdbuf[1];
  const uint8_t y = cmdbuf[2];
  const uint8_t v = cmdbuf[3];
  dmd.setPixel(
    x,
    y,
    v ? GRAPHICS_ON : GRAPHICS_OFF);
}

void command_text()
{
  // 0: big, 1/2: small and row
  // then length, text
  
  if (cmdlen < 2) return; // not even font given?

  uint8_t fontsize = cmdbuf[1];

  if (cmdbuf[cmdlen-1] != '\0') // strings must be null-terminated
    return;

  char *str = (char*)cmdbuf + 2;

  uint8_t y = 0;
  switch(fontsize)
  {
    default:
    case 0: y = 0; dmd.selectFont(Arial_Black_16); break;
    case 1: y = 0; dmd.selectFont(SystemFont5x7); break;
    case 2: y = 8; dmd.selectFont(SystemFont5x7); break;
  }

  dmd.drawString(0, y, str);

  do_gameoflife = false;
}

void command_bitmap()
{
  if (cmdlen != 1 + dmd.width*dmd.height) return;

  do_gameoflife = 0;
  
  uint8_t *p = cmdbuf+1; // skip packet type

  for (uint8_t y = 0; y < dmd.height; y += 1)
  for (uint8_t x = 0; x < dmd.width; x += 8)
    dmd.setByte(x, y, *p++);
}

void command_solid()
{
  if (cmdlen != 2) return;

  uint8_t value = cmdbuf[1];

  if (value <= 1)
  {
    init_cells(value);
  }
  else if (value == 0xff)
  {
    for (uint8_t y = 0; y < dmd.height; y += 1)
    for (uint8_t x = 0; x < dmd.width; x += 8)
      dmd.setByte(x, y, ~dmd.getByte(x, y));
  }
  else
  {
    return; // nothing matched
  }

  // something matched
  do_gameoflife = 0;
  noise_amount = 0;
}

void command_gameoflife()
{
  if (cmdlen != 2) return;
  uint8_t value = cmdbuf[1];

  if (value <= 1)
    do_gameoflife = value;

  if (!do_gameoflife)
    noise_amount = 0;
}

void command_noise()
{
  if (cmdlen != 2) return;
  noise_amount = cmdbuf[1];
}

void command_autonoise()
{
  if (cmdlen != 2) return;
  autonoise = cmdbuf[1];
}

void dispatch_command()
{
  cmdlen = rxbuf[0];
  
  switch(cmdbuf[0])
  {
    case 'D': command_dutycycle(); return;
    case 'S': command_solid(); return;
    case 'P': command_pixelflut(); return;
    case 'T': command_text(); return;
    case 'B': command_bitmap(); return;
    case '!':
    case 'G': command_gameoflife(); return;
    case 'N': command_noise(); return;
    case 'A': command_autonoise(); return;
    default: return;
  }
}

void parse_command()
{
  ATOMIC_BLOCK(ATOMIC_FORCEON)
  {
    if (newdata)
      newdata--;
    else
      return;
  }

  if (rxlen == 0) // nothing there
    return;

  uint8_t const payloadlen = rxbuf[0];

  if (rxlen-1 < payloadlen)
    return;

  dispatch_command();

  ATOMIC_BLOCK(ATOMIC_FORCEON)
  {
    for (uint16_t u = 0, v = 1 + payloadlen; v < rxlen; u += 1, v += 1)
      rxbuf[u] = rxbuf[v];
  
    rxlen -= 1;
    rxlen -= payloadlen;
  }
}

ISR(USART_RX_vect)
{
  uint8_t data = UDR0;

  if (rxlen < sizeof(rxbuf)-1)
  {
    rxbuf[rxlen++] = data;
    lastrecv = millis();
    if (newdata < 0xff)
      newdata += 1;
  }
}

// the setup routine runs once when you press reset:
void setup() {
  dmd.setBrightness(5);
  dmd.begin();

  randomSeed(analogRead(0));
  init_cells(0);

  UBRR0L = (uint8_t)(BAUD_PRESCALE & 0xff);
  UBRR0H = (uint8_t)(BAUD_PRESCALE >> 8);
  UCSR0A = (1 << U2X0);
  UCSR0C = (0b011 << UCSZ00);
  UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);
}

void loop() {
  if (newdata)
  {
    parse_command();
  }
  else if (rxlen > 0 && (millis() - lastrecv > RECVTIMEOUT))
  {
    UDR0 = 0xff;
    rxlen = 0;
    lastrecv = 0;
  }

  if (do_gameoflife)
    iterate_gameoflife();
}
