#include "vmm/mon/mon.h"


/* from linux/bcd.h */
#ifndef BCD2BIN
#  define BCD2BIN(val)	(((val) & 0x0f) + ((val)>>4)*10)
#endif

#ifndef BIN2BCD
#  define BIN2BCD(val)	((((val)/10)<<4) + (val)%10)
#endif

#ifndef RTC_SECONDS
#  define RTC_SECONDS		0
#endif

#ifndef RTC_MINUTES
#  define RTC_MINUTES		2
#endif

#ifndef RTC_HOURS
#  define RTC_HOURS		4
#endif

#ifndef RTC_DAY_OF_WEEK
#  define RTC_DAY_OF_WEEK	6
#endif

#ifndef RTC_DAY_OF_MONTH
#  define RTC_DAY_OF_MONTH	7
#endif

#ifndef RTC_MONTH
#  define RTC_MONTH		8
#endif

#ifndef RTC_YEAR
#  define RTC_YEAR		9
#endif


#define RTC_REG_A		10
#define RTC_REG_B		11
#define RTC_REG_C		12
#define RTC_REG_D		13

#define RTC_FREQ_SELECT	RTC_REG_A
# define RTC_UIP		0x80
# define RTC_RATE_SELECT 	0x0F


#define RTC_CONTROL	RTC_REG_B
# define RTC_DM_BINARY 0x04	/* all time/date values are BCD if clear */


#define RTC_INTR_FLAGS	RTC_REG_C


struct rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

void
Rtc_init ( struct rtc_t *x )
{
	ASSERT ( x != NULL );
	/* [TODO] */
	x->addr = 0;
}

void 
Rtc_pack ( struct rtc_t *x, int fd )
{
	Pack ( x, sizeof ( struct rtc_t ), fd ); 
}

void 
Rtc_unpack ( struct rtc_t *x, int fd )
{
	Unpack ( x, sizeof ( struct rtc_t ), fd ); 
}
 
static struct rtc_time
RtcTime_of_Seconds ( int val ) 
{
	const int DAYS_4Y = ( 365 * 4 + 1 );
	const int DAYS_100Y = ( DAYS_4Y * 100 - 1 );
	const int DAYS_400Y = ( DAYS_100Y * 4 + 1 );
	int year400, year100, year4, year1;
	struct rtc_time x;
   
	x.tm_sec = val % 60;
	val /= 60;

	x.tm_min = val % 60;
	val /= 60;

	x.tm_hour = val % 24;
	val /= 24;

	/* [TODO] lilyVMからとってきただけなので，あとで整理する．*/
	val = val + DAYS_400Y * 5 - ( 365 * 30 + 7 + 31 + 29 );
	year400 = val / DAYS_400Y;

	val = val % DAYS_400Y;
	year100 = val / DAYS_100Y;

	val = val % DAYS_100Y;
	year4 = val / DAYS_4Y;
   
	val = val % DAYS_4Y;
	year1 = val / 365 - val / ( 365 * 4 );

	val = val - year1 * 365;
	x.tm_mon = ( val * 10 - 5 ) / 306;

	val = val - ( 306 * x.tm_mon + 5 ) / 10;
	x.tm_year = year400 * 400 + year100 * 100 + year4 * 4 + year1;

	if ( x.tm_mon > 9 ) {
		x.tm_mon -= 9;
		x.tm_year++;
	} else {
		x.tm_mon += 3;
	}
	x.tm_mday = val + 1;

	x.tm_wday = x.tm_mday % 7; /* [TODO] */

	return x;
}

/* [TODO] Real Time Clock Driver の仕様が良く分からないので，本当に正しく動いてるかは
 * 不明．特に RTC_FREQ_SELECT と RTC_CONTROL */
bit32u_t
Rtc_read ( struct rtc_t *rtc, bit16u_t addr, size_t len )
{

	struct timeval tv;
	struct rtc_time x;
	bit32u_t retval = 0;
	static bool_t f = FALSE;
	ASSERT ( rtc != NULL );

	if ( addr != RTC_PORT ( 1 ) ) 
		return 0;

	DPRINT ( "Rtc_read: rtc.addr=%#x\n", rtc->addr );
   
	Gettimeofday ( &tv, NULL );
	x = RtcTime_of_Seconds ( tv.tv_sec );

	switch ( rtc->addr ) {
	case RTC_SECONDS:  	retval = BIN2BCD ( x.tm_sec ); break;
	case RTC_MINUTES:  	retval = BIN2BCD ( x.tm_min ); break;
	case RTC_HOURS: 	retval = BIN2BCD ( x.tm_hour ); break;
	case RTC_DAY_OF_WEEK:  	retval = BIN2BCD ( x.tm_wday ); break;
	case RTC_DAY_OF_MONTH:  retval = BIN2BCD ( x.tm_mday ); break;
	case RTC_MONTH: 	retval = BIN2BCD ( x.tm_mon ); break;
	case RTC_YEAR: 		retval = BIN2BCD ( x.tm_year % 100 ); break;

	case RTC_FREQ_SELECT: 	retval = ( f ? RTC_UIP : RTC_RATE_SELECT ); f = !f; break; /* [TODO] */
	case RTC_CONTROL:	retval = RTC_DM_BINARY; break; /* [TODO] */
	case RTC_INTR_FLAGS:	retval = 0; break; /* TODO*/

	case 0x10:		retval = 0; break; /* TODO */ 
	case 0x12:		retval = 0; break; /* TODO: probe_cmos_for_drives */ 

	default:	  	Match_failure ( "Rtc_read: %#x\n", rtc->addr );

	} // RTC_REF_CLCK_4MHZ
	return retval;
}

void
Rtc_write ( struct rtc_t *rtc, bit16u_t addr, bit32u_t val, size_t len )
{
	ASSERT ( rtc != NULL );

	switch ( addr ) {
	case RTC_PORT ( 0 ): rtc->addr = val; break;
	case RTC_PORT ( 1 ): /* write <val> at <mon->devs.rtc.addr> */ break;
	case RTC_PORT ( 2 ): assert ( 0 ); break;
	case RTC_PORT ( 3 ): assert ( 0 ); break;
	default:	    Match_failure ( "Rtc_write\n" );
	}

	DPRINT ( "Rtc_write: rtc.addr=%#x\n", rtc->addr );
} 
