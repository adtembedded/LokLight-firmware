/*
*
* This program is licensed under the Polyform Noncommercial License, version 1.0.0.
* You should have received a copy of the Polyform License. If not, refer to 
* https://polyformproject.org/licenses/noncommercial/1.0.0/
*
* Required Notice: 
* LokLight - Copyright (C) 2026 ADT Embedded (http://www.adte.nl)
*
*/

#ifndef DCC_DEFS_H
#define DCC_DEFS_H
//Contains common DCC definitions as per the standards 9.2 and 9.2.1

#include "loklight_wrapper.h"   // Types used on platform

// Conversion table for 28 step speed table
// The speed information has format [C] [S3] [S2] [S1] [S0]
// In 28-step mode, C is part of the speed step as LSB
// In 14-step mode, C denotes whether F0 (or FL, light) is on or off (C=1 means F0 on, C=0 means F0 off). It can use the the S bits tho.
typedef enum DccSpeedTable28_e : uint8_t {
    dcc_28ss_stop_dir = 0b00000,    //Stop command, direction bit must be observed
    dcc_28ss_stop_all = 0b10000,    //Stop command, direction bit may be ignored
    dcc_28ss_estop_dir = 0b00001,   //Emergency stop command, direction bit must be observed
    dcc_28ss_estop_all = 0b10001,   //Emergency stop command, direction bit may be ignored
    dcc_28ss_step_1 = 0b00010,
    dcc_28ss_step_2 = 0b10010,
    dcc_28ss_step_3 = 0b00011,
    dcc_28ss_step_4 = 0b10011,
    dcc_28ss_step_5 = 0b00100,
    dcc_28ss_step_6 = 0b10100,
    dcc_28ss_step_7 = 0b00101,
    dcc_28ss_step_8 = 0b10101,
    dcc_28ss_step_9 = 0b00110,
    dcc_28ss_step_10 = 0b10110,
    dcc_28ss_step_11 = 0b00111,
    dcc_28ss_step_12 = 0b10111,
    dcc_28ss_step_13 = 0b01000,
    dcc_28ss_step_14 = 0b11000,
    dcc_28ss_step_15 = 0b01001,
    dcc_28ss_step_16 = 0b11001,
    dcc_28ss_step_17 = 0b01010,
    dcc_28ss_step_18 = 0b11010,
    dcc_28ss_step_19 = 0b01011,
    dcc_28ss_step_20 = 0b11011,
    dcc_28ss_step_21 = 0b01100,
    dcc_28ss_step_22 = 0b11100,
    dcc_28ss_step_23 = 0b01101,
    dcc_28ss_step_24 = 0b11101,
    dcc_28ss_step_25 = 0b01110,
    dcc_28ss_step_26 = 0b11110,
    dcc_28ss_step_27 = 0b01111,
    dcc_28ss_step_28 = 0b11111
} DccSpeedTable28_t;

// conversion table for DCC short addresses
typedef enum DccShortAddrTable_e : uint8_t {
    dcc_short_addr_all = 0x00,              //Broadcast address (0)
    dcc_short_addr_multipurp_start = 0x01,  //Start of multipurpose addresses (1). Loklight is within this range
    dcc_short_addr_multipurp_end = 0x7f,    //End of multipurpose addresses (127)
    dcc_short_addr_accessory_start = 0x80,  //Start of accessory addresses (128)
    dcc_short_addr_accessory_end = 0xbf,    //End of accessory addresses (191)
    dcc_short_addr_14bmulti_start = 0xc0,   //Start of multipurpose with 14 addr bits (192)
    dcc_short_addr_14bmulti_end = 0xe7,     //End of multipurpose with 14 addr bits (231)
    dcc_short_addr_res_start = 0xe8,        //Start of reserved addresses (232)
    dcc_short_addr_res_end = 0xfc,          //End of reserved addresses (252)
    dcc_short_addr_adv_ext1 = 0xfd,         //Address for advanced extended address instruction (253)
    dcc_short_addr_adv_ext2 = 0xfe,         //Address for advanced extended address instruction (254)
    dcc_short_addr_idle = 0xff              //Idle packet address (255)
} DccShortAddrTable_t;

//This enum is a combination of message type and, if a message was received, what cmd it was
typedef enum DccMsgType_e : int8_t {
    dcc_reader_error = -4,  //Error status
    dcc_reader_unsupported = -3,  //Message type is valid but not supported by Loklight
    no_new_dcc_msg = -2,    //no new message
    dcc_msg_idle = -1,      //idle message
    dcc_msg_dcci = 0b000,   //000 Decoder and Consist Control Instruction. Not supported by Loklight
    dcc_msg_aoi = 0b001,    //001 Advanced Operation Instructions (contains 128 speed step instruction among other things)
    dcc_msg_sdir = 0b010,   //010 Speed and Direction Instruction for reverse operation & 
    dcc_msg_sdif = 0b011,   //011 Speed and Direction Instruction for forward operation
    dcc_msg_fgi1 = 0b100,   //100 Function Group One Instruction 
    dcc_msg_fgi2 = 0b101,   //101 Function Group Two Instruction 
    dcc_msg_fexp = 0b110,   //110 Future Expansion Instruction. Not supported by Loklight
    dcc_msg_cvai = 0b111,   //111 Configuration Variable Access Instruction
} DccMsgType_t;

// Definitions for Advanced Operation Instructions (AOI)
// The Message data byte format is [C C C][G G G G G], where C C C = 0b001 and G is defined below
typedef enum DCCAoiCmd_e : uint8_t {
    dcc_aoi_res_start = 0b00000, //Reserved codes start here
    dcc_aoi_res_end = 0b11100,
    dcc_aoi_analog = 0b11101,   //Set analog function. The next byte should be 0x01 (set volume). Other values are reserved. Then follows another byte with volume level. 
    dcc_aoi_zimo_ew = 0b11110,  //Zimo east west command
    dcc_aoi_128ss = 0b11111,    //Indicates we are receiving a 128 speed step command. This is the only supported AOI command by Loklight
} DCCAoiCmd_t;

// Definitions for Basic speed and direction instructions
// Format is [0, 1, D, C, S3, S2, S1, S0], where D is direction (1=FWD), C is either F0 or part of speed step (see DccControlMode_t), and Sx are speed bits
constexpr uint8_t DCC_BASIC_SI_VAL = 0x01<<6; //bits 7 and 6 must be 0 and 1 respectively for a basic speed & dir instruction
constexpr uint8_t DCC_BASIC_DIR_BIT = 0x1<<5; //Direction bit is bit 5
constexpr uint8_t DCC_BASIC_F0_BIT = 0x1<<4;  //F0 bit is bit 4 in 14 speed step mode
constexpr uint8_t DCC_BASIC_EXTRA_LSB = 0x1<<4; //in 28/128 speed step mode, bit 4 is part of speed step LSBs
constexpr uint8_t DCC_BASIC_SPEED_MASK = 0x0F; //Speed bits are bits 0-3

// Definitions for Function Group 1 Instructions
// Format is [1, 0, 0, F0, F4, F3, F2, F1], where Fx are function bits
constexpr uint8_t DCC_FGI1_F0 = 0x1<<4; //F0 is bit 4 but only in 28/128 speed step mode
constexpr uint8_t DCC_FGI1_F1 = 0x1<<0; //Function 1 is bit 0
constexpr uint8_t DCC_FGI1_F2 = 0x1<<1; //Function 2 is bit 1
constexpr uint8_t DCC_FGI1_F3 = 0x1<<2; //Function 3 is bit 2
constexpr uint8_t DCC_FGI1_F4 = 0x1<<3; //Function 4 is bit 3

// Definitions for Function Group 2 Instructions
// Format is [1, 0, 1, S, F5/9, F6/10, F7/11, F8/12], where Fx are function bits. If S=1: F5-F8 are set, if S=0: F9-F12 are set
constexpr uint8_t DCC_FGI2_SHIFT = 0x1<<4; //Shift bit is bit 4
constexpr uint8_t DCC_FGI2_F5_F9 = 0x1<<0; //Function 5 or 9 is bit 0
constexpr uint8_t DCC_FGI2_F6_F10 = 0x1<<1; //Function 6 or 10 is bit 1
constexpr uint8_t DCC_FGI2_F7_F11 = 0x1<<2; //Function 7 or 11 is bit 2
constexpr uint8_t DCC_FGI2_F8_F12 = 0x1<<3; //Function 8 or 12 is bit 3

// Definitions for CV Access Instructions
// Short form access. Not supported by Loklight
// Format is [1, 1, 1, 1, G, G, G, G], where GGGG is defined below
typedef enum DCCCvShortAccess_e : uint8_t {
    dcc_short_cv_accel = 0b0010,      //Acceleration CV23 access (not supported by Loklight)
    dcc_short_cv_decel = 0b0011,      //Deceleration CV24 access (not supported by Loklight)
    dcc_short_cv_long_addr = 0b0100,  //CV17, CV18 and CV29 will follow in next 3 bytes. Message needs to be sent with identical values twice
    dcc_short_cv_idx_cvs = 0b0101,    //Indexed CV31, 32 in next 2 bytes. Need to receive identical values twice
    dcc_short_cv_long_cnst = 0b0110,  //Consist CVs 19, 20 in next 2 bytes. Need to receive identical values twice
    dcc_short_cv_special = 0b1001,    //See S-9.2.3 Appendix B
} DCCCvShortAccess_t;

// Definitions for normal CV Access Instructions
// Format is [1, 1, 1, 0, G, G, V9, V8], [V7 .. V0] where GG is defined below and V9..0 is the CV number.
// MIND the CV number accessed is V+1, so to access CV1, send 0b0000000000
typedef enum DCCCvAccess_e : uint8_t {
    dcc_cv_verify_byte = 0b01<<2,      //Verify CV value (Not supported by Loklight)
    dcc_cv_write_byte = 0b11<<2,       //Write CV value. Needs to be received twice with identical values before setting the CV
    dcc_cv_bitmanipulate = 0b10<<2,    //Bit manipulation (Not supported by Loklight)
    dcc_cv_addr_msb_mask = 0b11        //Mask for the MSB bits of the CV number (bits 8 and 9)
} DCCCvAccess_t;

// XPOM is not supported by Loklight nor defined in this header

#endif // DCC_DEFS_H