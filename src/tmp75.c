/*
*	tmp75.c -	A user-space driver for TMP175
*				temperature sensor from Texas 
*				Instruments.
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
#include <linux/swab.h>

/*TMP75 Registers*/
#define TMP75_REG_TEMP		0x00
#define TMP75_REG_CONFIG	0x01
#define TMP75_REG_TLOW		0x02
#define TMP75_REG_THIGH		0x03		
/* straight from the datasheet */
#define TMP75_TEMP_MIN 		(-400)
#define TMP75_TEMP_MAX 		1250
#define TMP75_RESOLUTION_BITS_9		0x00
#define TMP75_RESOLUTION_BITS_12	0x60

//static const unsigned short tmp75_addr_list[] = { 0x48, 0x49, 0x4a, 0x4b, 0x4c, 
//											 0x4d, 0x4e, 0x4f, NULL };

const char tmp75_addr_low = 0x48;
const char tmp75_addr_high = 0x4f;

/* 	TEMP: 0.1C/bit (-40C to +125C)
	REG: (0.5C/bit, two's complement) << 7 */
static inline __u16 TMP75_TEMP_TO_REG(int temp){
        int ntemp;
        if(temp < TMP75_TEMP_MIN) 
        	ntemp = TMP75_TEMP_MIN;
        else if(temp > TMP75_TEMP_MAX) 
        	ntemp = TMP75_TEMP_MAX;
        else 
        	ntemp = temp;
        	
        ntemp += (ntemp<0 ? -2 : 2);
        return (__u16)((ntemp / 5) << 7);
}

static inline float TMP75_TEMP_FROM_REG_9BIT(__u16 reg_val){
        /* use integer division instead of equivalent right shift to
           guarantee arithmetic shift and preserve the sign */
        return ((__s16)reg_val / 128) * 0.5;
}

static inline float TMP75_TEMP_FROM_REG_12BIT(__u16 reg_val){
        /* use integer division instead of equivalent right shift to
           guarantee arithmetic shift and preserve the sign */
        return ((__s16)reg_val / 16) * 0.0625;
}				

/*Check functionality*/
int tmp75_functionality(int fd){

	unsigned long funcs;
	if(ioctl(fd, I2C_FUNCS, &funcs) < 0) {
		printf("Error: Could not get the adapter functionality matrix: %s\n", strerror(errno));
		return EXIT_FAILURE;
  	}
  	
  	if(!(funcs & (I2C_FUNC_SMBUS_READ_BYTE_DATA|I2C_FUNC_SMBUS_READ_WORD_DATA))){
	    printf("Error: Can't use SMBus Read/Write Byte/Word Data command on this bus; %s\n", strerror(errno));
	    return EXIT_FAILURE;
 	}/* Now it is safe to use the SMBus read_word_data command */
 	
 	return 0;
}
	
int tmp75_read_value(int fd, int addr, __u8 reg){
	
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	if(reg == TMP75_REG_CONFIG)
		return i2c_smbus_read_byte_data(fd, reg);
	else
		return __swab16(i2c_smbus_read_word_data(fd, reg));
}

int tmp75_write_value(int fd, int addr, __u8 reg, __u16 value){
	
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	if(reg == TMP75_REG_CONFIG){
		__u8 tmp = (__u8) value;
		return i2c_smbus_write_byte_data(fd, reg, tmp);
	}
	else
		return i2c_smbus_write_word_data(fd, reg, __swab16(value));
}

int tmp75_temp(int fd, int addr, __u16 *data){
	tmp75_write_value(fd, addr, TMP75_REG_CONFIG, TMP75_RESOLUTION_BITS_12); 
	data[0] = tmp75_read_value(fd, addr, TMP75_REG_TEMP);
	return 0;
}

void tmp75_print_val(__u16 *val, float lsb, char *data_type[8], int ch, 
															int log, int log_p){
	if(log){
		char str[16];
		sprintf(str, "%0.3f ", TMP75_TEMP_FROM_REG_12BIT(val[0]));
		write(log_p, str, strlen(str));
	}
	else
		printf("%s%d: %0.3f C\n", data_type[0], ch, 
											 TMP75_TEMP_FROM_REG_12BIT(val[0]));
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
	
	if(tmp75_functionality(fd) != 0){
		close(fd); 
		return EXIT_FAILURE;
	}
	
	int wr = tmp75_write_value(fd, 0x49, TMP75_REG_CONFIG, TMP75_RESOLUTION_BITS_12);
	printf("write: %d\n", wr);

	int config_reg = tmp75_read_value(fd, 0x49, TMP75_REG_CONFIG);
	printf("CONF REG: %#02x\n", config_reg);

	float temp = tmp75_temp(fd, 0x49);
	printf("t: %0.3f\n", temp);

	close(fd);
	return 0;

}
*/
