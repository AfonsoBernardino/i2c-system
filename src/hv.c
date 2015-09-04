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


int get_addr(int fd, int addr_low, int addr_high, int *addr_buf){
	int addri;
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

		if(i2c_smbus_write_quick(fd, I2C_SMBUS_WRITE) < 0)
			continue;
	}
	*addr_buf = addri;
	return 0;
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

	if(flag_vset){
		int addr = 0;
		get_addr(fd, AD5694_ADDR_LOW, AD5694_ADDR_HIGH, &addr);
		ad5694_write_ch(fd, addr, 0, vset_ilim_to_ad5694(vset));
	}

	if(flag_ilim){
		int addr = 0;
		get_addr(fd, AD5694_ADDR_LOW, AD5694_ADDR_HIGH, &addr);
		ad5694_write_ch(fd, addr, 1, vset_ilim_to_ad5694(ilim));
	}

	if(hv_on){
		int addr = 0;
		get_addr(fd, MCP23009_ADDR_LOW, MCP23009_ADDR_HIGH, &addr);
		mcp23009_write_val(fd, addr, MCP23009_REG_GPPU, 0x08);
		mcp23009_write_val(fd, addr, MCP23009_REG_IODIR, 0x17);
		mcp23009_write_val(fd, addr, MCP23009_REG_GPIO, 0x08);
	}
	else if(hv_off){
		int addr = 0;
		get_addr(fd, MCP23009_ADDR_LOW, MCP23009_ADDR_HIGH, &addr);
		mcp23009_write_val(fd, addr, MCP23009_REG_GPPU, 0x08);
		mcp23009_write_val(fd, addr, MCP23009_REG_IODIR, 0x17);
		mcp23009_write_val(fd, addr, MCP23009_REG_GPIO, 0x00);
	}

	return 0;
}
