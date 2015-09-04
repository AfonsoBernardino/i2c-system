/*
*	mpl115.c - 	A user-space driver for MPL115A2 digital 
*				barometer/termometer from Freescale.
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
#include <pthread.h>
#include <linux/swab.h>


/* MPL115 Registers */
#define MPL115_PADC		0x00 /* pressure ADC output value, MSB first, 10 bit */
#define MPL115_TADC 	0x02 /*temperature ADC output value, MSB first, 10 bit*/
#define MPL115_A0 		0x04 /* 12 bit integer, 3 bit fraction */
#define MPL115_B1 		0x06 /* 2 bit integer, 13 bit fraction */
#define MPL115_B2		0x08 /* 1 bit integer, 14 bit fraction */
#define MPL115_C12 		0x0a /* 0 bit integer, 13 bit fraction */
#define MPL115_CONVERT	0x12 /* convert temperature and pressure */
/* MPL115 I2C Address */
#define MPL115_ADDR					0x60
//
#define MPL115_CONVERSION_TIME_MAX	3000 //us

/*Check functionality*/
int mpl115_functionality(int fd){

	unsigned long funcs;
	if(ioctl(fd, I2C_FUNCS, &funcs) < 0) {
		printf("Error: Could not get the adapter functionality matrix: %s\n", strerror(errno));
		return EXIT_FAILURE;
  	}
  	
  	if(!(funcs & (I2C_FUNC_SMBUS_WRITE_BYTE_DATA|I2C_FUNC_SMBUS_READ_WORD_DATA))){
	    printf("Error: Can't use SMBus Read/Write Byte/Word Data command on this bus; %s\n", strerror(errno));
	    return EXIT_FAILURE;
 	}/* Now it is safe to use the SMBus read_word_data command */
 	
 	return 0;
}

int mpl115_convert(int fd, int addr){
	
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	int ret = i2c_smbus_write_byte_data(fd, MPL115_CONVERT, 0);
	
	if(ret < 0){
		printf("Failed to start conversion\n");
		return ret;	
	}
		
	usleep(MPL115_CONVERSION_TIME_MAX);
	
	return 0;	
}

int mpl115_temp(int fd, int addr){
// temperature -5.35 C / LSB, 472 LSB is 25 C 
	
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	int ret = i2c_smbus_write_byte_data(fd, MPL115_CONVERT, 0);
	
	if(ret < 0){
		printf("Failed to start conversion; %s\n", strerror(errno));
		return ret;	
	}
	
	usleep(MPL115_CONVERSION_TIME_MAX);
	
	ret = i2c_smbus_read_word_data(fd, MPL115_TADC);
	
	if(ret < 0){
		printf("Failed to read temperture data; %s\n", strerror(errno));
		return ret;	
	}
	
	return __swab16(ret)>>6;
}

pthread_mutex_t lock;

int mpl115_comp_pressure(int fd, int addr, int *val_i, int *val_f){

	int ret;
	__u16 tadc; __u16 padc;
	__s16 a0; __s16 b1; 
	__s16 b2; __s16 c12;
	int a1; int y1; int pcomp;
	unsigned pressure_kPa;
	
	if(pthread_mutex_init(&lock, NULL) != 0){
		printf("Failed to mutex init\n");
		return EXIT_FAILURE;
	}
	pthread_mutex_lock(&lock);

	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	ret = i2c_smbus_write_byte_data(fd, MPL115_CONVERT, 0);
	if(ret < 0){
		printf("Failed to start conversion; %s\n", strerror(errno));
		return ret;	
	}
	
	usleep(MPL115_CONVERSION_TIME_MAX);
	
	ret = i2c_smbus_read_word_data(fd, MPL115_TADC);
	if(ret < 0)	
		return ret;	 
	tadc = __swab16(ret) >> 6;
	
	ret = i2c_smbus_read_word_data(fd, MPL115_PADC);
	if(ret < 0)
		return ret;	 
	padc = __swab16(ret) >> 6;
	
	ret  = i2c_smbus_read_word_data(fd, MPL115_A0);
	if(ret < 0)
		return ret;	 
	a0 = __swab16(ret);
	
	ret  = i2c_smbus_read_word_data(fd, MPL115_B1);
	if(ret < 0)
		return ret;	 
	b1 = __swab16(ret);
	
	ret  = i2c_smbus_read_word_data(fd, MPL115_B2);
	if(ret < 0)
		return ret;	 
	b2 = __swab16(ret);
	
	ret = i2c_smbus_read_word_data(fd, MPL115_C12);
	if(ret < 0)
		return ret;	 
	c12 = __swab16(ret);

	pthread_mutex_unlock(&lock);
    pthread_mutex_destroy(&lock);
	
	a1 = b1 + ((c12 * tadc) >> 11);
    y1 = (a0 << 10) + a1 * padc;
    
	/* compensated pressure with 4 fractional bits */
    pcomp = (y1 + ((b2 * (int) tadc) >> 1)) >> 9;
    
	pressure_kPa = pcomp * (115 - 50) / 1023 + (50 << 4);
	
	*val_i = pressure_kPa >> 4;
	*val_f = (pressure_kPa & 15) * (1000000 >> 4);
	
	return 0;
}

/*
*
*Application specidfic functions
*
*/
int mpl115_press(int fd, int addr, __u16 *data){

	int ret;
	__u16 tadc; __u16 padc;
	__s16 a0; __s16 b1; 
	__s16 b2; __s16 c12;
	int a1; int y1; int pcomp;
	unsigned pressure_kPa;
	
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	ret = i2c_smbus_write_byte_data(fd, MPL115_CONVERT, 0);
	if(ret < 0){
		printf("Failed to start conversion; %s\n", strerror(errno));
		return ret;	
	}
	
	usleep(MPL115_CONVERSION_TIME_MAX);
	
	ret = i2c_smbus_read_word_data(fd, MPL115_TADC);
	if(ret < 0)	
		return ret;	 
	tadc = __swab16(ret) >> 6;
	
	ret = i2c_smbus_read_word_data(fd, MPL115_PADC);
	if(ret < 0)
		return ret;	 
	padc = __swab16(ret) >> 6;
	
	ret  = i2c_smbus_read_word_data(fd, MPL115_A0);
	if(ret < 0)
		return ret;	 
	a0 = __swab16(ret);
	
	ret  = i2c_smbus_read_word_data(fd, MPL115_B1);
	if(ret < 0)
		return ret;	 
	b1 = __swab16(ret);
	
	ret  = i2c_smbus_read_word_data(fd, MPL115_B2);
	if(ret < 0)
		return ret;	 
	b2 = __swab16(ret);
	
	ret = i2c_smbus_read_word_data(fd, MPL115_C12);
	if(ret < 0)
		return ret;	 
	c12 = __swab16(ret);
	
	a1 = b1 + ((c12 * tadc) >> 11);
    y1 = (a0 << 10) + a1 * padc;
    
	/* compensated pressure with 4 fractional bits */
    pcomp = (y1 + ((b2 * (int) tadc) >> 1)) >> 9;
    
	pressure_kPa = pcomp * (115 - 50) / 1023 + (50 << 4);
	
 	data[0] = pressure_kPa; 
	return 0;
}


void mpl115_print_val(__u16 *val, float lsb, char *data_type[8], int ch, 
															int log, int log_p){	

	int val_i = val[0] >> 4;
	int val_f = (val[0] & 15) * (1000000 >> 4);
	char tmp[20];
	sprintf(tmp, "%d.%d", val_i, val_f);
	float tmp2 = atof(tmp);
	if(log){
		char str[16];
		sprintf(str, "%0.3f ", tmp2);
		write(log_p, str, strlen(str));
	}
	else
		printf("%s%d: %0.3f kPa\n", data_type[0], ch, tmp2);
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
	
	if(mpl115_functionality(fd) != 0){
		close(fd); 
		return EXIT_FAILURE;
	}
	
	int i; int f;
	//int temp = mpl115_temp(fd, MPL115_ADDR);
	int status = mpl115_comp_pressure(fd, MPL115_ADDR, &i, &f);
	//printf("t: %d\n", temp);
	printf("p: %d.%d\n", i, f/1000);

	float press = mpl115_press(fd, MPL115_ADDR);
	printf("p: %0.3f\n", press);

	close(fd);
	return 0;
}
*/


