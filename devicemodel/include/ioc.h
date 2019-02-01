/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _IOC_H_
#define _IOC_H_

#include <stdint.h>
#include <pthread.h>

#include <sys/queue.h>
#include <sys/epoll.h>

/*
 * Carrier Board Communication(CBC) frame definition
 * +---+---------+-----------+---------------+---------+----------+---------+
 *
 *                                          +---------------+-------------+
 *                                          | ServiceHeader | DataPayload |
 *                                          |      8b       | 24b...504b  |
 *                                          ++--------------+-------------+
 *                                                       Service Layer
 *                                                     \               /
 *                             +-------------+----------\-------------/
 *                             | Multiplexer | Priority | UpperLayer  |
 *                             |      5b     |    3b    |             |
 *                             +-------------+------------------------+
 *                                                     Address Layer
 *                                                    \            /
 * +---+---------+-----------+---------------+---------\----------/---------+
 * |SOF|Extension|FrameLength|SequenceCounter|TimeStamp|UpperLayer|CheckSum |
 * |8b |    1b   |   5b      |     2b        |32B(n/a) |          |    8b   |
 * +---+---------+-----------+---------------+---------+----------+---------+
 *                                Link Layer
 *
 */

#define CBC_SOF_VALUE		0x05	/* CBC start of frame value */
#define CBC_EXT_VALUE		0x00	/* CBC extension bit value */
#define CBC_PRIO_MASK		0x07	/* CBC priority bitmask */
#define CBC_MUX_MASK		0x1F	/* CBC muxtiplexer bitmask */
#define CBC_LEN_MASK		0x1F	/* CBC frame length bitmask */
#define CBC_SEQ_MASK		0x03	/* CBC sequence bitmask */
#define CBC_EXT_MASK		0x01	/* CBC extension bits bitmask */
#define CBC_MUX_OFFSET		3	/* CBC muxtiplexer offset */
#define CBC_SEQ_OFFSET		0	/* CBC sequence offset */
#define CBC_LEN_OFFSET		2	/* CBC service frame length offset */
#define CBC_EXT_OFFSET		7	/* CBC extension bits offset */
#define CBC_LEN_UNIT		4	/* CBC frame content in block length */
#define CBC_PRIO_OFFSET	0	/* CBC priority offset */
#define CBC_CHKSUM_SIZE	1	/* CBC checksum size */
#define CBC_GRANULARITY	4	/* CBC frame alignment */
#define CBC_LINK_HDR_SIZE	3	/* CBC link layer header size */
#define CBC_ADDR_HDR_SIZE	1	/* CBC address layser header size */
#define CBC_SRV_HDR_SIZE	1	/* CBC service layer header size */
#define CBC_MAX_FRAME_SIZE	96	/* CBC maximum frame size */
#define CBC_MIN_FRAME_SIZE	8	/* CBC mininum frame size */
#define CBC_MAX_SERVICE_SIZE	64	/* CBC maximum service size */

/*
 * Define the start positions of each layer headers.
 * CBC_SOF_POS: start of frame start byte position
 * CBC_ELS_POS: externsion, frame length and sequence start byte position
 * CBC_ADDR_POS: address protocol start byte postion
 * CBC_SRV_POS: service protocol start byte position
 * CBC_PAYLOAD_POS: CBC payload start byte position
 */
#define CBC_SOF_POS		0
#define CBC_ELS_POS	(CBC_SOF_POS + 1)
#define CBC_ADDR_POS	(CBC_SOF_POS + CBC_LINK_HDR_SIZE - CBC_CHKSUM_SIZE)
#define CBC_SRV_POS	(CBC_ADDR_POS + CBC_ADDR_HDR_SIZE)
#define CBC_PAYLOAD_POS	(CBC_SRV_POS + CBC_SRV_HDR_SIZE)

#define CBC_WK_RSN_BTN	(1 << 5)	/* CBC wakeup reason field button */
#define CBC_WK_RSN_RTC	(1 << 9)	/* CBC wakeup reason field rtc */
#define CBC_WK_RSN_DOR	(1 << 11)	/* CBC wakeup reason field cardoor */
#define CBC_WK_RSN_FS5	(1 << 22)	/* CBC wakeup reason field force S5 */
#define CBC_WK_RSN_SOC	(1 << 23)	/* CBC wakeup reason field soc */
/* CBC wakeup reason field debug channel */
#define CBC_WK_RSN_DGB  (1 << 24)

/* CBC wakeup reason filed suspend or shutdown */
#define CBC_WK_RSN_SHUTDOWN	(0)

/*
 * IOC mediator permits ignition button, cardoor, RTC, SOC and force S5 wakeup
 * reasons which comes from IOC firmware, others will be masked.
 */
#define CBC_WK_RSN_ALL \
	(CBC_WK_RSN_BTN | CBC_WK_RSN_RTC | CBC_WK_RSN_DOR | CBC_WK_RSN_SOC | \
	 CBC_WK_RSN_FS5)

/*
 * CBC ring buffer is used to buffer bytes before build one complete CBC frame.
 */
#define CBC_RING_BUFFER_SIZE	256

/*
 * Default whitelist node is NULL before whitelist initialization.
 */
#define DEFAULT_WLIST_NODE	(0)

/*
 * Default IOC channels file descriptor is -1 before open.
 */
#define IOC_INIT_FD	-1

/*
 * Maximum CBC requests number.
 */
#define IOC_MAX_REQUESTS	200

/*
 * Maximum epoll events.
 */
#define IOC_MAX_EVENTS	32

/* IOC default path */
#define IOC_DP_NONE	""

/*
 * IOC native channel path definition.
 */
#define IOC_NP_PMT	"/dev/cbc-pmt"
#define IOC_NP_LF	"/dev/cbc-lifecycle"
#define IOC_NP_SIG	"/dev/cbc-signals"
#define IOC_NP_ESIG	"/dev/cbc-early-signals"
#define IOC_NP_DIAG	"/dev/cbc-diagnosis"
#define IOC_NP_DLT	"/dev/cbc-dlt"
#define IOC_NP_LIND	"/dev/cbc-linda"
#define IOC_NP_RAW0	"/dev/cbc-raw0"
#define IOC_NP_RAW1	"/dev/cbc-raw1"
#define IOC_NP_RAW2	"/dev/cbc-raw2"
#define IOC_NP_RAW3	"/dev/cbc-raw3"
#define IOC_NP_RAW4	"/dev/cbc-raw4"
#define IOC_NP_RAW5	"/dev/cbc-raw5"
#define IOC_NP_RAW6	"/dev/cbc-raw6"
#define IOC_NP_RAW7	"/dev/cbc-raw7"
#define IOC_NP_RAW8	"/dev/cbc-raw8"
#define IOC_NP_RAW9	"/dev/cbc-raw9"
#define IOC_NP_RAW10	"/dev/cbc-raw10"
#define IOC_NP_RAW11	"/dev/cbc-raw11"
#define IOC_NP_FLF	"/tmp/ioc_fake_lifecycle"
#define IOC_NP_FSIG	"/tmp/ioc_fake_signal"
#define IOC_NP_FRAW	"/tmp/ioc_fake_raw11"

/*
 * CBC signal data command types.
 * Signal Data Message
 * +----------------+--------------+
 * | SignalDataCMD  |   Payload    |
 * |        8b      |    0~56b     |
 * +----------------+--------------+
 */
enum cbc_signal_data_command {
	CBC_SD_SINGLE_SIGNAL	= 1,	/* Single signal update */
	CBC_SD_MULTI_SIGNAL	= 2,	/* Multi signal update */
	CBC_SD_GROUP_SIGNAL	= 3,	/* Group signal update */
	CBC_SD_DEFAULT_VALUES	= 4,	/* Update default values */
	CBC_SD_UPDATE_SNA	= 5,	/* Update SNA values */
	CBC_SD_INVAL_SSIG	= 6,	/* Invalidate signal */
	CBC_SD_INVAL_MSIG	= 7,	/* Invalidate multi signals */
	CBC_SD_INVAL_SGRP	= 8,	/* Invalidate signal group */
	CBC_SD_INVAL_MGRP	= 9,	/* Invalidate muliti groups */
	CBC_SD_OPEN_CHANNEL	= 253,	/* Open signal channel */
	CBC_SD_CLOSE_CHANNEL	= 254,	/* Clsoe signal channel */
	CBC_SD_RESET_CHANNEL	= 255,	/* Reset signal channel */
	CBC_SD_MAX
};

/*
 * CBC system control command types.
 * +------------------+------------+
 * | SystemControlCMD |   Payload  |
 * |        8b        |     24b    |
 * +------------------+------------+
 */
enum cbc_system_control_command {
	CBC_SC_WK_RSN	= 1,	/* Wakeup reasons */
	CBC_SC_HB	= 2,	/* Heartbeat */
	CBC_SC_BOOTSEL	= 3,	/* Boot selector */
	CBC_SC_SPRS_HB	= 4,	/* Suppress heartbeat check */
	CBC_SC_RTC 	= 5,	/* Set RTC wakeup timer */
	CBC_SC_MAX
};

/*
 * CBC system control - heartbeat: command types.
 * Heartbeat Message
 * +------------------+---------+-----------------+------+
 * | SystemControlCMD | Command | SUS_STAT Action | Resv |
 * |        8b        |   8b    |        8b       |  8b  |
 * +------------------+---------+-----------------+------+
 */
enum cbc_heartbeat_command {
	CBC_HB_SD_PREP,	/* Shutdown prepared */
	CBC_HB_ACTIVE,	/* Active */
	CBC_HB_SD_DLY,	/* Shutdown delay */
	CBC_HB_INITIAL,	/* Initial */
	CBC_HB_STANDBY,	/* Standby */
	CBC_HB_DIAG,	/* Diagnosis */
	CBC_HB_SD_REQ,	/* Cm shutdown request */
	CBC_HB_SD_EXE,	/* Shutdown execute */
	CBC_HB_EMG_SD,	/* Mmergency shutdown execute */
	CBC_HB_MAX
};

/*
 * CBC system control - heartbeat: suspend state action types.
 */
enum cbc_sus_stat_action {
	CBC_SS_INVALID,	/* Invalid */
	CBC_SS_HALT_I0,	/* Halt */
	CBC_SS_REBOOT0,	/* Reboot */
	CBC_SS_HALT_I1,	/* Ignore once then halt */
	CBC_SS_REBOOT1,	/* Ignore once then reboot */
	CBC_SS_HALT_I2,	/* Ignore twice then halt */
	CBC_SS_REBOOT2,	/* Ignore twice then reboot */
	CBC_SS_REFRESH,	/* Ram refresh, S3 */
	CBC_SS_MAX
};

/*
 * CBC system control - RTC: command type.
 * RTC Message
 * +------------------+-------------+-------------+-------------+
 * | SystemControlCMD | Timer value | Timer value | Granularity |
 * | SVC-Header: 5    | Bits 0...7  | Bits 8...15 | 0 - seconds |
 * |                  |             |             | 1 - minutes |
 * |                  |             |             | 2 - hours   |
 * |                  |             |             | 3 - days    |
 * |                  |             |             | 4 - week    |
 * |        8b        |     8b      |     8b      |     8b      |
 * +------------------+-------------+-------------+-------------+
 */
enum cbc_rtc_timer_unit {
	CBC_RTC_TIMER_U_SEC,
	CBC_RTC_TIMER_U_MIN,
	CBC_RTC_TIMER_U_HOUR,
	CBC_RTC_TIMER_U_DAY,
	CBC_RTC_TIMER_U_WEEK,
};

/*
 * CBC rx signal identity definition.
 */
enum cbc_rx_signal_id {
	CBC_SIG_ID_STFR		= 20000,	/* SetTunerFrequency */
	CBC_SIG_ID_EGYO		= 20001,	/* EnableGyro */
	CBC_SIG_ID_WACS		= 20002,	/* WriteAmplifierConfigurationSequence*/
	CBC_SIG_ID_RIFC		= 20003,	/* RequestIocFblChecksum */
	CBC_SIG_ID_RIWC		= 20004,	/* RequestIocWfChecksum */
	CBC_SIG_ID_RIAC		= 20005,	/* RequestIocAppChecksum */
	CBC_SIG_ID_RIVS		= 20006,	/* RequestIocVersion */
	CBC_SIG_ID_RRMS		= 20007,	/* RequestRuntimeMeasurement */
	CBC_SIG_ID_MTAM		= 20008,	/* MuteAmplifier */
	CBC_SIG_ID_PBST		= 20009,	/* ParkingBrakeSetting */
	CBC_SIG_ID_PBAT		= 20010,	/* ParkingBrakeAutomaticSetting */
	CBC_SIG_ID_HFSS		= 20011,	/* HvacFanSpeedSetting */
	CBC_SIG_ID_HFDST	= 20012,	/* HvacFanDirectionSetting */
	CBC_SIG_ID_HVAST	= 20013,	/* HvacAcSetting */
	CBC_SIG_ID_HAMS		= 20014,	/* HvacAcMaxSetting */
	CBC_SIG_ID_HATST	= 20015,	/* HvacAutoSetting */
	CBC_SIG_ID_HDEFST 	= 20016,	/* HvacDefrostSetting */
	CBC_SIG_ID_HDMXST	= 20017,	/* HvacDefrostMaxSetting */
	CBC_SIG_ID_HDST		= 20018,	/* HvacDualSetting */
	CBC_SIG_ID_HHSMS	= 20019,	/* HvacHeatingSideMirrorSetting */
	CBC_SIG_ID_HHSWS	= 20020,	/* HvacHeatingSteeringWheelSetting */
	CBC_SIG_ID_HPWST	= 20021,	/* HvacPowerSetting */
	CBC_SIG_ID_HRCST	= 20022,	/* HvacRecirculationSetting */
	CBC_SIG_ID_HTCST	= 20023,	/* HvacTemperatureCabinSetting */
	CBC_SIG_ID_HTSST	= 20024,	/* HvacTemperatureSeatSetting */
	CBC_SIG_ID_HTUST	= 20025,	/* HvacTemperatureUnitsSetting */
	CBC_SIG_ID_HVSST	= 20026,	/* HvacVentilationSeatSetting */
	CBC_SIG_ID_HRAST	= 20027,	/* HvacRecirculationAutomaticSetting */
	CBC_SIG_ID_USBVBUS	= 20028,	/* SupportUsbOtgVbusControl */
	CBC_SIG_ID_VICL		= 651,		/* VideoInCtrl */
};

/*
 * CBC tx signal identity definition.
 */
enum cbc_tx_signal_id {
	CBC_SIG_ID_MBV		= 501,	/* MainBatteryVoltage */
	CBC_SIG_ID_TSA		= 502,	/* TemperatureSensorAmplifier */
	CBC_SIG_ID_TSE		= 503,	/* TemperatureSensorEnvironment */
	CBC_SIG_ID_VSWA		= 701,	/* VehicleSteeringWheelAngle */
	CBC_SIG_ID_VSPD		= 702,	/* VehicleSpeed */
	CBC_SIG_ID_VESP		= 703,	/* VehicleEngineSpeed */
	CBC_SIG_ID_VECT		= 704,	/* VehicleEngineCoolantTemp */
	CBC_SIG_ID_VRGR		= 705,	/* VehicleReverseGear */
	CBC_SIG_ID_VPS		= 706,	/* VehiclePowerStatus */
	CBC_SIG_ID_VPM		= 707,	/* VehiclePowerMode */
	CBC_SIG_ID_VMD		= 708,	/* VehicleMode */
	CBC_SIG_ID_VIS		= 709,	/* VehicleImmobilizerState */
	CBC_SIG_ID_VGP		= 710,	/* VehicleGearshiftPosition */
	CBC_SIG_ID_VAG		= 711,	/* VehicleActualGear */
	CBC_SIG_ID_VFS		= 712,	/* VehicleFuelStatus */
	CBC_SIG_ID_VFL		= 713,	/* VehicleFuelLevel */
	CBC_SIG_ID_VDTE		= 714,	/* VehicleDistanceToEmpty */
	CBC_SIG_ID_SWUB		= 715,	/* SteeringWheelUpBtn */
	CBC_SIG_ID_SWRB		= 716,	/* SteeringWheelRightBtn */
	CBC_SIG_ID_SWPB		= 717,	/* SteeringWheelPrevBtn */
	CBC_SIG_ID_SWNB		= 718,	/* SteeringWheelNextBtn */
	CBC_SIG_ID_SWLB		= 719,	/* SteeringWheelLeftBtn */
	CBC_SIG_ID_SWDB		= 720,	/* SteeringWheelDownBtn */
	CBC_SIG_ID_SWVA		= 721,	/* SteeringWheelVolumeAdjust */
	CBC_SIG_ID_SWSCB	= 722,	/* SteeringWheelSpeechCtrlBtn */
	CBC_SIG_ID_SWPLB	= 723,	/* SteeringWheelPlayBtn */
	CBC_SIG_ID_SWPCB	= 724,	/* SteeringWheelPickupCallBtn */
	CBC_SIG_ID_SWPSB	= 725,	/* SteeringWheelPauseBtn */
	CBC_SIG_ID_SWHB		= 726,	/* SteeringWheelHomeBtn */
	CBC_SIG_ID_SWEB		= 727,	/* SteeringWheelEnterBtn */
	CBC_SIG_ID_SWECB	= 728,	/* SteeringWheelEndCallBtn */
	CBC_SIG_ID_SWCB		= 729,	/* SteeringWheelConfigBtn */
	CBC_SIG_ID_SWCLB	= 730,	/* SteeringWheelCancelBtn */
	CBC_SIG_ID_SWAMB	= 731,	/* SteeringWheelAudioMuteBtn */
	CBC_SIG_ID_RRSUB	= 732,	/* RightRearSeatUpBtn */
	CBC_SIG_ID_RRSRB	= 733,	/* RightRearSeatRightBtn */
	CBC_SIG_ID_RRSPB	= 734,	/* RightRearSeatPrevBtn */
	CBC_SIG_ID_RRSP9B	= 735,	/* RightRearSeatPosition9Btn */
	CBC_SIG_ID_RRSP8B	= 736,	/* RightRearSeatPosition8Btn */
	CBC_SIG_ID_RRSP7B	= 737,	/* RightRearSeatPosition7Btn */
	CBC_SIG_ID_RRSP6B	= 738,	/* RightRearSeatPosition6Btn */
	CBC_SIG_ID_RRSP5B	= 739,	/* RightRearSeatPosition5Btn */
	CBC_SIG_ID_RRSP4B	= 740,	/* RightRearSeatPosition4Btn */
	CBC_SIG_ID_RRSP3B	= 741,	/* RightRearSeatPosition3Btn */
	CBC_SIG_ID_RRSP2B	= 742,	/* RightRearSeatPosition2Btn */
	CBC_SIG_ID_RRSP1B	= 743,	/* RightRearSeatPosition1Btn */
	CBC_SIG_ID_RRSP0B	= 744,	/* RightRearSeatPosition0Btn */
	CBC_SIG_ID_RRSNB	= 745,	/* RightRearSeatNextBtn */
	CBC_SIG_ID_RRSLB	= 746,	/* RightRearSeatLeftBtn */
	CBC_SIG_ID_RRSDB	= 747,	/* RightRearSeatDownBtn */
	CBC_SIG_ID_RRSVA	= 748,	/* RightRearSeatVolumeAdjust */
	CBC_SIG_ID_RSSSB	= 749,	/* RightRearSeatStopBtn */
	CBC_SIG_ID_RRSSCB	= 750,	/* RightRearSeatSpeechCtrlBtn */
	CBC_SIG_ID_RRSSB	= 751,	/* RightRearSeatSearchBtn */
	CBC_SIG_ID_RRSRDB	= 752,	/* RightRearSeatRadioBtn */
	CBC_SIG_ID_RRSPLB	= 753,	/* RightRearSeatPlayBtn */
	CBC_SIG_ID_RRSPSB	= 754,	/* RightRearSeatPauseBtn */
	CBC_SIG_ID_RRSOMB	= 755,	/* RightRearSeatOpticalMediaBtn */
	CBC_SIG_ID_RRSHB	= 756,	/* RightRearSeatHomeBtn */
	CBC_SIG_ID_RRSHDB	= 757,	/* RightRearSeatHarddiskBtn */
	CBC_SIG_ID_RRSENB	= 758,	/* RightRearSeatEnterBtn */
	CBC_SIG_ID_RRSEJB	= 759,	/* RightRearSeatEjectBtn */
	CBC_SIG_ID_RRSCB	= 760,	/* RightRearSeatConfigBtn */
	CBC_SIG_ID_RRSCLB	= 761,	/* RightRearSeatCancelBtn */
	CBC_SIG_ID_RRSAMB	= 762,	/* RightRearSeatAudioMuteBtn */
	CBC_SIG_ID_RVCS		= 763,	/* RearViewCameraStatus */
	CBC_SIG_ID_PSS		= 764,	/* PdcSwitchStatus */
	CBC_SIG_ID_PUB		= 765,	/* PassengerUpBtn */
	CBC_SIG_ID_PRB		= 766,	/* PassengerRightBtn */
	CBC_SIG_ID_PPB		= 767,	/* PassengerPrevBtn */
	CBC_SIG_ID_PP9B		= 768,	/* PassengerPosition9Btn */
	CBC_SIG_ID_PP8B		= 769,	/* PassengerPosition8Btn */
	CBC_SIG_ID_PP7B		= 770,	/* PassengerPosition7Btn */
	CBC_SIG_ID_PP6B		= 771,	/* PassengerPosition6Btn */
	CBC_SIG_ID_PP5B		= 772,	/* PassengerPosition5Btn */
	CBC_SIG_ID_PP4B		= 773,	/* PassengerPosition4Btn */
	CBC_SIG_ID_PP3B		= 774,	/* PassengerPosition3Btn */
	CBC_SIG_ID_PP2B		= 775,	/* PassengerPosition2Btn */
	CBC_SIG_ID_PP1B		= 776,	/* PassengerPosition1Btn */
	CBC_SIG_ID_PP0B		= 777,	/* PassengerPosition0Btn */
	CBC_SIG_ID_PNB		= 778,	/* PassengerNextBtn */
	CBC_SIG_ID_PLB		= 779,	/* PassengerLeftBtn */
	CBC_SIG_ID_PDB		= 780,	/* PassengerDownBtn */
	CBC_SIG_ID_PVA		= 781,	/* PassengerVolumeAdjust */
	CBC_SIG_ID_PSB		= 782,	/* PassengerStopBtn */
	CBC_SIG_ID_PSCB		= 783,	/* PassengerSpeechCtrlBtn */
	CBC_SIG_ID_PSRB		= 784,	/* PassengerSearchBtn */
	CBC_SIG_ID_PRDB		= 785,	/* PassengerRadioBtn */
	CBC_SIG_ID_PPLB		= 786,	/* PassengerPlayBtn */
	CBC_SIG_ID_PPSB		= 787,	/* PassengerPauseBtn */
	CBC_SIG_ID_POMB		= 788,	/* PassengerOpticalMediaBtn */
	CBC_SIG_ID_PHMB		= 789,	/* PassengerHomeBtn */
	CBC_SIG_ID_PHDB		= 790,	/* PassengerHarddiskBtn */
	CBC_SIG_ID_PENB		= 791,	/* PassengerEnterBtn */
	CBC_SIG_ID_PEJB		= 792,	/* PassengerEjectBtn */
	CBC_SIG_ID_PCFB		= 793,	/* PassengerConfigBtn */
	CBC_SIG_ID_PCLB		= 794,	/* PassengerCancelBtn */
	CBC_SIG_ID_PAMB		= 795,	/* PassengerAudioMuteBtn */
	CBC_SIG_ID_LRSUB	= 796,	/* LeftRearSeatUpBtn */
	CBC_SIG_ID_LRSRB	= 797,	/* LeftRearSeatRightBtn */
	CBC_SIG_ID_LRSPB	= 798,	/* LeftRearSeatPrevBtn */
	CBC_SIG_ID_LRSP9B	= 799,	/* LeftRearSeatPosition9Btn */
	CBC_SIG_ID_LRSP8B	= 800,	/* LeftRearSeatPosition8Btn */
	CBC_SIG_ID_LRSP7B	= 801,	/* LeftRearSeatPosition7Btn */
	CBC_SIG_ID_LRSP6B	= 802,	/* LeftRearSeatPosition6Btn */
	CBC_SIG_ID_LRSP5B	= 803,	/* LeftRearSeatPosition5Btn */
	CBC_SIG_ID_LRSP4B	= 804,	/* LeftRearSeatPosition4Btn */
	CBC_SIG_ID_LRSP3B	= 805,	/* LeftRearSeatPosition3Btn */
	CBC_SIG_ID_LRSP2B	= 806,	/* LeftRearSeatPosition2Btn */
	CBC_SIG_ID_LRSP1B	= 807,	/* LeftRearSeatPosition1Btn */
	CBC_SIG_ID_LRSP0B	= 808,	/* LeftRearSeatPosition0Btn */
	CBC_SIG_ID_LRSNB	= 809,	/* LeftRearSeatNextBtn */
	CBC_SIG_ID_LRSLB	= 810,	/* LeftRearSeatLeftBtn */
	CBC_SIG_ID_LRSDB	= 811,	/* LeftRearSeatDownBtn */
	CBC_SIG_ID_LRSVA	= 812,	/* LeftRearSeatVolumeAdjust */
	CBC_SIG_ID_LRSAMB	= 813,	/* LeftRearSeatAudioMuteBtn */
	CBC_SIG_ID_LRSSB	= 814,	/* LeftRearSeatStopBtn */
	CBC_SIG_ID_LRSSCB	= 815,	/* LeftRearSeatSpeechCtrlBtn */
	CBC_SIG_ID_LRSSRB	= 816,	/* LeftRearSeatSearchBtn */
	CBC_SIG_ID_LRSRDB	= 817,	/* LeftRearSeatRadioBtn */
	CBC_SIG_ID_LRSPLB	= 818,	/* LeftRearSeatPlayBtn */
	CBC_SIG_ID_LRSPSB	= 819,	/* LeftRearSeatPauseBtn */
	CBC_SIG_ID_LRSOMB	= 820,	/* LeftRearSeatOpticalMediaBtn */
	CBC_SIG_ID_LRSHMB	= 821,	/* LeftRearSeatHomeBtn */
	CBC_SIG_ID_LRSHDB	= 822,	/* LeftRearSeatHarddiskBtn */
	CBC_SIG_ID_LRSENB	= 823,	/* LeftRearSeatEnterBtn */
	CBC_SIG_ID_LRSEJB	= 824,	/* LeftRearSeatEjectBtn */
	CBC_SIG_ID_LRSCFB	= 825,	/* LeftRearSeatConfigBtn */
	CBC_SIG_ID_LRSCLB	= 826,	/* LeftRearSeatCancelBtn */
	CBC_SIG_ID_DVA		= 827,	/* DriverVolumeAdjust */
	CBC_SIG_ID_DECSP	= 828,	/* DriverErgoCommanderSteps */
	CBC_SIG_ID_DECST	= 829,	/* DriverErgoCommanderStatus */
	CBC_SIG_ID_DAMB		= 830,	/* DriverAudioMuteBtn */
	CBC_SIG_ID_DNB		= 831,	/* DriverNextBtn */
	CBC_SIG_ID_DLB		= 832,	/* DriverLeftBtn */
	CBC_SIG_ID_DDB		= 833,	/* DriverDownBtn */
	CBC_SIG_ID_DUB		= 834,	/* DriverUpBtn */
	CBC_SIG_ID_DRB		= 835,	/* DriverRightBtn */
	CBC_SIG_ID_DPB		= 836,	/* DriverPrevBtn */
	CBC_SIG_ID_DP9B		= 837,	/* DriverPosition9Btn */
	CBC_SIG_ID_DP8B		= 838,	/* DriverPosition8Btn */
	CBC_SIG_ID_DP7B		= 839,	/* DriverPosition7Btn */
	CBC_SIG_ID_DP6B		= 840,	/* DriverPosition6Btn */
	CBC_SIG_ID_DP5B		= 841,	/* DriverPosition5Btn */
	CBC_SIG_ID_DP4B		= 842,	/* DriverPosition4Btn */
	CBC_SIG_ID_DP3B		= 843,	/* DriverPosition3Btn */
	CBC_SIG_ID_DP2B		= 844,	/* DriverPosition2Btn */
	CBC_SIG_ID_DP1B		= 845,	/* DriverPosition1Btn */
	CBC_SIG_ID_DP0B		= 846,	/* DriverPosition0Btn */
	CBC_SIG_ID_DSCB		= 847,	/* DriverSpeechCtrlBtn */
	CBC_SIG_ID_DSRB		= 848,	/* DriverSearchBtn */
	CBC_SIG_ID_DRDB		= 849,	/* DriverRadioBtn */
	CBC_SIG_ID_DSTB		= 850,	/* DriverStopBtn */
	CBC_SIG_ID_DPLB		= 851,	/* DriverPlayBtn */
	CBC_SIG_ID_DPSB		= 852,	/* DriverPauseBtn */
	CBC_SIG_ID_DOMB		= 853,	/* DriverOpticalMediaBtn */
	CBC_SIG_ID_DHMB		= 854,	/* DriverHomeBtn */
	CBC_SIG_ID_DHHB		= 855,	/* DriverHarddiskBtn */
	CBC_SIG_ID_DENB		= 856,	/* DriverEnterBtn */
	CBC_SIG_ID_DEJB		= 857,	/* DriverEjectBtn */
	CBC_SIG_ID_DCFB		= 858,	/* DriverConfigBtn */
	CBC_SIG_ID_DCLB		= 859,	/* DriverCancelBtn */
	CBC_SIG_ID_DSTG		= 860,	/* DoorStatusTailgate */
	CBC_SIG_ID_DSRR		= 861,	/* DoorStatusRightRear */
	CBC_SIG_ID_DSRF		= 862,	/* DoorStatusRightFront */
	CBC_SIG_ID_DSLR		= 863,	/* DoorStatusLeftRear */
	CBC_SIG_ID_DSLF		= 864,	/* DoorStatusLeftFront */
	CBC_SIG_ID_DSEH		= 865,	/* DoorStatusEngineHood */
	CBC_SIG_ID_CSSRRW	= 866,	/* ChildSafetyStatusRightRearWnd */
	CBC_SIG_ID_CSSRR	= 867,	/* ChildSafetyStatusRightRear */
	CBC_SIG_ID_CSSLRW	= 868,	/* ChildSafetyStatusLeftRearWnd */
	CBC_SIG_ID_CSSLR	= 869,	/* ChildSafetyStatusLeftRear */
	CBC_SIG_ID_ATEMP	= 870,	/* AmbientTemperature */
	CBC_SIG_ID_ANSL		= 871,	/* AmbientNoiseLevel */
	CBC_SIG_ID_ALTI		= 872,	/* AmbientLightIntensity */
	CBC_SIG_ID_VSA		= 873,	/* VehicleSteeringAngle */
	CBC_SIG_ID_LLAT		= 875,	/* LocationLatitude */
	CBC_SIG_ID_LLON		= 876,	/* LocationLongitude */
	CBC_SIG_ID_LALT		= 877,	/* LocationAltitude */
	CBC_SIG_ID_LACC		= 878,	/* LocationAccuracy */
	CBC_SIG_ID_LHED		= 879,	/* LocationHeading */
	CBC_SIG_ID_LSPD		= 880,	/* LocationSpeed */
	CBC_SIG_ID_LSRC		= 881,	/* LocationSource */
	CBC_SIG_ID_LSCT		= 882,	/* LocationSourceCount */
	CBC_SIG_ID_PDFB		= 884,	/* PdcDistanceFrontCenter */
	CBC_SIG_ID_PDFL1	= 885,	/* PdcDistanceFrontLeft1 */
	CBC_SIG_ID_PDFL2	= 886,	/* PdcDistanceFrontLeft2 */
	CBC_SIG_ID_PDFL3	= 887,	/* PdcDistanceFrontLeft3 */
	CBC_SIG_ID_PDFR1	= 888,	/* PdcDistanceFrontRight1 */
	CBC_SIG_ID_PDFR2	= 889,	/* PdcDistanceFrontRight2 */
	CBC_SIG_ID_PDFR3	= 890,	/* PdcDistanceFrontRight3 */
	CBC_SIG_ID_PDRC		= 892,	/* PdcDistanceRearCenter */
	CBC_SIG_ID_PDRL1	= 893,	/* PdcDistanceRearLeft1 */
	CBC_SIG_ID_PDRL2	= 894,	/* PdcDistanceRearLeft2 */
	CBC_SIG_ID_PDRL3	= 895,	/* PdcDistanceRearLeft3 */
	CBC_SIG_ID_PDRR1	= 896,	/* PdcDistanceRearRight1 */
	CBC_SIG_ID_PDRR2	= 897,	/* PdcDistanceRearRight2 */
	CBC_SIG_ID_PDRR3	= 898,	/* PdcDistanceRearRight3 */
	CBC_SIG_ID_VXA		= 900,	/* VehicleXAcceleration */
	CBC_SIG_ID_VYA		= 901,	/* VehicleYAcceleration */
	CBC_SIG_ID_VZA		= 902,	/* VehicleZAcceleration */
	CBC_SIG_ID_IACR		= 906,	/* IocAppChecksumResponse */
	CBC_SIG_ID_IWCR		= 907,	/* IocWfChecksumResponse */
	CBC_SIG_ID_IFCR		= 908,	/* IocFblChecksumResponse */
	CBC_SIG_ID_GYROX	= 911,	/* GyroX */
	CBC_SIG_ID_GYROY	= 912,	/* GyroY */
	CBC_SIG_ID_IAVB		= 915,	/* IocAppVersionBuild */
	CBC_SIG_ID_IAVMJ	= 916,	/* IocAppVersionMajor */
	CBC_SIG_ID_RAV		= 919,	/* RuntimeAverageValue */
	CBC_SIG_ID_RMAX		= 920,	/* RuntimeMaxValue */
	CBC_SIG_ID_RMIN		= 921,	/* RuntimeMinValue */
	CBC_SIG_ID_ACCX		= 924,	/* AccX */
	CBC_SIG_ID_ACCY		= 925,	/* AccY */
	CBC_SIG_ID_ACCZ		= 926,	/* AccZ */
	CBC_SIG_ID_MDS		= 927,	/* MrbDipSwitch */
	CBC_SIG_ID_FCP		= 928,	/* FanCurrentRpm */
	CBC_SIG_ID_GYROZ	= 929,	/* GyroZ */
	CBC_SIG_ID_IAVMN	= 930,	/* IocAppVersionMinor */
	CBC_SIG_ID_RTST		= 931,	/* RuntimeSamplesTaken */
	CBC_SIG_ID_PKBK		= 933,	/* ParkingBrake */
	CBC_SIG_ID_PKBKST	= 934,	/* ParkingBrakeSetting */
	CBC_SIG_ID_PKBKAT	= 935,	/* ParkingBrakeAutomatic */
	CBC_SIG_ID_PKBKAS	= 936,	/* ParkingBrakeAutomaticSetting */
	CBC_SIG_ID_HFSPD	= 937,	/* HvacFanSpeed */
	CBC_SIG_ID_HFSST	= 938,	/* HvacFanSpeedSetting */
	CBC_SIG_ID_HFDIR	= 939,	/* HvacFanDirection */
	CBC_SIG_ID_HFDSTT	= 940,	/* HvacFanDirectionSetting */
	CBC_SIG_ID_HVACA	= 941,	/* HvacAc */
	CBC_SIG_ID_HVASTT	= 942,	/* HvacAcSetting */
	CBC_SIG_ID_HAMAX	= 943,	/* HvacAcMax */
	CBC_SIG_ID_HVMST	= 944,	/* HvacAcMaxSetting */
	CBC_SIG_ID_HAUTO	= 945,	/* HvacAuto */
	CBC_SIG_ID_HATSTT	= 946,	/* HvacAutoSetting */
	CBC_SIG_ID_HVDEF	= 947,	/* HvacDefrost */
	CBC_SIG_ID_HDEFSTT	= 948,	/* HvacDefrostSetting */
	CBC_SIG_ID_HDFMAX	= 949,	/* HvacDefrostMax */
	CBC_SIG_ID_HDMXSTT	= 950,	/* HvacDefrostMaxSetting */
	CBC_SIG_ID_HDUAL	= 951,	/* HvacDual */
	CBC_SIG_ID_HDSTT	= 952,	/* HvacDualSetting */
	CBC_SIG_ID_HHSMR	= 953,	/* HvacHeatingSideMirror */
	CBC_SIG_ID_HHSMST	= 954,	/* HvacHeatingSideMirrorSetting */
	CBC_SIG_ID_HHSWL	= 955,	/* HvacHeatingSteeringWheel */
	CBC_SIG_ID_HHSWST	= 956,	/* HvacHeatingSteeringWheelSetting */
	CBC_SIG_ID_HPOWR	= 957,	/* HvacPower */
	CBC_SIG_ID_HPWSTT	= 958,	/* HvacPowerSetting */
	CBC_SIG_ID_HRECC	= 959,	/* HvacRecirculation */
	CBC_SIG_ID_HRECST	= 960,	/* HvacRecirculationSetting */
	CBC_SIG_ID_HTEMCB	= 961,	/* HvacTemperatureCabin */
	CBC_SIG_ID_HTCSTT	= 962,	/* HvacTemperatureCabinSetting */
	CBC_SIG_ID_HTMPST	= 963,	/* HvacTemperatureSeat */
	CBC_SIG_ID_HTSSTT	= 964,	/* HvacTemperatureSeatSetting */
	CBC_SIG_ID_HTMPU	= 965,	/* HvacTemperatureUnits */
	CBC_SIG_ID_HTUSTT	= 966,	/* HvacTemperatureUnitsSetting */
	CBC_SIG_ID_HVTST	= 967,	/* HvacVentilationSeat */
	CBC_SIG_ID_HVSSTT	= 968,	/* HvacVentilationSeatSetting */
	CBC_SIG_ID_HRCAT	= 969,	/* HvacRecirculationAutomatic */
	CBC_SIG_ID_HRASTT	= 970,	/* HvacRecirculationAutomaticSetting */
};

/*
 * CBC rx group identity definition.
 */
enum cbc_rx_group_id {
	CBC_GRP_ID_0	= 0,
};

/*
 * CBC tx group identity definition.
 */
enum cbc_tx_group_id {
	CBC_GRP_ID_LOC	= 874,	/* Location */
	CBC_GRP_ID_PDF	= 883,	/* PdcDistanceFront */
	CBC_GRP_ID_PDR	= 891,	/* PdcDistanceRear */
	CBC_GRP_ID_VAC	= 899,	/* VehicleAcceleration */
	CBC_GRP_ID_GAS	= 909,	/* GyroAbs */
	CBC_GRP_ID_IVR	= 913,	/* IocVersionResponse */
	CBC_GRP_ID_IRM	= 917,	/* IocRuntimeMeasurementResultsResponse */
	CBC_GRP_ID_GAC	= 922,	/* GyroAcc */
};

/*
 * IOC channels definition.
 * Include all native CBC channels and one virtual UART
 */
enum ioc_ch_id {
	IOC_NATIVE_PMT,		/* Native /dev/cbc-pmt */
	IOC_NATIVE_LFCC,	/* Native /dev/cbc-lifecycle */
	IOC_NATIVE_SIGNAL,	/* Native /dev/cbc-signals */
	IOC_NATIVE_ESIG,	/* Native /dev/cbc-early-signals */
	IOC_NATIVE_DIAG,	/* Native /dev/cbc-diagnosis */
	IOC_NATIVE_DLT,		/* Native /dev/cbc_dlt */
	IOC_NATIVE_LINDA,	/* Native /dev/cbc-lindata */
	IOC_NATIVE_RAW0,	/* Native /dev/cbc-raw0 */
	IOC_NATIVE_RAW1,	/* Native /dev/cbc-raw1 */
	IOC_NATIVE_RAW2,	/* Native /dev/cbc-raw2 */
	IOC_NATIVE_RAW3,	/* Native /dev/cbc-raw3 */
	IOC_NATIVE_RAW4,	/* Native /dev/cbc-raw4 */
	IOC_NATIVE_RAW5,	/* Native /dev/cbc-raw5 */
	IOC_NATIVE_RAW6,	/* Native /dev/cbc-raw6 */
	IOC_NATIVE_RAW7,	/* Native /dev/cbc-raw7 */
	IOC_NATIVE_RAW8,	/* Native /dev/cbc-raw8 */
	IOC_NATIVE_RAW9,	/* Native /dev/cbc-raw9 */
	IOC_NATIVE_RAW10,	/* Native /dev/cbc-raw10 */
	IOC_NATIVE_RAW11,	/* Native /dev/cbc-raw11 */
	IOC_VIRTUAL_UART,	/* Virtual UART */
	IOC_LOCAL_EVENT,	/* Local channel for IOC event */
	IOC_NATIVE_DUMMY0,	/* Native fake lifecycle channel */
	IOC_NATIVE_DUMMY1,	/* Native fake signal channel */
	IOC_NATIVE_DUMMY2,	/* Native Fake oem raw channel */
	IOC_CH_MAX
};

/*
 * CBC priority is used to pack CBC address layer header.
 */
enum cbc_prio {
	CBC_PRIO_LOW	= 2,
	CBC_PRIO_MEDIUM	= 3,
	CBC_PRIO_HIGH	= 6
};

/*
 * CBC invalidation types.
 */
enum cbc_inval_type {
	CBC_INVAL_T_SIGNAL,
	CBC_INVAL_T_GROUP
};

/*
 * CBC signal and group state flag.
 */
enum cbc_flag {
	CBC_INACTIVE,
	CBC_ACTIVE
};

/*
 * CBC queue types.
 * Rx queue buffers cbc_requests for virtual UART -> native CBC channels.
 * Tx queue buffers cbc_requests for antive CBC cdevs -> virtual UART.
 * Free queue buffers the cbc_requests that are not in rx/tx queues for new data
 * comming.
 */
enum cbc_queue_type {
	CBC_QUEUE_T_RX,
	CBC_QUEUE_T_TX,
	CBC_QUEUE_T_FREE
};

/*
 * CBC request types.
 */
enum cbc_request_type {
	CBC_REQ_T_PROT,		/* CBC protocol request */
	CBC_REQ_T_SUSPEND,	/* CBC suspend request */
	CBC_REQ_T_SHUTDOWN,	/* CBC shutdown request */
	CBC_REQ_T_HB_INIT,	/* CBC Heartbeat init request */
	CBC_REQ_T_UOS_ACTIVE,	/* CBC UOS active request */
	CBC_REQ_T_UOS_INACTIVE	/* CBC UOS inactive request */
};

/*
 * Open the channel and add it into IOC epoll event data if the channel state
 * is ON, otherwise ignore it.
 */
enum ioc_ch_stat {
	IOC_CH_OFF,
	IOC_CH_ON
};

struct cbc_signal {
	uint16_t id;		/* CBC signal id number */
	uint16_t len;		/* CBC signal length in bits not bytes */
	enum cbc_flag flag;	/* CBC signal active/inactive flag */
};

struct cbc_group {
	uint16_t id;		/* CBC group id number */
	enum cbc_flag flag;	/* CBC group active/inactive flag */
};

struct wlist_signal {
	uint16_t id;
	struct cbc_signal *sig;
};

struct wlist_group {
	uint16_t id;
	struct cbc_group *grp;
};

/*
 * CBC ring is used to buffer bytes before build one complete CBC frame.
 */
struct cbc_ring {
	uint32_t head;
	uint32_t tail;
	uint8_t buf[CBC_RING_BUFFER_SIZE];
};

/*
 * CBC configuration contains signal/group tables and whiltlist tables.
 */
struct cbc_config {
	int32_t cbc_sig_num;			/* CBC signals number */
	int32_t cbc_grp_num;			/* CBC groups number */
	int32_t wlist_sig_num;			/* Whitelist signals number */
	int32_t wlist_grp_num;			/* Whitelist groups number */
	struct cbc_signal *cbc_sig_tbl;		/* CBC signals table */
	struct cbc_group *cbc_grp_tbl;		/* CBC groups table */
	struct wlist_signal *wlist_sig_tbl;	/* Whitelist signals table */
	struct wlist_group *wlist_grp_tbl;	/* Whitelist groups table */
};

/*
 * IOC channel information.
 */
struct ioc_ch_info {
	int32_t fd;		/* IOC channel fd */
	char name[32];		/* IOC channel name */
	enum ioc_ch_id id;	/* IOC channel identity number */
	enum ioc_ch_stat stat;	/* IOC channel state */
};

/*
 * CBC request is mainly structure of communication between threads.
 */
struct cbc_request {
	int32_t srv_len;		/* Service frame length */
	int32_t link_len;		/* Link frame length */
	enum ioc_ch_id id;		/* Channel id number */
	enum cbc_request_type rtype;	/* Request types */
	uint8_t buf[CBC_MAX_FRAME_SIZE];

	SIMPLEQ_ENTRY(cbc_request) me_queue;
};

/*
 * IOC state types.
 */
enum ioc_state_type {
	IOC_S_INIT,
	IOC_S_ACTIVE,
	IOC_S_SUSPENDING,
	IOC_S_SUSPENDED
};

/*
 * IOC event types.
 */
enum ioc_event_type {
	IOC_E_INVALID,
	IOC_E_HB_ACTIVE,
	IOC_E_RAM_REFRESH,
	IOC_E_HB_INACTIVE,
	IOC_E_SHUTDOWN,
	IOC_E_RESUME,
	IOC_E_KNOCK,
};

/*
 * VM request types.
 */
enum vm_request_type {
	VM_REQ_NONE,
	VM_REQ_STOP,
	VM_REQ_SUSPEND,
	VM_REQ_RESUME
};

/*
 * CBC packet is mainly structure for CBC protocol process.
 */
struct cbc_pkt {
	bool uos_active;		/* Mark UOS active status */
	uint32_t reason;		/* Record current wakeup reason */
	struct cbc_request *req;	/* CBC packet data */
	struct cbc_config *cfg;		/* CBC and whitelist configurations */
	enum cbc_queue_type qtype;	/* Routes cbc_request to queue */
	enum ioc_event_type evt;	/* Record last event */
	struct ioc_dev *ioc;		/* IOC device */
};

/*
 * CBC simple queue head definition.
 */
SIMPLEQ_HEAD(cbc_qhead, cbc_request);

/*
 * IOC device structure.
 * IOC device is a virtual device and DM has virtual device data structure
 * for virtual devices management in the further.
 * So export the ioc_dev definition to the IOC header file.
 */
struct ioc_dev {
	char name[16];			/* Core thread name */
	bool cbc_enable;		/* Tx and Rx protocol enable flag */
	int closing;			/* Close IOC mediator device flag */
	int epfd;			/* Epoll fd */
	int32_t evt_fd;			/* Pipe write fd to trigger one event */
	uint32_t boot_reason;		/* Boot or resume wakeup reason */
	enum vm_request_type vm_req;	/* Request from VM Manager (acrnctl) */
	enum ioc_state_type state;	/* IOC state type */
	struct epoll_event *evts;	/* Epoll events table */
	struct cbc_request *pool;	/* CBC requests pool */
	struct cbc_ring ring;		/* Ring buffer */
	pthread_t tid;			/* Core thread id */
	struct cbc_qhead free_qhead;	/* Free queue head */
	pthread_mutex_t free_mtx;	/* Free queue mutex */

	char rx_name[16];		/* Rx thread name */
	struct cbc_qhead rx_qhead;	/* Rx queue head */
	struct cbc_config rx_config;	/* Rx configuration */
	pthread_t rx_tid;
	pthread_cond_t rx_cond;
	pthread_mutex_t rx_mtx;
	void (*ioc_dev_rx)(struct cbc_pkt *pkt);

	char tx_name[16];		/* Tx thread name */
	struct cbc_qhead tx_qhead;	/* Tx queue head */
	struct cbc_config tx_config;	/* Tx configuration */
	pthread_t tx_tid;
	pthread_cond_t tx_cond;
	pthread_mutex_t tx_mtx;
	void (*ioc_dev_tx)(struct cbc_pkt *pkt);
};

/*
 * IOC state information.
 */
struct ioc_state_info {
	enum ioc_state_type cur_stat;
	enum ioc_state_type next_stat;
	enum ioc_event_type evt;
	int32_t (*handler)(struct ioc_dev *ioc);
};

/* Parse IOC parameters */
int ioc_parse(const char *opts);

struct vmctx;

/* IOC mediator common ops */
int ioc_init(struct vmctx *ctx);
void ioc_deinit(struct vmctx *ctx);

/* Build a cbc_request and send it to CBC protocol stack */
void ioc_build_request(struct ioc_dev *ioc, int32_t link_len, int32_t srv_len);

/* Send data to native CBC cdevs and virtual PTY(UART DM) device */
int ioc_ch_xmit(enum ioc_ch_id id, const uint8_t *buf, size_t size);

/* Main handlers of CBC protocol stack */
void cbc_rx_handler(struct cbc_pkt *pkt);
void cbc_tx_handler(struct cbc_pkt *pkt);

/* Copy to buf to the ring buffer */
int cbc_copy_to_ring(const uint8_t *buf, size_t size, struct cbc_ring *ring);

/* Build a cbc_request based on CBC link layer protocol */
void cbc_unpack_link(struct ioc_dev *ioc);

/* Whitelist initialization */
void wlist_init_signal(struct cbc_signal *cbc_tbl, size_t cbc_size,
		struct wlist_signal *wlist_tbl, size_t wlist_size);
void wlist_init_group(struct cbc_group *cbc_tbl, size_t cbc_size,
		struct wlist_group *wlist_tbl, size_t wlist_size);

/* Set CBC log file */
void cbc_set_log_file(FILE *f);

/* Update IOC state by the event */
void ioc_update_event(int fd, enum ioc_event_type evt);
#endif
