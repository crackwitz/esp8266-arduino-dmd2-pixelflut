/*
  Game of Life display

  Simulates Conway's Game of Life
  https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life
 */

#include <SPI.h>
#include <DMD2.h>
#include <fonts/SystemFont5x7.h>
#include <fonts/Arial_Black_16.h>

#define SERIAL_TIMEOUT 100

uint8_t commandbuf[256];
uint8_t commandlen = 0;

bool do_gameoflife = true;
bool autonoise = true;
uint8_t noise_amount = 0;

#define NOISEHIST 8
uint16_t noisehist[NOISEHIST] = {0};

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

// the setup routine runs once when you press reset:
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(SERIAL_TIMEOUT);
  dmd.setBrightness(5);
  dmd.begin();

  randomSeed(analogRead(0));
  init_cells(0);
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
    for (int8_t i = NOISEHIST-1; i >= 0; i -= 1)
      if (noisehist[i] == curcount)
        match = true;

    if (match)
    {
      if (noise_amount < 0xff)
      {
        noise_amount += 1;
        //Serial.print("Noise ");Serial.println(noise_amount);
      }
    }
    else
    {
      if (noise_amount > 0x00)
      {
        noise_amount -= 1;
        //Serial.print("Noise ");Serial.println(noise_amount);
      }
    }
  }

  for (int8_t i = NOISEHIST-1; i >= 1; i -= 1)
    noisehist[i] = noisehist[i-1];
  noisehist[0] = curcount;
}

/*
int read_with_timeout(uint16_t timeout)
{
  uint16_t now = millis();
  int rv;

  while (!timeout || (millis() - timeout < now))
  {
    rv = Serial.read();
    if (rv != -1) break;
  }

  return rv;
}
*/

bool process_uart()
{
  static uint8_t prev_available = 0;
  const uint8_t cur_available = Serial.available();

  if (!cur_available)
    return false;

  if (cur_available > prev_available)
  {
    prev_available = cur_available;
    switch (Serial.peek())
    {
      default:
      {
        Serial.read();
        prev_available = 0;
        break;
      }
    }
  }

  return true;
}


///////////////////////////////////////////////////

void command_dutycycle()
{
  if (commandlen != 2) return;
  dmd.setBrightness(commandbuf[1]);
}

void command_pixelflut()
{
  if (commandlen != 4) return;
  const uint8_t x = commandbuf[1];
  const uint8_t y = commandbuf[2];
  const uint8_t v = commandbuf[3];
  dmd.setPixel(
    x,
    y,
    v ? GRAPHICS_ON : GRAPHICS_OFF);
}

void command_text()
{
  // 0: big, 1/2: small and row
  // then length, text
  
  if (commandlen < 2) return;

  uint8_t fontsize = commandbuf[1];

  char *str = (char*)commandbuf + 2;

  commandbuf[commandlen] = '\0';

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
  if (commandlen != 1+128) return;

  do_gameoflife = 0;
  
  uint8_t *p = commandbuf+1;

  for (uint8_t y = 0; y < dmd.height; y += 1)
  for (uint8_t x = 0; x < dmd.width; x += 8)
    dmd.setByte(x, y, *p++);
}

void command_solid()
{
  if (commandlen != 2) return;

  uint8_t value = commandbuf[1];

  if (value <= 1)
  {
    init_cells(value);
    do_gameoflife = 0;
    noise_amount = 0;
  }
}

void command_gameoflife()
{
  if (commandlen != 2) return;

  uint8_t value = commandbuf[1];

  if (value <= 1)
  {
    do_gameoflife = value;
  }
  else if (value >= 0xfe) // TODO: remove, obsolete
  {
    init_cells(0xff - value);
    do_gameoflife = 0;
  }

  if (!do_gameoflife)
    noise_amount = 0;
}

void command_noise()
{
  if (commandlen != 2) return;
  noise_amount = commandbuf[1];
}

void command_autonoise()
{
  if (commandlen != 2) return;
  autonoise = commandbuf[1];
}

void dispatch_command()
{
  switch(commandbuf[0])
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

void read_commands()
{
  while (Serial.available())
  {
    commandlen = Serial.read();
    uint8_t received = Serial.readBytes(commandbuf, commandlen);

    if (received != commandlen)
      continue;

    dispatch_command();
  }
}

// the loop routine runs over and over again forever:
void loop() {
  read_commands();

  if (do_gameoflife)
    iterate_gameoflife();
}
