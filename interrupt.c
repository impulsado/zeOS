/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>
#include <sched.h>  // For INITIAL_ESP
#include <utils.h>  // For writeMSR
#include <zeos_interrupt.h>

Gate idt[IDT_ENTRIES];
Register    idtR;

// IMPO: Com que aquesta funcio esta implementada en ASM i aixo es codi C
// el pre-compilador ha de saber previament les funcions a fer servir perque el linker
// ho relacioni amb la funcio escrita en ASM.
void page_fault_handler_new(void);
void clock_handler(void);
void keyboard_handler(void);
void system_call_handler(void);
void writeMSR(unsigned int i, unsigned int low);


// Ho definim aqui perque aixo es algo relacionat amb la interrupcio "getTime"
// No te sentit definir-ho a un altre lloc (system.c) perque aquell fitxer nomes s'encarrega de cridar a wrappers, no fer coses d'interrupcions
unsigned int zeos_ticks = 0;


char char_map[] =
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','¡','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','ñ',
  '\0','º','\0','ç','z','x','c','v',
  'b','n','m',',','.','-','\0','*',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','7',
  '8','9','-','4','5','6','+','1',
  '2','3','0','\0','\0','\0','<','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0'
};

void setInterruptHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE INTERRUPTION GATE FLAGS:                          R1: pg. 5-11  */
  /* ***************************                                         */
  /* flags = x xx 0x110 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void setTrapHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE TRAP GATE FLAGS:                                  R1: pg. 5-11  */
  /* ********************                                                */
  /* flags = x xx 0x111 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);

  //flags |= 0x8F00;    /* P = 1, D = 1, Type = 1111 (Trap Gate) */
  /* Changed to 0x8e00 to convert it to an 'interrupt gate' and so
     the system calls will be thread-safe. */
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}


void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;
  
  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */
  setInterruptHandler(14, page_fault_handler_new, 0);
  setInterruptHandler(32, clock_handler, 0); 
  setInterruptHandler(33, keyboard_handler, 0);
  
  // Lligar el handler general a la "int 0x80"
  // NOTA: No ho volem en aquest cas perque fem sysenter --> fem us de MSR
  //setTrapHandler(0x80, system_call_handler, 3);  // 3 pq les traps sempre venen de user mode

  set_idt_reg(&idtR);

  // NOTA: Fico "(unsigned int)" per castejar. No doni errors i seguretat.
  writeMSR(0x174, (unsigned int)__KERNEL_CS);
  writeMSR(0x175, (unsigned int)INITIAL_ESP);
  writeMSR(0x176, (unsigned int)system_call_handler);
}

void clock_routine(void)
{
	// 1. Call implemented routine
	zeos_show_clock();
	
	zeos_ticks++;
}

void keyboard_routine(void)
{
	unsigned char b;
	unsigned char mode;
	unsigned char scan_code;
	unsigned char c = 'x';

	// 1. Read the corresponding keyboard port (0x60)
	b = inb(0x60);
	
	// 2. make (key_pressed) || break (key_released)
	mode = b & 0x80;
	scan_code = b & 0x7F;

	// 2.0. Si el mode es break --> Ja hem pintat previament
	if (mode == 1)
		return;

	// 2.1. Get char from scan code using char_map
	c = char_map[scan_code];
	
	// 2.2. If is not ASCII --> Print 'C'
	if (c == '\0')
		c = 'C';

	// 3. Print screen
	// (0x0, 0x0) --> Upper-left
	printc_xy(0x0, 0x0, c);
}

// NOTE: Aixo es una exception, automaticament es guarda un codi d'error post al eip
// Com que fem servir el truc d'accedir com si fos un parametre, haurem d'afegir-lo i que el tingui en compte el compilador.
void page_fault_routine_new(unsigned int error, unsigned int eip)
{
	const char HEXA[] = "0123456789ABCDEF";
	char dir[32/4 + 1];  // 32b / 4b (1 hexa) = 0xXXXXXXXX (8 X's) ;; +1 = '\0'
	int i;
	int hexa_act;

  printk("\n Process generates a PAGE FAULT exception at EIP: 0x");

	// Convert to hexa
	for (i = 7; i >= 0; i--)
	{
		hexa_act = (eip >> 4*i) & 0x0F;  // Agafar els xxxx actual
		dir[7-i] = HEXA[hexa_act];
	}
	dir[8] = '\0';

	printk(dir);
        while (1);
}
