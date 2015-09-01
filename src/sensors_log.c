#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

int swap_byte(int word){	//Swap high byte with low byte
	return ((word&0xFF00)>>8)|((word&0x00FF)<<8);
}

void msleep (unsigned int ms) {
    int microsecs;
    struct timeval tv;
    microsecs = ms * 1000;
    tv.tv_sec  = microsecs / 1000000;
    tv.tv_usec = microsecs % 1000000;
    select (0, NULL, NULL, NULL, &tv);  
}

int main(int argc, char *argv[]){

	time_t rawtime;
  	struct tm *info;
	char date[16];
	char hour[16];
	char file_path [64] = "/home/hv/i2c/sensors/log/";
	int fd_log;
   	time( &rawtime );
   	char str[16];
   	int fd_dev;
	char filename[20];
	int adapter_nr = 1; // 0 for R.Pi A and B;
						// 1 for R.Pi B+ 
	info = localtime( &rawtime );
   	sprintf(date, "%02d-%02d-%04d.log", info->tm_mday, 
   										info->tm_mon+1, 
   										info->tm_year+1900);
   								
   	strcat(file_path, date);
   	fd_log = open(file_path, O_WRONLY|O_APPEND|O_CREAT);
   	fchmod(fd_log, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	
	sprintf(hour, "[%02d:%02d:%02d] ", info->tm_hour, 
   									   info->tm_min, 
   									   info->tm_sec);
  	write(fd_log, hour, 11);
  	
   	snprintf(filename, 19, "/dev/i2c-%d", adapter_nr);
	if((fd_dev = open(filename, O_RDWR)) < 0){
		char err_log[128];
		sprintf(err_log, "Failed to open the bus (adapter); %s\n", strerror(errno));
		write(fd_log, err_log, strlen(err_log));
		close(fd_log);
		return EXIT_FAILURE;
	}
	
	int wr_status = tmp75_write_value(fd_dev,  0x49, 0x01, 0x60);  
	float temp = tmp75_temp(fd_dev, 0x49);
	sprintf(str, "%0.1f ", temp/10);
	write(fd_log, str, strlen(str));
	
	int i; int f;
	int status = mpl115_comp_pressure(fd_dev, 0x60, &i, &f);
	sprintf(str, "%d.%d ", i, f/100000);
	write(fd_log, str, strlen(str));
	
	int reg_val = sht21_measur(fd_dev, 0x40, 0xf5);
	float relhumid = sht21_rh_ticks_to_per_cent_mille(reg_val);
	sprintf(str, "%0.1f ", relhumid/1000);
	write(fd_log, str, strlen(str));
	
	write(fd_log, "\n", 1);
	close(fd_log);
	close(fd_dev);
	return 0;
}

