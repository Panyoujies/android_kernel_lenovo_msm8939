//********************************************************************************
//
//		<< LC898122 Evaluation Soft>>
//		Program Name	: Ois.h
// 		Explanation		: LC898122 Global Declaration & ProtType Declaration
//		Design			: Y.Yamada
//		History			: First edition						2009.07.30 Y.Tashita
//********************************************************************************

#ifdef	OISINI
	#define	OISINI__
#else
	#define	OISINI__		extern
#endif







#ifdef	OISCMD
	#define	OISCMD__
#else
	#define	OISCMD__		extern
#endif


// Define According To Usage

/****************************** Define説明 ******************************/
/*	USE_3WIRE_DGYRO		Digital Gyro I/F 3線Mode使用					*/
/*	USE_INVENSENSE		Invensense Digital Gyro使用						*/
/*		USE_IDG2020		Inv IDG-2020使用								*/
/*	STANDBY_MODE		Standby制御使用(未確認)							*/
/************************************************************************/

/**************** Select Gyro Sensor **************/
//#define 	USE_3WIRE_DGYRO    //for D-Gyro SPI interface

#define		USE_INVENSENSE		// INVENSENSE
#ifdef USE_INVENSENSE
//			#define		FS_SEL_PRMX		0		/* ±262LSB/°/s  */
//			#define		FS_SEL_PRMX		1		/* ±131LSB/°/s  */
//			#define		FS_SEL_PRMX		2		/* ±65.5LSB/°/s  */
			#define		FS_SEL_PRMX		3		/* ±32.8LSB/°/s  */

//			#define		GYROSTBY			/* Sleep+STBY */
#endif

/**************** Model name *****************/
#define		MN_T0401300
/**************** FW version *****************/
#ifdef	MN_T0401300
 #define	MDL_VER			0x0F
 #define	FW_VER			0x03
#endif

/**************** Select Mode **************/
#define		STANDBY_MODE		// STANDBY Mode

#ifdef	MN_T0401300
 #define		ACTREG_LV1A5		// Use 10.9ohm	LV-1A5
 #define		CORRECT_1DEG			// Correct 1deg   disable 0.5deg
 #define		PWM_BREAK			// PWM mode select (disable standby)
 #define		IDG2030			// Disable is IDG-2021
#endif

//#define		DEF_SET				// default value re-setting
#define		NEUTRAL_CENTER		// Upper Position Current 0mA Measurement
#define		H1COEF_CHANGER			/* H1 coef lvl chage */
#define		MONITOR_OFF			// default Monitor output
//#define		MODULE_CALIBRATION		// for module maker   use float
//#define		ACCEPTANCE					// Examination of Acceptance



// Command Status
#define		EXE_END_PRMX		0x02		// Execute End (Adjust OK)
#define		EXE_HXADJ_PRMX		0x06		// Adjust NG : X Hall NG (Gain or Offset)
#define		EXE_HYADJ_PRMX		0x0A		// Adjust NG : Y Hall NG (Gain or Offset)
#define		EXE_LXADJ_PRMX		0x12		// Adjust NG : X Loop NG (Gain)
#define		EXE_LYADJ_PRMX		0x22		// Adjust NG : Y Loop NG (Gain)
#define		EXE_GXADJ_PRMX		0x42		// Adjust NG : X Gyro NG (offset)
#define		EXE_GYADJ_PRMX		0x82		// Adjust NG : Y Gyro NG (offset)
#define		EXE_OCADJ_PRMX		0x402		// Adjust NG : OSC Clock NG
#define		EXE_ERR_PRMX		0x99		// Execute Error End

#ifdef	ACCEPTANCE
 // Hall Examination of Acceptance
 #define		EXE_HXMVER	0x06		// X Err
 #define		EXE_HYMVER	0x0A		// Y Err
 
 // Gyro Examination of Acceptance
 #define		EXE_GXABOVE	0x06		// X Above
 #define		EXE_GXBELOW	0x0A		// X Below
 #define		EXE_GYABOVE	0x12		// Y Above
 #define		EXE_GYBELOW	0x22		// Y Below
#endif	//ACCEPTANCE

#ifdef	MODULE_CALIBRATION
// Common Define
#define	SUCCESS			0x00		// Success
#define	FAILURE			0x01		// Failure
#endif	//MODULE_CALIBRATION

#ifndef ON
 #define	ON				0x01		// ON
 #define	OFF				0x00		// OFF
#endif	//ON
 #define	SPC_PRMX				0x02		// Special Mode


#define	X_DIR_PRMX			0x00		// X Direction
#define	Y_DIR_PRMX			0x01		// Y Direction
#ifdef	MODULE_CALIBRATION
#define	X2_DIR			0x10		// X Direction
#define	Y2_DIR			0x11		// Y Direction
#endif	//MODULE_CALIBRATION

#define	NOP_TIME		0.00004166F

#ifdef STANDBY_MODE
 // Standby mode
 #define		STB1_ON		0x00		// Standby1 ON
 #define		STB1_OFF	0x01		// Standby1 OFF
 #define		STB2_ON		0x02		// Standby2 ON
 #define		STB2_OFF	0x03		// Standby2 OFF
 #define		STB3_ON		0x04		// Standby3 ON
 #define		STB3_OFF	0x05		// Standby3 OFF
 #define		STB4_ON		0x06		// Standby4 ON			/* for Digital Gyro Read */
 #define		STB4_OFF	0x07		// Standby4 OFF
 #define		STB2_OISON	0x08		// Standby2 ON (only OIS)
 #define		STB2_OISOFF	0x09		// Standby2 OFF(only OIS)
 #define		STB2_AFON	0x0A		// Standby2 ON (only AF)
 #define		STB2_AFOFF	0x0B		// Standby2 OFF(only AF)
#endif


// OIS Adjust Parameter
 #define		DAHLXO_INI_PRMX		0x0000
 #define		DAHLXB_INI_PRMX		0xE000
 #define		DAHLYO_INI_PRMX		0x0000
 #define		DAHLYB_INI_PRMX		0xE000
 #define		SXGAIN_INI_PRMX		0x3000
 #define		SYGAIN_INI_PRMX		0x3000
 #define		HXOFF0Z_INI_PRMX		0x0000
 #define		HYOFF1Z_INI_PRMX		0x0000

#ifdef ACTREG_LV1A5		// MTM 10.5/10.9ohm Actuator LV-1A5 ***************************
	 #define		BIAS_CUR_OIS_PRMX	0x44		//3.0mA/3.0mA
	 #define		AMP_GAIN_X_PRMX		0x03		//x75
	 #define		AMP_GAIN_Y_PRMX		0x03		//x75
/* OSC Init */
 #define		OSC_INI_PRMX			0x2E		/* VDD=2.8V */

/* AF Open para */
 #define		RWEXD1_L_AF_PRMX		0x7FFF		//
 #define		RWEXD2_L_AF_PRMX		0x1ACD		//
 #define		RWEXD3_L_AF_PRMX		0x71E0		//
// #define		FSTCTIME_AF_PRMX		0xF9		//
// #define		OPAFDIV_AF_PRMX		0x04		//
 #define		FSTCTIME_AF_PRMX		0xA4		//
 #define		OPAFDIV_AF_PRMX		0x06		//

 #define		FSTMODE_AF_PRMX		0x00		//

 /* (0.425X^3+0.55X)*(0.425X^3+0.55X) 10.9ohm*/
 #define		A3_IEXP3		0x3ED9999A
 #define		A1_IEXP1		0x3F0CCCCD
 
#endif

/* AF adjust parameter */
#define		DAHLZB_INI_PRMX		0x8001
#define		DAHLZO_INI_PRMX		0x0000
#define		BIAS_CUR_AF_PRMX		0x00		//0.25mA
#define		AMP_GAIN_AF_PRMX		0x00		//x6

// Digital Gyro offset Initial value 
#define		DGYRO_OFST_XH_PRMX	0x00
#define		DGYRO_OFST_XL_PRMX	0x00
#define		DGYRO_OFST_YH_PRMX	0x00
#define		DGYRO_OFST_YL_PRMX	0x00

#ifdef	MODULE_CALIBRATION
#define		SXGAIN_LOP		0x3000
#define		SYGAIN_LOP		0x3000
#endif	//MODULE_CALIBRATION

#define		TCODEH_ADJ		0x0000

 /************* Wide *************/
 #define		GYRLMT3_S1_W_PRMX	0x3F0CCCCD		//0.55F
 #define		GYRLMT3_S2_W_PRMX	0x3F0CCCCD		//0.55F

 #define		GYRLMT4_S1_W_PRMX	0x405147AE		//3.27F
 #define		GYRLMT4_S2_W_PRMX	0x405147AE		//3.27F

 #define		GYRISTP_W_PRMX		0x38D1B700		/* -80dB */

 #define		GYRA12_HGH_W_PRMX	0x40400000		/* 3.0F */
 
 /************* Narrow *************/
 #define		GYRLMT3_S1_PRMX		0x3ECCCCCD		//0.40F
 #define		GYRLMT3_S2_PRMX		0x3ECCCCCD		//0.40F

 #define		GYRLMT4_S1_PRMX		0x40000000		//2.0F
 #define		GYRLMT4_S2_PRMX		0x40000000		//2.0F

 #define		GYRISTP_PRMX			0x38D1B700		/* -80dB */

 #define		GYRA12_HGH_PRMX		0x3FE00000		/* 1.75F */

 /**********************************/
#define		GYRLMT1H_PRMX		0x3F800000		//1.0F

#define		GYRA12_MID_PRMX		0x3DCCCCC0		/* 0.1F */
#define		GYRA34_HGH_PRMX		0x3F000000		/* 0.5F */
#define		GYRA34_MID_PRMX		0x3DCCCCC0		/* 0.1F */

#define		GYRB12_HGH_PRMX		0x3E4CCCCD		/* 0.20F */
#define		GYRB12_MID_PRMX		0x3CA3D70A		/* 0.02F */
#define		GYRB34_HGH_PRMX		0x3CA3D70A		/* 0.02F */
#define		GYRB34_MID_PRMX		0x3C23D70A		/* 0.001F */


//#define		OPTCEN_X		0x0000
//#define		OPTCEN_Y		0x0000

#ifdef USE_INVENSENSE
 #ifdef	MN_T0401300
  #define		SXQ_INI_PRMX			0xBF800000
  #define		SYQ_INI_PRMX			0xBF800000

  #define		GXGAIN_INI_PRMX		0x3F147AE1
  #define		GYGAIN_INI_PRMX		0xBF147AE1

  #define		GYROX_INI_PRMX		0x45
  #define		GYROY_INI_PRMX		0x43
  
  #define		GXHY_GYHX_PRMX		0
  
  #define		G_45G_INI_PRMX		0x3EBFED46		// 32.8/87.5=0.3748...
 #endif
#endif


/* Optical Center & Gyro Gain for Mode */
 #define	VAL_SET				0x00		// Setting mode
 #define	VAL_FIX				0x01		// Fix Set value
 #define	VAL_SPC				0x02		// Special mode


struct STFILREG_PRMX {
	unsigned short	UsRegAdd ;
	unsigned char	UcRegDat ;
} ;													// Register Data Table
struct STFILRAM_PRMX {
	unsigned short	UsRamAdd ;
	unsigned long	UlRamDat ;
} ;													// Filter Coefficient Table

struct STCMDTBL_PRMX
{
	unsigned short Cmd ;
	unsigned int UiCmdStf ;
	void ( *UcCmdPtr )( void ) ;
} ;

/*** caution [little-endian] ***/

// Word Data Union
union	WRDVAL{
	unsigned short	UsWrdVal ;
	unsigned char	UcWrkVal[ 2 ] ;
	struct {
		unsigned char	UcLowVal ;
		unsigned char	UcHigVal ;
	} StWrdVal ;
} ;

typedef union WRDVAL	UnWrdVal ;

union	DWDVAL {
	unsigned long	UlDwdVal ;
	unsigned short	UsDwdVal[ 2 ] ;
	struct {
		unsigned short	UsLowVal ;
		unsigned short	UsHigVal ;
	} StDwdVal ;
	struct {
		unsigned char	UcRamVa0 ;
		unsigned char	UcRamVa1 ;
		unsigned char	UcRamVa2 ;
		unsigned char	UcRamVa3 ;
	} StCdwVal ;
} ;

typedef union DWDVAL	UnDwdVal;

#ifdef	MODULE_CALIBRATION
// Float Data Union
union	FLTVAL {
	float			SfFltVal ;
	unsigned long	UlLngVal ;
	unsigned short	UsDwdVal[ 2 ] ;
	struct {
		unsigned short	UsLowVal ;
		unsigned short	UsHigVal ;
	} StFltVal ;
} ;

typedef union FLTVAL	UnFltVal ;
#endif	//MODULE_CALIBRATION


typedef struct STADJPAR_PRMX {
	struct {
		unsigned char	UcAdjPhs ;				// Hall Adjust Phase

		unsigned short	UsHlxCna ;				// Hall Center Value after Hall Adjust
		unsigned short	UsHlxMax ;				// Hall Max Value
		unsigned short	UsHlxMxa ;				// Hall Max Value after Hall Adjust
		unsigned short	UsHlxMin ;				// Hall Min Value
		unsigned short	UsHlxMna ;				// Hall Min Value after Hall Adjust
		unsigned short	UsHlxGan ;				// Hall Gain Value
		unsigned short	UsHlxOff ;				// Hall Offset Value
		unsigned short	UsAdxOff ;				// Hall A/D Offset Value
		unsigned short	UsHlxCen ;				// Hall Center Value

		unsigned short	UsHlyCna ;				// Hall Center Value after Hall Adjust
		unsigned short	UsHlyMax ;				// Hall Max Value
		unsigned short	UsHlyMxa ;				// Hall Max Value after Hall Adjust
		unsigned short	UsHlyMin ;				// Hall Min Value
		unsigned short	UsHlyMna ;				// Hall Min Value after Hall Adjust
		unsigned short	UsHlyGan ;				// Hall Gain Value
		unsigned short	UsHlyOff ;				// Hall Offset Value
		unsigned short	UsAdyOff ;				// Hall A/D Offset Value
		unsigned short	UsHlyCen ;				// Hall Center Value
	} StHalAdj ;

#ifdef	MODULE_CALIBRATION
	struct {
		unsigned short	UsLxgVal ;				// Loop Gain X
		unsigned short	UsLygVal ;				// Loop Gain Y
		unsigned short	UsLxgSts ;				// Loop Gain X Status
		unsigned short	UsLygSts ;				// Loop Gain Y Status
	} StLopGan ;

	struct {
		unsigned short	UsGxoVal ;				// Gyro A/D Offset X
		unsigned short	UsGyoVal ;				// Gyro A/D Offset Y
		unsigned short	UsGxoSts ;				// Gyro Offset X Status
		unsigned short	UsGyoSts ;				// Gyro Offset Y Status
	} StGvcOff ;
#else	// MODULE_CALIBRATION
	struct {
		unsigned short	UsLxgVal ;				// Loop Gain X
		unsigned short	UsLygVal ;				// Loop Gain Y	} StLopGan ;
	} StLopGan ;

	struct {
		unsigned short	UsGxoVal ;				// Gyro A/D Offset X
		unsigned short	UsGyoVal ;				// Gyro A/D Offset Y
	} StGvcOff ;
	
	struct {
		unsigned long	UlGxgVal ;				// Gyro gain X
		unsigned long	UlGygVal ;				// Gyro gain Y
	} StGyrGan ;
#endif	// MODULE_CALIBRATION
	unsigned char		UcOscVal ;				// OSC value

} stAdjPar_PRMX ;

OISCMD__	stAdjPar_PRMX	StAdjPar_PRMX ;				// Execute Command Parameter

#ifdef	MODULE_CALIBRATION
OISCMD__	unsigned char	UcOscAdjFlg ;		// For Measure trigger
  #define	MEASSTR		0x01
  #define	MEASCNT		0x08
  #define	MEASFIX		0x80
#endif	//MODULE_CALIBRATION

#ifdef	MODULE_CALIBRATION
OISINI__	unsigned short	UsCntXof ;				/* OPTICAL Center Xvalue */
OISINI__	unsigned short	UsCntYof ;				/* OPTICAL Center Yvalue */
#endif	//MODULE_CALIBRATION

OISINI__	unsigned char	UcPwmMod_PRMX ;				/* PWM MODE */
#define		PWMMOD_CVL_PRMX	0x00		// CVL PWM MODE
#define		PWMMOD_PWM_PRMX	0x01		// PWM MODE

#define		INIT_PWMMODE_PRMX	PWMMOD_CVL_PRMX		// initial output mode

OISINI__	unsigned char	UcCvrCod_PRMX ;				/* CverCode */
 #define	CVER122_PRMX		0x93		 // LC898122
 #define	CVER122A_PRMX		0xA1		 // LC898122A

//Top Function
OISINI__ int			IniSetTop( void ) ;								// Initial Top Function
OISINI__ unsigned char	RtnCenTop( unsigned char ) ;					// RtnCen Top Function
OISINI__ void			OisEnaTop( void ) ;								// OisEna Top Function
OISINI__ void			OisOffTop( void ) ;								// OisOff Top Function
OISINI__ void			SetH1cModTop( unsigned char ) ;					// SetH1cMod Top Function

// Prottype Declation
OISINI__ void	IniSetAll( void ) ;													// Initial Top Function
OISINI__ void	IniSetPrmx( void ) ;													// Initial Top Function
OISINI__ void	IniSetAfPrmx( void ) ;													// Initial Top Function

OISINI__ void	ClrGyr_PRMX( unsigned short, unsigned char ); 							   // Clear Gyro RAM
	#define CLR_FRAM0_PRMX		 	0x01
	#define CLR_FRAM1_PRMX 			0x02
	#define CLR_ALL_RAM_PRMX 		0x03
OISINI__ void	BsyWit_PRMX( unsigned short, unsigned char ) ;				// Busy Wait Function
//OISINI__ void	WitTim_PRMX( unsigned short ) ;											// Wait
OISINI__ void	MemClr_PRMX( unsigned char *, unsigned short ) ;							// Memory Clear Function
OISINI__ void	GyOutSignal_PRMX( void ) ;									// Slect Gyro Output signal Function
OISINI__ void	GyOutSignalCont_PRMX( void ) ;								// Slect Gyro Output Continuos Function
#ifdef STANDBY_MODE
OISINI__ void	AccWit_PRMX( unsigned char ) ;								// Acc Wait Function
OISINI__ void	SelectGySleep_PRMX( unsigned char ) ;						// Select Gyro Mode Function
#endif
OISINI__ void	AutoGainControlSw_PRMX( unsigned char ) ;							// Auto Gain Control Sw
OISINI__ void	DrvSw_PRMX( unsigned char UcDrvSw ) ;						// Driver Mode setting function
OISINI__ void	AfDrvSw_PRMX( unsigned char UcDrvSw ) ;						// AF Driver Mode setting function
OISINI__ void	RamAccFixMod_PRMX( unsigned char ) ;							// Ram Access Fix Mode setting function
OISINI__ void	IniPtMovMod_PRMX( unsigned char ) ;							// Pan/Tilt parameter setting by mode function
OISINI__ void	SelectPtRange_PRMX( unsigned char ) ;						// Pan/Tilt parameter Range function
OISINI__ void	SelectIstpMod_PRMX( unsigned char ) ;						// Pan/Tilt parameter Range function
OISINI__ void	ChkCvr_PRMX( void ) ;													// Check Function
OISINI__ void	StartUpGainContIni( void ) ;							// Start Up routine for Drift control
OISINI__ unsigned char	InitGainControl( unsigned char ) ;						// Start Up routine for Drift control
	
OISCMD__ void			SrvCon_PRMX( unsigned char, unsigned char ) ;					// Servo ON/OFF
OISCMD__ unsigned short	TneRun( void ) ;											// Hall System Auto Adjustment Function
OISCMD__ unsigned char	RtnCen_PRMX( unsigned char ) ;									// Return to Center Function
OISCMD__ void			GyrCon_PRMX( unsigned char ) ;								// Gyro Filter Control
OISCMD__ void			OisEna_PRMX( void ) ;											// OIS Enable Function
OISCMD__ void			OisEnaLin_PRMX( void ) ;											// OIS Enable Function for Line adjustment
OISCMD__ void  StbOnn_PRMX( void );
#ifdef	MODULE_CALIBRATION
OISCMD__ void			TimPro( void ) ;											// Timer Interrupt Process Function
#endif	//MODULE_CALIBRATION
OISCMD__ void			S2cPro_PRMX( unsigned char ) ;									// S2 Command Process Function
	#define		DIFIL_S2_PRMX		0x3F7FF800
#ifdef	MODULE_CALIBRATION
OISCMD__ void			SetSinWavePara( unsigned char , unsigned char ) ;			// Sin wave Test Function
	#define		SINEWAVE	0
	#define		XHALWAVE	1
	#define		YHALWAVE	2
	#define		XACTTEST	10
	#define		YACTTEST	11
	#define		CIRCWAVE	255
#endif	//MODULE_CALIBRATION

OISCMD__ unsigned char	TneGvc( void ) ;											// Gyro VC Offset Adjust

OISCMD__ void			SetZsp( unsigned char ) ;									// Set Zoom Step parameter Function
#ifdef	MODULE_CALIBRATION
OISCMD__ void			OptCen( unsigned char, unsigned short, unsigned short ) ;	// Set Optical Center adjusted value Function
#endif	//MODULE_CALIBRATION
OISCMD__ void			StbOnnN_PRMX( unsigned char , unsigned char ) ;					// Stabilizer For Servo On Function
#ifdef	MODULE_CALIBRATION
OISCMD__ unsigned char	LopGan( unsigned char ) ;									// Loop Gain Adjust
#endif
#ifdef STANDBY_MODE
 OISCMD__ void			SetStandby_PRMX( unsigned char ) ;								/* Standby control	*/
#endif
#ifdef	MODULE_CALIBRATION
OISCMD__ unsigned short	OscAdj( void ) ;											/* OSC clock adjustment */

OISCMD__ void			GyrGan( unsigned char , unsigned long , unsigned long ) ;	/* Set Gyro Gain Function */
#endif
OISCMD__ void			SetPanTiltMode_PRMX( unsigned char ) ;							/* Pan_Tilt control Function */
 OISCMD__ unsigned long	TnePtp( unsigned char, unsigned char ) ;					// Get Hall Peak to Peak Values
	#define		HALL_H_VAL	0x3F800000			/* 1.0 */
 OISCMD__ unsigned char	TneCen( unsigned char, UnDwdVal ) ;							// Tuning Hall Center
 #define		PTP_BEFORE		0
 #define		PTP_AFTER		1
 #define		PTP_ACCEPT		2
OISCMD__ unsigned char	TriSts_PRMX( void ) ;													// Read Status of Tripod mode Function
OISCMD__ unsigned char	DrvPwmSw_PRMX( unsigned char ) ;											// Select Driver mode Function
	#define		Mlnp_PRMX		0					// Linear PWM
	#define		Mpwm_PRMX		1					// PWM
 #ifdef	NEUTRAL_CENTER											// Gyro VC Offset Adjust
 OISCMD__ unsigned char	TneHvc( void ) ;											// Hall VC Offset Adjust
 #endif	//NEUTRAL_CENTER
OISCMD__ void			SetGcf_PRMX( unsigned char ) ;									// Set DI filter coefficient Function
//OISCMD__	unsigned long	UlH1Coefval ;		// H1 coefficient value
#ifdef H1COEF_CHANGER
 OISCMD__	unsigned char	UcH1LvlMod_PRMX ;		// H1 level coef mode
 OISCMD__	void			SetH1cMod_PRMX( unsigned char ) ;								// Set H1C coefficient Level chang Function
 #define		S2MODE_PRMX			0x40
 #define		ACTMODE_PRMX		0x80
 #define		MOVMODE_PRMX		0xFE
 #define		MOVMODE_W_PRMX		0xFF
 #ifdef	MODULE_CALIBRATION
 #define		STILLMODE_PRMX		0x00
 #define		STILLMODE_W_PRMX	0x01
 #else	//	MODULE_CALIBRATION
 #define		STILLMODE_PRMX		0x01
 #define		STILLMODE_W_PRMX	0x00
 #endif		//MODULE_CALIBRATION

#endif
OISCMD__	unsigned short	RdFwVr( void ) ;										// Read Fw Version Function

#ifdef	ACCEPTANCE
OISCMD__	unsigned char	RunHea( void ) ;										// Hall Examination of Acceptance
 #define		ACT_CHK_LVL		0x3ECCCCCD		// 0.4
 #define		ACT_THR			0x0400			// 28dB 20log(4/(0.4*256))
OISCMD__	unsigned char	RunGea( void ) ;										// Gyro Examination of Acceptance
 #define		GEA_DIF_HIG		0x0010
 #define		GEA_DIF_LOW		0x0001
#endif	//ACCEPTANCE
// Dead Lock Check
OISCMD__	unsigned char CmdRdChk( void );
	#define READ_COUNT_NUM	3
#ifdef	MODULE_CALIBRATION
 OISCMD__	void	MeasGyroAmp( unsigned char , unsigned char ) ;					// Gyro amp measurement Function
 OISCMD__		unsigned short		UsMeasPPX ;
 OISCMD__		unsigned short		UsMeasPPY ;
 OISCMD__		unsigned long		UlAngleX ;
 OISCMD__		unsigned long		UlAngleY ;
#endif
