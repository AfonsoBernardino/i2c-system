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
#include "func_reg.h"
#include "mcp23009.h"

#define BUS_NUM_LOW		0
#define BUS_NUM_HIGH	8
#define AD5694_ADDR_LOW 	0x0C
#define AD5694_ADDR_HIGH	0x0F
#define MCP23009_ADDR_LOW	0x20
#define MCP23009_ADDR_HIGH	0x27
#define Vref		4.53 			//(V)

static const float LSB = Vref/4096; // ( V/bit)

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
 
struct i2c_child_bus{
	char type[16];
	int bus_num;
	struct device *device_list;
};

__u16 vset_ilim_to_ad5694(float val){	//val can be: vset in kV or ilim in uA
	val = val/2; 						//division by 2 is to convert units
	return (__u16) (val/LSB + 0.5);		//float to int rounding conversion;
}

static void help(void){
	printf("Usage:\n"
"     tool -Vset (Ilim) VAL                   *write VAL to Vset (Ilim)*\n"
"     tool -on (-off)                         *turn HV on (off)*\n"
"     tool -v                                 *tool software version*\n"
"     tool -h                                 *help menu*\n");

}


int get_addr(int fd, int addr_low, int addr_high){
	int addri = -1;
	for(addri=addr_low; addri<=addr_high; addri++){
		if(ioctl(fd, I2C_SLAVE, addri) < 0) {
			if (errno == EBUSY) {
				continue;
			} else {
				fprintf(stderr, "Error: Could not set address to 0x%02x: %s\n",
															addri,
					 				                      	strerror(errno));
				return EXIT_FAILURE;
			}
		}

		if(i2c_smbus_write_quick(fd, I2C_SMBUS_WRITE) > -1)
			break;
	} 
	return addri;
}


int main(int argc, char *argv[]){

	int bus = -1;
	float vset = -1; float ilim = -1;
	int flag_vset = 0; int flag_ilim = 0; 
	int hv_on = 0; int hv_off = 0;
	//int version = 0; int hlp = 0; 
	int flags = 0;
	while (1+flags < argc && argv[1+flags][0] == '-') {
	    switch (argv[1+flags][1]) {
			//case 'h': hlp = 1; break;
			//case 'v': version = 1; break;
			case 'b': 
					flags++;
					bus = atoi(argv[1+flags]);
					break;
	        case 'V': 
					flags++;
					vset = atof(argv[1+flags]);
					flag_vset = 1;  
					break;
            case 'I': 
					flags++;
					ilim = atof(argv[1+flags]);
					flag_ilim = 1; 					
					break;
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

	if(bus < BUS_NUM_LOW || bus > BUS_NUM_HIGH){
		fprintf(stderr, "Error: Bad bus number \"%d\"\n", bus);
		return EXIT_FAILURE;
	}

	char filename[20];
	int fd;
   	snprintf(filename, 19, "/dev/i2c-%d", bus+1);
   	
	if((fd = open(filename, O_RDWR)) < 0){
		printf("Failed to open the bus (adapter); %s\n", strerror(errno));
		
		return EXIT_FAILURE;
	}
	int addr = 0;
	if(flag_vset){
		addr = get_addr(fd, AD5694_ADDR_LOW, AD5694_ADDR_HIGH);
		ad5694_write_ch(fd, addr, 0, vset_ilim_to_ad5694(vset));
	}

	if(flag_ilim){
		addr = get_addr(fd, AD5694_ADDR_LOW, AD5694_ADDR_HIGH);
		ad5694_write_ch(fd, addr, 1, vset_ilim_to_ad5694(ilim));
	}

	if(hv_on){
		addr = get_addr(fd, MCP23009_ADDR_LOW, MCP23009_ADDR_HIGH);
		mcp23009_write_val(fd, addr, MCP23009_REG_GPPU, 0x08);
		mcp23009_write_val(fd, addr, MCP23009_REG_IODIR, 0x17);
		mcp23009_write_val(fd, addr, MCP23009_REG_GPIO, 0x08);
	}
	else if(hv_off){
		addr = get_addr(fd, MCP23009_ADDR_LOW, MCP23009_ADDR_HIGH);
		mcp23009_write_val(fd, addr, MCP23009_REG_GPPU, 0x08);
		mcp23009_write_val(fd, addr, MCP23009_REG_IODIR, 0x17);
		mcp23009_write_val(fd, addr, MCP23009_REG_GPIO, 0x00);
	}

	if(!(flag_vset || flag_ilim || hv_on || hv_off)){ //Read all devices
		
		struct i2c_child_bus subsystem;

		subsystem.bus_num = bus;
		subsystem.device_list = hv_dev_list;
		int n; int logfile = 0; int log = 0;
		//char file_path [32] = "/home/hv/log/";
		
		for(n=0; subsystem.device_list[n].name[0]!='\0'; n++){
			int addri; int i=0;
			int low = subsystem.device_list[n].addr_low;
			int high = subsystem.device_list[n].addr_high; 
			for(addri=low; addri<=high; addri++){
				if(ioctl(fd, I2C_SLAVE, addri) < 0) {
				    if (errno == EBUSY) {
			        	//if(log){
						//	sprintf(str, "0 ");
						//	write(logfile, str, strlen(str));
						//}
						//else{
						printf("%s%d: 0\n", 
										subsystem.device_list[n].data_type[0], 
										i);
						i++;
					//}
						continue;
			        } else {
			            fprintf(stderr, "Error: Could not set "
	 			                                 	"address to 0x%02x: %s\n", 
													addri,
			     				                 	strerror(errno));
			            return -1;
			        }
				}

				if( i2c_smbus_write_quick(fd, I2C_SMBUS_WRITE) < 0)
					continue;

				subsystem.device_list[n].read_val(fd, addri, 
										   subsystem.device_list[n].val);
	
				subsystem.device_list[n].print_val(
									  	subsystem.device_list[n].val, 
									  	subsystem.device_list[n].lsb,
									  	subsystem.device_list[n].data_type,
									  	i, log, logfile);
				i++;	
			}
		}
	}

	return 0;
}
