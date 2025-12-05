/*
 * libc.h - macros per fer els traps amb diferents arguments
 *          definició de les crides a sistema
 */

#ifndef __LIBC_H__
#define __LIBC_H__

#include <stats.h>

// NOTA: Aquest truc de fer-ho aixi m'ho ha dit el ChatGPT.
int *__errno_location(void);
#define errno (*__errno_location())

int write(int fd, char *buffer, int size);

void itoa(int a, char *b);

int strlen(char *a);

void perror();

int getpid();

int fork();

void exit();

int yield();

int get_stats(int pid, struct stats *st);

// === THREAD
int ThreadCreate(void (*function)(void *arg), void *parameter);
void ThreadExit(void);
void ThreadWrapper(void (*function)(void *arg), void *parameter);

// === KEYBOARD
int KeyboardEvent(void (*func)(char key, int pressed));
void KeyboardWrapper(void (*func)(char, int), char key, int pressed);

#endif /* __LIBC_H__ */
