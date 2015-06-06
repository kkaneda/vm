#ifndef _VMM_MON_RTC_H
#define _VMM_MON_RTC_H

#include "vmm/common.h"


#ifndef RTC_PORT
# define RTC_PORT( x )		 ( 0x70 + ( x ) )
#endif


/* Real Time CLock */
struct rtc_t {
	int 			addr;
};

void     Rtc_init ( struct rtc_t *x );
void     Rtc_pack ( struct rtc_t *x, int fd );
void     Rtc_unpack ( struct rtc_t *x, int fd );
bit32u_t Rtc_read ( struct rtc_t *rtc, bit16u_t addr, size_t len );
void     Rtc_write ( struct rtc_t *rtc, bit16u_t addr, bit32u_t val, size_t len );


#endif /*_VMM_MON_RTC_H */

