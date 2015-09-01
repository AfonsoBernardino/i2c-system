/*
*	ad5694.c -	A user-space driver for AD5694
*				12-Bit DAC with I2C interface 
*				from Analog Devices
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include "swap_byte.h"

/* DAC AD5694 Definitions */
//Command Definitions
#define AD5694_NOP 					0x00	//No operation
#define AD5694_INPUT_REG_WRITE		0x01	//Write to Input Register n 
#define	AD5694_INPUT_REG_UPDATE		0x02	//Update DAC Register 
#define AD5694_CHANNEL_WRITE_UPDATE	0x03	//Write to and update DAC Channel n 
#define	AD5694_PWR_DOWN_UP			0x04	//Power down/power up DAC 
#define	AD5694_MASK_LDAC_PIN		0x05	//Hardware !LDAC mask register
#define AD5694_SOFT_RESET			0x06	//Software reset(power-on reset)
#define AD5694_RESERVED1			0x07	//Reserved
//...										//Reserved
#define AD5694_RESERVED9			0x0F	//Reserved
#define AD5694_NCH					8

const int ad5694_addr_high = 0x0c;
const int ad5694_addr_low = 0x0f;
 
static inline __u16 AD5694_VAL_TO_REG(int val){
	return val<<4;
}

static inline int AD5694_REG_TO_VAL(__u16 reg){
	return reg>>4;
}

/*Check bus functionality*/
int ad5694_functionality(int fd){

	unsigned long funcs;
	if(ioctl(fd, I2C_FUNCS, &funcs) < 0) {
		printf("Error: Could not get the adapter functionality matrix: %s\n", 
															strerror(errno));
		return EXIT_FAILURE;
  	}
  	
  	if(!(funcs & I2C_FUNC_SMBUS_WORD_DATA)){
	    printf("Error: Can't use SMBus Read/Write Word Data command on this "
	    										"bus; %s\n", strerror(errno));
	    return EXIT_FAILURE;
 	}/* Now it is safe to use the SMBus read_word_data command */
 	
 	return 0;
}

int ad5694_read_ch(int fd, int addr, __u8 reg){
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	if(reg > 8){
		printf("Error: wrong channel\n");
		return EXIT_FAILURE;
	}
	
	return AD5694_REG_TO_VAL(SWAP_BYTE(i2c_smbus_read_word_data(fd, 1<<reg)));
}

int ad5694_read_all(int fd, int addr, __u16 data[AD5694_NCH]){
	int ch;
	for(ch=0;ch<AD5694_NCH; ch++)
		data[ch] = ad5694_read_ch(fd, addr, ch);

	return 0;
}

void ad5694_print_val(__u16 val[AD5694_NCH], float lsb, char *data_type[8], 
													 int i, int log, int log_p){
	int ch;
	if(!log)
		printf("-----DAC------\n");
	for(ch=0;ch<2; ch++){
		if(log){
			char str[16];
			sprintf(str,"%0.3f ", val[ch]*lsb);
			write(log_p, str, strlen(str));
		}
		else
			printf("%s: %0.3f %s\n", data_type[ch], val[ch]*lsb, (ch==1?"uA":"kV"));
	}
}

int ad5694_write_ch(int fd, int addr, __u8 ch, __u16 val){
	if(ch > 8){
	printf("Error: wrong channel\n");
		return EXIT_FAILURE;
	}
	
	__u8 reg = (AD5694_CHANNEL_WRITE_UPDATE<<4)|(1<<ch);
	
	if( ioctl(fd, I2C_SLAVE, addr) < 0){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	return i2c_smbus_write_word_data(fd, reg, SWAP_BYTE(AD5694_VAL_TO_REG(val)));
}

/*
*
*		APPLICATION
*
*/
/*
#define Vref		4.53 			//(V)
static const float LSB = Vref/4096; // ( V/bit)
__u16 vset_ilim_to_ad5694(float val){//val can be: vset in kV or ilim in uA
	val = val/2; 					// division by 2 is to convert units
	return (__u16) (val/LSB + 0.5);	//float to int rounding conversion;
}

float ad5694_to_vset_ilim(__u16 val){
	return val*2*LSB;				  // multiplication by 2 is to convert units
}

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
	
	if(ad5694_functionality(fd) != 0){
		close(fd); 
		return EXIT_FAILURE;
	}
	
	__u16 vset = vset_ilim_to_ad5694(0.5);
	int wr_status = ad5694_write_ch(fd, 0x0d, 0, AD5694_VAL_TO_REG(vset));
	printf("status: %d\n", wr_status);
	
	int data = AD5694_REG_TO_VAL(ad5694_read_ch(fd, 0x0d, 0));
	printf("data: %f\n", ad5694_to_vset_ilim(data));

	close(fd);
	return 0;
}
*/
