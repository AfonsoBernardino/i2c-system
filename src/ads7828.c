/*
*	ads7828.c -	A user-space driver for ADS7828
*				12-bit, 8-Channel ADC with I2C
*				interface from Texas Instruments.
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

/* The ADS7828 registers */
#define ADS7828_NCH             8       /* 8 channels supported */
#define ADS7828_CMD_SD_SE       0x80    /* Single ended inputs */
#define ADS7828_CMD_PD1         0x04    /* Internal vref OFF && A/D ON */
#define ADS7828_CMD_PD3         0x0C    /* Internal vref ON && A/D ON */
#define ADS7828_INT_VREF_MV     2500    /* Internal vref is 2.5V, 2500mV */
#define ADS7828_EXT_VREF_MV_MIN 50      /* External vref min value 0.05V */
#define ADS7828_EXT_VREF_MV_MAX 5250    /* External vref max value 5.25V */


const char ads7828_addr_low = 0x48;
const char ads7828_addr_high = 0x4b;

/* Command byte C2,C1,C0 - see datasheet */
static inline __u8 ads7828_cmd_byte(__u8 cmd, int ch){
	return cmd | (((ch >> 1) | (ch & 0x01) << 2) << 4);
}

/*Check bus functionality*/
int ads7828_functionality(int fd){

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

int ads7828_read_ch(int fd, int addr, int ch){
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	if(ch < 0 || ch >= ADS7828_NCH){
		printf("Error: bad channel number\n");
		return EXIT_FAILURE;
	}
	
	return __swab16(i2c_smbus_read_word_data(fd, ads7828_cmd_byte(
														ADS7828_CMD_SD_SE|
														ADS7828_CMD_PD1, ch)));
}

int ads7828_read_all(int fd, int addr, __u16 data[ADS7828_NCH]){
	int ch;
	for(ch=0; ch<ADS7828_NCH; ch++)
		data[ch] = ads7828_read_ch(fd, addr, ch);

	return 0;
}

void ads7828_print_val(__u16 val[ADS7828_NCH], float lsb, float *conv_param, 
												  char *data_type[ADS7828_NCH],
												     int i, int log, int log_p){
	int ch; char str[16];
	if(!log)
		printf("-----ADC------\n");
	for(ch=0;ch<8; ch++){
		if(log){
			sprintf(str, "%0.3f ", val[ch]*lsb*conv_param[ch]);
			write( log_p, str, strlen(str) );
		}
		else{	
			if(ch==4)
				printf("%s: %0.3f nA\n", data_type[ch], val[ch]*lsb*conv_param[ch]);
			else if(ch==5)
				printf("%s: %0.3f V\n", data_type[ch], val[ch]*lsb*conv_param[ch]);
			else if(ch==0 || ch==1 || ch==7)
				printf("%s: %0.3f uA\n", data_type[ch], val[ch]*lsb*conv_param[ch]);
			else
				printf("%s: %0.3f kV\n", data_type[ch], val[ch]*lsb*conv_param[ch]);
		}
	}
}

/*
*
*	APPLICATION
*
*/
/*
#define Vref		4.53 	//(V)
static const float LSB = Vref/4096; // ( V/bit)
const int ADC_CODES_TO_kV = 2;

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
	
	if(ads7828_functionality(fd) != 0){
		close(fd); 
		return EXIT_FAILURE;
	}
	
	//int data = ads7828_read_ch(fd, 0x4a, 6);
	//printf("data: %f\n", data*2*LSB);
	
	__u16 all_data[8];

	ads7828_read_all(fd, 0x4a,all_data);

	int ch;
	for(ch=0;ch <8; ch++)
		printf("%0.3f\n", all_data[ch]*LSB*ADC_CODES_TO_kV );

	printf("\n");

	ads7828_print_val(all_data);

	close(fd);
	return 0;
}
*/

