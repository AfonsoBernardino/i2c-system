/*
*	tmp75.c - A user-space driver for SHT21.
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

/* SHT21 Commands */
#define SHT21_TRIG_T_MEASUR_HM		0xe3
#define SHT21_TRIG_RH_MEASUR_HM		0xe5
#define SHT21_TRIG_T_MEASUR_NH		0xf3
#define SHT21_TRIG_RH_MEASUR_NH		0xf5
#define SHT21_USR_REG_WR		0xe6
#define SHT21_USR_REG_RD		0xe7
#define SHT21_SOFT_RESET		0xfe

/* SHT21 I2C Address*/
#define SHT21_ADDR	0x40 //single address

#define SHT21_MEAS_TIME_HUMIDITY 	29000 // 29000 us, max at 12 bit resolution
#define SHT21_MEAS_TIME_TEMPERATURE 85000 // 85000 us, max at 14 bit resolution



static inline int sht21_temp_ticks_to_millicelsius(int ticks){
	
	ticks &= ~0x0003; /* clear status bits */
    /*
    * Formula T = -46.85 + 175.72 * ST / 2^16 from data sheet 6.2,
    * optimized for integer fixed point (3 digits) arithmetic
    */
    return ((21965 * ticks) >> 13) - 46850;
}

inline int sht21_rh_ticks_to_per_cent_mille(int ticks){
	ticks &= ~0x0003; /* clear status bits */
	/*
	 * Formula RH = -6 + 125 * SRH / 2^16 from data sheet 6.1,
	 * optimized for integer fixed point (3 digits) arithmetic
	 */
	return ((15625 * ticks) >> 13) - 6000;
}

/*Check functionality*/
int sht21_functionality(int fd){

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

int sht21_read_value(int fd, int addr, __u8 reg){
	
	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	if(reg == SHT21_USR_REG_RD)
		return i2c_smbus_read_byte_data(fd, reg);
	else
		return __swab16(i2c_smbus_read_word_data(fd, reg));	
}

int sht21_write_value(int fd, int addr, __u8 reg, __u8 val){

	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	if(reg == SHT21_SOFT_RESET) //here val is ignored
		return i2c_smbus_write_byte(fd, reg);
	else
		return i2c_smbus_write_byte_data(fd, reg, val);
}

pthread_mutex_t lock;

int sht21_measur(int fd, int addr, __u8 reg){
	
	int data[2];

	if(pthread_mutex_init(&lock, NULL) != 0){
		printf("Failed to mutex init\n");
		return EXIT_FAILURE;
	}

	pthread_mutex_lock(&lock);

	if( ioctl(fd, I2C_SLAVE, addr) < 0 ){
		printf("Failed to configure the device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
		
	if(i2c_smbus_write_byte(fd, reg) < 0){
		printf("Failed writing to device; %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	if(reg == SHT21_TRIG_T_MEASUR_NH || reg == SHT21_TRIG_T_MEASUR_HM)
		usleep(SHT21_MEAS_TIME_TEMPERATURE);
	else if(reg == SHT21_TRIG_RH_MEASUR_NH || reg == SHT21_TRIG_RH_MEASUR_HM)
		usleep(SHT21_MEAS_TIME_HUMIDITY);
	else
		return EXIT_FAILURE;
	
	data[0] = i2c_smbus_read_byte(fd);
	data[1] = i2c_smbus_read_byte(fd);

	usleep(500000); //to guarantee a maximum of two measurments
				    //per second at 12 bit acuracy (datasheet 2.4)

	pthread_mutex_unlock(&lock);
    pthread_mutex_destroy(&lock);

	return (data[0]<<8) | data[1];
}

int sht21_humid(int fd, int addr, __u16 *data){ 
	data[0] = sht21_measur(fd, addr, SHT21_TRIG_RH_MEASUR_NH);
	return 0;
}

void sht21_print_val(__u16 val[8], float lsb, float conv_param[8], 
															char *data_type[8], 
															int ch, int log, 
															int log_p){
	float humid = sht21_rh_ticks_to_per_cent_mille(val[0])/1000;
	if(log){
		char str[16];
		sprintf(str, "%0.3f ", humid);
		write(log_p, str, strlen(str));
	}
	else
		printf("%s%d: %0.3f %%\n", data_type[0], ch, humid);
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
	
	if(sht21_functionality(fd) != 0){
		close(fd); 
		return EXIT_FAILURE;
	}

	int reg_val1 = 0;
	reg_val1 = sht21_measur(fd, 0x40, SHT21_TRIG_T_MEASUR_NH);
	float temp = sht21_temp_ticks_to_millicelsius(reg_val1);
	int reg_val2 = sht21_measur(fd, 0x40, SHT21_TRIG_RH_MEASUR_NH);
	float relhumid = sht21_rh_ticks_to_per_cent_mille(reg_val2);
	
	printf("t: %0.3f\n", temp/1000);
	printf("h: %0.3f\n", relhumid/1000);
	
	close(fd);
	return 0;
}
*/
