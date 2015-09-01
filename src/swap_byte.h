#ifndef __SWAP_BYTE_H__
#define __SWAP_BYTE_H__
static inline __u16 SWAP_BYTE(int word){ //Swap high byte with low byte
	return ((word&0x00ff)<<8)|(word>>8); //(SMBus standard sends low byte first)
} 
#endif
