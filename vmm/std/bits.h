#ifndef _VMM_STD_BITS_H
#define _VMM_STD_BITS_H

/* [NOTE] (~(~0 << n)) = ((1 << n) - 1) */
#define BIT_MASK(n)  ( ~ ( ~0 << (n) ) )
#define BIT_ALIGN(x, n)  ( (x) & ( ( ~0 ) << (n) ) )

#define SET_BIT(x, n)	( x |= ( 1 << (n) ) ) 
#define CLEAR_BIT(x, n)	( x &= ~( 1 << (n) ) ) 
#define TEST_BIT(x, n)	( ( (x) >> (n) ) & BIT_MASK ( 1 ) ) 

#define SUB_BIT(x, start, len) ( ( ( (x) >> (start) ) & BIT_MASK ( len ) ) )
#define LSHIFTED_SUB_BIT(x, start, len, shift) ( ( SUB_BIT ( x, start, len ) ) << (shift) )

#define SET_BITS(x, start, len, val)  ( x |= ( ( SUB_BIT ( val, 0, len ) ) << (start) ) )
#define CLEAR_BITS(x, start, len) ( x &= ~( ( BIT_MASK ( len ) << (start) ) ) ) 


/* for 64bit */
#define BIT_MASK_LL(n)  ( ~ (~0LL << (n) ) )
#define SUB_BIT_LL(x, start, len) ( ( ( (x) >> (start) ) & BIT_MASK_LL ( len ) ) )
#define LSHIFTED_SUB_BIT_LL(x, start, len, shift) ( ( SUB_BIT_LL ( x, start, len ) ) << (shift) )


#endif /* _VMM_STD_BITS_H */
