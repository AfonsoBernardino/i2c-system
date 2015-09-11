//Sensors
int tmp75_temp(int fd, int addr, __u16 *data);
void tmp75_print_val(__u16 val[8], float lsb, float conv_param[8], 
															char *data_type[8], 
															int i, int log, 
															int log_p);
int sht21_humid(int fd, int addr, __u16 *data);
void sht21_print_val(__u16 val[8], float lsb, float conv_param[8], 
															char *data_type[8], 
															int i, int log, 
															int log_p);

int mpl115_press(int fd, int addr, __u16 *data);
void mpl115_print_val(__u16 val[8], float lsb, float conv_param[8], 
															char *data_type[8], 
															int i, int log, 
															int log_p);
//HV
int ads7828_read_all(int fd, int addr, __u16 data[8]);
void ads7828_print_val(__u16 val[8], float lsb, float conv_param[8], 
															char *data_type[8], 
															int i, int log, 
															int log_p);

int ad5694_read_all(int fd, int addr, __u16 data[8]);
int ad5694_write_ch(int fd, int addr, __u8 ch, __u16 val);
void ad5694_print_val(__u16 val[8], float lsb, float conv_param[8], 
															char *data_type[8], 
															int i,int log, 
															int log_p);

int mcp23009_read_val2(int fd, int addr, __u16 data[8]);
int mcp23009_write_val(int fd, int addr, __u8 reg, __u8 val);
void mcp23009_print_val(__u16 val[8], float lsb, float conv_param[8], 
															char *data_type[8], 
															int i, int log, 
															int log_p);
