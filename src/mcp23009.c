/*
*
*	mcp23009.c - A user-space driver for Microchip MCP23009
*				 8-bit I/O expander with I2C interface. 
*
*
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "mcp23009.h"

const mcp23009_addr_low = 0x20;
const mcp23009_addr_high = 0x27;

/*Check bus functionality*/
int mcp23009_functionality(int fd){

	unsigned long funcs;
	if(ioctl(fd, I2C_FUNCS, &funcs) < 0) {
		printf("Error: Could not get the adapter functionality matrix: %s\n", 
															strerror(errno));
		return EXIT_FAILURE;
  	}
  	
  	if(!(funcs & I2C_FUNC_SMBUS_BYTE_DATA)){
	    printf("Error: Can't use SMBus Read/Write Byte Data command on this "
	    										"bus; %s\n", strerror(errno));
	    return EXIT_FAILURE;
 	}/* Now it is safe to use the SMBus read_byte_data command */
 	
 	return 0;
}

int mcp23009_write_val(int fd, int addr, __u8 reg, __u8 val){
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	return i2c_smbus_write_byte_data(fd, reg, val);	
}

int mcp23009_read_val(int fd, int addr, __u8 reg){
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	return i2c_smbus_read_byte_data(fd, reg);
}

/*
*
*	APPLICATION
*
*/

int mcp23009_read_val2(int fd, int addr, __u16 data[8]){
	mcp23009_write_val(fd, addr, MCP23009_REG_GPPU, 0x08); //Config pull-up 
	mcp23009_write_val(fd, addr, MCP23009_REG_IODIR, 0x17);//Config ports dir.
	//mcp23009_write_val(fd, addr, MCP23009_REG_GPIO, 0x08);
	data[0] = mcp23009_read_val(fd, addr, MCP23009_REG_GPIO);
	return 0;
}

void mcp23009_print_val(__u16 val[8], float lsb, char *data_type[8], int i, 
															int log, int log_p){
	int bit;
	if(!log)
		printf("------IO------\n");
//	printf("data: %#x\n", val[0]);
	for(bit=0; bit<5; bit++){
		if(log){
			char str[16];
			sprintf(str, "%d ", val[0]&(1<<bit)?1:0);
			write(log_p, str, strlen(str));
		}
		else
			printf("%s: %d\n", data_type[bit], val[0]&(1<<bit)?1:0);
	}
}

/*
int open_bus(int fd, int bus){
	if( ioctl(fd, I2C_SLAVE, 0x70) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	if(bus < 0 || bus > 7 ){
		printf("Error: bad bus number\n");
		return EXIT_FAILURE;
	}
	
	return i2c_smbus_write_byte(fd, 0x08+bus);
}

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
	
	open_bus(fd, 7);
	
	if(mcp23009_functionality(fd) != 0){
		close(fd); 
		return EXIT_FAILURE;
	}
	
	int status = mcp23009_write_val(fd, 0x20, MCP23009_REG_GPPU, 0x08);
	printf("pull up status: %d\n", status);
	
	status = mcp23009_write_val(fd, 0x20, MCP23009_REG_IODIR, 0x17);
	printf("dir status: %d\n", status);
	
	//status = mcp23009_write_val(fd, 0x20, MCP23009_REG_GPIO, 0x08);
	//printf("on status: %d\n", status);
	
	int data = mcp23009_read_val(fd, 0x20, MCP23009_REG_GPIO);
	printf("data: %#x\n", data);
	int bit;
	for(bit = 0; bit < 8; bit++){
		printf("data[%d]: %d\n", bit, data&(1<<bit)?1:0);
	}

	__u16 data_l[8];
	mcp23009_read_val2(fd, 0x20, data_l);

	char *data_type[8] = {"D0  ","D1  ","D2  ","HVon","D4  ","D5  ","D6  ","D7  "};
	int i = 0;
	float lsb;
	mcp23009_print_val(data_l, lsb, data_type, i);

	close(fd);
	return 0;
}
*/
