#ifndef	K7_CMD_H_
#define	K7_CMD_H_

	
	
	void RamWrite32A(unsigned short, unsigned long);
	void RamWrite32(unsigned char, unsigned char, unsigned short, unsigned short);
	   
	void RamWriteA(unsigned short, unsigned short);
	void RamWriteA_Ex(uint32_t, uint8_t *, uint8_t);	
	   
	void RamWrite(unsigned char, unsigned char, unsigned char, unsigned char);

	void RamWriteSensorA(unsigned short, unsigned short);
	void RamWriteSensor(unsigned char, unsigned char, unsigned char, unsigned char);
	
	void RegWriteA(unsigned short, unsigned char);
	void RegWrite(unsigned char, unsigned char, unsigned char);
	
	void RamRead32( unsigned char, unsigned char, unsigned long * ) ;
	void RamRead32A( unsigned short RamAddr, void * ReadData );
	void RamReadA( unsigned short, void * ) ;
	void RamRead( unsigned char, unsigned char, unsigned short * ) ;

	void RamReadSensorA( unsigned short, void * ) ;
	void RamReadSensor( unsigned char, unsigned char, unsigned short * ) ;

	void RegReadA(unsigned short, unsigned char *);
	void RegRead(unsigned char, unsigned char, unsigned char *);

	void E2PRegWriteA(unsigned short, unsigned char);
	void E2PRegWrite(unsigned char, unsigned char, unsigned char);
	
	void E2PRegReadA(unsigned short,  unsigned char *);
	void E2PRegRead(unsigned char, unsigned char, unsigned char *);

	void E2pRed( unsigned short, unsigned char, unsigned char * ) ;	// E2P ROM Data Read
	void E2pWrt( unsigned short, unsigned char, unsigned char * ) ;	// E2P ROM Data Write
#endif
/* _CMD_H_ */