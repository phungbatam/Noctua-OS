#ifndef PORTS_H
#define PORTS_H

unsigned char inb(unsigned short port);
unsigned short inw(unsigned short port);
unsigned long inl(unsigned short port);
void outb(unsigned short port, unsigned char data);
void outw(unsigned short port, unsigned short data);
void outl(unsigned short port, unsigned long data);

#endif
