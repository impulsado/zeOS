#include <libc.h>
#include <tls.h>
#include <types.h>

/* ======================================================================== */
/* DEFINICIONS I MACROS                                                     */
/* ======================================================================== */

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

// TECLES
// https://www.rapidtables.com/code/text/ascii-table.html
#define KEY_ESC   0x01
#define KEY_S     0x1F
#define KEY_UP    0x48
#define KEY_LEFT  0x4B
#define KEY_RIGHT 0x4D
#define KEY_DOWN  0x50

// CASELLES
#define TILE_EMPTY ' '
#define TILE_DIRT  176  // Terra
#define CHAR_PLAYER 0x01 
#define CHAR_ENEMY  0x0F

// ESTATS
#define STATE_LOADING 0
#define STATE_PLAYING 1
#define STATE_GAMEOVER 2

/* ========================================================================= */
/* VARIABLES GLOBALS                                                         */
/* ========================================================================= */

// MAPA
// NOTA: 1 --> Terra ; 0 --> Buit
char map[SCREEN_COLS][SCREEN_ROWS];

// ESTATS
int game_state = STATE_LOADING;
int score = 0;

// MAIN CHAR
int p_x = 40;
int p_y = 12;
int p_dir_x = 0;
int p_dir_y = 0;
int p_alive = 1;  // Saber si estic viu

// ENEMIES
#define MAX_ENEMIES 3
int e_x[MAX_ENEMIES];
int e_y[MAX_ENEMIES];

// INPUTS
int input_dx = 0;
int input_dy = 0;
int key_start_pressed = 0;  // Saber si s'ha pulsat S

/* ========================================================================= */
/* FUNCIONS AUXILIARS                                                        */
/* ========================================================================= */

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

/* ========================================================================= */
/* FUNCIONS AUXILIARS DE PANTALLA                                            */
/* ========================================================================= */

void screen_clear(Byte bg_color)
{
  int i;
  Word empty = SCREEN_CHAR(' ', bg_color);

  // Omplim tot el buffer amb el caracter buit i el color de fons
  for (i = 0; i < SCREEN_SIZE; i++)
    screen[i] = empty;
}

void screen_putchar(int x, int y, char c, Byte color)
{
  // === BASE CASE
  // Comprovem limits per evitar escriure fora de memoria
  if (x < 0 || x >= SCREEN_COLS || y < 0 || y >= SCREEN_ROWS)
    return;

  // === GENERAL CASE
  screen[SCREEN_POS(x, y)] = SCREEN_CHAR(c, color);
}

void screen_putstr(int x, int y, const char *str, Byte color)
{
  int i;

  // Escrivim caracter a caracter fins acabar o sortir de pantalla
  for (i = 0; str[i] != '\0' && (x + i) < SCREEN_COLS; i++)
    screen_putchar(x + i, y, str[i], color);
}

void screen_flush(void)
{
  // Fem servir la syscall write amb el FD especial de pantalla (10)
  write(SCREEN_FD, (char *)screen, SCREEN_BYTES);
}

/* ========================================================================= */
/* HANDLER DE TECLAT (CALLBACK)                                              */
/* ========================================================================= */

void keyboard_handler(char key, int pressed)
{
  // === BASE CASE
  // CASE 1: Si ESC --> Sortir
  if (pressed && key == KEY_ESC) 
  {
    if (game_state == STATE_PLAYING || game_state == STATE_GAMEOVER) 
    {
      game_state = STATE_LOADING;
      p_alive = 1;
      score = 0;
    } 

    return;
  }

  // CASE 2: Si Loading i S
  if (pressed && game_state == STATE_LOADING && key == KEY_S) 
  {
    key_start_pressed = 1;
    return;
  }

  //  == GENERAL CASE
  // Control de direccio
  // TODO: Aqui podriem cream una struct de movimients predefinits
  if (pressed) 
  {
    switch(key) 
    {
      case KEY_UP:    input_dx = 0;  input_dy = -1; break;
      case KEY_DOWN:  input_dx = 0;  input_dy = 1;  break;
      case KEY_LEFT:  input_dx = -1; input_dy = 0;  break;
      case KEY_RIGHT: input_dx = 1;  input_dy = 0;  break;
    }
  } 
  else 
  {
    // Aturar moviment si deixem anar la tecla de la direccio actual
    switch(key) {
      case KEY_UP:    if (input_dy == -1) input_dy = 0; break;
      case KEY_DOWN:  if (input_dy == 1)  input_dy = 0; break;
      case KEY_LEFT:  if (input_dx == -1) input_dx = 0; break;
      case KEY_RIGHT: if (input_dx == 1)  input_dx = 0; break;
    }
  }
}

/* ========================================================================= */
/* THREAD 1: PANTALLA                                                        */
/* ========================================================================= */

void thread_screen(void *arg)
{
  int x;
  int y;
  int i;

  while (1) 
  {
    // 0. Iniciar la pantalla de base i ja modificarem
    screen_clear(COLOR_BLACK);

    // 1. Modificar en funcio estat
    if (game_state == STATE_LOADING)
    {
      screen_putstr(30, 10, "=== ZEOS DIG DUG ===", COLOR_LCYAN);
      screen_putstr(28, 12, "PRESS 'S' TO START", COLOR_YELLOW);
      screen_putstr(25, 14, "PAU & MAR - SOA 2025-2026", COLOR_LGRAY);
    }
    else if (game_state == STATE_GAMEOVER) 
    {  
      screen_putstr(35, 12, "GAME OVER", COLOR_RED);
      screen_putstr(28, 14, "PRESS ESC TO EXIT", COLOR_WHITE);
    }
    else  // Partida
    {
      // 1. Pintar Mapa
      for (y = 2; y < SCREEN_ROWS; y++)  // Primeres dos linies reservades per a info + "espai"
      {
        for (x = 0; x < SCREEN_COLS; x++) 
        {
          if (map[x][y] == 1)  // Si encara no s'ha menjat la terra --> Pintar
          {
            // Pintar terra amb colors segons profunditat
            Byte c = (y < 10) ? COLOR_BROWN : COLOR_LRED;
            screen_putchar(x, y, TILE_DIRT, c);
          }
        }
      }

      // 2. Pintar Info
      char score_buf[16];
      itoa(score, score_buf);
      screen_putstr(0, 0, "SCORE: ", COLOR_WHITE);
      screen_putstr(7, 0, score_buf, COLOR_WHITE);

      // 3. Pintar Jugador
      if (p_alive)
        screen_putchar(p_x, p_y, CHAR_PLAYER, COLOR_YELLOW);

      // 4. Pintar Enemics
      for (i = 0; i < MAX_ENEMIES; i++) 
      {
        screen_putchar(e_x[i], e_y[i], CHAR_ENEMY, COLOR_LGREEN);
      }
    }

    screen_flush();  // Pintar realment la pantalla
    
    // IMPO: WaitForTick es necessari per cedir la CPU
    // Si no ho fem, aquest bucle consumiria el 100% de CPU i tot mes lent (es juga fatal)
    WaitForTick();
  }
}

/* ========================================================================= */
/* THREAD 2: COSES DEL ENEMIC (IA MES INTELIGENT QUE EL CHATGPT)             */
/* ========================================================================= */

void thread_enemy(void *arg)
{
  int i;
  int tick_counter = 0;

  while (1) 
  {  
    // Nomes processem si estem jugant
    if (game_state == STATE_PLAYING) 
    {  
      // Els enemics es mouen mes lents (cada 6 ticks aprox)
      if (tick_counter++ > 6) 
      {
        tick_counter = 0;

        for (i = 0; i < MAX_ENEMIES; i++) 
        {
          // === IA SIMPLE: PERSEGUIR JUGADOR
          // Anar cap a la direccio on esta el jugador i au
          // IMPO: Comprovar limits pantalla que sino peta
          int dx = 0;
          int dy = 0;

          if (p_x > e_x[i]) dx = 1;
          else if (p_x < e_x[i]) dx = -1;

          if (p_y > e_y[i]) dy = 1;
          else if (p_y < e_y[i]) dy = -1;

          // 1. Primer intentar moure en Horizontal (trivial)
          // NOTA: Fem moviment manhattan
          int next_x = e_x[i] + dx;
          
          // IMPO: Si dx es 0, no hem d'entrar aqui o no es moura mai verticalment
          if (dx != 0 && 0 <= next_x && next_x < SCREEN_COLS) 
          {
            e_x[i] += dx;
          }
          else if (dy != 0) // 1.2. Sino en movem en verical
          {
            int next_y = e_y[i] + dy;
            if (2 <= next_y && next_y < SCREEN_ROWS)
            {
              e_y[i] += dy;
            }
          }

          // Si col·lisio --> GAME OVER
          if (e_x[i] == p_x && e_y[i] == p_y) 
          {
            p_alive = 0;
            game_state = STATE_GAMEOVER;
          }
        }
      }
    }

    WaitForTick();
  }
}

/* ========================================================================= */
/* THREAD 3: LOGICA PRINCIPAL (JUGADOR I ESTATS)                             */
/* ========================================================================= */

void thread_logic(void *arg)
{
  int x;
  int y;
  int i;
  int tick_counter = 0;

  // === INICIALITZACIO
  // Inicialitzar dades del joc
  p_x = 40; p_y = 12;
  input_dx = 0; input_dy = 0;
  
  // Inicialitzar mapa
  for (x = 0; x < SCREEN_COLS; x++) 
  {
    for (y = 0; y < SCREEN_ROWS; y++) 
    {
      if (y < 2) 
        map[x][y] = 0;  // Linea de info o divisoria (cel)
      else 
        map[x][y] = 1;  // Terra
    }
  }

  // Buidar on jugador (mes bonic)
  map[p_x][p_y] = 0; map[p_x+1][p_y] = 0; map[p_x-1][p_y] = 0;

  // Inicialitzar enemics
  for (i = 0; i < MAX_ENEMIES; i++) 
  {
    e_x[i] = 10 + (i * 20);
    e_y[i] = 5 + (i * 5);
  }

  while (1) 
  {

    // === MAQUINA D'ESTATS
    switch (game_state) 
    {  
      case STATE_LOADING:
        if (key_start_pressed) 
        {
          // === REINICIAR JOC
          p_x = 40; p_y = 12;
          input_dx = 0; input_dy = 0;
          p_alive = 1;
          score = 0;
          
          // Reiniciar mapa
          for (x = 0; x < SCREEN_COLS; x++) 
          {
            for (y = 0; y < SCREEN_ROWS; y++) 
            {
              if (y < 2) 
                map[x][y] = 0;
              else 
                map[x][y] = 1;
            }
          }

          map[p_x][p_y] = 0; map[p_x+1][p_y] = 0; map[p_x-1][p_y] = 0;

          // Reiniciar enemics
          for (i = 0; i < MAX_ENEMIES; i++) 
          {
            e_x[i] = 10 + (i * 20);
            e_y[i] = 5 + (i * 5);
            map[e_x[i]][e_y[i]] = 0;
          }

          game_state = STATE_PLAYING;
          key_start_pressed = 0;
        }

        break;

      case STATE_PLAYING:
        // Moure jugador cada 3 ticks
        if (tick_counter++ > 3) 
        {
          tick_counter = 0;

          int new_x = p_x + input_dx;
          int new_y = p_y + input_dy;

          // Comprovar limits
          if (new_x >= 0 && new_x < SCREEN_COLS && new_y >= 2 && new_y < SCREEN_ROWS) 
          {
            p_x = new_x;
            p_y = new_y;

            // Excavar: Si toquem terra --> Eliminaarlo
            if (map[p_x][p_y] == 1) 
            {
              map[p_x][p_y] = 0;
              score += 10; // Punts per excavar
            }
          }
        }
        break;

      case STATE_GAMEOVER:
        // Esperem a que l'usuari premi ESC (gestionat al handler)
        break;
    }

    WaitForTick();
  }
}

/* ========================================================================= */
/* MAIN                                                                      */
/* ========================================================================= */

int __attribute__((__section__(".text.main"))) main(void)
{
  // === INICIALITZACIO DEL SISTEMA
  
  // 1. Activem el teclat
  if (KeyboardEvent(keyboard_handler) < 0) 
  {
    println("ERROR: Keyboard Event");
    while (1);
  }

  // 2. Creem Thread de Pantalla
  // Aquest thread nomes s'encarrega d'omplir pantalla i mostrar-ho
  if (ThreadCreate(thread_screen, 0) < 0) 
  {
    println("ERROR: Screen Thread");
    while (1);
  }

  // 3. Creem Thread de Enemics
  // Aquest thread controla la logica dels fantasmes
  if (ThreadCreate(thread_enemy, 0) < 0) 
  {
    println("ERROR: Enemy Thread");
    while (1);
  }

  // 4. Creem Thread de Logica Principal
  // Aquest thread controla l'estat del joc i el jugador
  if (ThreadCreate(thread_logic, 0) < 0) 
  {
    println("ERROR: Logic Thread");
    while (1);
  }

  while (1);  // NOTA: Per mantindre els threads vius  
}