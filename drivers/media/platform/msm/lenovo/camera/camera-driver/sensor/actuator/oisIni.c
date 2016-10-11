//********************************************************************************
//
//		<< LC898122 Evaluation Soft >>
//		Program Name	: OisIni.c
//		Design			: Y.Yamada
//		History			: LC898122 						2013.01.09 Y.Shigeoka
//********************************************************************************
//**************************
//	Include Header File		
//**************************
#define		OISINI
#define		OISCMD

//#include	"Main.h"
//#include	"Cmd.h"
#include	"ois.h"
#include	"oisFil.h"
//#include	"OisDef.h"
#ifdef	MODULE_CALIBRATION
#else	//	MODULE_CALIBRATION
#include "../../msm_sd.h"
#include "../eeprom/msm_eeprom.h"

#include	"onsemi_cmd.h"
#include	"onsemi_ois.h"
//#include	<math.h>
#include <linux/delay.h>
#undef	MODULE_CALIBRATION
#endif	//MODULE_CALIBRATION


#ifdef H1COEF_CHANGER
  #define		MAXLMT_W		0x40400000				// 3.0
  #define		MINLMT_W		0x4019999A				// 2.4
  #define		CHGCOEF_W		0xBA855555				// 
  #define		MINLMT_MOV_W	0x4019999A				// 2.4
  #define		CHGCOEF_MOV_W	0xBA200000

  #define		MAXLMT_PRIMAX		0x40000000				// 2.0
  #define		MINLMT_PRIMAX			0x3F99999A				// 1.2
  #define		CHGCOEF_PRIMAX			0xBA480000				// 
  #define		MINLMT_MOV_PRIMAX		0x3F99999A				// 1.2
  #define		CHGCOEF_MOV_PRIMAX		0xB9F00000
#endif

//**************************
//	Const					
//**************************
// gxzoom Setting Value
#define		ZOOMTBL	16
const unsigned long	ClGyxZom[ ZOOMTBL ]	= {
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000
	} ;

// gyzoom Setting Value
const unsigned long	ClGyyZom[ ZOOMTBL ]	= {
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000
	} ;


//**************************
//	Local Function Prottype	
//**************************
void	IniClkPrmx( void ) ;		// Clock Setting
void	IniIopPrmx( void ) ;		// I/O Port Initial Setting
void	IniMonPrmx( void ) ;		// Monitor & Other Initial Setting
void	IniSrvPrmx( void ) ;		// Servo Register Initial Setting
void	IniGyrPrmx( void ) ;		// Gyro Filter Register Initial Setting
void	IniFilPrmx( void ) ;		// Gyro Filter Initial Parameter Setting
void	IniAdjPrmx( void ) ;		// Adjust Fix Value Setting
void	IniCmdPrmx( void ) ;		// Command Execute Process Initial
void	IniDgyPrmx( void ) ;		// Digital Gyro Initial Setting
void	IniAfPrmx( void ) ;			// Open AF Initial Setting
void	IniPtAvePrmx( void ) ;		// Average setting
void	AutoGainContIniPrmx( void ) ;		// Auto Gain control initial setting
#ifndef	MODULE_CALIBRATION
//void	E2pDat_PRMX( void );
#endif	//ifndef	MODULE_CALIBRATION

//********************************************************************************
// Top Function
//********************************************************************************
int			IniSetTop( void )
{
         int rc = 0;
	RegReadA( CVER ,	&UcCvrCod_PRMX );		// 0x027E
	pr_err("%s: %d: UcCvrCod_PRMX = %x \n", __func__, __LINE__,UcCvrCod_PRMX);
	if(UcCvrCod_PRMX == CVER122_PRMX){		// Primax
		IniSetAll();
		SetPanTiltMode_PRMX(ON);
	}else{							// LGIT
		rc = IniSet();
	}

	return rc;
}

unsigned char	RtnCenTop( unsigned char	UcCmdPar )
{
	unsigned char	UcCmdSts ;
	pr_err("%s: %d: UcCvrCod_PRMX = %x \n", __func__, __LINE__,UcCvrCod_PRMX);
	if(UcCvrCod_PRMX == CVER122_PRMX){		// Primax
		UcCmdSts = RtnCen_PRMX( UcCmdPar );
	}else{							// LGIT
		UcCmdSts = RtnCen( UcCmdPar );
	}
	
	return( UcCmdSts ) ;

}

void	OisEnaTop( void )
{
	pr_err("%s: %d: UcCvrCod_PRMX = %x \n", __func__, __LINE__,UcCvrCod_PRMX);
	if(UcCvrCod_PRMX == CVER122_PRMX){		// Primax
		OisEna_PRMX();
	}else{							// LGIT
		OisEna();
	}
}

void	OisOffTop( void )
{
	pr_err("%s: %d: UcCvrCod_PRMX = %x \n", __func__, __LINE__,UcCvrCod_PRMX);
	if(UcCvrCod_PRMX == CVER122_PRMX){		// Primax
		GyrCon_PRMX( OFF );
	}else{							// LGIT
		OisOff();
	}
}

void	SetH1cModTop( unsigned char	UcSetNum )
{
	pr_err("%s: %d: UcCvrCod_PRMX = %x \n", __func__, __LINE__,UcCvrCod_PRMX);
	if(UcCvrCod_PRMX == CVER122_PRMX){		// Primax
		SetH1cMod_PRMX( UcSetNum );
	}else{							// LGIT
		SetH1cMod( UcSetNum ) ;
	}
}
//********************************************************************************


//********************************************************************************
// Function Name 	: IniSetAll
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Initial Setting Function
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	IniSetAll( void )
{
	// Command Execute Process Initial
//		IniCmdPrmx() ;
	// I/O Port Initial Setting
		IniClkPrmx() ;
		pr_err("%s: %d:  add for Synchronous Abort handler before \n", __func__, __LINE__);
	// AF Initial Setting
		IniAfPrmx() ;
		pr_err("%s: %d:  add for Synchronous Abort handler after \n", __func__, __LINE__);

#ifdef	MODULE_CALIBRATION
#else
//		E2pDat_PRMX() ;	//
#endif

	// I/O Port Initial Setting
		IniIopPrmx() ;
	// DigitalGyro Initial Setting
		IniDgyPrmx() ;
	// Monitor & Other Initial Setting
		IniMonPrmx() ;
	// Servo Initial Setting
		IniSrvPrmx() ;
	// Gyro Filter Initial Setting
		IniGyrPrmx() ;
	// Gyro Filter Initial Setting
		IniFilPrmx() ;
	// Adjust Fix Value Setting
		IniAdjPrmx() ;

		SetPanTiltMode_PRMX(ON) ;

}
//********************************************************************************
// Function Name 	: IniSetPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Initial Setting Function
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	IniSetPrmx( void )
{
	// Command Execute Process Initial
//	IniCmdPrmx() ;
	// Clock Setting
	IniClkPrmx() ;
	// I/O Port Initial Setting
	IniIopPrmx() ;
	// DigitalGyro Initial Setting
	IniDgyPrmx() ;
	// Monitor & Other Initial Setting
	IniMonPrmx() ;
	// Servo Initial Setting
	IniSrvPrmx() ;
	// Gyro Filter Initial Setting
	IniGyrPrmx() ;
	// Gyro Filter Initial Setting
	IniFilPrmx() ;
	// Adjust Fix Value Setting
	IniAdjPrmx() ;

}

//********************************************************************************
// Function Name 	: IniSetAfPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Initial AF Setting Function
// History			: First edition 						2013.09.12 Y.Shigeoka
//********************************************************************************
void	IniSetAfPrmx( void )
{
	// Command Execute Process Initial
//	IniCmdPrmx() ;
	// Clock Setting
	IniClkPrmx() ;
	// AF Initial Setting
	IniAfPrmx() ;

}



//********************************************************************************
// Function Name 	: IniClkPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Clock Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void	IniClkPrmx( void )
{
	ChkCvr_PRMX() ;								/* Read Cver */
	
	/*OSC Enables*/
	UcOscAdjFlg	= 0 ;					// Osc adj flag 
	
#ifdef	DEF_SET
	/*OSC ENABLE*/
	RegWriteA( OSCSTOP,		0x00 ) ;			// 0x0256
	RegWriteA( OSCSET,		0x90 ) ;			// 0x0257	OSC ini
	RegWriteA( OSCCNTEN,	0x00 ) ;			// 0x0258	OSC Cnt disable
#endif
	/*Clock Enables*/
	RegWriteA( CLKON,		0x1F ) ;			// 0x020B

#ifdef	DEF_SET
	RegWriteA( CLKSEL,		0x00 ) ;			// 0x020C	
	RegWriteA( PWMDIV,		0x00 ) ;			// 0x0210	48MHz/1
	RegWriteA( SRVDIV,		0x00 ) ;			// 0x0211	48MHz/1
	RegWriteA( GIFDIV,		0x03 ) ;			// 0x0212	48MHz/3 = 16MHz
	RegWriteA( AFPWMDIV,	0x02 ) ;			// 0x0213	48MHz/2 = 24MHz
#endif
	RegWriteA( OPAFDIV,		OPAFDIV_AF_PRMX ) ;		// 0x0214	48MHz/DIV
}



//********************************************************************************
// Function Name 	: IniIopPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: I/O Port Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void	IniIopPrmx( void )
{
#ifdef	DEF_SET
	/*set IOP direction*/
	RegWriteA( P0LEV,		0x00 ) ;	// 0x0220	[ - 	| - 	| WLEV5 | WLEV4 ][ WLEV3 | WLEV2 | WLEV1 | WLEV0 ]
	RegWriteA( P0DIR,		0x00 ) ;	// 0x0221	[ - 	| - 	| DIR5	| DIR4	][ DIR3  | DIR2  | DIR1  | DIR0  ]
	/*set pull up/down*/
	RegWriteA( P0PON,		0x0F ) ;	// 0x0222	[ -    | -	  | PON5 | PON4 ][ PON3  | PON2 | PON1 | PON0 ]
	RegWriteA( P0PUD,		0x0F ) ;	// 0x0223	[ -    | -	  | PUD5 | PUD4 ][ PUD3  | PUD2 | PUD1 | PUD0 ]
#endif
	/*select IOP signal*/
#ifdef	USE_3WIRE_DGYRO
	RegWriteA( IOP1SEL,		0x02 ); 	// 0x0231	IOP1 : IOP1
#else
	RegWriteA( IOP1SEL,		0x00 ); 	// 0x0231	IOP1 : DGDATAIN (ATT:0236h[0]=1)
#endif
#ifdef	DEF_SET
	RegWriteA( IOP0SEL,		0x02 ); 	// 0x0230	IOP0 : IOP0
	RegWriteA( IOP2SEL,		0x02 ); 	// 0x0232	IOP2 : IOP2
	RegWriteA( IOP3SEL,		0x00 ); 	// 0x0233	IOP3 : DGDATAOUT
	RegWriteA( IOP4SEL,		0x00 ); 	// 0x0234	IOP4 : DGSCLK
	RegWriteA( IOP5SEL,		0x00 ); 	// 0x0235	IOP5 : DGSSB
	RegWriteA( DGINSEL,		0x00 ); 	// 0x0236	DGDATAIN 0:IOP1 1:IOP2
	RegWriteA( I2CSEL,		0x00 );		// 0x0248	I2C noise reduction ON
	RegWriteA( DLMODE,		0x00 );		// 0x0249	Download OFF
#endif
	
}

//********************************************************************************
// Function Name 	: IniDgyPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Digital Gyro Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void	IniDgyPrmx( void )
{
 #ifdef USE_INVENSENSE
	unsigned char	UcGrini ;
 #endif
	
	/*************/
	/*For ST gyro*/
	/*************/
	
	/*Set SPI Type*/
 #ifdef USE_3WIRE_DGYRO
	RegWriteA( SPIM 	, 0x00 );							// 0x028F 	[ - | - | - | - ][ - | - | - | DGSPI4 ]
 #else
	RegWriteA( SPIM 	, 0x01 );							// 0x028F 	[ - | - | - | - ][ - | - | - | DGSPI4 ]
 #endif
															//				DGSPI4	0: 3-wire SPI, 1: 4-wire SPI

	/*Set to Command Mode*/
	RegWriteA( GRSEL	, 0x01 );							// 0x0280	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]

	/*Digital Gyro Read settings*/
	RegWriteA( GRINI	, 0x80 );							// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]

 #ifdef USE_INVENSENSE

	RegReadA( GRINI	, &UcGrini );					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
	RegWriteA( GRINI, ( UcGrini | SLOWMODE) );		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
	
	RegWriteA( GRADR0,	0x6A ) ;					// 0x0283	Set I2C_DIS
	RegWriteA( GSETDT,	0x10 ) ;					// 0x028A	Set Write Data
	RegWriteA( GRACC,	0x10 ) ;					/* 0x0282	Set Trigger ON				*/
	AccWit_PRMX( 0x10 ) ;								/* Digital Gyro busy wait 				*/

	RegWriteA( GRADR0,	0x1B ) ;					// 0x0283	Set GYRO_CONFIG
	RegWriteA( GSETDT,	( FS_SEL_PRMX << 3) ) ;		// 0x028A	Set Write Data
	RegWriteA( GRACC,	0x10 ) ;					/* 0x0282	Set Trigger ON				*/
	AccWit_PRMX( 0x10 ) ;								/* Digital Gyro busy wait 				*/

	RegReadA( GRINI	, &UcGrini );					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
	RegWriteA( GRINI, ( UcGrini & ~SLOWMODE) );		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]

 #endif
	
	RegWriteA( RDSEL,	0x7C ) ;				// 0x028B	RDSEL(Data1 and 2 for continuos mode)
	
	GyOutSignal_PRMX() ;
	

}


//********************************************************************************
// Function Name 	: IniMonPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Monitor & Other Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void	IniMonPrmx( void )
{
#ifndef	MONITOR_OFF	
	RegWriteA( PWMMONA, 0x00 ) ;				// 0x0030	0:off
	
	RegWriteA( MONSELA, 0x5C ) ;				// 0x0270	DLYMON1
	RegWriteA( MONSELB, 0x5D ) ;				// 0x0271	DLYMON2
	RegWriteA( MONSELC, 0x00 ) ;				// 0x0272	
	RegWriteA( MONSELD, 0x00 ) ;				// 0x0273	

	// Monitor Circuit
	RegWriteA( WC_PINMON1,	0x00 ) ;			// 0x01C0		Filter Monitor
	RegWriteA( WC_PINMON2,	0x00 ) ;			// 0x01C1		
	RegWriteA( WC_PINMON3,	0x00 ) ;			// 0x01C2		
	RegWriteA( WC_PINMON4,	0x00 ) ;			// 0x01C3		
	/* Delay Monitor */
	RegWriteA( WC_DLYMON11,	0x04 ) ;			// 0x01C5		DlyMonAdd1[10:8]
	RegWriteA( WC_DLYMON10,	0x40 ) ;			// 0x01C4		DlyMonAdd1[ 7:0]
	RegWriteA( WC_DLYMON21,	0x04 ) ;			// 0x01C7		DlyMonAdd2[10:8]
	RegWriteA( WC_DLYMON20,	0xC0 ) ;			// 0x01C6		DlyMonAdd2[ 7:0]
	RegWriteA( WC_DLYMON31,	0x00 ) ;			// 0x01C9		DlyMonAdd3[10:8]
	RegWriteA( WC_DLYMON30,	0x00 ) ;			// 0x01C8		DlyMonAdd3[ 7:0]
	RegWriteA( WC_DLYMON41,	0x00 ) ;			// 0x01CB		DlyMonAdd4[10:8]
	RegWriteA( WC_DLYMON40,	0x00 ) ;			// 0x01CA		DlyMonAdd4[ 7:0]

/* Monitor */
	RegWriteA( PWMMONA, 0x80 ) ;				// 0x0030	1:on 
//	RegWriteA( IOP0SEL,		0x01 ); 			// 0x0230	IOP0 : MONA
/**/
#endif


}

//********************************************************************************
// Function Name 	: IniSrvPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Servo Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void	IniSrvPrmx( void )
{
	unsigned char	UcStbb0 ;

	UcPwmMod_PRMX = INIT_PWMMODE_PRMX ;					// Driver output mode

	RegWriteA( WC_EQON,		0x00 ) ;				// 0x0101		Filter Calcu
	RegWriteA( WC_RAMINITON,0x00 ) ;				// 0x0102		
	ClrGyr_PRMX( 0x0000 , CLR_ALL_RAM_PRMX );					// All Clear
	
	RegWriteA( WH_EQSWX,	0x02 ) ;				// 0x0170		[ - | - | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
	RegWriteA( WH_EQSWY,	0x02 ) ;				// 0x0171		[ - | - | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
	
	RamAccFixMod_PRMX( OFF ) ;							// 32bit Float mode
#ifndef	MONITOR_OFF	
	/* Monitor Gain */
	RamWrite32A( dm1g, 0x3F800000 ) ;			// 0x109A
	RamWrite32A( dm2g, 0x3F800000 ) ;			// 0x109B
	RamWrite32A( dm3g, 0x3F800000 ) ;			// 0x119A
	RamWrite32A( dm4g, 0x3F800000 ) ;			// 0x119B
#endif	
	/* Hall output limitter */
	RamWrite32A( sxlmta1,   0x3F800000 ) ;			// 0x10E6		Hall X output Limit
	RamWrite32A( sylmta1,   0x3F800000 ) ;			// 0x11E6		Hall Y output Limit
	
	/* Emargency Stop */
	RegWriteA( WH_EMGSTPON,	0x00 ) ;				// 0x0178		Emargency Stop OFF
	RegWriteA( WH_EMGSTPTMR,0xFF ) ;				// 0x017A		255*(16/23.4375kHz)=174ms
	
	RamWrite32A( sxemglev,   0x3F800000 ) ;			// 0x10EC		Hall X Emargency threshold
	RamWrite32A( syemglev,   0x3F800000 ) ;			// 0x11EC		Hall Y Emargency threshold
	
	/* Hall Servo smoothing */
	RegWriteA( WH_SMTSRVON,	0x00 ) ;				// 0x017C		Smooth Servo OFF
	RegWriteA( WH_SMTSRVSMP,0x06 ) ;				// 0x017D		2.7ms=2^06/23.4375kHz
	RegWriteA( WH_SMTTMR,	0x0F ) ;				// 0x017E		10ms=(15+1)*16/23.4375kHz
	
	RamWrite32A( sxsmtav,   0xBC800000 ) ;			// 0x10ED		1/64 X smoothing ave coefficient
	RamWrite32A( sysmtav,   0xBC800000 ) ;			// 0x11ED		1/64 Y smoothing ave coefficient
	RamWrite32A( sxsmtstp,  0x3AE90466 ) ;			// 0x10EE		0.001778 X smoothing offset
	RamWrite32A( sysmtstp,  0x3AE90466 ) ;			// 0x11EE		0.001778 Y smoothing offset
	
	/* High-dimensional correction  */
	RegWriteA( WH_HOFCON,	0x11 ) ;				// 0x0174		OUT 3x3
	
	/* Front */
	RamWrite32A( sxiexp3,   A3_IEXP3 ) ;			// 0x10BA		
	RamWrite32A( sxiexp2,   0x00000000 ) ;			// 0x10BB		
	RamWrite32A( sxiexp1,   A1_IEXP1 ) ;			// 0x10BC		
	RamWrite32A( sxiexp0,   0x00000000 ) ;			// 0x10BD		
	RamWrite32A( sxiexp,    0x3F800000 ) ;			// 0x10BE		

	RamWrite32A( syiexp3,   A3_IEXP3 ) ;			// 0x11BA		
	RamWrite32A( syiexp2,   0x00000000 ) ;			// 0x11BB		
	RamWrite32A( syiexp1,   A1_IEXP1 ) ;			// 0x11BC		
	RamWrite32A( syiexp0,   0x00000000 ) ;			// 0x11BD		
	RamWrite32A( syiexp,    0x3F800000 ) ;			// 0x11BE		

	/* Back */
	RamWrite32A( sxoexp3,   A3_IEXP3 ) ;			// 0x10FA		
	RamWrite32A( sxoexp2,   0x00000000 ) ;			// 0x10FB		
	RamWrite32A( sxoexp1,   A1_IEXP1 ) ;			// 0x10FC		
	RamWrite32A( sxoexp0,   0x00000000 ) ;			// 0x10FD		
	RamWrite32A( sxoexp,    0x3F800000 ) ;			// 0x10FE		

	RamWrite32A( syoexp3,   A3_IEXP3 ) ;			// 0x11FA		
	RamWrite32A( syoexp2,   0x00000000 ) ;			// 0x11FB		
	RamWrite32A( syoexp1,   A1_IEXP1 ) ;			// 0x11FC		
	RamWrite32A( syoexp0,   0x00000000 ) ;			// 0x11FD		
	RamWrite32A( syoexp,    0x3F800000 ) ;			// 0x11FE		
	
	/* Sine wave */
#ifdef	DEF_SET
	RegWriteA( WC_SINON,	0x00 ) ;				// 0x0180		Sin Wave off
	RegWriteA( WC_SINFRQ0,	0x00 ) ;				// 0x0181		
	RegWriteA( WC_SINFRQ1,	0x60 ) ;				// 0x0182		
	RegWriteA( WC_SINPHSX,	0x00 ) ;				// 0x0183		
	RegWriteA( WC_SINPHSY,	0x00 ) ;				// 0x0184		
	
	/* AD over sampling */
	RegWriteA( WC_ADMODE,	0x06 ) ;				// 0x0188		AD Over Sampling
	
	/* Measure mode */
	RegWriteA( WC_MESMODE,		0x00 ) ;				// 0x0190		Measurement Mode
	RegWriteA( WC_MESSINMODE,	0x00 ) ;				// 0x0191		
	RegWriteA( WC_MESLOOP0,		0x08 ) ;				// 0x0192		
	RegWriteA( WC_MESLOOP1,		0x02 ) ;				// 0x0193		
	RegWriteA( WC_MES1ADD0,		0x00 ) ;				// 0x0194		
	RegWriteA( WC_MES1ADD1,		0x00 ) ;				// 0x0195		
	RegWriteA( WC_MES2ADD0,		0x00 ) ;				// 0x0196		
	RegWriteA( WC_MES2ADD1,		0x00 ) ;				// 0x0197		
	RegWriteA( WC_MESABS,		0x00 ) ;				// 0x0198		
	RegWriteA( WC_MESWAIT,		0x00 ) ;				// 0x0199		
	
	/* auto measure */
	RegWriteA( WC_AMJMODE,		0x00 ) ;				// 0x01A0		Automatic measurement mode
	
	RegWriteA( WC_AMJLOOP0,		0x08 ) ;				// 0x01A2		Self-Aadjustment
	RegWriteA( WC_AMJLOOP1,		0x02 ) ;				// 0x01A3		
	RegWriteA( WC_AMJIDL0,		0x02 ) ;				// 0x01A4		
	RegWriteA( WC_AMJIDL1,		0x00 ) ;				// 0x01A5		
	RegWriteA( WC_AMJ1ADD0,		0x00 ) ;				// 0x01A6		
	RegWriteA( WC_AMJ1ADD1,		0x00 ) ;				// 0x01A7		
	RegWriteA( WC_AMJ2ADD0,		0x00 ) ;				// 0x01A8		
	RegWriteA( WC_AMJ2ADD1,		0x00 ) ;				// 0x01A9		
	
#endif
	/* Data Pass */
	RegWriteA( WC_DPI1ADD0,		0x2F ) ;				// 0x01B0		Data Pass
	RegWriteA( WC_DPI1ADD1,		0x00 ) ;				// 0x01B1		
	RegWriteA( WC_DPI2ADD0,		0xAF ) ;				// 0x01B2		
	RegWriteA( WC_DPI2ADD1,		0x00 ) ;				// 0x01B3		
	RegWriteA( WC_DPI3ADD0,		0x1D ) ;				// 0x01B4		
	RegWriteA( WC_DPI3ADD1,		0x00 ) ;				// 0x01B5		
	RegWriteA( WC_DPI4ADD0,		0x9D ) ;				// 0x01B6		
	RegWriteA( WC_DPI4ADD1,		0x00 ) ;				// 0x01B7		
	RegWriteA( WC_DPO1ADD0,		0x1B ) ;				// 0x01B8		Data Pass
	RegWriteA( WC_DPO1ADD1,		0x00 ) ;				// 0x01B9		
	RegWriteA( WC_DPO2ADD0,		0x9B ) ;				// 0x01BA		
	RegWriteA( WC_DPO2ADD1,		0x00 ) ;				// 0x01BB		
	RegWriteA( WC_DPO3ADD0,		0x06 ) ;				// 0x01BC		
	RegWriteA( WC_DPO3ADD1,		0x00 ) ;				// 0x01BD		
	RegWriteA( WC_DPO4ADD0,		0x86 ) ;				// 0x01BE		
	RegWriteA( WC_DPO4ADD1,		0x00 ) ;				// 0x01BF		
	RegWriteA( WC_DPON,			0x0F ) ;				// 0x0105		Data pass ON
	
	/* Interrupt Flag */
	RegWriteA( WC_INTMSK,	0xFF ) ;				// 0x01CE		All Mask
	
	
	/* Ram Access */
	RamAccFixMod_PRMX( OFF ) ;							// 32bit float mode

	// PWM Signal Generate
	DrvSw_PRMX( OFF ) ;									/* 0x0070	Drvier Block Ena=0 */
	RegWriteA( DRVFC2	, 0x90 );					// 0x0002	Slope 3, Dead Time = 30 ns
	RegWriteA( DRVSELX	, 0xFF );					// 0x0003	PWM X drv max current  DRVSELX[7:0]
	RegWriteA( DRVSELY	, 0xFF );					// 0x0004	PWM Y drv max current  DRVSELY[7:0]

#ifdef	PWM_BREAK
	pr_err("%s:  %d: UcCvrCod_PRMX = %x \n", __func__, __LINE__,UcCvrCod_PRMX);
	if( UcCvrCod_PRMX == CVER122_PRMX ) {
		RegWriteA( PWMFC,   0x2D ) ;					// 0x0011	VREF, PWMCLK/256, MODE0B, 12Bit Accuracy
	} else {
		RegWriteA( PWMFC,   0x3D ) ;					// 0x0011	VREF, PWMCLK/128, MODE0B, 12Bit Accuracy
	}
#else
	pr_err("%s:  %d: UcCvrCod_PRMX = %x \n", __func__, __LINE__,UcCvrCod_PRMX);
	if( UcCvrCod_PRMX == CVER122_PRMX ) {
		RegWriteA( PWMFC,   0x29 ) ;					// 0x0011	VREF, PWMCLK/256, MODE0S, 12Bit Accuracy
	} else {
		RegWriteA( PWMFC,   0x39 ) ;					// 0x0011	VREF, PWMCLK/128, MODE0S, 12Bit Accuracy
	}
#endif


#ifdef	DEF_SET
	RegWriteA( PWMA,    0x00 ) ;					// 0x0010	PWM X/Y standby
#endif	
	RegWriteA( PWMDLYX,  0x04 ) ;					// 0x0012	X Phase Delay Setting
	RegWriteA( PWMDLYY,  0x04 ) ;					// 0x0013	Y Phase Delay Setting
	
#ifdef	DEF_SET
	RegWriteA( DRVCH1SEL,	0x00 ) ;				// 0x0005	OUT1/OUT2	X axis
	RegWriteA( DRVCH2SEL,	0x00 ) ;				// 0x0006	OUT3/OUT4	Y axis
	
	RegWriteA( PWMDLYTIMX,	0x00 ) ;				// 0x0014		PWM Timing
	RegWriteA( PWMDLYTIMY,	0x00 ) ;				// 0x0015		PWM Timing
#endif
	pr_err("%s: %d: UcCvrCod_PRMX = %x \n", __func__, __LINE__,UcCvrCod_PRMX);	
	if( UcCvrCod_PRMX == CVER122_PRMX ) {
		RegWriteA( PWMPERIODY,	0x00 ) ;				// 0x001A		PWM Carrier Freq
		RegWriteA( PWMPERIODY2,	0x00 ) ;				// 0x001B		PWM Carrier Freq
	} else {
		RegWriteA( PWMPERIODX,	0x00 ) ;				// 0x0018		PWM Carrier Freq
		RegWriteA( PWMPERIODX2,	0x00 ) ;				// 0x0019		PWM Carrier Freq
		RegWriteA( PWMPERIODY,	0x00 ) ;				// 0x001A		PWM Carrier Freq
		RegWriteA( PWMPERIODY2,	0x00 ) ;				// 0x001B		PWM Carrier Freq
	}
	
	/* Linear PWM circuit setting */
	RegWriteA( CVA		, 0xC0 );			// 0x0020	Linear PWM mode enable
	pr_err("%s: %d: UcCvrCod_PRMX = %x \n", __func__, __LINE__,UcCvrCod_PRMX);
	if( UcCvrCod_PRMX == CVER122_PRMX ) {
		RegWriteA( CVFC 	, 0x22 );			// 0x0021	
	}

#ifdef	PWM_BREAK
	RegWriteA( CVFC2 	, 0x80 );			// 0x0022
#else
	RegWriteA( CVFC2 	, 0x00 );			// 0x0022
#endif
	pr_err("%s: %d: UcCvrCod_PRMX = %x \n", __func__, __LINE__,UcCvrCod_PRMX);
	if( UcCvrCod_PRMX == CVER122_PRMX ) {
		RegWriteA( CVSMTHX	, 0x00 );			// 0x0023	smooth off
		RegWriteA( CVSMTHY	, 0x00 );			// 0x0024	smooth off
	}

	RegReadA( STBB0 	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
	UcStbb0 &= 0x80 ;
	RegWriteA( STBB0, UcStbb0 ) ;			// 0x0250	OIS standby
	
}



//********************************************************************************
// Function Name 	: IniGyrPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Gyro Filter Setting Initialize Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void	IniGyrPrmx( void )
{
	
	/* CPU control */
	RegWriteA( WC_CPUOPEON , 0x11 );	// 0x0103	 	CPU control
//	RegWriteA( WC_CPUOPEON , 0x10 );	// 0x0103	 	CPU control
	RegWriteA( WC_CPUOPE1ADD , 0x06 );	// 0x018A	 	
	RegWriteA( WC_CPUOPE2ADD , 0x1B );	// 0x018B	 	
	
	/*Gyro Filter Setting*/
	RegWriteA( WG_EQSW	, 0x43 );		// 0x0110		[ - | Sw6 | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
	
	/*Gyro Filter Down Sampling*/
	
	RegWriteA( WG_SHTON	, 0x10 );		// 0x0107		[ - | - | - | CmSht2PanOff ][ - | - | CmShtOpe(1:0) ]
										//				CmShtOpe[1:0] 00: シャッターOFF, 01: シャッターON, 1x:外部制御
										
#ifdef	DEF_SET
	RegWriteA( WG_SHTDLYTMR , 0x00 );	// 0x0117	 	Shutter Delay
	RegWriteA( WG_GADSMP, 	  0x00 );	// 0x011C		Sampling timing
	RegWriteA( WG_HCHR, 	  0x00 );	// 0x011B		H-filter limitter control not USE
	RegWriteA( WG_LMT3MOD , 0x00 );		// 0x0118 	[ - | - | - | - ][ - | - | - | CmLmt3Mod ]
										//				CmLmt3Mod	0: 通常リミッター動作, 1: 円の半径リミッター動作
	RegWriteA( WG_VREFADD , 0x12 );		// 0x0119	 	センター戻しを行う遅延RAMのアドレス下位6ビット　(default 0x12 = GXH1Z2/GYH1Z2)
#endif
	RegWriteA( WG_SHTMOD , 0x06 );		// 0x0116	 	Shutter Hold mode

	// Limiter
	RamWrite32A( gxlmt1H, GYRLMT1H_PRMX ) ;			// 0x1028
	RamWrite32A( gylmt1H, GYRLMT1H_PRMX ) ;			// 0x1128

	RamWrite32A( Sttx12aM, 	GYRA12_MID_PRMX );		// 0x104F
	RamWrite32A( Sttx12bM, 	GYRB12_MID_PRMX );		// 0x106F
	RamWrite32A( Sttx12bH, 	GYRB12_HGH_PRMX );		// 0x107F
	RamWrite32A( Sttx34aM, 	GYRA34_MID_PRMX );		// 0x108F
	RamWrite32A( Sttx34aH, 	GYRA34_HGH_PRMX );		// 0x109F
	RamWrite32A( Sttx34bM, 	GYRB34_MID_PRMX );		// 0x10AF
	RamWrite32A( Sttx34bH, 	GYRB34_HGH_PRMX );		// 0x10BF
	RamWrite32A( Stty12aM, 	GYRA12_MID_PRMX );		// 0x114F
	RamWrite32A( Stty12bM, 	GYRB12_MID_PRMX );		// 0x116F
	RamWrite32A( Stty12bH, 	GYRB12_HGH_PRMX );		// 0x117F
	RamWrite32A( Stty34aM, 	GYRA34_MID_PRMX );		// 0x118F
	RamWrite32A( Stty34aH, 	GYRA34_HGH_PRMX );		// 0x119F
	RamWrite32A( Stty34bM, 	GYRB34_MID_PRMX );		// 0x11AF
	RamWrite32A( Stty34bH, 	GYRB34_HGH_PRMX );		// 0x11BF
	
#ifdef	CORRECT_1DEG
	SelectPtRange_PRMX( ON ) ;
#else
	SelectPtRange_PRMX( OFF ) ;
#endif
	
	/* Pan/Tilt parameter */
//	RegWriteA( WG_PANADDA, 		0x2F );		// 0x0130	PXMBZ2/PYMBZ2 Select
	RegWriteA( WG_PANADDA, 		0x12 );		// 0x0130	GXH2Z2/GYH2Z2 Select
//	RegWriteA( WG_PANADDB, 		0x3B );		// 0x0131	GXK2Z2/GYK2Z2 Select
	RegWriteA( WG_PANADDB, 		0x38 );		// 0x0131	GXK1Z2/GYK1Z2 Select
	
	 //Threshold
	RamWrite32A( SttxHis, 	0x00000000 );			// 0x1226
	RamWrite32A( SttxaL, 	0x00000000 );			// 0x109D
	RamWrite32A( SttxbL, 	0x00000000 );			// 0x109E
	RamWrite32A( SttyaL, 	0x00000000 );			// 0x119D
	RamWrite32A( SttybL, 	0x00000000 );			// 0x119E
	
	// Pan level
	RegWriteA( WG_PANLEVABS, 		0x00 );		// 0x0133
	
	// Average parameter are set IniAdjPrmx

	// Phase Transition Setting
	// State 2 -> 1
	RegWriteA( WG_PANSTT21JUG0, 	0x07 );		// 0x0140
	RegWriteA( WG_PANSTT21JUG1, 	0x00 );		// 0x0141
	// State 3 -> 1
	RegWriteA( WG_PANSTT31JUG0, 	0x00 );		// 0x0142
	RegWriteA( WG_PANSTT31JUG1, 	0x00 );		// 0x0143
	// State 4 -> 1
	RegWriteA( WG_PANSTT41JUG0, 	0x07 );		// 0x0144
	RegWriteA( WG_PANSTT41JUG1, 	0x00 );		// 0x0145
	// State 1 -> 2
	RegWriteA( WG_PANSTT12JUG0, 	0x00 );		// 0x0146
	RegWriteA( WG_PANSTT12JUG1, 	0x00 );		// 0x0147
	// State 1 -> 3
	RegWriteA( WG_PANSTT13JUG0, 	0x00 );		// 0x0148
	RegWriteA( WG_PANSTT13JUG1, 	0x07 );		// 0x0149
	// State 2 -> 3
	RegWriteA( WG_PANSTT23JUG0, 	0x00 );		// 0x014A
	RegWriteA( WG_PANSTT23JUG1, 	0x00 );		// 0x014B
	// State 4 -> 3
	RegWriteA( WG_PANSTT43JUG0, 	0x00 );		// 0x014C
	RegWriteA( WG_PANSTT43JUG1, 	0x00 );		// 0x014D
	// State 3 -> 4
	RegWriteA( WG_PANSTT34JUG0, 	0x07 );		// 0x014E
	RegWriteA( WG_PANSTT34JUG1, 	0x00 );		// 0x014F
	// State 2 -> 4
	RegWriteA( WG_PANSTT24JUG0, 	0x00 );		// 0x0150
	RegWriteA( WG_PANSTT24JUG1, 	0x00 );		// 0x0151
	// State 4 -> 2
	RegWriteA( WG_PANSTT42JUG0, 	0x00 );		// 0x0152
	RegWriteA( WG_PANSTT42JUG1, 	0x00 );		// 0x0153

	// State Timer
	RegWriteA( WG_PANSTT1LEVTMR, 	0x00 );		// 0x015B
	RegWriteA( WG_PANSTT2LEVTMR, 	0x00 );		// 0x015C
	RegWriteA( WG_PANSTT3LEVTMR, 	0x00 );		// 0x015D
	RegWriteA( WG_PANSTT4LEVTMR, 	0x00 );		// 0x015E
	
	// Control filter
	RegWriteA( WG_PANTRSON0, 		0x1B );		// 0x0132	USE iSTP
	
	// State Setting
	IniPtMovMod_PRMX( OFF ) ;							// Pan/Tilt setting (Still)
	
	// Hold
	RegWriteA( WG_PANSTTSETILHLD,	0x00 );		// 0x015F
	
	
	// State2,4 Step Time Setting
	RegWriteA( WG_PANSTT2TMR0,	0x01 );		// 0x013C
	RegWriteA( WG_PANSTT2TMR1,	0x00 );		// 0x013D	
	RegWriteA( WG_PANSTT4TMR0,	0x00 );		// 0x013E
	RegWriteA( WG_PANSTT4TMR1,	0x01 );		// 0x013F	
	
	RegWriteA( WG_PANSTTXXXTH,	0x00 );		// 0x015A

 #if 1
	AutoGainContIniPrmx();
	
	/* exe function */
	AutoGainControlSw_PRMX( ON ) ;							/* Auto Gain Control Mode ON  */

 #else
	StartUpGainContIni();
 #endif
	
}


//********************************************************************************
// Function Name 	: IniFilPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Gyro Filter Initial Parameter Setting
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	IniFilPrmx( void )
{
#if 0
	unsigned char	UcAryId ;
	unsigned short	UsDatId, UsDatNum ;

	RegWriteA( WC_RAMACCXY, 0x01 ) ;			// 0x018D	Filter copy on
	// Filter Registor Parameter Setting
	UcAryId	= 0 ;
	UsDatNum = 0 ;
	UsDatId	= 0 ;
	while( CsFilReg[ UcAryId ] != 0xFF )
	{
		UsDatNum	= CsFilReg[ UcAryId ];
		CntWrt( ( unsigned char * )&CsFilRegDat[ UsDatId ], UsDatNum ) ;
		UcAryId++ ;
		UsDatId	+= UsDatNum ;
	}
	// Filter X-axis Ram Parameter Setting	
	UcAryId	= 0 ;
	UsDatNum = 0 ;
	UsDatId	= 0 ;
	while( CsFilRam[ UcAryId ] != 0xFF )
	{
		UsDatNum	= CsFilRam[ UcAryId ];
		CntWrt( ( unsigned char * )&CsFilRamDat[ UsDatId ], UsDatNum ) ;
		UsDatId	+= UsDatNum ;
		UcAryId++ ;
	}
	// Filter Y-axis Ram Parameter Setting	
	UcAryId	= 0 ;
	UsDatNum = 0 ;
	UsDatId	= 0 ;
	while( CsFilRamY[ UcAryId ] != 0xFF )
	{
		UsDatNum	= CsFilRamY[ UcAryId ];
		CntWrt( ( unsigned char * )&CsFilRamYDat[ UsDatId ], UsDatNum ) ;
		UsDatId	+= UsDatNum ;
		UcAryId++ ;
	}	
#else
 #if 0
 	unsigned short	UsAryId ;
	// Filter Registor Parameter Setting
	UsAryId	= 0 ;
	while( CsFilReg[ UsAryId ].UsRegAdd != 0xFFFF )
	{
		RegWriteA( CsFilReg[ UsAryId ].UsRegAdd, CsFilReg[ UsAryId ].UcRegDat ) ;
		UsAryId++ ;
	}

	// Filter Ram Parameter Setting
	UsAryId	= 0 ;
	while( CsFilRam[ UsAryId ].UsRamAdd != 0xFFFF )
	{
		RamWrite32A( CsFilRam[ UsAryId ].UsRamAdd, CsFilRam[ UsAryId ].UlRamDat ) ;
		UsAryId++ ;
	}
 #else
 	unsigned short	UsAryId ;
	// Filter Registor Parameter Setting
	UsAryId	= 0 ;
	while( CsFilReg_PRMX[ UsAryId ].UsRegAdd != 0xFFFF )
	{
		RegWriteA( CsFilReg_PRMX[ UsAryId ].UsRegAdd, CsFilReg_PRMX[ UsAryId ].UcRegDat ) ;
		UsAryId++ ;
	}

	// Filter X Ram Parameter Setting
	UsAryId	= 0 ;
	while( CsFilRamX_PRMX[ UsAryId ].UsRamAdd != 0xFFFF )
	{
		RamWrite32A( CsFilRamX_PRMX[ UsAryId ].UsRamAdd, CsFilRamX_PRMX[ UsAryId ].UlRamDat ) ;
		UsAryId++ ;
	}
	
	// Filter Z Ram Parameter Setting
	UsAryId	= 0 ;
	while( CsFilRamZ_PRMX[ UsAryId ].UsRamAdd != 0xFFFF )
	{
		RamWrite32A( CsFilRamZ_PRMX[ UsAryId ].UsRamAdd, CsFilRamZ_PRMX[ UsAryId ].UlRamDat ) ;
		UsAryId++ ;
	}
 #endif
#endif	
}



//********************************************************************************
// Function Name 	: IniAdjPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Adjust Value Setting
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	IniAdjPrmx( void )
{
	RegWriteA( WC_RAMACCXY, 0x00 ) ;			// 0x018D	Filter copy off

#ifdef	CORRECT_1DEG
	SelectIstpMod_PRMX( ON ) ;
#else
	SelectIstpMod_PRMX( OFF ) ;
#endif
	IniPtAvePrmx( ) ;								// Average setting
	
	/* OIS */
	RegWriteA( CMSDAC0, BIAS_CUR_OIS_PRMX ) ;		// 0x0251	Hall Dac電流
	RegWriteA( OPGSEL0, AMP_GAIN_X_PRMX ) ;			// 0x0253	Hall amp Gain X
	RegWriteA( OPGSEL1, AMP_GAIN_Y_PRMX ) ;			// 0x0254	Hall amp Gain Y
	/* AF */
	RegWriteA( CMSDAC1, BIAS_CUR_AF_PRMX ) ;			// 0x0252	Hall Dac電流
	RegWriteA( OPGSEL2, AMP_GAIN_AF_PRMX ) ;			// 0x0255	Hall amp Gain AF

#ifdef	MODULE_CALIBRATION
	RegWriteA( OSCSET, OSC_INI_PRMX ) ;				// 0x0257	OSC ini
	
	/* adjusted value */
	RegWriteA( IZAH,	DGYRO_OFST_XH_PRMX ) ;	// 0x02A0		Set Offset High byte
	RegWriteA( IZAL,	DGYRO_OFST_XL_PRMX ) ;	// 0x02A1		Set Offset Low byte
	RegWriteA( IZBH,	DGYRO_OFST_YH_PRMX ) ;	// 0x02A2		Set Offset High byte
	RegWriteA( IZBL,	DGYRO_OFST_YL_PRMX ) ;	// 0x02A3		Set Offset Low byte
#else	// MODULE_CALIBRATION
	RegWriteA( OSCSET, StAdjPar_PRMX.UcOscVal ) ;		// 0x0257	OSC ini
	
	/* adjusted value */
	RegWriteA( IZAH,	(unsigned char)(StAdjPar_PRMX.StGvcOff.UsGxoVal >> 8) ) ;	// 0x02A0		Set Offset High byte
	RegWriteA( IZAL,	(unsigned char)StAdjPar_PRMX.StGvcOff.UsGxoVal ) ;			// 0x02A1		Set Offset Low byte
	RegWriteA( IZBH,	(unsigned char)(StAdjPar_PRMX.StGvcOff.UsGyoVal >> 8) ) ;	// 0x02A2		Set Offset High byte
	RegWriteA( IZBL,	(unsigned char)StAdjPar_PRMX.StGvcOff.UsGyoVal ) ;			// 0x02A3		Set Offset Low byte
#endif	// MODULE_CALIBRATION
	
	/* Ram Access */
	RamAccFixMod_PRMX( ON ) ;							// 16bit Fix mode
	
	/* OIS adjusted parameter */
#ifdef	MODULE_CALIBRATION
	RamWriteA( DAXHLO,		DAHLXO_INI_PRMX ) ;		// 0x1479
	RamWriteA( DAXHLB,		DAHLXB_INI_PRMX ) ;		// 0x147A
	RamWriteA( DAYHLO,		DAHLYO_INI_PRMX ) ;		// 0x14F9
	RamWriteA( DAYHLB,		DAHLYB_INI_PRMX ) ;		// 0x14FA
	RamWriteA( OFF0Z,		HXOFF0Z_INI_PRMX ) ;		// 0x1450
	RamWriteA( OFF1Z,		HYOFF1Z_INI_PRMX ) ;		// 0x14D0
	RamWriteA( sxg,			SXGAIN_INI_PRMX ) ;		// 0x10D3
	RamWriteA( syg,			SYGAIN_INI_PRMX ) ;		// 0x11D3
#else	// MODULE_CALIBRATION
	RamWriteA( DAXHLO,		StAdjPar_PRMX.StHalAdj.UsHlxOff ) ;		// 0x1479
	RamWriteA( DAXHLB,		StAdjPar_PRMX.StHalAdj.UsHlxGan ) ;		// 0x147A
	RamWriteA( DAYHLO,		StAdjPar_PRMX.StHalAdj.UsHlyOff ) ;		// 0x14F9
	RamWriteA( DAYHLB,		StAdjPar_PRMX.StHalAdj.UsHlyGan ) ;		// 0x14FA
	RamWriteA( OFF0Z,		StAdjPar_PRMX.StHalAdj.UsAdxOff ) ;		// 0x1450
	RamWriteA( OFF1Z,		StAdjPar_PRMX.StHalAdj.UsAdyOff ) ;		// 0x14D0
	RamWriteA( sxg,			StAdjPar_PRMX.StLopGan.UsLxgVal ) ;		// 0x10D3
	RamWriteA( syg,			StAdjPar_PRMX.StLopGan.UsLygVal ) ;		// 0x11D3
#endif	// MODULE_CALIBRATION
//	UsCntXof = OPTCEN_X ;						/* Clear Optical center X value */
//	UsCntYof = OPTCEN_Y ;						/* Clear Optical center Y value */
//	RamWriteA( SXOFFZ1,		UsCntXof ) ;		// 0x1461
//	RamWriteA( SYOFFZ1,		UsCntYof ) ;		// 0x14E1

	/* AF adjusted parameter */
	RamWriteA( DAZHLO,		DAHLZO_INI_PRMX ) ;		// 0x1529
	RamWriteA( DAZHLB,		DAHLZB_INI_PRMX ) ;		// 0x152A

	/* Ram Access */
	RamAccFixMod_PRMX( OFF ) ;							// 32bit Float mode
	
#ifdef	MODULE_CALIBRATION
	RamWrite32A( gxzoom, GXGAIN_INI_PRMX ) ;		// 0x1020 Gyro X axis Gain adjusted value
	RamWrite32A( gyzoom, GYGAIN_INI_PRMX ) ;		// 0x1120 Gyro Y axis Gain adjusted value
#else	// MODULE_CALIBRATION
	RamWrite32A( gxzoom, StAdjPar_PRMX.StGyrGan.UlGxgVal ) ;		// 0x1020 Gyro X axis Gain adjusted value
	RamWrite32A( gyzoom, StAdjPar_PRMX.StGyrGan.UlGygVal ) ;		// 0x1120 Gyro Y axis Gain adjusted value
#endif	// MODULE_CALIBRATION

	RamWrite32A( sxq, SXQ_INI_PRMX ) ;			// 0x10E5	X axis output direction initial value
	RamWrite32A( syq, SYQ_INI_PRMX ) ;			// 0x11E5	Y axis output direction initial value
	
#ifdef USE_INVENSENSE
 #ifdef 	IDG2030
	RamWrite32A( gx45g, G_45G_INI_PRMX ) ;			// 0x1000
	RamWrite32A( gy45g, G_45G_INI_PRMX ) ;			// 0x1100
	ClrGyr_PRMX( 0x00FF , CLR_FRAM1_PRMX );		// Gyro Delay RAM Clear
 #endif
#endif
	if( GXHY_GYHX_PRMX ){			/* GX -> HY , GY -> HX */
		RamWrite32A( sxgx, 0x00000000 ) ;			// 0x10B8
		RamWrite32A( sxgy, 0x3F800000 ) ;			// 0x10B9
		
		RamWrite32A( sygy, 0x00000000 ) ;			// 0x11B8
		RamWrite32A( sygx, 0x3F800000 ) ;			// 0x11B9
	}
	
	SetZsp(0) ;								// Zoom coefficient Initial Setting
	
	RegWriteA( PWMA 	, 0xC0 );			// 0x0010		PWM enable

	RegWriteA( STBB0 	, 0xDF );			// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
	RegWriteA( WC_EQSW	, 0x02 ) ;			// 0x01E0
	RegWriteA( WC_MESLOOP1	, 0x02 ) ;		// 0x0193
	RegWriteA( WC_MESLOOP0	, 0x00 ) ;		// 0x0192
	RegWriteA( WC_AMJLOOP1	, 0x02 ) ;		// 0x01A3
	RegWriteA( WC_AMJLOOP0	, 0x00 ) ;		// 0x01A2
	
	
	SetPanTiltMode_PRMX( OFF ) ;					/* Pan/Tilt OFF */
	
#ifdef H1COEF_CHANGER
	SetH1cMod_PRMX( ACTMODE_PRMX ) ;					/* Lvl Change Active mode */
#else
	SetGcf_PRMX( 0 ) ;							/* DI initial value */
#endif
	
	DrvSw_PRMX( ON ) ;							/* 0x0001		Driver Mode setting */
	
	RegWriteA( WC_EQON, 0x01 ) ;			// 0x0101	Filter ON
}



//********************************************************************************
// Function Name 	: IniCmdPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Command Execute Process Initial
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	IniCmdPrmx( void )
{

	MemClr_PRMX( ( unsigned char * )&StAdjPar_PRMX, sizeof( stAdjPar_PRMX ) ) ;	// Adjust Parameter Clear
	
}


//********************************************************************************
// Function Name 	: BsyWit_PRMX
// Retun Value		: NON
// Argment Value	: Trigger Register Address, Trigger Register Data
// Explanation		: Busy Wait Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void	BsyWit_PRMX( unsigned short	UsTrgAdr, unsigned char	UcTrgDat )
{
	unsigned char	UcFlgVal ;

	RegWriteA( UsTrgAdr, UcTrgDat ) ;	// Trigger Register Setting

	UcFlgVal	= 1 ;

	while( UcFlgVal ) {

		RegReadA( UsTrgAdr, &UcFlgVal ) ;
		UcFlgVal	&= 	( UcTrgDat & 0x0F ) ;

		if( CmdRdChk() !=0 )	break;		// Dead Lock check (responce check)

	} ;

}


//********************************************************************************
// Function Name 	: MemClr_PRMX
// Retun Value		: void
// Argment Value	: Clear Target Pointer, Clear Byte Number
// Explanation		: Memory Clear Function
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	MemClr_PRMX( unsigned char	*NcTgtPtr, unsigned short	UsClrSiz )
{
	unsigned short	UsClrIdx ;

	for ( UsClrIdx = 0 ; UsClrIdx < UsClrSiz ; UsClrIdx++ )
	{
		*NcTgtPtr	= 0 ;
		NcTgtPtr++ ;
	}
}

//********************************************************************************
// Function Name 	: GyOutSignal_PRMX
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Select Gyro Signal Function
// History			: First edition 						2010.12.27 Y.Shigeoka
//********************************************************************************
void	GyOutSignal_PRMX( void )
{

	RegWriteA( GRADR0,	GYROX_INI_PRMX ) ;			// 0x0283	Set Gyro XOUT H~L
	RegWriteA( GRADR1,	GYROY_INI_PRMX ) ;			// 0x0284	Set Gyro YOUT H~L
	
	/*Start OIS Reading*/
	RegWriteA( GRSEL	, 0x02 );			// 0x0280	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]

}

//********************************************************************************
// Function Name 	: GyOutSignalCont_PRMX
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Select Gyro Continuosl Function
// History			: First edition 						2013.06.06 Y.Shigeoka
//********************************************************************************
void	GyOutSignalCont_PRMX( void )
{

	/*Start OIS Reading*/
	RegWriteA( GRSEL	, 0x04 );			// 0x0280	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]

}

#ifdef STANDBY_MODE
//********************************************************************************
// Function Name 	: AccWit_PRMX
// Retun Value		: NON
// Argment Value	: Trigger Register Data
// Explanation		: Acc Wait Function
// History			: First edition 						2010.12.27 Y.Shigeoka
//********************************************************************************
void	AccWit_PRMX( unsigned char UcTrgDat )
{
	unsigned char	UcFlgVal ;

	UcFlgVal	= 1 ;

	while( UcFlgVal ) {
		RegReadA( GRACC, &UcFlgVal ) ;			// 0x0282
		UcFlgVal	&= UcTrgDat ;

		if( CmdRdChk() !=0 )	break;		// Dead Lock check (responce check)

	} ;

}

//********************************************************************************
// Function Name 	: SelectGySleep_PRMX
// Retun Value		: NON
// Argment Value	: mode	
// Explanation		: Select Gyro mode Function
// History			: First edition 						2010.12.27 Y.Shigeoka
//********************************************************************************
void	SelectGySleep_PRMX( unsigned char UcSelMode )
{
 #ifdef USE_INVENSENSE
	unsigned char	UcRamIni ;
	unsigned char	UcGrini ;

	if(UcSelMode == ON)
	{
		RegWriteA( WC_EQON, 0x00 ) ;		// 0x0101	Equalizer OFF
		RegWriteA( GRSEL,	0x01 ) ;		/* 0x0280	Set Command Mode			*/

		RegReadA( GRINI	, &UcGrini );					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
		RegWriteA( GRINI, ( UcGrini | SLOWMODE) );		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
		
		RegWriteA( GRADR0,	0x6B ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GRACC,	0x01 ) ;		/* 0x0282	Set Read Trigger ON				*/
		AccWit_PRMX( 0x01 ) ;					/* Digital Gyro busy wait 				*/
		RegReadA( GRDAT0H, &UcRamIni ) ;	/* 0x0290 */
		
		UcRamIni |= 0x40 ;					/* Set Sleep bit */
  #ifdef GYROSTBY
		UcRamIni &= ~0x01 ;					/* Clear PLL bit(internal oscillator */
  #endif
		
		RegWriteA( GRADR0,	0x6B ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GSETDT,	UcRamIni ) ;	/* 0x028A	Set Write Data(Sleep ON)	*/
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit_PRMX( 0x10 ) ;					/* Digital Gyro busy wait 				*/

  #ifdef GYROSTBY
		RegWriteA( GRADR0,	0x6C ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GSETDT,	0x07 ) ;		/* 0x028A	Set Write Data(STBY ON)	*/
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit_PRMX( 0x10 ) ;					/* Digital Gyro busy wait 				*/
  #endif
	}
	else
	{
  #ifdef GYROSTBY
		RegWriteA( GRADR0,	0x6C ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GSETDT,	0x00 ) ;		/* 0x028A	Set Write Data(STBY OFF)	*/
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit_PRMX( 0x10 ) ;					/* Digital Gyro busy wait 				*/
  #endif
		RegWriteA( GRADR0,	0x6B ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GRACC,	0x01 ) ;		/* 0x0282	Set Read Trigger ON				*/
		AccWit_PRMX( 0x01 ) ;					/* Digital Gyro busy wait 				*/
		RegReadA( GRDAT0H, &UcRamIni ) ;	/* 0x0290 */
		
		UcRamIni &= ~0x40 ;					/* Clear Sleep bit */
  #ifdef GYROSTBY
		UcRamIni |=  0x01 ;					/* Set PLL bit */
  #endif
		
		RegWriteA( GSETDT,	UcRamIni ) ;	// 0x028A	Set Write Data(Sleep OFF)
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit_PRMX( 0x10 ) ;					/* Digital Gyro busy wait 				*/
		
		RegReadA( GRINI	, &UcGrini );					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ LSBF | SLOWMODE | I2CMODE | - ]
		RegWriteA( GRINI, ( UcGrini & ~SLOWMODE) );		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ LSBF | SLOWMODE | I2CMODE | - ]
		
		GyOutSignal_PRMX( ) ;					/* Select Gyro output signal 			*/
		
		WitTim( 50 ) ;						// 50ms wait
		
		RegWriteA( WC_EQON, 0x01 ) ;		// 0x0101	GYRO Equalizer ON

		ClrGyr_PRMX( 0x007F , CLR_FRAM1_PRMX );		// Gyro Delay RAM Clear
	}
 #else									/* Panasonic */
	
//	unsigned char	UcRamIni ;


	if(UcSelMode == ON)
	{
		RegWriteA( WC_EQON, 0x00 ) ;		// 0x0101	GYRO Equalizer OFF
		RegWriteA( GRSEL,	0x01 ) ;		/* 0x0280	Set Command Mode			*/
		RegWriteA( GRADR0,	0x4C ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GSETDT,	0x02 ) ;		/* 0x028A	Set Write Data(Sleep ON)	*/
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit_PRMX( 0x10 ) ;					/* Digital Gyro busy wait 				*/
	}
	else
	{
		RegWriteA( GRADR0,	0x4C ) ;		// 0x0283	Set Write Command
		RegWriteA( GSETDT,	0x00 ) ;		// 0x028A	Set Write Data(Sleep OFF)
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit_PRMX( 0x10 ) ;					/* Digital Gyro busy wait 				*/
		GyOutSignal_PRMX( ) ;					/* Select Gyro output signal 			*/
		
		WitTim( 50 ) ;						// 50ms wait
		
		RegWriteA( WC_EQON, 0x01 ) ;		// 0x0101	GYRO Equalizer ON
		ClrGyr_PRMX( 0x007F , CLR_FRAM1_PRMX );		// Gyro Delay RAM Clear
	}
 #endif
}
#endif

//********************************************************************************
// Function Name 	: ClrGyr_PRMX
// Retun Value		: NON
// Argment Value	: UsClrFil - Select filter to clear.  If 0x0000, clears entire filter
//					  UcClrMod - 0x01: FRAM0 Clear, 0x02: FRAM1, 0x03: All RAM Clear
// Explanation		: Gyro RAM clear function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void	ClrGyr_PRMX( unsigned short UsClrFil , unsigned char UcClrMod )
{
	unsigned char	UcRamClr;
	unsigned char	count = 0; 

	/*Select Filter to clear*/
	RegWriteA( WC_RAMDLYMOD1,	(unsigned char)(UsClrFil >> 8) ) ;		// 0x018F		FRAM Initialize Hbyte
	RegWriteA( WC_RAMDLYMOD0,	(unsigned char)UsClrFil ) ;				// 0x018E		FRAM Initialize Lbyte

	/*Enable Clear*/
	RegWriteA( WC_RAMINITON	, UcClrMod ) ;	// 0x0102	[ - | - | - | - ][ - | - | 遅延Clr | 係数Clr ]
	
	/*Check RAM Clear complete*/
	do{
		RegReadA( WC_RAMINITON, &UcRamClr );
		UcRamClr &= UcClrMod;

		if( count++ >= 100 ){
			break;
		}

	}while( UcRamClr != 0x00 );
}


//********************************************************************************
// Function Name 	: DrvSw_PRMX
// Retun Value		: NON
// Argment Value	: 0:OFF  1:ON
// Explanation		: Driver Mode setting function
// History			: First edition 						2012.04.25 Y.Shigeoka
//********************************************************************************
void	DrvSw_PRMX( unsigned char UcDrvSw )
{
	if( UcDrvSw == ON )
	{
		if( UcPwmMod_PRMX == PWMMOD_CVL_PRMX ) {
			RegWriteA( DRVFC	, 0xF0 );			// 0x0001	Drv.MODE=1,Drv.BLK=1,MODE2,LCEN
		} else {
			RegWriteA( DRVFC	, 0x00 );			// 0x0001	Drv.MODE=0,Drv.BLK=0,MODE0B
		}
	}
	else
	{
		if( UcPwmMod_PRMX == PWMMOD_CVL_PRMX ) {
			RegWriteA( DRVFC	, 0x30 );				// 0x0001	Drvier Block Ena=0
		} else {
			RegWriteA( DRVFC	, 0x00 );				// 0x0001	Drv.MODE=0,Drv.BLK=0,MODE0B
		}
	}
}

//********************************************************************************
// Function Name 	: AfDrvSw_PRMX
// Retun Value		: NON
// Argment Value	: 0:OFF  1:ON
// Explanation		: AF Driver Mode setting function
// History			: First edition 						2013.09.12 Y.Shigeoka
//********************************************************************************
void	AfDrvSw_PRMX( unsigned char UcDrvSw )
{
	if( UcDrvSw == ON )
	{
		RegWriteA( DRVFCAF	, 0x20 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-2
		RegWriteA( CCAAF,   0x80 ) ;				// 0x00A0	[7]=0:OFF 1:ON
	}
	else
	{
		RegWriteA( CCAAF,   0x00 ) ;				// 0x00A0	[7]=0:OFF 1:ON
	}
}

//********************************************************************************
// Function Name 	: RamAccFixMod_PRMX
// Retun Value		: NON
// Argment Value	: 0:OFF  1:ON
// Explanation		: Ram Access Fix Mode setting function
// History			: First edition 						2013.05.21 Y.Shigeoka
//********************************************************************************
void	RamAccFixMod_PRMX( unsigned char UcAccMod )
{
	switch ( UcAccMod ) {
		case OFF :
			RegWriteA( WC_RAMACCMOD,	0x00 ) ;		// 0x018C		GRAM Access Float32bit
			break ;
		case ON :
			RegWriteA( WC_RAMACCMOD,	0x31 ) ;		// 0x018C		GRAM Access Fix32bit
			break ;
	}
}
	

//********************************************************************************
// Function Name 	: IniAfPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Open AF Initial Setting
// History			: First edition 						2013.09.12 Y.Shigeoka
//********************************************************************************
void	IniAfPrmx( void )
{
	unsigned char	UcStbb0 ;
	
	AfDrvSw_PRMX( OFF ) ;								/* AF Drvier Block Ena=0 */
	RegWriteA( DRVFCAF	, 0x20 );					// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-2
	RegWriteA( DRVFC4AF	, 0x80 );					// 0x0084	DOFSTDAF
	RegWriteA( AFFC,   0x80 ) ;						// 0x0088	OpenAF/-/-
#ifdef	DEF_SET
	RegWriteA( DRVFC3AF	, 0x00 );					// 0x0083	DGAINDAF	Gain 0
	RegWriteA( PWMAAF,    0x00 ) ;					// 0x0090	AF PWM standby

	RegWriteA( DRVFC2AF,    0x00 ) ;				// 0x0082	AF slope0
	RegWriteA( DRVCH3SEL,   0x00 ) ;				// 0x0085	AF H bridge control
#endif
	RegWriteA( PWMFCAF,     0x01 ) ;				// 0x0091	AF VREF , Carrier , MODE1
	RegWriteA( PWMPERIODAF, 0x20 ) ;				// 0x0099	AF none-synchronism
	RegWriteA( CCFCAF,   0x40 ) ;					// 0x00A1	GND/-
	
	RegReadA( STBB0 	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
	UcStbb0 &= 0x7F ;
	RegWriteA( STBB0, UcStbb0 ) ;			// 0x0250	OIS standby
	RegWriteA( STBB1, 0x00 ) ;				// 0x0264	All standby
	
	/* AF Initial setting */
	RegWriteA( FSTMODE,		FSTMODE_AF_PRMX ) ;		// 0x0302
	RamWriteA( RWEXD1_L,	RWEXD1_L_AF_PRMX ) ;		// 0x0396 - 0x0397 (Register continuos write)
	RamWriteA( RWEXD2_L,	RWEXD2_L_AF_PRMX ) ;		// 0x0398 - 0x0399 (Register continuos write)
	RamWriteA( RWEXD3_L,	RWEXD3_L_AF_PRMX ) ;		// 0x039A - 0x039B (Register continuos write)
	RegWriteA( FSTCTIME,	FSTCTIME_AF_PRMX ) ;		// 0x0303 	
	RamWriteA( TCODEH,		0x0000 ) ;			// 0x0304 - 0x0305 (Register continuos write)
	

	UcStbb0 |= 0x80 ;
	RegWriteA( STBB0, UcStbb0 ) ;			// 0x0250	
	RegWriteA( STBB1	, 0x05 ) ;			// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]

	AfDrvSw_PRMX( ON ) ;								/* AF Drvier Block Ena=1 */
}



//********************************************************************************
// Function Name 	: IniPtAvePrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Pan/Tilt Average parameter setting function
// History			: First edition 						2013.09.26 Y.Shigeoka
//********************************************************************************
void	IniPtAvePrmx( void )
{
	RegWriteA( WG_PANSTT1DWNSMP0, 0x00 );		// 0x0134
	RegWriteA( WG_PANSTT1DWNSMP1, 0x00 );		// 0x0135
	RegWriteA( WG_PANSTT2DWNSMP0, 0x00 );		// 0x0136
	RegWriteA( WG_PANSTT2DWNSMP1, 0x00 );		// 0x0137
	RegWriteA( WG_PANSTT3DWNSMP0, 0x00 );		// 0x0138
	RegWriteA( WG_PANSTT3DWNSMP1, 0x00 );		// 0x0139
	RegWriteA( WG_PANSTT4DWNSMP0, 0x00 );		// 0x013A
	RegWriteA( WG_PANSTT4DWNSMP1, 0x00 );		// 0x013B

	RamWrite32A( st1mean, 0x3f800000 );		// 0x1235
	RamWrite32A( st2mean, 0x3f800000 );		// 0x1236
	RamWrite32A( st3mean, 0x3f800000 );		// 0x1237
	RamWrite32A( st4mean, 0x3f800000 );		// 0x1238
			
}
	
//********************************************************************************
// Function Name 	: IniPtMovMod_PRMX
// Retun Value		: NON
// Argment Value	: OFF:Still  ON:Movie
// Explanation		: Pan/Tilt parameter setting by mode function
// History			: First edition 						2013.09.26 Y.Shigeoka
//********************************************************************************
void	IniPtMovMod_PRMX( unsigned char UcPtMod )
{
	switch ( UcPtMod ) {
		case OFF :
			RegWriteA( WG_PANSTTSETGYRO, 	0x00 );		// 0x0154
			RegWriteA( WG_PANSTTSETGAIN, 	0x00 );		// 0x0155
			RegWriteA( WG_PANSTTSETISTP, 	0x50 );		// 0x0156
			RegWriteA( WG_PANSTTSETIFTR,	0x00 );		// 0x0157
			RegWriteA( WG_PANSTTSETLFTR,	0x90 );		// 0x0158

			break ;
		case ON :
			RegWriteA( WG_PANSTTSETGYRO, 	0x00 );		// 0x0154
			RegWriteA( WG_PANSTTSETGAIN, 	0x00 );		// 0x0155
			RegWriteA( WG_PANSTTSETISTP, 	0x00 );		// 0x0156
			RegWriteA( WG_PANSTTSETIFTR,	0x00 );		// 0x0157
			RegWriteA( WG_PANSTTSETLFTR,	0x00 );		// 0x0158
			break ;
	}
}
	
//********************************************************************************
// Function Name 	: SelectPtRange_PRMX
// Retun Value		: NON
// Argment Value	: OFF:Narrow  ON:Wide
// Explanation		: Pan/Tilt parameter Range function
// History			: First edition 						2014.04.08 Y.Shigeoka
//********************************************************************************
void	SelectPtRange_PRMX( unsigned char UcSelRange )
{
	switch ( UcSelRange ) {
		case OFF :
			RamWrite32A( gxlmt3HS0, GYRLMT3_S1_PRMX ) ;		// 0x1029
			RamWrite32A( gylmt3HS0, GYRLMT3_S1_PRMX ) ;		// 0x1129
			
			RamWrite32A( gxlmt3HS1, GYRLMT3_S2_PRMX ) ;		// 0x102A
			RamWrite32A( gylmt3HS1, GYRLMT3_S2_PRMX ) ;		// 0x112A

			RamWrite32A( gylmt4HS0, GYRLMT4_S1_PRMX ) ;		//0x112B	Y軸Limiter4 High閾値0
			RamWrite32A( gxlmt4HS0, GYRLMT4_S1_PRMX ) ;		//0x102B	X軸Limiter4 High閾値0
			
			RamWrite32A( gxlmt4HS1, GYRLMT4_S2_PRMX ) ;		//0x102C	X軸Limiter4 High閾値1
			RamWrite32A( gylmt4HS1, GYRLMT4_S2_PRMX ) ;		//0x112C	Y軸Limiter4 High閾値1
		
			RamWrite32A( Sttx12aH, 	GYRA12_HGH_PRMX );		// 0x105F
			RamWrite32A( Stty12aH, 	GYRA12_HGH_PRMX );		// 0x115F

			break ;
		
		case ON :
			RamWrite32A( gxlmt3HS0, GYRLMT3_S1_W_PRMX ) ;		// 0x1029
			RamWrite32A( gylmt3HS0, GYRLMT3_S1_W_PRMX ) ;		// 0x1129
			
			RamWrite32A( gxlmt3HS1, GYRLMT3_S2_W_PRMX ) ;		// 0x102A
			RamWrite32A( gylmt3HS1, GYRLMT3_S2_W_PRMX ) ;		// 0x112A

			RamWrite32A( gylmt4HS0, GYRLMT4_S1_W_PRMX ) ;		//0x112B	Y軸Limiter4 High閾値0
			RamWrite32A( gxlmt4HS0, GYRLMT4_S1_W_PRMX ) ;		//0x102B	X軸Limiter4 High閾値0
			
			RamWrite32A( gxlmt4HS1, GYRLMT4_S2_W_PRMX ) ;		//0x102C	X軸Limiter4 High閾値1
			RamWrite32A( gylmt4HS1, GYRLMT4_S2_W_PRMX ) ;		//0x112C	Y軸Limiter4 High閾値1
		
			RamWrite32A( Sttx12aH, 	GYRA12_HGH_W_PRMX );			// 0x105F
			RamWrite32A( Stty12aH, 	GYRA12_HGH_W_PRMX );			// 0x115F

			break ;
	}
}

//********************************************************************************
// Function Name 	: SelectIstpMod_PRMX
// Retun Value		: NON
// Argment Value	: OFF:Narrow  ON:Wide
// Explanation		: Pan/Tilt parameter Range function
// History			: First edition 						2014.04.08 Y.Shigeoka
//********************************************************************************
void	SelectIstpMod_PRMX( unsigned char UcSelRange )
{
	switch ( UcSelRange ) {
		case OFF :
			RamWrite32A( gxistp_1, GYRISTP_PRMX ) ;		// 0x1083
			RamWrite32A( gyistp_1, GYRISTP_PRMX ) ;		// 0x1183
			break ;
		
		case ON :
			RamWrite32A( gxistp_1, GYRISTP_W_PRMX ) ;	// 0x1083
			RamWrite32A( gyistp_1, GYRISTP_W_PRMX ) ;	// 0x1183
			break ;
	}
}

//********************************************************************************
// Function Name 	: ChkCvr_PRMX
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Check Cver function
// History			: First edition 						2013.10.03 Y.Shigeoka
//********************************************************************************
void	ChkCvr_PRMX( void )
{
	RegReadA( CVER ,	&UcCvrCod_PRMX );		// 0x027E
	pr_err("%s: %d: UcCvrCod_PRMX = %x \n", __func__, __LINE__,UcCvrCod_PRMX);
	RegWriteA( 0x00FF ,	MDL_VER );			// 0x00FF	Model
	RegWriteA( 0x02D0 ,	FW_VER );			// 0x02D0	Version
}


//********************************************************************************
// Function Name 	: AutoGainContIniPrmx
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Gain Control initial function
// History			: First edition 						2014.09.16 Y.Shigeoka
//********************************************************************************
  #define	TRI_LEVEL		0x3A031280		/* 0.0005 */
  #define	TIMELOW			0x50			/* */
  #define	TIMEHGH			0x05			/* */
  #define	TIMEBSE			0x5D			/* 3.96ms */
  #define	MONADR			GXXFZ
  #define	GANADR			gxadj
  #define	XMINGAIN		0x00000000
  #define	XMAXGAIN		0x3F800000
  #define	YMINGAIN		0x00000000
  #define	YMAXGAIN		0x3F800000
  #define	XSTEPUP			0x38D1B717		/* 0.0001	 */
  #define	XSTEPDN			0xBD4CCCCD		/* -0.05 	 */
  #define	YSTEPUP			0x38D1B717		/* 0.0001	 */
  #define	YSTEPDN			0xBD4CCCCD		/* -0.05 	 */

void	AutoGainContIniPrmx( void )
{
	RamWrite32A( gxlevlow, TRI_LEVEL );					// 0x10AE	Low Th
	RamWrite32A( gylevlow, TRI_LEVEL );					// 0x11AE	Low Th
	RamWrite32A( gxadjmin, XMINGAIN );					// 0x1094	Low gain
	RamWrite32A( gxadjmax, XMAXGAIN );					// 0x1095	Hgh gain
	RamWrite32A( gxadjdn, XSTEPDN );					// 0x1096	-step
	RamWrite32A( gxadjup, XSTEPUP );					// 0x1097	+step
	RamWrite32A( gyadjmin, YMINGAIN );					// 0x1194	Low gain
	RamWrite32A( gyadjmax, YMAXGAIN );					// 0x1195	Hgh gain
	RamWrite32A( gyadjdn, YSTEPDN );					// 0x1196	-step
	RamWrite32A( gyadjup, YSTEPUP );					// 0x1197	+step
	
	RegWriteA( WG_LEVADD, (unsigned char)MONADR );		// 0x0120	Input signal
	RegWriteA( WG_LEVTMR, 		TIMEBSE );				// 0x0123	Base Time
	RegWriteA( WG_LEVTMRLOW, 	TIMELOW );				// 0x0121	X Low Time
	RegWriteA( WG_LEVTMRHGH, 	TIMEHGH );				// 0x0122	X Hgh Time
	RegWriteA( WG_ADJGANADD, (unsigned char)GANADR );		// 0x0128	control address
	RegWriteA( WG_ADJGANGO, 		0x00 );					// 0x0108	manual off
}
	
//********************************************************************************
// Function Name 	: AutoGainControlSw_PRMX
// Retun Value		: NON
// Argment Value	: 0 :OFF  1:ON
// Explanation		: Select Gyro Signal Function
// History			: First edition 						2010.11.30 Y.Shigeoka
//********************************************************************************
void	AutoGainControlSw_PRMX( unsigned char UcModeSw )
{

	if( UcModeSw == OFF )
	{
		RegWriteA( WG_ADJGANGXATO, 	0xA0 );					// 0x0129	X exe off
		RegWriteA( WG_ADJGANGYATO, 	0xA0 );					// 0x012A	Y exe off
		RamWrite32A( GANADR			 , XMAXGAIN ) ;			// Gain Through
		RamWrite32A( GANADR | 0x0100 , YMAXGAIN ) ;			// Gain Through
	}
	else
	{
		RegWriteA( WG_ADJGANGXATO, 	0xA3 );					// 0x0129	X exe on
		RegWriteA( WG_ADJGANGYATO, 	0xA3 );					// 0x012A	Y exe on
	}

}


//********************************************************************************
// Function Name 	: StartUpGainContIni
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Start UP Gain Control initial function
// History			: First edition 						2014.09.16 Y.Shigeoka
//********************************************************************************
  #define	ST_TRI_LEVEL		0x3A031280		/* 0.0005 */
  #define	ST_TIMELOW			0x00			/* */
  #define	ST_TIMEHGH			0x00			/* */
  #define	ST_TIMEBSE			0x00			/* */
  #define	ST_MONADR			GXXFZ
  #define	ST_GANADR			pxmbb
  #define	ST_XMINGAIN		0x3A031240		/* Target gain 0x10A5*/
  #define	ST_XMAXGAIN		0x3C031280		/* Initial gain*/
  #define	ST_YMINGAIN		0x3A031240
  #define	ST_YMAXGAIN		0x3C031280
  #define	ST_XSTEPUP			0x3F800000		/* 1	 */
  #define	ST_XSTEPDN			0xB3E50F84		/* -0.0000001 	 */
  #define	ST_YSTEPUP			0x3F800000		/* 1	 */
  #define	ST_YSTEPDN			0xB3E50F84		/* -0.05 	 */
	
void	StartUpGainContIni( void )
{
	RamWrite32A( gxlevlow, ST_TRI_LEVEL );					// 0x10AE	Low Th
	RamWrite32A( gylevlow, ST_TRI_LEVEL );					// 0x11AE	Low Th
	RamWrite32A( gxadjmin, ST_XMINGAIN );					// 0x1094	Low gain
	RamWrite32A( gxadjmax, ST_XMAXGAIN );					// 0x1095	Hgh gain
	RamWrite32A( gxadjdn, ST_XSTEPDN );					// 0x1096	-step
	RamWrite32A( gxadjup, ST_XSTEPUP );					// 0x1097	+step
	RamWrite32A( gyadjmin, ST_YMINGAIN );					// 0x1194	Low gain
	RamWrite32A( gyadjmax, ST_YMAXGAIN );					// 0x1195	Hgh gain
	RamWrite32A( gyadjdn, ST_YSTEPDN );					// 0x1196	-step
	RamWrite32A( gyadjup, ST_YSTEPUP );					// 0x1197	+step
	
	RegWriteA( WG_LEVADD, (unsigned char)ST_MONADR );		// 0x0120	Input signal
	RegWriteA( WG_LEVTMR, 		ST_TIMEBSE );				// 0x0123	Base Time
	RegWriteA( WG_LEVTMRLOW, 	ST_TIMELOW );				// 0x0121	X Low Time
	RegWriteA( WG_LEVTMRHGH, 	ST_TIMEHGH );				// 0x0122	X Hgh Time
	RegWriteA( WG_ADJGANADD, (unsigned char)ST_GANADR );		// 0x0128	control address
	RegWriteA( WG_ADJGANGO, 		0x00 );					// 0x0108	manual off
}
	
//********************************************************************************
// Function Name 	: AutoGainControl
// Retun Value		: NON
// Argment Value	: OFF,  ON
// Explanation		: Gain Control function
// History			: First edition 						2014.09.16 Y.Shigeoka
//********************************************************************************
unsigned char	InitGainControl( unsigned char uc_mode )
{
	unsigned char	uc_rtval;
	
	uc_rtval = 0x00 ;
	
	switch( uc_mode) {
	case	0x00 :
		RamWrite32A( gx2x4xb, 0x00000000 ) ;		// 0x1021 
		RamWrite32A( gy2x4xb, 0x00000000 ) ;		// 0x1121 
		
		RegWriteA( WG_ADJGANGO, 		0x22 );					// 0x0108	manual on to go to max
		uc_rtval = 0x22 ;
		while( uc_rtval ){
			
			RegReadA( WG_ADJGANGO, 		&uc_rtval );			// 0x0108	status read
		} ;
		
	case	0x01 :
		RamWrite32A( gx2x4xb, 0x00000000 ) ;		// 0x1021 
		RamWrite32A( gy2x4xb, 0x00000000 ) ;		// 0x1121 
	
		RegWriteA( WG_ADJGANGO, 		0x11 );					// 0x0108	manual on to go to min(initial)
		break;
		
	case	0x02 :
		RegReadA( WG_ADJGANGO, 		&uc_rtval );			// 0x0108	status read
		break;
		
	case	0x03 :
		
		ClrGyr_PRMX( 0x000E , CLR_FRAM1_PRMX );		// Gyro Delay RAM Clear
		RamWrite32A( gx2x4xb, 0x3F800000 ) ;		// 0x1021 
		RamWrite32A( gy2x4xb, 0x3F800000 ) ;		// 0x1121 
		
		AutoGainContIniPrmx() ;
		AutoGainControlSw_PRMX( ON ) ;								/* Auto Gain Control Mode ON  */
		break;
	}
	
	return( uc_rtval ) ;
}


	
#ifndef	MODULE_CALIBRATION
#if 0
void	E2pDat_PRMX( void )
{

	EepRed( (unsigned short)HALL_BIAS_X,			2,	( unsigned char * )&StAdjPar_PRMX.StHalAdj.UsHlxGan ) ; WitTim(5);
	EepRed( (unsigned short)HALL_BIAS_Y,			2,	( unsigned char * )&StAdjPar_PRMX.StHalAdj.UsHlyGan ) ; WitTim(5);

	EepRed( (unsigned short)HALL_OFFSET_X,			2,	( unsigned char * )&StAdjPar_PRMX.StHalAdj.UsHlxOff ) ; WitTim(5);
	EepRed( (unsigned short)HALL_OFFSET_Y,			2,	( unsigned char * )&StAdjPar_PRMX.StHalAdj.UsHlyOff ) ; WitTim(5);

	EepRed( (unsigned short)LOOP_GAIN_X,			2,	( unsigned char * )&StAdjPar_PRMX.StLopGan.UsLxgVal ) ; WitTim(5);
	EepRed( (unsigned short)LOOP_GAIN_Y,			2,	( unsigned char * )&StAdjPar_PRMX.StLopGan.UsLygVal ) ; WitTim(5);

	EepRed( (unsigned short)LENS_CENTER_FINAL_X,	2,	( unsigned char * )&StAdjPar_PRMX.StHalAdj.UsAdxOff ) ; WitTim(5);
	EepRed( (unsigned short)LENS_CENTER_FINAL_Y,	2,	( unsigned char * )&StAdjPar_PRMX.StHalAdj.UsAdyOff ) ; WitTim(5);

	EepRed( (unsigned short)GYRO_AD_OFFSET_X,		2,	( unsigned char * )&StAdjPar_PRMX.StGvcOff.UsGxoVal ) ; WitTim(5);
	EepRed( (unsigned short)GYRO_AD_OFFSET_Y,		2,	( unsigned char * )&StAdjPar_PRMX.StGvcOff.UsGyoVal ) ; WitTim(5);

	EepRed( (unsigned short)OSC_CLK_VAL,			1,	( unsigned char * )&StAdjPar_PRMX.UcOscVal ) ; WitTim(5);

	EepRed( (unsigned short)GYRO_GAIN_X,			4,	( unsigned char * )&StAdjPar_PRMX.StGyrGan.UlGxgVal ) ; WitTim(5);
	EepRed( (unsigned short)GYRO_GAIN_Y,			4,	( unsigned char * )&StAdjPar_PRMX.StGyrGan.UlGygVal ) ; WitTim(5);

}
#endif
#endif	//#ifndef	MODULE_CALIBRATION

//********************************************************************************
// Function Name 	: RtnCen_PRMX
// Retun Value		: Command Status
// Argment Value	: Command Parameter
// Explanation		: Return to center Command Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
unsigned char	RtnCen_PRMX( unsigned char	UcCmdPar )
{
	unsigned char	UcCmdSts ;

	UcCmdSts	= EXE_END_PRMX ;

	GyrCon_PRMX( OFF ) ;											// Gyro OFF

	if( !UcCmdPar ) {										// X,Y Centering

		StbOnn_PRMX() ;											// Slope Mode
		
	} else if( UcCmdPar == 0x01 ) {							// X Centering Only

		SrvCon_PRMX( X_DIR_PRMX, ON ) ;								// X only Servo ON
		SrvCon_PRMX( Y_DIR_PRMX, OFF ) ;
	} else if( UcCmdPar == 0x02 ) {							// Y Centering Only

		SrvCon_PRMX( X_DIR_PRMX, OFF ) ;								// Y only Servo ON
		SrvCon_PRMX( Y_DIR_PRMX, ON ) ;
	}

	return( UcCmdSts ) ;
}

//********************************************************************************
// Function Name 	: OisEna_PRMX
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: OIS Enable Control Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	OisEna_PRMX( void )
{
	// Servo ON
	SrvCon_PRMX( X_DIR_PRMX, ON ) ;
	SrvCon_PRMX( Y_DIR_PRMX, ON ) ;

	GyrCon_PRMX( ON ) ;
}

//********************************************************************************
// Function Name 	: GyrCon_PRMX
// Retun Value		: NON
// Argment Value	: Gyro Filter ON or OFF
// Explanation		: Gyro Filter Control Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	GyrCon_PRMX( unsigned char	UcGyrCon )
{
	// Return HPF Setting
	RegWriteA( WG_SHTON, 0x00 ) ;									// 0x0107
	
	if( UcGyrCon == ON ) {												// Gyro ON

		
//		ClrGyr_PRMX( 0x000E , CLR_FRAM1_PRMX );		// Gyro Delay RAM Clear
		ClrGyr_PRMX( 0x000A , CLR_FRAM1_PRMX );		// Gyro Delay RAM Clear

		RamWrite32A( sxggf, 0x3F800000 ) ;	// 0x10B5
		RamWrite32A( syggf, 0x3F800000 ) ;	// 0x11B5
		
	} else if( UcGyrCon == SPC_PRMX ) {										// Gyro ON for LINE

		

		RamWrite32A( sxggf, 0x3F800000 ) ;	// 0x10B5
		RamWrite32A( syggf, 0x3F800000 ) ;	// 0x11B5
		

	} else {															// Gyro OFF
		
		RamWrite32A( sxggf, 0x00000000 ) ;	// 0x10B5
		RamWrite32A( syggf, 0x00000000 ) ;	// 0x11B5
		

	}
}

//********************************************************************************
// Function Name 	: SetZsp
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: Set Zoom Step parameter Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	SetZsp( unsigned char	UcZoomStepDat )
{
	unsigned long	UlGyrZmx, UlGyrZmy, UlGyrZrx, UlGyrZry ;

	
	/* Zoom Step */
	if(UcZoomStepDat > (ZOOMTBL - 1))
		UcZoomStepDat = (ZOOMTBL -1) ;										/* \8F\E3\8C\C0\82\F0ZOOMTBL-1\82\u0250\u0752\u80b7\82\E9 */

	if( UcZoomStepDat == 0 )				/* initial setting	*/
	{
		UlGyrZmx	= ClGyxZom[ 0 ] ;		// Same Wide Coefficient
		UlGyrZmy	= ClGyyZom[ 0 ] ;		// Same Wide Coefficient
		/* Initial Rate value = 1 */
	}
	else
	{
		UlGyrZmx	= ClGyxZom[ UcZoomStepDat ] ;
		UlGyrZmy	= ClGyyZom[ UcZoomStepDat ] ;
		
		
	}
	
	// Zoom Value Setting
	RamWrite32A( gxlens, UlGyrZmx ) ;		/* 0x1022 */
	RamWrite32A( gylens, UlGyrZmy ) ;		/* 0x1122 */

	RamRead32A( gxlens, &UlGyrZrx ) ;		/* 0x1022 */
	RamRead32A( gylens, &UlGyrZry ) ;		/* 0x1122 */

	// Zoom Value Setting Error Check
	if( UlGyrZmx != UlGyrZrx ) {
		RamWrite32A( gxlens, UlGyrZmx ) ;		/* 0x1022 */
	}

	if( UlGyrZmy != UlGyrZry ) {
		RamWrite32A( gylens, UlGyrZmy ) ;		/* 0x1122 */
	}

}

//********************************************************************************
// Function Name 	: SetPanTiltMode_PRMX
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Pan-Tilt Enable/Disable
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void	SetPanTiltMode_PRMX( unsigned char UcPnTmod )
{
	switch ( UcPnTmod ) {
		case OFF :
			RegWriteA( WG_PANON, 0x00 ) ;			// 0x0109	X,Y Pan/Tilt Function OFF
			break ;
		case ON :
			RegWriteA( WG_PANON, 0x01 ) ;			// 0x0109	X,Y Pan/Tilt Function ON
			break ;
	}

}

#ifdef H1COEF_CHANGER
//********************************************************************************
// Function Name 	: SetH1cMod_PRMX
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: Set H1C coefficient Level chang Function
// History			: First edition 						2013.04.18 Y.Shigeoka
//********************************************************************************
void	SetH1cMod_PRMX( unsigned char	UcSetNum )
{
	
	switch( UcSetNum ){
	case ( ACTMODE_PRMX ):				// initial 
		IniPtMovMod_PRMX( OFF ) ;							// Pan/Tilt setting (Still)
		
		/* enable setting */
			
		UcH1LvlMod_PRMX = UcSetNum ;
		
		// Limit value Value Setting
 #ifdef	CORRECT_1DEG
		RamWrite32A( gxlmt6L, MINLMT_W ) ;		/* 0x102D L-Limit */
		RamWrite32A( gxlmt6H, MAXLMT_W ) ;		/* 0x102E H-Limit */

		RamWrite32A( gylmt6L, MINLMT_W ) ;		/* 0x112D L-Limit */
		RamWrite32A( gylmt6H, MAXLMT_W ) ;		/* 0x112E H-Limit */

		RamWrite32A( gxmg, 		CHGCOEF_W ) ;		/* 0x10AA Change coefficient gain */
		RamWrite32A( gymg, 		CHGCOEF_W ) ;		/* 0x11AA Change coefficient gain */
 #else
		RamWrite32A( gxlmt6L, MINLMT_PRIMAX ) ;		/* 0x102D L-Limit */
		RamWrite32A( gxlmt6H, MAXLMT_PRIMAX ) ;		/* 0x102E H-Limit */

		RamWrite32A( gylmt6L, MINLMT_PRIMAX ) ;		/* 0x112D L-Limit */
		RamWrite32A( gylmt6H, MAXLMT_PRIMAX ) ;		/* 0x112E H-Limit */

		RamWrite32A( gxmg, 		CHGCOEF_PRIMAX ) ;		/* 0x10AA Change coefficient gain */
		RamWrite32A( gymg, 		CHGCOEF_PRIMAX ) ;		/* 0x11AA Change coefficient gain */
 #endif
		RamWrite32A( gxhc_tmp, 	DIFIL_S2_PRMX ) ;	/* 0x100E Base Coef */
		RamWrite32A( gyhc_tmp, 	DIFIL_S2_PRMX ) ;	/* 0x110E Base Coef */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
		
	case( S2MODE_PRMX ):				// cancel lvl change mode 
		RegWriteA( WG_HCHR, 0x10 ) ;			// 0x011B	GmHChrOn[1]=0 Sw OFF
		break ;
		
	case( MOVMODE_PRMX ):			// Movie mode 
		IniPtMovMod_PRMX( ON ) ;						// Pan/Tilt setting (Movie)
		SelectPtRange_PRMX( OFF ) ;					// Range narrow
		SelectIstpMod_PRMX( OFF ) ;					// Range narrow
		
		RamWrite32A( gxlmt6L, MINLMT_MOV_PRIMAX ) ;	/* 0x102D L-Limit */
		RamWrite32A( gylmt6L, MINLMT_MOV_PRIMAX ) ;	/* 0x112D L-Limit */

		RamWrite32A( gxlmt6H, MAXLMT_PRIMAX ) ;		/* 0x102E H-Limit */
		RamWrite32A( gylmt6H, MAXLMT_PRIMAX ) ;		/* 0x112E H-Limit */
		
		RamWrite32A( gxmg, CHGCOEF_MOV_PRIMAX ) ;		/* 0x10AA Change coefficient gain */
		RamWrite32A( gymg, CHGCOEF_MOV_PRIMAX ) ;		/* 0x11AA Change coefficient gain */
		RamWrite32A( gxhc_tmp, DIFIL_S2_PRMX ) ;		/* 0x100E Base Coef */
		RamWrite32A( gyhc_tmp, DIFIL_S2_PRMX ) ;		/* 0x110E Base Coef */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
		
	case( MOVMODE_W_PRMX ):			// Movie mode (wide)
		IniPtMovMod_PRMX( ON ) ;							// Pan/Tilt setting (Movie)
		SelectPtRange_PRMX( ON ) ;					// Range wide
		SelectIstpMod_PRMX( ON ) ;					// Range wide
		
		RamWrite32A( gxlmt6L, MINLMT_MOV_W ) ;	/* 0x102D L-Limit */
		RamWrite32A( gylmt6L, MINLMT_MOV_W ) ;	/* 0x112D L-Limit */

		RamWrite32A( gxlmt6H, MAXLMT_W ) ;		/* 0x102E H-Limit */
		RamWrite32A( gylmt6H, MAXLMT_W ) ;		/* 0x112E H-Limit */
		
		RamWrite32A( gxmg, CHGCOEF_MOV_W ) ;		/* 0x10AA Change coefficient gain */
		RamWrite32A( gymg, CHGCOEF_MOV_W ) ;		/* 0x11AA Change coefficient gain */
			
		RamWrite32A( gxhc_tmp, DIFIL_S2_PRMX ) ;		/* 0x100E Base Coef */
		RamWrite32A( gyhc_tmp, DIFIL_S2_PRMX ) ;		/* 0x110E Base Coef */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
		
	case( STILLMODE_PRMX ):				// Still mode 
		IniPtMovMod_PRMX( OFF ) ;							// Pan/Tilt setting (Still)
		SelectPtRange_PRMX( OFF ) ;					// Range narrow
		SelectIstpMod_PRMX( OFF ) ;					// Range narrow
		
		UcH1LvlMod_PRMX = UcSetNum ;
			
		RamWrite32A( gxlmt6L, MINLMT_PRIMAX ) ;		/* 0x102D L-Limit */
		RamWrite32A( gylmt6L, MINLMT_PRIMAX ) ;		/* 0x112D L-Limit */
		
		RamWrite32A( gxlmt6H, MAXLMT_PRIMAX ) ;		/* 0x102E H-Limit */
		RamWrite32A( gylmt6H, MAXLMT_PRIMAX ) ;		/* 0x112E H-Limit */
		
		RamWrite32A( gxmg, 	CHGCOEF_PRIMAX ) ;			/* 0x10AA Change coefficient gain */
		RamWrite32A( gymg, 	CHGCOEF_PRIMAX ) ;			/* 0x11AA Change coefficient gain */
			
		RamWrite32A( gxhc_tmp, DIFIL_S2_PRMX ) ;		/* 0x100E Base Coef */
		RamWrite32A( gyhc_tmp, DIFIL_S2_PRMX ) ;		/* 0x110E Base Coef */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
		
	case( STILLMODE_W_PRMX ):			// Still mode (Wide)
		IniPtMovMod_PRMX( OFF ) ;							// Pan/Tilt setting (Still)
		SelectPtRange_PRMX( ON ) ;					// Range wide
		SelectIstpMod_PRMX( ON ) ;					// Range wide
		
		UcH1LvlMod_PRMX = UcSetNum ;
			
		RamWrite32A( gxlmt6L, MINLMT_W ) ;		/* 0x102D L-Limit */
		RamWrite32A( gylmt6L, MINLMT_W ) ;		/* 0x112D L-Limit */
		
		RamWrite32A( gxlmt6H, MAXLMT_W ) ;		/* 0x102E H-Limit */
		RamWrite32A( gylmt6H, MAXLMT_W ) ;		/* 0x112E H-Limit */
		
		RamWrite32A( gxmg, 	CHGCOEF_W ) ;			/* 0x10AA Change coefficient gain */
		RamWrite32A( gymg, 	CHGCOEF_W ) ;			/* 0x11AA Change coefficient gain */
			
		RamWrite32A( gxhc_tmp, DIFIL_S2_PRMX ) ;		/* 0x100E Base Coef */
		RamWrite32A( gyhc_tmp, DIFIL_S2_PRMX ) ;		/* 0x110E Base Coef */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
		
	default :
		IniPtMovMod_PRMX( OFF ) ;							// Pan/Tilt setting (Still)
		SelectPtRange_PRMX( OFF ) ;					// Range narrow
		SelectIstpMod_PRMX( OFF ) ;					// Range narrow
		
		UcH1LvlMod_PRMX = UcSetNum ;
			
		RamWrite32A( gxlmt6L, MINLMT_PRIMAX ) ;		/* 0x102D L-Limit */
		RamWrite32A( gylmt6L, MINLMT_PRIMAX ) ;		/* 0x112D L-Limit */
		
		RamWrite32A( gxlmt6H, MAXLMT_PRIMAX ) ;		/* 0x102E H-Limit */
		RamWrite32A( gylmt6H, MAXLMT_PRIMAX ) ;		/* 0x112E H-Limit */
		
		RamWrite32A( gxmg, 	CHGCOEF_PRIMAX ) ;			/* 0x10AA Change coefficient gain */
		RamWrite32A( gymg, 	CHGCOEF_PRIMAX ) ;			/* 0x11AA Change coefficient gain */
			
		RamWrite32A( gxhc_tmp, DIFIL_S2_PRMX ) ;		/* 0x100E Base Coef */
		RamWrite32A( gyhc_tmp, DIFIL_S2_PRMX ) ;		/* 0x110E Base Coef */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
	}
}
#endif

//********************************************************************************
// Function Name 	: StbOnn_PRMX
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Stabilizer For Servo On Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
 
void StbOnn_PRMX( void )
{
	unsigned char	UcRegValx,UcRegValy;					// Registor value 
	unsigned char	UcRegIni ;
	unsigned char	UcRegIniCnt = 0;
	
	RegReadA( WH_EQSWX , &UcRegValx ) ;			// 0x0170
	RegReadA( WH_EQSWY , &UcRegValy ) ;			// 0x0171
	
	if( (( UcRegValx & 0x01 ) != 0x01 ) && (( UcRegValy & 0x01 ) != 0x01 ))
	{
		
		RegWriteA( WH_SMTSRVON,	0x01 ) ;				// 0x017C		Smooth Servo ON
		
		SrvCon_PRMX( X_DIR_PRMX, ON ) ;
		SrvCon_PRMX( Y_DIR_PRMX, ON ) ;
		
		UcRegIni = 0x11;
		while( (UcRegIni & 0x77) != 0x66 )
		{
			RegReadA( RH_SMTSRVSTT,	&UcRegIni ) ;		// 0x01F8		Smooth Servo phase read
			
			if( CmdRdChk() !=0 )	break;				// Dead Lock check (responce check)
			if((UcRegIni & 0x77 ) == 0 )	UcRegIniCnt++ ;
			if( UcRegIniCnt > 10 ){
				break ;			// Status Error
			}
			
		}
		RegWriteA( WH_SMTSRVON,	0x00 ) ;				// 0x017C		Smooth Servo OFF
		
	}
	else
	{
		SrvCon_PRMX( X_DIR_PRMX, ON ) ;
		SrvCon_PRMX( Y_DIR_PRMX, ON ) ;
	}
}

//********************************************************************************
// Function Name 	: SrvCon_PRMX
// Retun Value		: NON
// Argment Value	: X or Y Select, Servo ON/OFF
// Explanation		: Servo ON,OFF Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void	SrvCon_PRMX( unsigned char	UcDirSel, unsigned char	UcSwcCon )
{
	if( UcSwcCon ) {
		if( !UcDirSel ) {						// X Direction
			RegWriteA( WH_EQSWX , 0x03 ) ;			// 0x0170
			RamWrite32A( sxggf, 0x00000000 ) ;		// 0x10B5
		} else {								// Y Direction
			RegWriteA( WH_EQSWY , 0x03 ) ;			// 0x0171
			RamWrite32A( syggf, 0x00000000 ) ;		// 0x11B5
		}
	} else {
		if( !UcDirSel ) {						// X Direction
			RegWriteA( WH_EQSWX , 0x02 ) ;			// 0x0170
			RamWrite32A( SXLMT, 0x00000000 ) ;		// 0x1477
		} else {								// Y Direction
			RegWriteA( WH_EQSWY , 0x02 ) ;			// 0x0171
			RamWrite32A( SYLMT, 0x00000000 ) ;		// 0x14F7
		}
	}
}

//********************************************************************************
// Function Name 	: CmdRdChk
// Retun Value		: 1 : ERROR
// Argment Value	: NON
// Explanation		: Check Cver function
// History			: First edition 						2014.02.27 K.abe
//********************************************************************************

unsigned char CmdRdChk( void )
{
	unsigned char UcTestRD;
	unsigned char UcCount;
	
	for(UcCount=0; UcCount < READ_COUNT_NUM; UcCount++){
		RegReadA( TESTRD ,	&UcTestRD );					// 0x027F
		if( UcTestRD == 0xAC){
			return(0);
		}
	}
	return(1);
}

int	E2pDat_Primax_Lenovo( uint8_t * memory_data)
{

	pr_err("ois here eeprom HALL_BIAS_X = 0x%x   memory_data=%p memory_data[2112]=0x%X\n",( unsigned short )*(memory_data+HALL_BIAS_X),memory_data,memory_data[2112]);

	if (memory_data)
	{
		MemClr( ( unsigned char * )&StAdjPar_PRMX, sizeof( stAdjPar_PRMX ) ) ;
	    StAdjPar_PRMX.StHalAdj.UsHlxGan  = ( unsigned short )(((*(memory_data+HALL_BIAS_X))<<8)|(*(memory_data+HALL_BIAS_X+1)));
	    pr_err("ois UsHlxGan = 0x%x\n",StCalDat.StHalAdj.UsHlxGan);

	    StAdjPar_PRMX.StHalAdj.UsHlyGan  = ( unsigned short )(((*(memory_data+HALL_BIAS_Y))<<8)|(*(memory_data+HALL_BIAS_Y+1)));
	    pr_err("ois UsHlyGan = 0x%x\n",StCalDat.StHalAdj.UsHlyGan);

	    StAdjPar_PRMX.StHalAdj.UsHlxOff  = ( unsigned short )(((*(memory_data+HALL_OFFSET_X))<<8)|(*(memory_data+HALL_OFFSET_X+1)));
	    pr_err("ois UsHlxOff = 0x%x\n",StCalDat.StHalAdj.UsHlxOff);

	    StAdjPar_PRMX.StHalAdj.UsHlyOff  = ( unsigned short )(((*(memory_data+HALL_OFFSET_Y))<<8)|(*(memory_data+HALL_OFFSET_Y+1)));
	    pr_err("ois UsHlyOff = 0x%x\n",StCalDat.StHalAdj.UsHlyOff);

	    StAdjPar_PRMX.StLopGan.UsLxgVal  = ( unsigned short )(((*(memory_data+LOOP_GAIN_X))<<8)|(*(memory_data+LOOP_GAIN_X+1)));
	    pr_err("ois UsLxgVal = 0x%x\n",StCalDat.StLopGan.UsLxgVal);

	    StAdjPar_PRMX.StLopGan.UsLygVal  = ( unsigned short )(((*(memory_data+LOOP_GAIN_Y))<<8)|(*(memory_data+LOOP_GAIN_Y+1)));
	    pr_err("ois UsLygVal = 0x%x\n",StCalDat.StLopGan.UsLygVal);

	    StAdjPar_PRMX.StHalAdj.UsAdxOff  = ( unsigned short )((*(memory_data+LENS_CENTER_FINAL_X)<<8)|(*(memory_data+LENS_CENTER_FINAL_X+1)));
	    pr_err("ois UsLsxVal = 0x%x\n",StCalDat.StLenCen.UsLsxVal);

	    StAdjPar_PRMX.StHalAdj.UsAdyOff   = ( unsigned short )(((*(memory_data+LENS_CENTER_FINAL_Y))<<8)|(*(memory_data+LENS_CENTER_FINAL_Y+1)));
	    pr_err("ois UsLsyVal = 0x%x\n",StCalDat.StLenCen.UsLsyVal);

	    StAdjPar_PRMX.StGvcOff.UsGxoVal  = ( unsigned short )(((*(memory_data+GYRO_AD_OFFSET_X))<<8)|(*(memory_data+GYRO_AD_OFFSET_X+1)));
	    pr_err("ois UsGxoVal = 0x%x\n",StCalDat.StGvcOff.UsGxoVal);

	    StAdjPar_PRMX.StGvcOff.UsGyoVal  = ( unsigned short )(((*(memory_data+GYRO_AD_OFFSET_Y))<<8)|(*(memory_data+GYRO_AD_OFFSET_Y+1)));
	    pr_err("ois UsGyoVal = 0x%x\n",StCalDat.StGvcOff.UsGyoVal);

	    StAdjPar_PRMX.UcOscVal  = ( unsigned char )(*(memory_data+OSC_CLK_VAL));
	    pr_err("ois UcOscVal = 0x%x\n",StCalDat.UcOscVal);

//	    StAdjPar_PRMX.UsAdjHallF  = ( unsigned short )(((*(memory_data+ADJ_HALL_FLAG))<<8)|(*(memory_data+ADJ_HALL_FLAG+1)));
//	    pr_err("ois UsAdjHallF = 0x%x\n",StCalDat.UsAdjHallF);



//	    StAdjPar_PRMX.UsAdjGyroF  = ( unsigned short )((*(memory_data+ADJ_GYRO_FLAG)<<8)|(*(memory_data+ADJ_GYRO_FLAG+1)));
//	    pr_err("ois UsAdjGyroF = 0x%x\n",StCalDat.UsAdjGyroF);

//	    StAdjPar_PRMX.UsAdjLensF  = ( unsigned short )((*(memory_data+ADJ_LENS_FLAG)<<8)|(*(memory_data+ADJ_LENS_FLAG+1)));
//	    pr_err("ois UsAdjLensF = 0x%x\n",StCalDat.UsAdjLensF);

	    StAdjPar_PRMX.StGyrGan.UlGxgVal   = ( unsigned long )((*(memory_data+GYRO_GAIN_X)<<24)|(*(memory_data+GYRO_GAIN_X+1)<<16)|(*(memory_data+GYRO_GAIN_X+2)<<8)|(*(memory_data+GYRO_GAIN_X+3)));
	    pr_err("ois UlGxgVal = 0x%lx\n",StCalDat.UlGxgVal);

	    StAdjPar_PRMX.StGyrGan.UlGygVal  = ( unsigned long )((*(memory_data+GYRO_GAIN_Y)<<24)|(*(memory_data+GYRO_GAIN_Y+1)<<16)|(*(memory_data+GYRO_GAIN_Y+2)<<8)|(*(memory_data+GYRO_GAIN_Y+3)));
	    pr_err("ois UlGygVal = 0x%lx\n",StCalDat.UlGygVal);
//	    StAdjPar_PRMX.UsVerDat  = ( unsigned short )((*(memory_data+FW_VERSION_INFO)<<8)|(*(memory_data+FW_VERSION_INFO+1)));
//	    pr_err("ois UsVerDat = 0x%x\n",StCalDat.UsVerDat);
	}
	return 0;
}

