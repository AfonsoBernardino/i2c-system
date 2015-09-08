/*
*	dac7578.c -	A user-space I2C driver for DAC7578
*				8 channel, 12 bit Ditital-to-Analogue 
*				Converter from Texas Instruments.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/swab.h>

//DAC7578 Command definitions
//Power Commmads
#define DAC7578_PWR_ON_SELECTED_CH				0x0
#define DAC7578_PWR_OFF_SELECTED_CH				0x1
#define DAC7578_PWR_OFF_SELECTED_CH_VOUT_TO_GND 0x2
#define DAC7578_PWR_OFF_SELECTED_CH_VOUT_HIGH_Z	0x3
//Clear Pin Commands
#define DAC7578_CLRS_TO_ZERO_SCALE		0x0
#define DAC7578_CLRS_TO_MID_SCALE		0x1
#define DAC7578_CLRS_TO_FULL_SCALE		0x2
#define DAC7578_CLR_PIN_DISABLED		0x3
//Write Sequences
#define DAC7578_REG_INPUT_CH_WRITE				0x0
#define DAC7578_REG_INPUT_CH_UPDATE				0x1
#define DAC7578_REG_INPUT_CH_WRITE_ALL_UPDATE	0x2
#define DAC7578_REG_INPUT_CH_WRITE_UPDATE		0x3
#define DAC7578_REG_POWER_WRITE					0x4
#define DAC7578_REG_CLEAR_CODE_WRITE			0x5
#define DAC7578_REG_LDAC_WRITE				0x6
#define DAC7578_SOFT_RESET					0x7
//Read Sequences
#define DAC7578_REG_INPUT_CH_READ	0x0
#define DAC7578_REG_CH_READ			0x1
#define DAC7578_REG_POWER_READ		0x4
#define DAC7578_REG_CLEAR_CODE_READ	0x5
#define DAC7578_REG_LDAC_READ		0x6
//Access Sequences
#define DAC7578_CH_A		0x0
#define DAC7578_CH_B 		0x1
#define DAC7578_CH_C		0x2
#define DAC7578_CH_D		0x3
#define DAC7578_CH_E		0x4
#define DAC7578_CH_F		0x5
#define DAC7578_CH_G		0x6
#define DAC7578_CH_H		0x7
#define DAC7578_CH_ALL		0xf

const int dac7578_addr_list[4] = {0x48, 0x4a, 0x4c, 0};

static inline __u16 DAC7875_VAL_TO_REG(int val){
	return val<<4;
}

static inline int DAC7875_REG_TO_VAL(__u16 reg){
	return reg>>4;
}

int dac7578_read_reg(int fd, int addr, __u8 reg){
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if(reg != DAC7578_REG_POWER_READ && reg != DAC7578_REG_CLEAR_CODE_READ
									 && reg != DAC7578_REG_LDAC_READ){
		printf("Error: attempt to read invalid register\n");
		return EXIT_FAILURE;
	}

	return __swab16(i2c_smbus_read_word_data(fd, reg<<4));
}

int dac7578_read_ch(int fd, int addr, int ch){
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	if(ch < DAC7578_CH_A || ch > DAC7578_CH_H){
		printf("Error: attempt to read an invalid channel\n");
		return EXIT_FAILURE;
	}

	__u8 reg = (DAC7578_REG_CH_READ<<4)|((__u8)ch);

	return DAC7875_REG_TO_VAL(__swab16(i2c_smbus_read_word_data(fd, reg)));
}

int dac7875_write_reg(int fd, int addr, int reg, __u16 val){
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	if(reg != DAC7578_REG_POWER_WRITE && reg != DAC7578_REG_CLEAR_CODE_WRITE
									  && reg != DAC7578_REG_LDAC_WRITE){
		printf("Error: attempt to write an invalid register\n");
		return EXIT_FAILURE;
	}
	if(reg == DAC7578_REG_POWER_WRITE)
		return i2c_smbus_write_word_data(fd, reg<<4, __swab16(val<<5));
	else
		return i2c_smbus_write_word_data(fd, reg<<4, __swab16(val<<4));
}

int dac7875_write_ch(int fd, int addr, int ch, __u16 val){
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if((ch < DAC7578_CH_A || ch > DAC7578_CH_H) && ch != DAC7578_CH_ALL){
		printf("Error: attempt to write on invalid channel\n");
		return EXIT_FAILURE;
	}

	__u8 reg = (DAC7578_REG_INPUT_CH_WRITE_UPDATE<<4)|((__u8)ch);

	return i2c_smbus_write_word_data(fd, reg, __swab16(DAC7875_VAL_TO_REG(val)));
}

__u16 val_to_dac(float val, float lsb){	//val can be: vset in kV or ilim in uA
	 						//division by 2 is to convert units
	return (__u16) (val/lsb + 0.5);		//float to int rounding conversion;
}

int main(int argc, char *argv[]){
	int fd = 0;
	char filename[20];
	//int adapter_nr = 2;			
	float lsb = 0.097680; //mV   
	int addr = 0x4a;
	int flags = 0; 
	int bus = -1;
	int ch = -1;
	float val = -1;

	while (1+flags < argc && argv[1+flags][0] == '-') {
	    switch (argv[1+flags][1]) {
			case 'b':
					bus = atoi(argv[2+flags]);
					flags++;
					break;
			case 'c':
					ch = atoi(argv[2+flags]);
					flags++;
					break;
			case 's': 
					val = atof(argv[2+flags]);
					flags++;
					break;
			default:
					fprintf(stderr, "Error: Unsupported option "
													"\"%s\"!\n", argv[1+flags]);
					return EXIT_FAILURE;
		}
		flags++;	
	}
	
	if(bus < 0 || bus > 11){	
		printf("Error: wrong bus number\n");
		return EXIT_FAILURE;
	}

	snprintf(filename, 19, "/dev/i2c-%d", bus);

	if((fd = open(filename, O_RDWR)) < 0){
		printf("Failed to open the bus (adapter); %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if(ch < -1 || ch > 8){
		fprintf(stderr, "Error: wrong channel\n");
		return EXIT_FAILURE;
	}
		
	if(val == -1){ //Read
		__u16 data = dac7578_read_ch(fd, addr, ch);
		printf("Threshold: %0.1f mV\n", data*lsb);
		goto OUT;
	}

	if (val < 0 || val > 400){
		printf("Error: wrong threshold. Must be between 0 and 400 mV\n");
		return EXIT_FAILURE;
	}

	__u16 dac_val = val_to_dac(val, lsb);

	if(ch == -1)
		dac7875_write_ch(fd, addr, 0xf, dac_val);
	else
		dac7875_write_ch(fd, addr, ch, dac_val);

	OUT:
	return 0;
}
