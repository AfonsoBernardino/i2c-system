#ifndef __MCP23009_H__
#define __MCP23009_H__
//MCP23009  Registers Address
#define MCP23009_REG_IODIR		0x00	//I/O Direction Register
#define MCP23009_REG_IPOL 		0x01	//Input Polarity Port Register
#define MCP23009_REG_GPINTEN	0x02	//Interrupt-On-Change Pins
#define MCP23009_REG_DEFVAL		0x03	//Default Value Register
#define MCP23009_REG_INTCON		0x04	//Interrupt-On-Change Control Register 
#define MCP23009_REG_IOCON		0x05	//I/O Expander Configuration Resgister
#define MCP23009_REG_GPPU		0x06	//GPIO Pull-Up Resistor Register
#define MCP23009_REG_INTF		0x07	//Interrupt Flag Register
#define MCP23009_REG_INTCAP		0x08	//Interrut Captured Value From Port Register
#define MCP23009_REG_GPIO		0x09	//General Purpouse I/O Port Register
#define MCP23009_REG_OLAT		0x0a	//Output Latch Register 0

#endif
