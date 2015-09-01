/*
*
*	tool.c	-	An application to: print or log sensors values
*								   print or log ADC values
*								   print or log IO values
*								   print DAC settings
*								   change DAC settings
*								   turn HV on/off 
*
* Need libi2c-dev, i2c-dev, i2c-mux, i2c-mux-pca954x, pca9547-overlay.dtb
* 
* Devices supported: 
*		Sensors:TMP75, SHT21, MPL115
*       ADC: 	ADS7828
*		DAC: 	AD5694
*		IO: 	MCP23009
*
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/i2c-dev.h>
#include "msleep.h"
#include "func_reg.h"
#include "mcp23009.h"

#define MODE_AUTO       0
#define MODE_QUICK      1
#define MODE_READ       2
#define MODE_FUNC       3

#define CONFIG_FILE_LINE_MAX 	20
#define SUBSYS_N_MAX 			8

struct device{
	char name[20];
	char *data_type[8];
	int addr_low;
	int addr_high;
	int (*read_val)(int, int, __u16[8]);
	void (*print_val)(__u16[8], float, char*[8], int, int, int);
	__u16 val[8];
	float lsb;
};

struct device hv_dev_list[4] = {
	{.name      = "ads7828",
	 .data_type = {"IHVp","IHVn","VHVn","VHVp","VHVs","Vpwr","Vset","Ilim"}, 
	 .addr_low  = 0x48,
	 .addr_high = 0x4b,
	 .read_val  = ads7828_read_all, 
	 .print_val = ads7828_print_val, 
	 .lsb = 2*4.53/4096, },
	{.name      = "ad5694",
	 .data_type = {"Vset","Ilim","DAC2","DAC3","DAC4","DAC5","DAC6","DAC7"},
	 .addr_low  = 0x0c,
	 .addr_high = 0x0f,
	 .read_val  = ad5694_read_all, 
	 .print_val = ad5694_print_val, 
	 .lsb = 2*4.53/4096, },
    {.name      = "mcp23009",
	 .data_type = {"D0  ","D1  ","D2  ","HVon","D4  ","D5  ","D6  ","D7  "},
	 .addr_low  = 0x20,
	 .addr_high = 0x27,
	 .read_val  = mcp23009_read_val2, 
	 .print_val = mcp23009_print_val, },
	{.name = ""}
};

struct device sensors_dev_list[4] = {
	{.name = "tmp75",
	 .data_type = {"TMP"},
	 .addr_low = 0x48,
	 .addr_high = 0x4f,
	 .read_val = tmp75_temp,
	 .print_val = tmp75_print_val, },
	{.name = "sht21",
	 .data_type = {"HMD"},
	 .addr_low = 0x40,		//single address
	 .addr_high = 0x40,
	 .read_val = sht21_humid,
	 .print_val = sht21_print_val,}, 
	{.name = "mpl115",
	 .data_type = {"PRS"},
	 .addr_low = 0x60,		//single address
	 .addr_high = 0x60,
	 .read_val = mpl115_press,
	 .print_val = mpl115_print_val,},
	{.name = ""}
};

struct i2c_child_bus{
	char type[16];
	int bus_num;
	struct device *device_list;
};

static void help(void){
	printf("Usage:\n"
"     tool -HV (or -sensors)                  *display HV (sensors) values*\n"
"     tool -l                                 *print all values to log*\n"
"     tool -l -HV (or -sensors)               *print respective values to log*\n"
"     tool -Vset (Ilim) VAL                   *write VAL to Vset (Ilim)*\n"
"     tool -on (-off)                         *turn HV on (off)*\n"
"     tool -v                                 *tool software version*\n"
"     tool -h                                 *help menu*\n");

}

int setup_mux_child_bus(int ch){

	char filename[20];
	int fd;
	if(ch<0 || ch>8){
		printf("Error: wrong mux child bus number");
		return EXIT_FAILURE;
	}

   	snprintf(filename, 19, "/dev/i2c-%d", ch+1);
   	
	if((fd = open(filename, O_RDWR)) < 0){
		printf("Failed to open the bus (adapter); %s\n", strerror(errno));
		
		return EXIT_FAILURE;
	}
	return fd;
}
/*
***************DAC******************
*
*
*/
#define Vref		4.53 			//(V)
static const float LSB = Vref/4096; // ( V/bit)
 
__u16 vset_ilim_to_ad5694(float val){	//val can be: vset in kV or ilim in uA
	val = val/2; 						//division by 2 is to convert units
	return (__u16) (val/LSB + 0.5);		//float to int rounding conversion;
}
/**********************************
*                                 *
*          MAIN                   *
*                                 *
**********************************/
int main(int argc, char *argv[]){
	int flags=0;	int hlp = 0;	   int version = 0;
	int log = 0;    int hv = 0;        int sensors = 0;
	int dac = 0;    int hv_on = 0;     int hv_off = 0;
	int dac_ch = 0; float dac_val = 0; int vset = 0; int ilim = 0 ;


	while (1+flags < argc && argv[1+flags][0] == '-') {

	    switch (argv[1+flags][1]) {
			case 'h': hlp = 1; break;
			case 'H': hv = 1; break;
			case 's': sensors = 1; break;
			case 'S': sensors = 1; break;
			case 'v': version = 1; break;
	        case 'V': 
					dac = 1;
					dac_ch = 0;
					dac_val = atof(argv[2+flags]); 
					break;
            case 'I': 
					dac = 1;
					dac_ch = 1;
					dac_val = atof(argv[2+flags]);
					break;
            case 'l': log = 1; break;
            case 'o':
                    if ( !strcasecmp(argv[1+flags], "-on") ){
						hv_on = 1; break;
					}
					if ( !strcasecmp(argv[1+flags], "-off") ){
						hv_off = 1; break;
					}
					else{
						help();
						return EXIT_FAILURE;	
					}
            default:
                    fprintf(stderr, "Error: Unsupported option "
                            "\"%s\"!\n", argv[1+flags]);
                    help();
                    return EXIT_FAILURE;
     		}
			flags++;	
	}
	if(hlp){
		help();
		return 0;
	}
	if(version){
		fprintf(stdout, "tool version 2.1\n");
		return 0;
	}
	FILE *fp_conf;
			
	fp_conf = fopen("/home/hv/network.conf", "r");

	int c;
	char line[CONFIG_FILE_LINE_MAX];
	struct i2c_child_bus subsystem[SUBSYS_N_MAX];
	int pos = 0; int subsys_n=0;
	
	while((c = getc(fp_conf)) != EOF){
		if(c == '#'){
			while((c = fgetc(fp_conf)) != '\n');
		} else if(c == '\n' && pos == 0){
			continue;
		} else if(c==' ' || c=='\t'){
			continue;
		} else if(pos >= CONFIG_FILE_LINE_MAX){
			line[CONFIG_FILE_LINE_MAX-1] = '\0';
			pos = 0;
			fprintf(stderr, "Error in network.conf file\n");
			return EXIT_FAILURE;
		} else if(c == '\n' && pos != 0){
			line[pos] = '\0';
			//printf("%s\n", line);

			char *token_frst; char *token_scnd; char tmp_type[16];
			token_frst = strtok(line, "=");
			token_scnd = strtok(NULL, " ");
			struct device *tmp_dev_list;
			int tmp_bus_num = token_frst[3] - 0x30;
			if(tmp_bus_num<0 || tmp_bus_num>8){
				fprintf(stderr, "Error in network.conf: %s not an bus\n", 
																	token_frst);
				return EXIT_FAILURE;
			}
			if(!strcasecmp(token_scnd, "hv")){
				tmp_dev_list = hv_dev_list;
				strcpy(tmp_type, "hv");
			}
			else if(!strcasecmp(token_scnd, "sensors")){
				tmp_dev_list = sensors_dev_list;
				strcpy(tmp_type, "sensors");
			}
			else{
				fprintf(stderr, "Error in network.conf: %s not a bus option\n", 
																	token_scnd);
				return EXIT_FAILURE;
			}
			if(subsys_n>=SUBSYS_N_MAX){
				fprintf(stderr, "Error in network.conf: too much busses");
				return EXIT_FAILURE;
			}
			strcpy(subsystem[subsys_n].type, tmp_type);
			subsystem[subsys_n].bus_num = tmp_bus_num;
			subsystem[subsys_n].device_list = tmp_dev_list;

			pos = 0;
			subsys_n++;
			line[pos] = '\0';
		} else{
			line[pos] = c;
  			pos++;
		}
	}

	subsystem[subsys_n].bus_num = -1; 
//	int sb; int sbb;
//	for(sb=0; subsystem[sb].bus_num !=-1; sb++){
//		printf("%d\n", subsystem[sb].bus_num);
//		for(sbb=0; subsystem[sb].device_list[sbb].name[0] !='\0'; sbb++){
//			printf("%s\n", subsystem[sb].device_list[sbb].name);
//		}
//	}	

   	int fd_dev;
	int res;
	char str[20];
	int logfile;

	if(log){
		time_t rawtime;
	  	struct tm *info;
		char date[20];
		char hour[16];
		char file_path [32] = "/home/hv/log/";
	   	time( &rawtime );
		info = localtime( &rawtime );

	   	if(hv){
			sprintf(date, "hv%04d-%02d-%02d.log", info->tm_year+1900,
	   											  info->tm_mon+1,
	   											  info->tm_mday);
		}
		else if(sensors){
			sprintf(date, "sensors%04d-%02d-%02d.log", info->tm_year+1900,
	   												   info->tm_mon+1,
	   												   info->tm_mday);
		}
		else{
			sprintf(date, "%04d-%02d-%02d.log", info->tm_year+1900,
	   												   info->tm_mon+1,
	   												   info->tm_mday);
		}

//		sprintf(date, "%04d-%02d-%02d.log", info->tm_year+1900,
//	   										info->tm_mon+1,
//	   										info->tm_mday);

	   	strcat(file_path, date);								
	   	logfile = open(file_path, O_WRONLY|O_APPEND|O_CREAT);
	   	fchmod(logfile, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	   		
	   	sprintf(hour, "[%02d:%02d:%02d] ", info->tm_hour, 
	   									   info->tm_min, 
	   									   info->tm_sec);
	
		write(logfile, hour, 11);
	}

	int m; int n;
	for(m=0; subsystem[m].bus_num != -1; m++){
		if(hv){
			if(strcmp(subsystem[m].type, "hv")) //increment m until reach hv
					continue;
			}
		if(sensors){
			if(strcmp(subsystem[m].type, "sensors"))
					continue;
			}
		fd_dev = setup_mux_child_bus(subsystem[m].bus_num);
		for(n=0; subsystem[m].device_list[n].name[0]!='\0'; n++){
			//printf("node min: %x max: %x \n", i2c_nodes_list[m].dev.addr_low,
			//									i2c_nodes_list[m].dev.addr_high);
			if(dac){
				if(strcmp(subsystem[m].device_list[n].name,"ad5694"))
					continue;
			}
			else if(hv_on || hv_off){
				if(strcmp(subsystem[m].device_list[n].name,"mcp23009"))
					continue;
			}

			//__u8 addr_list[8];
			//res = scan_i2c_bus(fd_dev, MODE_AUTO, funcs, 
			//										i2c_nodes_list[m].dev.addr_low,
			//										i2c_nodes_list[m].dev.addr_high,
			//										addr_list);

			float data; int addri; int i=0;
			int low = subsystem[m].device_list[n].addr_low;
			int high = subsystem[m].device_list[n].addr_high; 
			int last_ch=-1;
			for(addri=low; addri<=high; addri++){
				if(ioctl(fd_dev, I2C_SLAVE, addri) < 0) {
				    if (errno == EBUSY) {
			        	if(log){
							sprintf(str, "0 ");
							write(logfile, str, strlen(str));
						}
						else
							printf("%s%d: 0\n", subsystem[m].device_list[n].data_type, addri);
						continue;
			        } else {
			            fprintf(stderr, "Error: Could not set "
	 			                                 	"address to 0x%02x: %s\n", addri,
			     				                               	strerror(errno));
			            return -1;
			        }
				}

				if( i2c_smbus_write_quick(fd_dev, I2C_SMBUS_WRITE) < 0)
					continue;

				if(dac){
					ad5694_write_ch(fd_dev, addri, dac_ch, 
													  vset_ilim_to_ad5694(dac_val));
				}
				else if(hv_on){
						mcp23009_write_val(fd_dev, addri, MCP23009_REG_GPPU, 0x08);
						mcp23009_write_val(fd_dev, addri, MCP23009_REG_IODIR, 0x17);
						mcp23009_write_val(fd_dev, addri, MCP23009_REG_GPIO, 0x08);
				}
				else if(hv_off){
					mcp23009_write_val(fd_dev, addri, MCP23009_REG_GPPU, 0x08);
					mcp23009_write_val(fd_dev, addri, MCP23009_REG_IODIR, 0x17);
					mcp23009_write_val(fd_dev, addri, MCP23009_REG_GPIO, 0x00);
				}
				else{
					subsystem[m].device_list[n].read_val(fd_dev, addri, 
														subsystem[m].device_list[n].val);
					//printf("%s%d: %0.3f\n", i2c_nodes_list[m].dev.data_type, 
					//													   i, data);
		
					subsystem[m].device_list[n].print_val(subsystem[m].device_list[n].val, 
													subsystem[m].device_list[n].lsb,
													subsystem[m].device_list[n].data_type,
													i, log, logfile);
					i++;
				}	
			}
		
		}
	}
	
	fclose(fp_conf);
	close(fd_dev);
	if(log){
		write(logfile, "\n", 1);
		close(logfile);
	}
	
	return res;
}
/*

#define Vref		4.53 			//(V)

static const float LSB = Vref/4096; // ( V/bit)
 
__u16 vset_ilim_to_ad5694(float val){	//val can be: vset in kV or ilim in uA
	val = val/2; 						//division by 2 is to convert units
	return (__u16) (val/LSB + 0.5);		//float to int rounding conversion;
}

float ad5694_to_vset_ilim(__u16 val){
	return val*2*LSB;				  	//multiplication by 2 to convert units
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
*/
/*
*
*	Main	
*
*/
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
	
	open_bus(fd, 7);
	
	//Config IO
	int check; 
	check = mcp23009_write_val(fd, 0x20, MCP23009_REG_GPPU, 0x08);
	check = mcp23009_write_val(fd, 0x20, MCP23009_REG_IODIR, 0x17);
	
	int dac_data; int help = 0;
    if(argc > 1 && argc < 5){
		if(!strcasecmp("DAC", argv[1])){
		
		 	if(fd == -1){
				printf("Can't setup the I2C device\n");	
			}
			else if(argc == 2){
				dac_data = ad5694_read_ch(fd, 0x0d, 0);
				printf("DAC_0 = %0.1f kV\n", ad5694_to_vset_ilim(dac_data));
				dac_data = ad5694_read_ch(fd, 0x0d, 1);
				printf("DAC_1 = %0.1f uA\n", ad5694_to_vset_ilim(dac_data));
				//uncomment to debug 
				//dac_data = readDAC(fd, "B");
				//printf("DAC_B = %X\n", dac_data);
			}
			else{//write to DAC channel
				float val = atof(argv[3]);
				if(val >= 0 && val <= 10 ){
					ad5694_write_ch(fd, 0x0d, atoi(argv[2]), vset_ilim_to_ad5694(val));//to debug use atoi()
				}
				else{help = 1;}
			}
		}
		else if(!strcasecmp("ADC", argv[1])){
			if(fd == -1){
				printf("Can't setup the I2C device\n");	
			}
			else{
				int adc_data = ads7828_read_ch(fd, 0x4a, 0);
				printf("IHVp = %0.1f uA\n", 0, adc_data*2*LSB);
				adc_data = ads7828_read_ch(fd, 0x4a, 1);
				printf("IHVn = %0.1f uA\n", 1, adc_data*2*LSB);
				adc_data = ads7828_read_ch(fd, 0x4a, 2);
				printf("VHVn = %0.1f kV\n", 2, adc_data*2*LSB);
				adc_data = ads7828_read_ch(fd, 0x4a, 3);
				printf("VHVp = %0.1f kV\n", 3, adc_data*2*LSB);
				adc_data = ads7828_read_ch(fd, 0x4a, 4);
				printf("VHVs = %0.3f V\n", 4, adc_data*2000*LSB);
				adc_data = ads7828_read_ch(fd, 0x4a, 5);
				printf("Vpwr = %0.1f kV\n", 5, adc_data*2*LSB);
				adc_data = ads7828_read_ch(fd, 0x4a, 6);
				printf("Vset = %0.1f kV\n", 6, adc_data*2*LSB);
				adc_data = ads7828_read_ch(fd, 0x4a, 7);
				printf("Ilim = %0.1f uA\n", 7, adc_data*2*LSB);
			}
		}
		else if(!strcasecmp("IO", argv[1])){
			if(argc == 2){
				int data = mcp23009_read_val(fd, 0x20, MCP23009_REG_GPIO);
				int bit;
				for(bit = 0; bit < 5; bit++){
					bit==3 ? printf("HVon: %d\n",data&(1<<bit)?1:0):
					      	 printf("D%d: %d\n", bit, data&(1<<bit)?1:0);
				}

			}
			else{help = 1;}
		}
		else if(!strcasecmp("ON", argv[1])){
			check = mcp23009_write_val(fd, 0x20, MCP23009_REG_GPIO, 0x08);
		}
		else if(!strcasecmp("OFF", argv[1])){
			check = mcp23009_write_val(fd, 0x20, MCP23009_REG_GPIO, 0x00);
		}
		else{help = 1;}
	}
	else{help = 1;}
	
	if(help){
		printf("Need help?\n");
		printf("Options:\n");
		printf(" ADC\t\t*Read all ADC channels\n");
		printf(" DAC\t\t*Read all DAC output values\n");
		printf(" IO\t\t*Read all IO output values\n");
		printf(" DAC A x\t*Set channel A output value x Volts, 0 V <= x <=4,53 V\n");
		printf(" DAC B x\t*Set channel B output value x Volts, 0 V <= x <=4,53 V\n");
		printf(" ON\t\t*Turn HV on\n");
		printf(" OFF\t\t*Turn HV off\n");
	}
	if(close(fd) < 0){
		printf("Failed to the close bus; %s" ,strerror(errno));
	}
	return 0;
}
*/
