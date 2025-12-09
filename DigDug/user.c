#include <libc.h>
#include <tls.h>
#include <types.h>

/* Declaracions externes */
int gettime(void);

#define SCREEN_COLS 80
#define SCREEN_ROWS 25
#define SCREEN_SIZE (SCREEN_COLS * SCREEN_ROWS)
#define SCREEN_BYTES (SCREEN_SIZE * 2) /* 2 bytes per posicio (color + char) */
#define SCREEN_FD 10                   /* File descriptor per escriure a pantalla */

// Macro per calcular posicio en el buffer de pantalla
#define SCREEN_POS(x, y) ((y) * SCREEN_COLS + (x))

// Macro per crear un caracter amb color
#define SCREEN_CHAR(c, color) ((Word)(((color) << 8) | ((c) & 0xFF)))

// COLORS
// https://www.plantation-productions.com/Webster/www.artofasm.com/DOS/pdf/ch23.pdf
#define COLOR_BLACK 0x00
#define COLOR_BLUE 0x01
#define COLOR_GREEN 0x02
#define COLOR_CYAN 0x03
#define COLOR_RED 0x04
#define COLOR_MAGENTA 0x05
#define COLOR_BROWN 0x06
#define COLOR_LGRAY 0x07
#define COLOR_DGRAY 0x08
#define COLOR_LBLUE 0x08
#define COLOR_LGREEN 0x0A
#define COLOR_LCYAN 0x0B
#define COLOR_LRED 0x0C
#define COLOR_LMAGENTA 0x0D
#define COLOR_YELLOW 0x0E
#define COLOR_WHITE 0x0F

Word screen[SCREEN_SIZE];

#define TEST_FPS_FRAMES 1000
#define TEST_FPS_MIN 30
#define TICKS_PER_SEC 18  // IMPO: AIxo ho he tret d'internet que diu que va a 18Hz (Confirmat pel profe)

// ===========
// === AUX ===
// ===========
void print(const char *msg)
{
  write(1, (char *)msg, strlen((char *)msg));
}

void println(const char *msg)
{
  print(msg);
  write(1, "\n", 1);
}

void print_int(const char *tag, int value)
{
  char buf[16];
  itoa(value, buf);
  print(tag);
  print(": ");
  println(buf);
}

// ==============
// === SCREEN ===
// ==============
void screen_clear(Byte bg_color)
{
  int i;
  Word empty = SCREEN_CHAR(' ', bg_color);

  for (i = 0; i < SCREEN_SIZE; i++)
    screen[i] = empty;
}

void screen_putchar(int x, int y, char c, Byte color)
{
  if (x < 0 || x >= SCREEN_COLS || y < 0 || y >= SCREEN_ROWS)
    return;

  screen[SCREEN_POS(x, y)] = SCREEN_CHAR(c, color);
}

void screen_putstr(int x, int y, const char *str, Byte color)
{
  int i;

  for (i = 0; str[i] != '\0' && (x + i) < SCREEN_COLS; i++)
    screen_putchar(x + i, y, str[i], color);
}

void screen_flush(void)
{
  write(SCREEN_FD, (char *)screen, SCREEN_BYTES);
}

// =============
// === TESTS ===
// =============
void test_slot(int depth)
{
  volatile char page[4096]; // IMPO: Que sigui volatile perque sino compilador optimitza

  page[0] = 'p';
  page[sizeof(page) - 1] = 'a';

  // Fer recursivitat
  if (depth > 0)
  {
    test_slot(depth - 1);
    page[(sizeof(page) - 1) / 2] = 'u';
  }
}

void test_stack(const char *tag)
{
  print(tag);
  println(": stack realloc test");

  test_slot(5); // > 5 --> PAG_FAULT

  print(tag);
  println(": SUCCESS");
}

void test_screen_fps(void)
{
  int start_t;
  int end_t;
  int elapsed;
  int fps;

  println("=== TEST: Screen FPS");

  start_t = gettime();

  for (int i = 0; i < TEST_FPS_FRAMES; i++)
  {
    // 1. Netejar pantalla
    screen_clear(COLOR_BLACK);

    // 2. Ficar contingut
    screen_putstr(30, 2, "=== ZEOS ===", COLOR_LGREEN);
    screen_putstr(32, 4, "DigDug", COLOR_YELLOW);
    screen_putstr(29, 6, "PAU & MAR", COLOR_LCYAN);

    // 3. Actualitzar pantalla
    screen_flush();
  }

  end_t = gettime();

  // Calcular resultat
  elapsed = end_t - start_t;
  
  if (elapsed == 0) 
    elapsed = 1;  // Evitar div 0

  // total_frames / (num_ticks/18) --> (total_frames*18)/num_ticks  
  fps = (TEST_FPS_FRAMES * TICKS_PER_SEC) / elapsed;

  // Resultat
  println("");
  println("=== RESULTATS TEST FPS");
  print_int("Frames renderitzats", TEST_FPS_FRAMES);
  print_int("Ticks totals", elapsed);
  print_int("FPS aproximats", fps);

  if (fps >= TEST_FPS_MIN)
    println("RESULTAT: FPS > 30 --> OK");
  else
    println("RESULTAT: FPS < 30 --> KO");

  println("");
}

// ============
// === MAIN ===
// ============
int __attribute__((__section__(".text.main"))) main(void)
{
  // NOTA: Simulem ja que hi ha un thread nomes per a la pantalla
  int ret = ThreadCreate((void *)test_screen_fps, NULL);
  int i = 0;
  if (ret < 0)
    println("ERROR: Creant el thread");

  while (1)
  {
    i++;
  }

  return 0;
}
