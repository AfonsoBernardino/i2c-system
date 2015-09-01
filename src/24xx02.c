/*
*	24xx02.c -	A user-space driver for 24AA02/24LC02B 
*				EEPROM devices from Microchip.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include "msleep.h"

#define EEPROM_24XX02_WRITE_CYCLE_TIME_MAX	5 // ms 

/*24xx02 I2C Address, 7-bit address: 101 0xxx (x = don't care)*/
unsigned int eeprom24xx02_addr_list[] = {0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
																	0x57, 0};

/*Check functionality*/
int eeprom_24xx02_functionality(int fd){

	unsigned long funcs;
	if(ioctl(fd, I2C_FUNCS, &funcs) < 0) {
		printf("Error: Could not get the adapter functionality matrix: %s\n", strerror(errno));
		return EXIT_FAILURE;
  	}
  	
  	if(!(funcs & (I2C_FUNC_SMBUS_BLOCK_DATA))){
	    printf("Error: Can't use SMBus Read/Write Block Data command on this bus; %s\n", strerror(errno));
	    return EXIT_FAILURE;
 	}/* Now it is safe to use the SMBus block_data command */
 	
 	return 0;
}

int eeprom_24xx02_write_byte(int fd, int addr, __u8 reg, __u8 val){
	
	int ret;
	
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	ret = i2c_smbus_write_byte_data(fd, reg, val);
	if(ret < 0){
		printf("Failed to write byte; %s\n", strerror(errno));
		return ret;	
	}
	
	msleep(EEPROM_24XX02_WRITE_CYCLE_TIME_MAX);
	
	return ret;
}


int eeprom_24xx02_read_byte(int fd, int addr, __u8 reg){

	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	return i2c_smbus_read_byte_data(fd, reg);
}
/*
int main(int argc, char *argv[]){

	int fd = 0;
	char filename[20];
	int adapter_nr = 1; // 0 for R.Pi A and B;
						// 1 for R.Pi B+ 
	snprintf(filename, 19, "/dev/i2c-%d", adapter_nr);
	if((fd = open(filename, O_RDWR)) < 0){
		printf("Failed to open the bus (adapter); %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	int func = eeprom_24xx02_functionality(fd);
	
	int wr_status = eeprom_24xx02_write_byte(fd, eeprom24xx02_addr_list[0], 0x00, 'j');
	printf("WR:%d\n", wr_status);
	
	
	int reg_val = eeprom_24xx02_read_byte(fd, eeprom24xx02_addr_list[0], 0x00);
	printf("REG VAL: %c\n", reg_val);
	
	close(fd);
	return 0;
}
*/


