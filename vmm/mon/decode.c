#include "vmm/mon/mon.h"


/*
 * [Note]
 * The current implementaion of the instruction emulation only
 * supports 32-bit addressing mode
 *
 * [Reference]
 * IA-32 manual Vol.2A 2-1 
 * IA-32 manual Vol.2A appendix B 
 * source code of Bochs ( cpu/fetchdecode.cc )
 */

static void send_start_or_stop_profile_request ( struct mon_t *mon );


typedef bit16u_t	op_attr_t;

enum {
	ATTR_NONE 			= 0x0000,

	ATTR_IMMEDIATE  		= 0x000f, // bits 3..0: any immediate
	ATTR_IMMEDIATE_IB		= 0x0001, // 8 bits regardless
	ATTR_IMMEDIATE_IB_SE  		= 0x0002, // sign extend to OS size
	ATTR_IMMEDIATE_IV    		= 0x0003, // 16 or 32 depending on OS size
	ATTR_IMMEDIATE_BROFF32 		= 0x0003, // = ATTR_IMMEDIATE_IV
	ATTR_IMMEDIATE_IW    		= 0x0004, // 16 bits regardless
	ATTR_IMMEDIATE_IV_IW  		= 0x0005, // call_Ap
	ATTR_IMMEDIATE_IW_IB  		= 0x0006, // enter_IwIb
	ATTR_IMMEDIATE_O    		= 0x0007, // mov_ALOb, mov_ObAL, mov_eAXOv, mov_OveAX
	ATTR_IMMEDIATE_BR_OFF8 		= 0x0008, // Relative branch offset byte
	ATTR_IMMEDIATE_BR_OFF16	 	= 0x0009, // Relative branch offset word
	ATTR_IMMEDIATE_BR_OFF32	 	= ATTR_IMMEDIATE_IV,

	ATTR_GROUP_X      		= 0x0070,
	ATTR_GROUP_N     		= 0x0010,
	ATTR_GROUP_1      		= 0x0010,
	ATTR_GROUP_2     		= 0x0010,
	ATTR_GROUP_3    		= 0x0010,
	ATTR_GROUP_4    		= 0x0010,
	ATTR_GROUP_5    		= 0x0010,
	ATTR_GROUP_6    		= 0x0010,
	ATTR_GROUP_7     		= 0x0010,
	ATTR_GROUP_8    		= 0x0010,
	ATTR_GROUP_15    		= 0x0010,
	ATTR_FP_GROUP	 		= 0x0040,

	ATTR_PREFIX      		= 0x0080,
	ATTR_ANOTHER     		= 0x0100,
	ATTR_SENSITIVE   		= 0x0200,

	ATTR_REPEATABLE   		= 0x0800
};


/* prefixes */
enum {
	/* Group 1 */
	PREFIX_LOCK			= 0xf0,
	PREFIX_REPNE_REPNZ 		= 0xf2,
	PREFIX_REP_REPE_REPZ	 	= 0xf3,

	/* Group 2 */
	PREFIX_CS_OVERRIDE		= 0x2e, /* branch info ( if instr. is Jcc ) */
	PREFIX_SS_OVERRIDE		= 0x36,
	PREFIX_DS_OVERRIDE		= 0x3e,
	PREFIX_ES_OVERRIDE		= 0x26,
	PREFIX_FS_OVERRIDE		= 0x64,
	PREFIX_GS_OVERRIDE		= 0x65, /* branch info ( if instr. is Jcc ) */
   
	/* Group 3 */
	PREFIX_OPSIZE_OVERRIDE		= 0x66,

	/* Group 4 */
	PREFIX_ADDRSIZE_OVERRIDE	= 0x67
};

enum {
	ESCAPE_OPCODE 			= 0x0f
};


struct opcode_info_t {
	bool_t			is_supported;
	char			*name;
	op_attr_t		attr;
	execute_t		*execute;
	mem_access_func_t	*maccess;
	bool_t			is_sensitive;
	struct opcode_info_t	*next;
};


enum { 
	NUM_OF_OPCODE_INFO	 = 0x10000, 
	NUM_OF_OPCODE_INFOG	 = 8
};

static struct opcode_info_t opcode_info[NUM_OF_OPCODE_INFO];
static struct opcode_info_t opcode_info_group1eb_ib[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_group1ed[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_group2eb[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_group2ed[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_group3eb[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_group3ed[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_group4[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_group5d[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_group6[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_group7[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_group8ev_ib[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_group15[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_fp_group_d8[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_fp_group_d9[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_fp_group_db[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_fp_group_dc[NUM_OF_OPCODE_INFOG];
static struct opcode_info_t opcode_info_fp_group_dd[NUM_OF_OPCODE_INFOG];

static struct opcode_info_t *opcode_info_groups[] = 
{
	opcode_info_group1eb_ib,
	opcode_info_group1ed,
	opcode_info_group2eb,
	opcode_info_group2ed,
	opcode_info_group3eb,
	opcode_info_group3ed,
	opcode_info_group4,
	opcode_info_group5d,
	opcode_info_group6,
	opcode_info_group7,
	opcode_info_group8ev_ib,
	opcode_info_group15,
	opcode_info_fp_group_d8,
	opcode_info_fp_group_d9,
	opcode_info_fp_group_db,
	opcode_info_fp_group_dc,
	opcode_info_fp_group_dd,
	NULL
};

/* sreg_decode_info[sib_flag][mod][rm_or_base] */
static seg_reg_index_t sreg_decode_info[2][3][8] =
{
	/* sreg_decode_info[0][...][...] */
	{
		/* sreg_decode_info[0][0][...] */
		{
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS
		},

		/* sreg_decode_info[0][1][...] */
		{
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_NULL,
			SEG_REG_SS,
			SEG_REG_DS,
			SEG_REG_DS
		},

		/* sreg_decode_info[0][2][...] */
		{
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_NULL,
			SEG_REG_SS,
			SEG_REG_DS,
			SEG_REG_DS
		}
	},

	/* sreg_decode_info[1][...][...] */
	{
		/* sreg_decode_info[1][0][...] */
		{
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_SS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS
		},

		/* sreg_decode_info[1][1][...] */
		{
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_SS,
			SEG_REG_SS,
			SEG_REG_DS,
			SEG_REG_DS
		},

		/* sreg_decode_info[1][2][...] */
		{
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_DS,
			SEG_REG_SS,
			SEG_REG_SS,
			SEG_REG_DS,
			SEG_REG_DS
		}
	}
};

#define define_resolve_xxx( name ) \
bit32u_t resolve_ ## name ( struct instruction_t *instr, struct user_regs_struct *regs ) \
{ \
	return regs->name; \
}

define_resolve_xxx ( eax )
define_resolve_xxx ( ecx )
define_resolve_xxx ( edx )
define_resolve_xxx ( ebx )
//define_resolve_xxx ( esp )
define_resolve_xxx ( esi )
define_resolve_xxx ( edi )

bit32u_t
resolve_disp ( struct instruction_t *instr, struct user_regs_struct *regs )
{ 
	return instr->disp; 
}

#define define_resolve_xxx_disp( name ) \
bit32u_t resolve_ ## name ## _disp ( struct instruction_t *instr, struct user_regs_struct *regs ) \
{ \
	return regs->name + instr->disp; \
}

define_resolve_xxx_disp ( eax )
define_resolve_xxx_disp ( ecx )
define_resolve_xxx_disp ( edx )
define_resolve_xxx_disp ( ebx )
define_resolve_xxx_disp ( ebp )
define_resolve_xxx_disp ( esi )
define_resolve_xxx_disp ( edi )

static bit32u_t
get_scaled_index ( struct instruction_t *instr, struct user_regs_struct *regs )
{
	ASSERT ( instr != NULL );
	ASSERT ( regs != NULL );

	return ( ( instr->sib_index == 4 )
		 ? 0 
		 : ( ( UserRegs_get ( regs, instr->sib_index ) ) << instr->sib_scale ) );
}

#define define_resolve_xxx_scaled( name ) \
bit32u_t resolve_ ## name ## _scaled ( struct instruction_t *instr, struct user_regs_struct *regs ) \
{ \
	return regs->name + get_scaled_index ( instr, regs ); \
}

define_resolve_xxx_scaled ( eax )
define_resolve_xxx_scaled ( ecx )
define_resolve_xxx_scaled ( edx )
define_resolve_xxx_scaled ( ebx )
define_resolve_xxx_scaled ( esp )
define_resolve_xxx_scaled ( esi )
define_resolve_xxx_scaled ( edi )

bit32u_t
resolve_disp_scaled ( struct instruction_t *instr, struct user_regs_struct *regs )
{ 
	return instr->disp + get_scaled_index ( instr, regs );
}


#define define_resolve_xxx_disp_scaled( name ) \
bit32u_t resolve_ ## name ## _disp_scaled ( struct instruction_t *instr, struct user_regs_struct *regs ) \
{ \
	return regs->name + instr->disp + get_scaled_index ( instr, regs ); \
}

define_resolve_xxx_disp_scaled ( eax )
define_resolve_xxx_disp_scaled ( ecx )
define_resolve_xxx_disp_scaled ( edx )
define_resolve_xxx_disp_scaled ( ebx )
define_resolve_xxx_disp_scaled ( esp )
define_resolve_xxx_disp_scaled ( ebp )
define_resolve_xxx_disp_scaled ( esi )
define_resolve_xxx_disp_scaled ( edi )

bit32u_t
resolve_dummy ( struct instruction_t *instr, struct user_regs_struct *regs )
{
	assert ( 0 );
	return 0;
}


/* resolve_decode_info[sib_flag][mod][rm_or_base] */
static resolve_t *resolve_decode_info[2][3][8] =
{
	/* resolve_decode_info[0][...][...] */
	{
		/* resolve_decode_info[0][0][...] */
		{
			&resolve_eax,
			&resolve_ecx,
			&resolve_edx,
			&resolve_ebx,
			NULL,
			&resolve_disp,
			&resolve_esi,
			&resolve_edi
		},

		/* resolve_decode_info[0][1][...] */
		{
			&resolve_eax_disp,
			&resolve_ecx_disp,
			&resolve_edx_disp,
			&resolve_ebx_disp,
			NULL,
			&resolve_ebp_disp,
			&resolve_esi_disp,
			&resolve_edi_disp
		},

		/* resolve_decode_info[0][2][...] */
		{
			&resolve_eax_disp,
			&resolve_ecx_disp,
			&resolve_edx_disp,
			&resolve_ebx_disp,
			NULL,
			&resolve_ebp_disp,
			&resolve_esi_disp,
			&resolve_edi_disp
		}
	},

	/* resolve_decode_info[1][...][...] */
	{
		/* resolve_decode_info[1][0][...] */
		{
			&resolve_eax_scaled,
			&resolve_ecx_scaled,
			&resolve_edx_scaled,
			&resolve_ebx_scaled,
			&resolve_esp_scaled,
			&resolve_disp_scaled,
			&resolve_esi_scaled,
			&resolve_edi_scaled
		},

		/* resolve_decode_info[1][1][...] */
		{
			&resolve_eax_disp_scaled,
			&resolve_ecx_disp_scaled,
			&resolve_edx_disp_scaled,
			&resolve_ebx_disp_scaled,
			&resolve_esp_disp_scaled,
			&resolve_ebp_disp_scaled,
			&resolve_esi_disp_scaled,
			&resolve_edi_disp_scaled
		},

		/* resolve_decode_info[1][2][...] */
		{
			&resolve_eax_disp_scaled,
			&resolve_ecx_disp_scaled,
			&resolve_edx_disp_scaled,
			&resolve_ebx_disp_scaled,
			&resolve_esp_disp_scaled,
			&resolve_ebp_disp_scaled,
			&resolve_esi_disp_scaled,
			&resolve_edi_disp_scaled
		}
	}
};


static void
clear_opcode_info ( struct opcode_info_t *x )
{
	ASSERT ( x != NULL );

	x->is_supported = FALSE;
	x->name = "";
	x->attr = -1;
	x->execute = NULL;
	x->maccess = NULL;
	x->is_sensitive = FALSE;
	x->next = NULL;   
}

static void
clear_opcode_info_group ( struct opcode_info_t x[] )
{
	int i;

	for ( i = 0; i < NUM_OF_OPCODE_INFOG; i++ )
		clear_opcode_info ( &x[i] );
}

static void
clear_all_opcode_info ( void )
{
	int i;

	for ( i = 0; i < NUM_OF_OPCODE_INFO; i++ )
		clear_opcode_info ( &opcode_info[i] );

	for ( i = 0; opcode_info_groups[i] != NULL; i++ )
		clear_opcode_info_group ( opcode_info_groups[i] );
}

static void
set_opcode_g ( struct opcode_info_t group[], int index, op_attr_t attr, char *name, execute_t *execute, mem_access_func_t *maccess )
{
	struct opcode_info_t *x;
	
	ASSERT ( execute != NULL );

	x = &group[index];
	x->is_supported = TRUE;
	x->name = name;
	x->attr = attr;
	x->execute = execute;
	x->maccess = maccess;
}

static void
init_opcode_info_group1eb_ib ( void )
{
	struct opcode_info_t *x = opcode_info_group1eb_ib;
   
	set_opcode_g ( x, 0, ATTR_NONE, "add_eb_ib", &add_eb_ib, &maccess_rdwr_byte );
	set_opcode_g ( x, 1, ATTR_NONE, "or_eb_ib", &or_eb_ib, &maccess_rdwr_byte );
	set_opcode_g ( x, 2, ATTR_NONE, "adc_eb_ib", &adc_eb_ib, &maccess_rdwr_byte );
	set_opcode_g ( x, 3, ATTR_NONE, "sbb_eb_ib", &sbb_eb_ib, &maccess_rdwr_byte );
	set_opcode_g ( x, 4, ATTR_NONE, "and_eb_ib", &and_eb_ib, &maccess_rdwr_byte );
	set_opcode_g ( x, 5, ATTR_NONE, "sub_eb_ib", &sub_eb_ib, &maccess_rdwr_byte );
	set_opcode_g ( x, 6, ATTR_NONE, "xor_eb_ib", &xor_eb_ib, &maccess_rdwr_byte );
	set_opcode_g ( x, 7, ATTR_NONE, "cmp_eb_ib", &cmp_eb_ib, &maccess_rdonly_byte );
}

static void
init_opcode_info_group1ed ( void )
{
	struct opcode_info_t *x = opcode_info_group1ed;

	set_opcode_g ( x, 0, ATTR_NONE, "add_ed_id", &add_ed_id, &maccess_rdwr_dword );
	set_opcode_g ( x, 1, ATTR_NONE, "or_ed_id", &or_ed_id, &maccess_rdwr_dword );
	set_opcode_g ( x, 2, ATTR_NONE, "adc_ed_id", &adc_ed_id, &maccess_rdwr_dword );
	set_opcode_g ( x, 3, ATTR_NONE, "sbb_ed_id", &sbb_ed_id, &maccess_rdwr_dword );
	set_opcode_g ( x, 4, ATTR_NONE, "and_ed_id", &and_ed_id, &maccess_rdwr_dword );
	set_opcode_g ( x, 5, ATTR_NONE, "sub_ed_id", &sub_ed_id, &maccess_rdwr_dword );
	set_opcode_g ( x, 6, ATTR_NONE, "xor_ed_id", &xor_ed_id, &maccess_rdwr_dword );
	set_opcode_g ( x, 7, ATTR_NONE, "cmp_ed_id", &cmp_ed_id, &maccess_rdonly_dword );
}

static void
init_opcode_info_group2eb ( void )
{
	struct opcode_info_t *x = opcode_info_group2eb;

	set_opcode_g ( x, 0, ATTR_NONE, "rol_eb", &rol_eb, &maccess_rdwr_byte );
	set_opcode_g ( x, 1, ATTR_NONE, "ror_eb", &ror_eb, &maccess_rdwr_byte );
	set_opcode_g ( x, 2, ATTR_NONE, "rcl_eb", &rcl_eb, &maccess_rdwr_byte );
	set_opcode_g ( x, 3, ATTR_NONE, "rcr_eb", &rcr_eb, &maccess_rdwr_byte );
	set_opcode_g ( x, 4, ATTR_NONE, "shl_eb", &shl_eb, &maccess_rdwr_byte );
	set_opcode_g ( x, 5, ATTR_NONE, "shr_eb", &shr_eb, &maccess_rdwr_byte );
	set_opcode_g ( x, 6, ATTR_NONE, "shl_eb", &shl_eb, &maccess_rdwr_byte );
	set_opcode_g ( x, 7, ATTR_NONE, "sar_eb", &sar_eb, &maccess_rdwr_byte );
}

static void
init_opcode_info_group2ed ( void )
{
	struct opcode_info_t *x = opcode_info_group2ed;

	set_opcode_g ( x, 0, ATTR_NONE, "rol_ed", &rol_ed, &maccess_rdwr_dword );
	set_opcode_g ( x, 1, ATTR_NONE, "ror_ed", &ror_ed, &maccess_rdwr_dword );
	set_opcode_g ( x, 2, ATTR_NONE, "rcl_ed", &rcl_ed, &maccess_rdwr_dword );
	set_opcode_g ( x, 3, ATTR_NONE, "rcr_ed", &rcr_ed, &maccess_rdwr_dword );
	set_opcode_g ( x, 4, ATTR_NONE, "shl_ed", &shl_ed, &maccess_rdwr_dword );
	set_opcode_g ( x, 5, ATTR_NONE, "shr_ed", &shr_ed, &maccess_rdwr_dword );
	set_opcode_g ( x, 6, ATTR_NONE, "shl_ed", &shl_ed, &maccess_rdwr_dword );
	set_opcode_g ( x, 7, ATTR_NONE, "sar_ed", &sar_ed, &maccess_rdwr_dword );
}

static void
init_opcode_info_group3eb ( void )
{ 
	struct opcode_info_t *x = opcode_info_group3eb;
	int i;

	for ( i = 0; i < 2; i++ ) {
		set_opcode_g ( x, i, ATTR_IMMEDIATE_IB, "test_eb_ib", &test_eb_ib, &maccess_rdonly_byte );
	}
	set_opcode_g ( x, 2, ATTR_NONE, "not_eb", &not_eb, &maccess_rdwr_byte );
	set_opcode_g ( x, 3, ATTR_NONE, "neg_eb", &neg_eb, &maccess_rdwr_byte );
	set_opcode_g ( x, 4, ATTR_NONE, "mul_al_eb", &mul_al_eb, &maccess_rdonly_byte );
	set_opcode_g ( x, 5, ATTR_NONE, "imul_al_eb", &imul_al_eb, &maccess_rdonly_byte );
	set_opcode_g ( x, 6, ATTR_NONE, "div_al_eb", &div_al_eb , &maccess_rdonly_byte );
	set_opcode_g ( x, 7, ATTR_NONE, "idiv_al_eb", &idiv_al_eb, &maccess_rdonly_byte );
}

static void
init_opcode_info_group3ed ( void )
{ 
	struct opcode_info_t *x = opcode_info_group3ed;
	int i;

	for ( i = 0; i < 2; i++ ) {
		set_opcode_g ( x, i, ATTR_IMMEDIATE_IV, "test_ed_id", &test_ed_id, &maccess_rdonly_dword );
	}
	set_opcode_g ( x, 2, ATTR_NONE, "not_ed", &not_ed, &maccess_rdwr_byte );
	set_opcode_g ( x, 3, ATTR_NONE, "neg_ed", &neg_ed, &maccess_rdwr_dword );
	set_opcode_g ( x, 4, ATTR_NONE, "mul_eax_ed", &mul_eax_ed, &maccess_rdonly_dword );
	set_opcode_g ( x, 5, ATTR_NONE, "imul_eax_ed", &imul_eax_ed, &maccess_rdonly_dword );
	set_opcode_g ( x, 6, ATTR_NONE, "div_eax_ed", &div_eax_ed , &maccess_rdonly_dword );
	set_opcode_g ( x, 7, ATTR_NONE, "idiv_eax_ed", &idiv_eax_ed, &maccess_rdonly_dword );
}

static void
init_opcode_info_group4 ( void )
{
	struct opcode_info_t *x = opcode_info_group4; 

	set_opcode_g ( x, 0, ATTR_NONE, "inc_eb", &inc_eb, &maccess_rdwr_byte );
	set_opcode_g ( x, 1, ATTR_NONE, "dec_eb", &dec_eb, &maccess_rdwr_byte );
}

static void
init_opcode_info_group5d ( void )
{
	struct opcode_info_t *x = opcode_info_group5d; 

	set_opcode_g ( x, 0, ATTR_NONE, "inc_ed", &inc_ed, &maccess_rdwr_dword );
	set_opcode_g ( x, 1, ATTR_NONE, "dec_ed", &dec_ed, &maccess_rdwr_dword );
	set_opcode_g ( x, 2, ATTR_NONE, "call_ed", &call_ed, &call_ed_mem );
	set_opcode_g ( x, 3, ATTR_SENSITIVE, "call32_ep", &call32_ep, &call32_ep_mem );
	set_opcode_g ( x, 4, ATTR_NONE, "jmp_ed", &jmp_ed, &jmp_ed_mem );
	set_opcode_g ( x, 5, ATTR_NONE, "jmp32_ep", &jmp32_ep, &jmp32_ep_mem );
	set_opcode_g ( x, 6, ATTR_NONE, "push_ed", &push_ed, &push_ed_mem );
}

static void
init_opcode_info_group6 ( void )
{
	struct opcode_info_t *x = opcode_info_group6; 
   
	set_opcode_g ( x, 0, ATTR_SENSITIVE, "sldt_ew", &sldt_ew, &maccess_wronly_word );
	set_opcode_g ( x, 1, ATTR_SENSITIVE, "str_ew", &str_ew, &maccess_wronly_word );
	set_opcode_g ( x, 2, ATTR_SENSITIVE, "lldt_ew", &lldt_ew, &maccess_rdonly_word );
	set_opcode_g ( x, 3, ATTR_SENSITIVE, "ltr_ew", &ltr_ew, &maccess_rdonly_word );
	set_opcode_g ( x, 4, ATTR_SENSITIVE, "verr_ew", &verr_ew, NULL );
	set_opcode_g ( x, 5, ATTR_SENSITIVE, "verw_ew", &verw_ew, NULL );
}

static void
init_opcode_info_group7 ( void )
{
	struct opcode_info_t *x = opcode_info_group7; 

	set_opcode_g ( x, 0, ATTR_SENSITIVE, "sgdt_ms", &sgdt_ms, &sgdt_ms_mem );
	set_opcode_g ( x, 1, ATTR_SENSITIVE, "sidt_ms", &sidt_ms, &sidt_ms_mem );
	set_opcode_g ( x, 2, ATTR_SENSITIVE, "lgdt_ms", &lgdt_ms, &lgdt_ms_mem );
	set_opcode_g ( x, 3, ATTR_SENSITIVE, "lidt_ms", &lidt_ms, &lidt_ms_mem );
	set_opcode_g ( x, 4, ATTR_SENSITIVE, "smsw_ew", &smsw_ew, NULL );
	set_opcode_g ( x, 6, ATTR_SENSITIVE, "lmsw_ew", &lmsw_ew, NULL );
	set_opcode_g ( x, 7, ATTR_SENSITIVE, "invlpg", &invlpg, &maccess_none );
}

static void
init_opcode_info_group8ev_ib ( void )
{
	struct opcode_info_t *x = opcode_info_group8ev_ib; 

	set_opcode_g ( x, 4, ATTR_NONE, "bt_ev_ib", &bt_ev_ib, &maccess_rdwr_dword );
	set_opcode_g ( x, 5, ATTR_NONE, "bts_ev_ib", &bts_ev_ib, &maccess_rdwr_dword );
	set_opcode_g ( x, 6, ATTR_NONE, "btr_ev_ib", &btr_ev_ib, &maccess_rdwr_dword );
	set_opcode_g ( x, 7, ATTR_NONE, "btc_ev_ib", &btc_ev_ib, &maccess_rdwr_dword );
}

/* [DEBUG] */
void
mfence ( struct mon_t *mon, struct instruction_t *instr )
{
	if ( mon->cpuid == BSP_CPUID ) {
		start_or_stop_profile ( mon );
	} else {
#ifdef ENABLE_MP
		send_start_or_stop_profile_request ( mon );
#endif
	}
}

/* [DEBUG] */
void
wbinvd ( struct mon_t *mon, struct instruction_t *instr )
{
	Warning ( "wbinvd (writeinstruction back and invalidate cache) at eip=%#x\n", mon->regs->user.eip );
	skip_instr ( mon, instr );
}


struct mem_access_t *
fxsave_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_write_resolve ( mon, instr, 512 );	
}

struct mem_access_t *
fxrstor_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_read_resolve ( mon, instr, 512 );	
}

struct mem_access_t *
maccess_fop_rdonly_word ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_read_resolve ( mon, instr, 2 );	
}

struct mem_access_t *
maccess_fop_wronly_word ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_write_resolve ( mon, instr, 2 );	
}

struct mem_access_t *
maccess_fop_rdonly_dword ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_read_resolve ( mon, instr, 4 );	
}

struct mem_access_t *
maccess_fop_wronly_dword ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_write_resolve ( mon, instr, 4 );	
}

struct mem_access_t *
maccess_fop_rdonly_ddword ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_read_resolve ( mon, instr, 8 );	
}

struct mem_access_t *
maccess_fop_wronly_ddword ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_write_resolve ( mon, instr, 8 );	
}

struct mem_access_t *
maccess_fop_rdonly_ddeword ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_read_resolve ( mon, instr, 10 );	
}

struct mem_access_t *
maccess_fop_wronly_ddeword ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_write_resolve ( mon, instr, 10 );	
}

static void
init_opcode_info_group15 ( void )
{
	struct opcode_info_t *x = opcode_info_group15; 

	set_opcode_g ( x, 0, ATTR_NONE, "fxsave", &dummy_instr, &fxsave_mem );
	set_opcode_g ( x, 1, ATTR_NONE, "fxrstor", &dummy_instr, &fxrstor_mem );

//	set_opcode_g ( x, 5, ATTR_NONE, "lfence", &lfence, &maccess_none );
	set_opcode_g ( x, 6, ATTR_SENSITIVE, "mfence", &mfence, &maccess_none );
//	set_opcode_g ( x, 7, ATTR_NONE, "sfence", &sfence, &maccess_none );
}

static void
init_opcode_info_fp_group_d8 ( void )
{
	struct opcode_info_t *x = opcode_info_fp_group_d8;
	set_opcode_g ( x, 0, ATTR_NONE, "fadd_single_real", &dummy_instr, &maccess_fop_rdonly_dword );
	set_opcode_g ( x, 1, ATTR_NONE, "fmul_single_real", &dummy_instr, &maccess_fop_rdonly_dword );
	set_opcode_g ( x, 2, ATTR_NONE, "fcom_single_real", &dummy_instr, &maccess_fop_rdonly_dword );
	set_opcode_g ( x, 3, ATTR_NONE, "fcomp_single_real", &dummy_instr, &maccess_fop_rdonly_dword );
	set_opcode_g ( x, 4, ATTR_NONE, "fsub_single_real", &dummy_instr, &maccess_fop_rdonly_dword );
	set_opcode_g ( x, 5, ATTR_NONE, "fsubr_single_real", &dummy_instr, &maccess_fop_rdonly_dword );
	set_opcode_g ( x, 6, ATTR_NONE, "fdiv_single_real", &dummy_instr, &maccess_fop_rdonly_dword );
	set_opcode_g ( x, 7, ATTR_NONE, "fdivr_single_real", &dummy_instr, &maccess_fop_rdonly_dword );
}

static void
init_opcode_info_fp_group_d9 ( void )
{
	struct opcode_info_t *x = opcode_info_fp_group_d9;

	set_opcode_g ( x, 0, ATTR_NONE, "fld_single_real", &dummy_instr, &maccess_fop_rdonly_dword );
//	set_opcode_g ( x, 1, ATTR_NONE, "bxerror", &, &maccess_ );
	set_opcode_g ( x, 2, ATTR_NONE, "fst_single_real", &dummy_instr, &maccess_fop_wronly_dword ); 
	set_opcode_g ( x, 3, ATTR_NONE, "fstp_single_real", &dummy_instr, &maccess_fop_wronly_dword );

	set_opcode_g ( x, 5, ATTR_NONE, "fldcw", &dummy_instr, &maccess_fop_rdonly_word );

	set_opcode_g ( x, 7, ATTR_NONE, "fnstcw", &dummy_instr, &maccess_fop_wronly_word );
}

static void
init_opcode_info_fp_group_db ( void )
{
	struct opcode_info_t *x = opcode_info_fp_group_db;

	set_opcode_g ( x, 0, ATTR_NONE, "fild_dword_integer", &dummy_instr, &maccess_fop_rdonly_dword );
	set_opcode_g ( x, 1, ATTR_NONE, "fisttp64", &dummy_instr, &maccess_fop_wronly_dword );
	set_opcode_g ( x, 2, ATTR_NONE, "fist_dword_integer", &dummy_instr, &maccess_fop_wronly_dword ); 
	set_opcode_g ( x, 3, ATTR_NONE, "fistp_dword_integer", &dummy_instr, &maccess_fop_wronly_dword );
	set_opcode_g ( x, 5, ATTR_NONE, "fld_extended_real", &dummy_instr, &maccess_fop_rdonly_ddeword );
	set_opcode_g ( x, 7, ATTR_NONE, "fstp_extended_real", &dummy_instr, &maccess_fop_wronly_ddeword );
}

static void
init_opcode_info_fp_group_dc ( void )
{
	struct opcode_info_t *x = opcode_info_fp_group_dc;

	set_opcode_g ( x, 0, ATTR_NONE, "fadd_double_real", &dummy_instr, &maccess_fop_rdonly_ddword );
	set_opcode_g ( x, 1, ATTR_NONE, "fmul_double_real", &dummy_instr, &maccess_fop_rdonly_ddword );
	set_opcode_g ( x, 4, ATTR_NONE, "fsub_double_real", &dummy_instr, &maccess_fop_rdonly_ddword );
	set_opcode_g ( x, 5, ATTR_NONE, "fsubr_double_real", &dummy_instr, &maccess_fop_rdonly_ddword );
	set_opcode_g ( x, 6, ATTR_NONE, "fdiv_double_real", &dummy_instr, &maccess_fop_rdonly_ddword );
	set_opcode_g ( x, 7, ATTR_NONE, "fdivr_double_real", &dummy_instr, &maccess_fop_rdonly_ddword );
}

static void
init_opcode_info_fp_group_dd ( void )
{
	struct opcode_info_t *x = opcode_info_fp_group_dd;

	set_opcode_g ( x, 0, ATTR_NONE, "fld_double_real", &dummy_instr, &maccess_fop_rdonly_ddword );
//	set_opcode_g ( x, 1, ATTR_NONE, "fisttp32", &dummy_instr, &maccess_ );
	set_opcode_g ( x, 2, ATTR_NONE, "fst_double_real", &dummy_instr, &maccess_fop_wronly_ddword );
	set_opcode_g ( x, 3, ATTR_NONE, "fstp_double_real", &dummy_instr, &maccess_fop_wronly_ddword ); 
//	set_opcode_g ( x, 4, ATTR_NONE, "frstor", &dummy_instr, &maccess_ );
//	set_opcode_g ( x, 5, ATTR_NONE, "bxerror", &, &maccess_ );
//	set_opcode_g ( x, 6, ATTR_NONE, "fnsave", &dummy_instr, &maccess_ );
//	set_opcode_g ( x, 7, ATTR_NONE, "fnstsw", &dummy_instr, &maccess_ );

}

static void
init_opcode_info_group ( void )
{
	init_opcode_info_group1eb_ib ( );
	init_opcode_info_group1ed ( );
	init_opcode_info_group2eb ( );
	init_opcode_info_group2ed ( );
	init_opcode_info_group3eb ( );
	init_opcode_info_group3ed ( );
	init_opcode_info_group4 ( );
	init_opcode_info_group5d ( );
	init_opcode_info_group6 ( );
	init_opcode_info_group7 ( );
	init_opcode_info_group8ev_ib ( );
	init_opcode_info_group15 ( );
	init_opcode_info_fp_group_d8 ( );
	init_opcode_info_fp_group_d9 ( );
	init_opcode_info_fp_group_db ( );
	init_opcode_info_fp_group_dc ( );
	init_opcode_info_fp_group_dd ( );
}

static void
set_opcode ( int opcode, op_attr_t attr, char *name, execute_t *execute, mem_access_func_t *maccess )
{
	struct opcode_info_t *x;

	ASSERT ( name != NULL );
	ASSERT ( execute != NULL );

	x = &opcode_info[opcode];
	x->is_supported = TRUE;
	x->name = name;
	x->attr = attr;
	x->execute = execute;
	x->maccess = maccess;
}

static void
set_opcode_prefix ( int opcode, op_attr_t attr )
{
	struct opcode_info_t *x;

	x = &opcode_info[opcode];
	x->is_supported = TRUE;
	x->attr = attr;
	x->name = "prefix";
}

static void
set_opcode_group ( int opcode, op_attr_t attr, struct opcode_info_t next[] )
{
	struct opcode_info_t *x;

	ASSERT ( next != NULL );

	x = &opcode_info[opcode];
	x->is_supported = TRUE;
	x->attr = attr;
	x->next = next;
}

void ud2a ( struct mon_t *mon, struct instruction_t *instr ) { }

/* Set opcode_info and sreg_decode_date ( for temporal usage ). */
static void
init_opcode_info ( void )
{
	int i;

	clear_all_opcode_info ( );

	init_opcode_info_group ( );

	set_opcode_prefix ( PREFIX_LOCK, ATTR_ANOTHER | ATTR_PREFIX );
	set_opcode_prefix ( PREFIX_OPSIZE_OVERRIDE, ATTR_ANOTHER | ATTR_PREFIX );
	set_opcode_prefix ( PREFIX_REP_REPE_REPZ, ATTR_ANOTHER | ATTR_PREFIX );
	set_opcode_prefix ( PREFIX_REPNE_REPNZ, ATTR_ANOTHER | ATTR_PREFIX );
	set_opcode_prefix ( ESCAPE_OPCODE, ATTR_ANOTHER );

	set_opcode_prefix ( PREFIX_GS_OVERRIDE, ATTR_ANOTHER | ATTR_PREFIX );


	set_opcode ( 0x00, ATTR_ANOTHER, "add_eb_gb", &add_eb_gb, &maccess_rdwr_byte );
	set_opcode ( 0x01, ATTR_ANOTHER, "add_ed_gd", &add_ed_gd, &maccess_rdwr_dword );
	set_opcode ( 0x02, ATTR_ANOTHER, "add_gb_eb", &add_gb_eb, &maccess_rdonly_byte );
	set_opcode ( 0x03, ATTR_ANOTHER, "add_gd_ed", &add_gd_ed, &maccess_rdonly_dword );
	set_opcode ( 0x06, ATTR_SENSITIVE, "push_es", &push_es, &maccess_push_dword );
	set_opcode ( 0x07, ATTR_SENSITIVE, "pop_es", &pop_es, &maccess_pop_dword );
	set_opcode ( 0x08, ATTR_ANOTHER, "or_eb_gb", &or_eb_gb, &maccess_rdwr_byte );
	set_opcode ( 0x09, ATTR_ANOTHER, "or_ed_gd", &or_ed_gd, &maccess_rdwr_dword );
	set_opcode ( 0x0a, ATTR_ANOTHER, "or_gb_eb", &dummy_instr, &maccess_rdonly_dword );
	set_opcode ( 0x0b, ATTR_ANOTHER, "or_gd_ed", &dummy_instr, &maccess_rdonly_dword );

	set_opcode ( 0x11, ATTR_ANOTHER, "adc_ed_gd", &dummy_instr, &maccess_rdwr_dword );

	set_opcode ( 0x1e, ATTR_SENSITIVE, "push_ds", &push_ds, &maccess_push_dword );
	set_opcode ( 0x1f, ATTR_SENSITIVE, "pop_ds", &pop_ds, &maccess_pop_dword );

	set_opcode ( 0x20, ATTR_ANOTHER, "and_eb_gb", &and_eb_gb, &maccess_rdwr_byte );
	set_opcode ( 0x21, ATTR_ANOTHER, "and_ed_gd", &and_ed_gd, &maccess_rdwr_dword );
	set_opcode ( 0x22, ATTR_ANOTHER, "and_gb_eb", &dummy_instr, &maccess_rdonly_byte );
	set_opcode ( 0x23, ATTR_ANOTHER, "and_gd_ed", &dummy_instr, &maccess_rdonly_dword );

	set_opcode ( 0x28, ATTR_ANOTHER, "sub_eb_gb", &sub_eb_gb, &maccess_rdwr_byte );
	set_opcode ( 0x29, ATTR_ANOTHER, "sub_ed_gd", &sub_ed_gd, &maccess_rdwr_dword );
	set_opcode ( 0x2a, ATTR_ANOTHER, "sub_gb_eb", &sub_gb_eb, &maccess_rdonly_byte );
	set_opcode ( 0x2b, ATTR_ANOTHER, "sub_gd_ed", &sub_gd_ed, &maccess_rdonly_dword );
	set_opcode ( 0x31, ATTR_ANOTHER, "xor_ed_gd", &xor_ed_gd, &maccess_rdwr_dword );
	set_opcode ( 0x32, ATTR_ANOTHER, "xor_gb_eb", &dummy_instr, &maccess_rdonly_byte );
	set_opcode ( 0x33, ATTR_ANOTHER, "xor_gw_ew", &dummy_instr, &maccess_rdonly_word );

	set_opcode ( 0x34, ATTR_IMMEDIATE_IB, "xor_al_ib", &xor_al_ib, &maccess_none );
	set_opcode ( 0x35, ATTR_IMMEDIATE_IV, "xor_eax_id", &xor_eax_id, &maccess_none );

	set_opcode ( 0x38, ATTR_ANOTHER, "cmp_eb_gb", &cmp_eb_gb, &maccess_rdonly_byte );
	set_opcode ( 0x39, ATTR_ANOTHER, "cmp_ed_gd", &cmp_gd_ed, &maccess_rdonly_dword );
	set_opcode ( 0x3a, ATTR_ANOTHER, "cmp_gb_eb", &cmp_gb_eb, &maccess_rdonly_byte );
	set_opcode ( 0x3b, ATTR_ANOTHER, "cmp_gd_ed", &cmp_gd_ed, &maccess_rdonly_dword );

	set_opcode ( 0x3c, ATTR_IMMEDIATE_IB, "cmp_al_ib", &dummy_instr, &maccess_none );
	set_opcode ( 0x3d, ATTR_IMMEDIATE_IV, "cmp_eax_id", &dummy_instr, &maccess_none );

	for ( i = 0x40; i <= 0x47; i++ )
		set_opcode ( i, ATTR_NONE, "inc_erx", &dummy_instr, &maccess_none );
	for ( i = 0x48; i <= 0x4f; i++ )
		set_opcode ( i, ATTR_NONE, "dec_erx", &dummy_instr, &maccess_none );
	for ( i = 0x50; i <= 0x57; i++ )
		set_opcode ( i, ATTR_NONE, "push_erx", &push_erx, &maccess_push_dword );
	for ( i = 0x58; i <= 0x5f; i++ )
		set_opcode ( i, ATTR_NONE, "pop_erx", &pop_erx, &maccess_pop_dword );

	set_opcode ( 0x68, ATTR_IMMEDIATE_IV, "push_id", &push_id, &maccess_push_dword );
	set_opcode ( 0x69, ATTR_IMMEDIATE_IV, "imul_gd_ed_id", &dummy_instr, &maccess_rdonly_dword );
	set_opcode ( 0x6a, ATTR_IMMEDIATE_IB_SE, "push_id", &push_id, &maccess_push_dword );
	set_opcode ( 0x6b, ATTR_IMMEDIATE_IB_SE, "imul_gd_ed_id", &dummy_instr, &maccess_rdonly_dword );

	set_opcode ( 0x6c, ATTR_SENSITIVE, "insb_yb_dx", &insb_yb_dx, &insb_yb_dx_mem );
	set_opcode ( 0x6d, ATTR_SENSITIVE, "insw_yv_dx", &insw_yv_dx, &insw_yv_dx_mem );
	set_opcode ( 0x6e, ATTR_SENSITIVE, "outsb_dx_xb", &outsb_dx_xb, &outsb_dx_xb_mem );
	set_opcode ( 0x6f, ATTR_SENSITIVE, "outsw_dx_xv", &outsw_dx_xv, &outsw_dx_xv_mem );

	for ( i = 0x70; i <= 0x73; i++ )  
		set_opcode ( i, ATTR_IMMEDIATE_BR_OFF8, "jcc_jd", &dummy_instr, &maccess_none );
	set_opcode ( 0x74, ATTR_IMMEDIATE_BR_OFF8, "jz_jd", &dummy_instr, &maccess_none );
	set_opcode ( 0x75, ATTR_IMMEDIATE_BR_OFF8, "jnz_jd", &dummy_instr, &maccess_none );
	for ( i = 0x76; i <= 0x7f; i++ )  
		set_opcode ( i, ATTR_IMMEDIATE_BR_OFF8, "jcc_jd", &dummy_instr, &maccess_none );

	set_opcode_group ( 0x80, ATTR_ANOTHER | ATTR_GROUP_1, opcode_info_group1eb_ib );
	set_opcode_group ( 0x81, ATTR_ANOTHER | ATTR_IMMEDIATE_IV | ATTR_GROUP_1, opcode_info_group1ed );
	set_opcode_group ( 0x82, ATTR_ANOTHER | ATTR_GROUP_1, opcode_info_group1eb_ib );
	set_opcode_group ( 0x83, ATTR_ANOTHER | ATTR_IMMEDIATE_IB_SE | ATTR_GROUP_1, opcode_info_group1ed );

	set_opcode ( 0x84, ATTR_ANOTHER, "test_eb_gb", &test_eb_gb, &maccess_rdonly_byte );
	set_opcode ( 0x85, ATTR_ANOTHER, "test_ed_gd", &test_ed_gd, &maccess_rdonly_dword );
	set_opcode ( 0x86, ATTR_ANOTHER, "xchg_eb_gb", &xchg_eb_gb, &maccess_rdwr_byte );
	set_opcode ( 0x87, ATTR_ANOTHER, "xchg_ed_gd", &xchg_ed_gd, &maccess_rdwr_dword );
	set_opcode ( 0x88, ATTR_ANOTHER, "mov_eb_gb", &mov_eb_gb, &maccess_wronly_byte );
	set_opcode ( 0x89, ATTR_ANOTHER, "mov_ed_gd", &mov_ed_gd, &maccess_wronly_dword );
	set_opcode ( 0x8a, ATTR_ANOTHER, "mov_gb_eb", &mov_gb_eb, &maccess_rdonly_byte );
	set_opcode ( 0x8b, ATTR_ANOTHER, "mov_gd_ed", &mov_gd_ed, &maccess_rdonly_dword );
	set_opcode ( 0x8c, ATTR_SENSITIVE | ATTR_ANOTHER, "mov_ew_sw", &mov_ew_sw, &maccess_wronly_dword );
	set_opcode ( 0x8d, ATTR_ANOTHER, "lea_gdm", &lea_gdm, &lea_gdm_mem );
	set_opcode ( 0x8e, ATTR_SENSITIVE | ATTR_ANOTHER, "mov_sw_ew", &mov_sw_ew, &maccess_rdonly_word );
	set_opcode ( 0x8f, ATTR_ANOTHER, "pop_ed", &dummy_instr, &pop_ed_mem );
	set_opcode ( 0x90, ATTR_NONE, "nop", &dummy_instr, &maccess_none );
	set_opcode ( 0x9c, ATTR_SENSITIVE, "pushf_fv", &pushf_fv, &maccess_push_dword );
	set_opcode ( 0x9d, ATTR_SENSITIVE, "popf_fv", &popf_fv, &maccess_pop_dword );
	set_opcode ( 0xa0, ATTR_IMMEDIATE_O, "mov_al_ob", &mov_al_ob, &mov_al_ob_mem );
	set_opcode ( 0xa1, ATTR_IMMEDIATE_O, "mov_eax_od", &mov_eax_od, &mov_eax_od_mem );
	set_opcode ( 0xa2, ATTR_IMMEDIATE_O, "mov_ob_al", &mov_ob_al, &mov_ob_al_mem );
	set_opcode ( 0xa3, ATTR_IMMEDIATE_O, "mov_od_eax", &mov_od_eax, &mov_od_eax_mem );
	set_opcode ( 0xa4, ATTR_REPEATABLE, "movsb_xb_yb", &movsb_xb_yb, &movsb_xb_yb_mem );
	set_opcode ( 0xa5, ATTR_REPEATABLE, "movsw_xv_yv", &movsw_xv_yv, &movsw_xv_yv_mem );
	set_opcode ( 0xa6, ATTR_REPEATABLE, "cmpsb_xb_yb", &cmpsb_xb_yb, &cmpsb_xb_yb_mem );
	set_opcode ( 0xa7, ATTR_REPEATABLE, "cmpsw_xv_yv", &cmpsw_xv_yv, &cmpsw_xv_yv_mem );

	set_opcode ( 0xaa, ATTR_REPEATABLE, "stosw_yb_al", &stosb_yb_al, &stosb_yb_al_mem );
	set_opcode ( 0xab, ATTR_REPEATABLE, "stosw_yv_eax", &stosw_yv_eax, &stosw_yv_eax_mem );
	set_opcode ( 0xac, ATTR_REPEATABLE, "lodsb_al_xb", &lodsb_al_xb, &lodsb_al_xb_mem );
	set_opcode ( 0xad, ATTR_REPEATABLE, "lodsw_eax_xv", &lodsw_eax_xv, &lodsw_eax_xv_mem );

	set_opcode ( 0xae, ATTR_REPEATABLE, "scasb_al_xb", &scasb_al_xb, &scasb_al_xb_mem );
	set_opcode ( 0xaf, ATTR_REPEATABLE, "scasw_eax_xv", &scasw_eax_xv, &scasw_eax_xv_mem );


	for ( i = 0xb0; i <= 0xb7; i++ )
		set_opcode ( i, ATTR_IMMEDIATE_IB, "mov_rl_ib", &dummy_instr, &maccess_none );

	for ( i = 0xb8; i <= 0xbf; i++ )
		set_opcode ( i, ATTR_IMMEDIATE_IV, "mov_erx_id", &mov_erx_id, &maccess_none );

	set_opcode_group ( 0xc0, ATTR_ANOTHER | ATTR_GROUP_2 | ATTR_IMMEDIATE_IB, opcode_info_group2eb );
	set_opcode_group ( 0xc1, ATTR_ANOTHER | ATTR_GROUP_2 | ATTR_IMMEDIATE_IB, opcode_info_group2ed );

	set_opcode ( 0xc2, ATTR_IMMEDIATE_IW, "ret_near32_iw", &dummy_instr, &maccess_pop_dword );
	set_opcode ( 0xc3, ATTR_NONE, "ret_near32", &dummy_instr, &maccess_pop_dword );

	set_opcode ( 0xc6, ATTR_ANOTHER | ATTR_IMMEDIATE_IB, "mov_eb_ib", &mov_eb_ib, &maccess_wronly_byte );
	set_opcode ( 0xc7, ATTR_ANOTHER | ATTR_IMMEDIATE_IV, "mov_ed_id", &mov_ed_id, &maccess_wronly_dword );
	set_opcode ( 0xc9, ATTR_NONE, "leave", &dummy_instr, &leave_mem );
	set_opcode ( 0xcb, ATTR_SENSITIVE, "ret_far32", &ret_far32, &ret_far32_mem );
	set_opcode ( 0xcf, ATTR_SENSITIVE, "iret32", &iret32, &iret32_mem );

	set_opcode_group ( 0xd0, ATTR_ANOTHER | ATTR_GROUP_2, opcode_info_group2eb );
	set_opcode_group ( 0xd1, ATTR_ANOTHER | ATTR_GROUP_2, opcode_info_group2ed );
	set_opcode_group ( 0xd2, ATTR_ANOTHER | ATTR_GROUP_2, opcode_info_group2eb );
	set_opcode_group ( 0xd3, ATTR_ANOTHER | ATTR_GROUP_2, opcode_info_group2ed );

	set_opcode_group ( 0xd8, ATTR_ANOTHER | ATTR_FP_GROUP, opcode_info_fp_group_d8 );
	set_opcode_group ( 0xd9, ATTR_ANOTHER | ATTR_FP_GROUP, opcode_info_fp_group_d9 );
	set_opcode_group ( 0xdb, ATTR_ANOTHER | ATTR_FP_GROUP, opcode_info_fp_group_db );
	set_opcode_group ( 0xdc, ATTR_ANOTHER | ATTR_FP_GROUP, opcode_info_fp_group_dc );
	set_opcode_group ( 0xdd, ATTR_ANOTHER | ATTR_FP_GROUP, opcode_info_fp_group_dd );

	set_opcode ( 0xe4, ATTR_SENSITIVE | ATTR_IMMEDIATE_IB, "in_al_ib", &in_al_ib, &maccess_none );
	set_opcode ( 0xe6, ATTR_SENSITIVE | ATTR_IMMEDIATE_IB, "out_ib_al", &out_ib_al, &maccess_none );

	set_opcode ( 0xe8, ATTR_IMMEDIATE_BROFF32, "call_ad", &call_ad, &call_ad_mem );
	set_opcode ( 0xe9, ATTR_IMMEDIATE_BROFF32, "jmp_jd", &dummy_instr, &maccess_none );
	set_opcode ( 0xea, ATTR_SENSITIVE | ATTR_IMMEDIATE_IV_IW, "jmp_ap", &jmp_ap, &jmp_ap_mem );
	set_opcode ( 0xeb, ATTR_IMMEDIATE_BR_OFF8, "jmp_jd", &dummy_instr, &maccess_none );
	set_opcode ( 0xec, ATTR_SENSITIVE, "in_al_dx", &in_al_dx, &maccess_none );
	set_opcode ( 0xed, ATTR_SENSITIVE, "in_eax_dx", &in_eax_dx, &maccess_none );
	set_opcode ( 0xee, ATTR_SENSITIVE, "out_dx_al", &out_dx_al, &maccess_none );
	set_opcode ( 0xef, ATTR_SENSITIVE, "out_dx_eax", &out_dx_eax, &maccess_none );
	set_opcode ( 0xf4, ATTR_SENSITIVE, "hlt", &hlt, &maccess_none );

	set_opcode_group ( 0xf6, ATTR_ANOTHER | ATTR_GROUP_3, opcode_info_group3eb );
	set_opcode_group ( 0xf7, ATTR_ANOTHER | ATTR_GROUP_3, opcode_info_group3ed );

	set_opcode ( 0xfa, ATTR_SENSITIVE, "cli", &cli, &maccess_none );
	set_opcode ( 0xfb, ATTR_SENSITIVE, "sti", &sti, &maccess_none );
	set_opcode ( 0xfc, ATTR_SENSITIVE, "cld", &cld, &maccess_none );
	set_opcode ( 0xfd, ATTR_SENSITIVE, "std", &std, &maccess_none );

	set_opcode_group ( 0xfe, ATTR_ANOTHER | ATTR_GROUP_4, opcode_info_group4 );
	set_opcode_group ( 0xff, ATTR_ANOTHER | ATTR_GROUP_5, opcode_info_group5d );

	set_opcode_group ( 0x0f00, ATTR_ANOTHER | ATTR_GROUP_6, opcode_info_group6 );
	set_opcode_group ( 0x0f01, ATTR_ANOTHER | ATTR_GROUP_7, opcode_info_group7 );

	set_opcode ( 0x0f06, ATTR_SENSITIVE, "clts", &clts, &maccess_none );
	set_opcode ( 0x0f09, ATTR_SENSITIVE, "wbinvd", &wbinvd, &maccess_none );
	set_opcode ( 0x0f0b, ATTR_NONE, "ud2a", &ud2a, &maccess_none );
	set_opcode ( 0x0f20, ATTR_SENSITIVE | ATTR_ANOTHER, "mov_rd_cd", &mov_rd_cd, &maccess_none );
	set_opcode ( 0x0f22, ATTR_SENSITIVE | ATTR_ANOTHER, "mov_cd_rd", &mov_cd_rd, &maccess_none );
	set_opcode ( 0x0f23, ATTR_SENSITIVE | ATTR_ANOTHER, "mov_dd_rd", &mov_dd_rd, &maccess_none );
	set_opcode ( 0x0f30, ATTR_SENSITIVE, "wrmsr", &wrmsr, &maccess_none );
	set_opcode ( 0x0f31, ATTR_NONE, "rdtsc", &dummy_instr, &maccess_none );
	set_opcode ( 0x0f32, ATTR_SENSITIVE, "rdmsr", &rdmsr, &maccess_none );

	for ( i = 0x0f40; i <= 0x0f4f; i++ )
		set_opcode ( i, ATTR_ANOTHER, "cmov_gd_ed", &cmov_gd_ed, &maccess_rdonly_dword );

	for ( i = 0x0f80; i <= 0x0f83; i++ )  
		set_opcode ( i, ATTR_IMMEDIATE_BR_OFF32, "jcc_jd", &dummy_instr, &maccess_none );
	set_opcode ( 0x0f84, ATTR_IMMEDIATE_BR_OFF32, "jz_jd", &dummy_instr, &maccess_none );
	set_opcode ( 0x0f85, ATTR_IMMEDIATE_BR_OFF32, "jnz_jd", &dummy_instr, &maccess_none );
	for ( i = 0x0f86; i <= 0x0f8f; i++ )  
		set_opcode ( i, ATTR_IMMEDIATE_BR_OFF32, "jcc_jd", &dummy_instr, &maccess_none );

	set_opcode ( 0x0f95, ATTR_ANOTHER, "setnz_eb", &setnz_eb, &maccess_wronly_byte );

	set_opcode ( 0x0fa2, ATTR_SENSITIVE, "cpuid", &cpuid, &maccess_none );
	set_opcode ( 0x0fa3, ATTR_ANOTHER, "bt_ev_gv", &bt_ev_gv, &bit_mem );
	set_opcode ( 0x0fab, ATTR_ANOTHER, "bts_ev_gv", &bts_ev_gv, &bit_mem );

	set_opcode_group ( 0x0fae, ATTR_ANOTHER | ATTR_GROUP_15, opcode_info_group15 );
	set_opcode ( 0x0faf, ATTR_ANOTHER, "imul_gd_ed", &dummy_instr, &maccess_rdonly_dword );

	set_opcode ( 0x0fb0, ATTR_ANOTHER, "cmpxchg_eb_gb", &cmpxchg_eb_gb, &cmpxchg_eb_gb_mem );
	set_opcode ( 0x0fb1, ATTR_ANOTHER, "cmpxchg_ed_gd", &cmpxchg_ed_gd, &cmpxchg_ed_gd_mem );
	set_opcode ( 0x0fb2, ATTR_SENSITIVE | ATTR_ANOTHER, "lss_gv_mp", &lss_gv_mp, &lss_gv_mp_mem );
	set_opcode ( 0x0fb3, ATTR_ANOTHER, "btr_ev_gv", &btr_ev_gv, &bit_mem );
	set_opcode ( 0x0fb6, ATTR_ANOTHER, "movzx_gd_eb", &movzx_gd_eb, &maccess_rdonly_byte );
	set_opcode ( 0x0fb7, ATTR_ANOTHER, "movzx_gd_ew", &movzx_gd_ew, &maccess_rdonly_word ); 

	set_opcode_group ( 0x0fba, ATTR_ANOTHER | ATTR_GROUP_8, opcode_info_group8ev_ib );

	set_opcode ( 0x0fbb, ATTR_ANOTHER, "btc_ev_gv", &dummy_instr, &bit_mem );
	set_opcode ( 0x0fbe, ATTR_ANOTHER, "movsx_gd_eb", &movsx_gd_eb, &maccess_rdonly_byte );
	set_opcode ( 0x0fbf, ATTR_ANOTHER, "movsx_gd_ew", &movsx_gd_ew, &maccess_rdonly_word );

	set_opcode ( 0x0fc0, ATTR_ANOTHER, "xadd_eb_gb", &xadd_eb_gb, &maccess_rdwr_byte );
	set_opcode ( 0x0fc1, ATTR_ANOTHER, "xadd_ed_gd", &xadd_ed_gd, &maccess_rdwr_dword );
}

/********************************************************************/

struct decode_state_t {
	struct instruction_t 	instr;
	bit32u_t		eip;
	bit8u_t			val;
};

static void
init_decode_state ( struct decode_state_t *state, bit32u_t eip )
{
	ASSERT ( state != NULL );

	Instruction_init ( &state->instr );
	state->eip = eip;
	state->val = 0;
}

static bit8u_t
read_instr_byte ( struct decode_state_t *state )
{
	ASSERT ( state != NULL );

	state->val = Monitor_read_byte_with_vaddr ( SEG_REG_CS, state->eip + state->instr.len );
	state->instr.len++;
	
	return state->val;
}

static bit16u_t
read_instr_word ( struct decode_state_t *state )
{
	bit32u_t retval;
	
	ASSERT ( state != NULL );

	retval = Monitor_read_word_with_vaddr ( SEG_REG_CS, state->eip + state->instr.len );
	/* [NOTE] state->val becomes an invalid value. */
	state->instr.len += 2;
	
	return retval;
}

static bit32u_t
read_instr_dword ( struct decode_state_t *state )
{
	bit32u_t retval;
	
	ASSERT ( state != NULL );

	retval = Monitor_read_dword_with_vaddr ( SEG_REG_CS, state->eip + state->instr.len );
	/* [NOTE] state->val becomes an invalid value. */
	state->instr.len += 4;
	
	return retval;
}

/********************************************************************/

static void
decode_prefix ( struct decode_state_t *state )
{
	struct instruction_t *instr = &state->instr;

	switch ( instr->opcode ) {
		/* Group 1 */
	case PREFIX_LOCK: 			instr->is_locked = TRUE; break;
	case PREFIX_REPNE_REPNZ: 		instr->repne_repnz = TRUE; break;
	case PREFIX_REP_REPE_REPZ: 		instr->rep_repe_repz = TRUE; break;

		/* Group 2 */
	case PREFIX_CS_OVERRIDE: 		instr->sreg_index = SEG_REG_CS; break;
	case PREFIX_SS_OVERRIDE: 		instr->sreg_index = SEG_REG_SS; break;
	case PREFIX_DS_OVERRIDE: 		instr->sreg_index = SEG_REG_DS; break;
	case PREFIX_ES_OVERRIDE: 		instr->sreg_index = SEG_REG_ES; break;
	case PREFIX_FS_OVERRIDE: 		instr->sreg_index = SEG_REG_FS; break;
	case PREFIX_GS_OVERRIDE: 		instr->sreg_index = SEG_REG_GS; break;
   
		/* Group 3 */
	case PREFIX_OPSIZE_OVERRIDE: 		instr->opsize_override = TRUE; break;

		/* Group 4 */
	case PREFIX_ADDRSIZE_OVERRIDE: 		assert ( 0 ); break;

	default: 				Match_failure ( "decode_prefix: instr->opcode=%#x\n", instr->opcode );
	}
}

static bool_t
decode_opcode ( struct decode_state_t *state )
{
	struct instruction_t *instr = &state->instr;
	bool_t f;
   
	instr->opcode = read_instr_byte ( state );
	instr->name = opcode_info[instr->opcode].name;

	if ( ! opcode_info[instr->opcode].is_supported ) {
		Warning ( "decode_opcode: not implemented: opcode=%#x\n", instr->opcode );
		return FALSE;
	}

	/* [TODO] the foolowing code is not sophisticated. */
	f = TRUE;
	while ( f ) {
		struct opcode_info_t *p = &opcode_info[instr->opcode];

		if ( ( p->attr & ATTR_ANOTHER ) == 0 )
			break;

		if ( p->attr & ATTR_PREFIX ) { 
			decode_prefix ( state );
			instr->opcode = read_instr_byte ( state );
			instr->name = opcode_info[instr->opcode].name;
			continue;
		}

		if ( instr->opcode == ESCAPE_OPCODE ) {
			bit8u_t x;

			x = read_instr_byte ( state );
			instr->opcode = 0x0f00 | x;
			instr->name = opcode_info[instr->opcode].name;
	    
			if ( ! opcode_info[instr->opcode].is_supported ) {
				Warning ( "decode_opcode: not implemented: opcode=%#x\n", instr->opcode );
				return FALSE;
			}
			continue; 
		}

		 ( void )read_instr_byte ( state );
		f = FALSE;
	} 

	DPRINT ( "opcode = %#x, name=\"%s\"\n", instr->opcode, instr->name );
	return TRUE;
}

static bool_t
is_dword_disp ( int mod, int rm_or_base )
{
	return ( ( ( mod == 0 ) && ( rm_or_base == 5 ) ) ||
		 ( mod == 2 ) );
}

static bool_t
is_byte_disp ( int mod )
{
	return ( mod == 1 );
}

static void
decode_reg_addr_mode_specifier_sub ( struct decode_state_t *state, 
				  bool_t sib_flag,
				  int mod, int reg, int rm_or_base )
{
	struct instruction_t *instr = &state->instr;

	instr->resolve = resolve_decode_info[sib_flag][mod][rm_or_base];

	if ( instr->sreg_index == SEG_REG_NULL )
		instr->sreg_index = sreg_decode_info[sib_flag][mod][rm_or_base];
   
	if ( is_dword_disp ( mod, rm_or_base ) ) {
		instr->disp = read_instr_dword ( state ); /* read 32-bit displacement */
	} else if ( is_byte_disp ( mod ) ) {
		// must be signed char ??
		instr->disp = (char)read_instr_byte ( state ); /* read 8-bit displacement */
	}
}

static void
decode_reg_addr_mode_specifier ( struct decode_state_t *state )
{
	struct instruction_t *instr = &state->instr;

	/* [Note]
	 * The ModR/M byte has already been read in decode_opcde function.
	 */

	instr->mod = SUB_BIT ( state->val, 6, 2 );
	instr->reg = SUB_BIT ( state->val, 3, 3 ); /* bochs では nnnという変数名 */
	instr->rm = SUB_BIT ( state->val, 0, 3 );
	DPRINT ( "mod=%d, reg=%d, rm=%d\n", instr->mod, instr->reg, instr->rm );

	if ( instr->mod == 3 ) { /* mod == 11B */
		/* In this case, effective address provides ways of
		 * specifying general purpose, MMX technology, XMM
		 * registers 
		 */
		return;
	}

	/* In this case, effective address provides ways of specifying a
	 * memory location.
	 */

	/* The rest of this function set the displacement and segment registers. */

	if ( instr->rm != 4 ) {/* rm == 100B */
		/* No SIB byte */
		decode_reg_addr_mode_specifier_sub ( state, FALSE, instr->mod, instr->reg, instr->rm );
		return;
	}

	{
		/* rm == 100B ( mod == 00B, 01B or 11B ) */
		int sib, base;
	 
		/* decode sib */
		sib = read_instr_byte ( state );

		instr->sib_scale = SUB_BIT ( state->val, 6, 2 );
		instr->sib_index = SUB_BIT ( state->val, 3, 3 );
		base = SUB_BIT ( state->val, 0, 3 );

		DPRINT ( "scale=%d, index=%d, base=%d\n", instr->sib_scale, instr->sib_index, base );

		decode_reg_addr_mode_specifier_sub ( state, TRUE, instr->mod, instr->reg, base );
	}
}

static void
set_execute ( struct decode_state_t *state )
{
	struct instruction_t *instr = &state->instr;
	struct opcode_info_t *p;

	p = &opcode_info[instr->opcode];

	while ( p->attr & ATTR_GROUP_X ) {
		switch ( p->attr & ATTR_GROUP_X ) {
		case ATTR_GROUP_N:
			p = & ( p->next[instr->reg] ); 
			if ( ( p == NULL ) || ( ! p->is_supported ) ) {
				Fatal_failure ( "not implemented group: opcode=%#x, instr->reg=%#x\n",
						instr->opcode, instr->reg );
			}
			break;

		case ATTR_FP_GROUP:
			if ( instr->mod == 3 ) {
				DPRINT ( "mod=%d\n", 3 );
				/*
				 int index = ( b1-0xD8 )*64 + ( 0x3f & b2 );
				 OpcodeInfoPtr = & ( BxOpcodeInfo_FloatingPoint[index] );
				*/
				assert ( 0 );
				exit ( 1 ); // DEBUG
			} else {
				p = & ( p->next[instr->reg] ); 
			}
			if ( ! p->is_supported ) {
				Print ( stderr, "unsupported instruction: opcode = %#x, mod = %d, reg=%d\n", instr->opcode, instr->mod, instr->reg );
			}

			ASSERT ( p != NULL );
			ASSERT ( p->is_supported );		 
			break;

		default:
			/* [TODO] other attributes */
			Match_failure ( "set_execute: opcode=%#x, attr=%#x\n", 
					instr->opcode, p->attr );
		}
	}
	instr->name = p->name;
	instr->execute = p->execute;
	instr->maccess = p->maccess;
	instr->is_sensitive = ( p->attr & ATTR_SENSITIVE ) ? TRUE : FALSE;
}

static void
decode_immediate ( struct decode_state_t *state )
{
	struct instruction_t *instr = &state->instr;
	int imm_mode;

	/* [TODO] */
   
	imm_mode = opcode_info[instr->opcode].attr & ATTR_IMMEDIATE;

	if ( ! imm_mode )
		return;

	switch ( imm_mode ) {
	case ATTR_IMMEDIATE_IB:
	case ATTR_IMMEDIATE_IB_SE:
		instr->immediate[0] = read_instr_byte ( state );
		break;
		
	case ATTR_IMMEDIATE_IV:
	case ATTR_IMMEDIATE_IV_IW:
		instr->immediate[0] = ( instr->opsize_override ) ? read_instr_word ( state ) : read_instr_dword ( state );
		if ( imm_mode == ATTR_IMMEDIATE_IV_IW ) {
			instr->immediate[1] = read_instr_word ( state );
		}
		break;
		
	case ATTR_IMMEDIATE_O:
		instr->immediate[0] = read_instr_dword ( state );
		break;
		
	case ATTR_IMMEDIATE_IW:
	case ATTR_IMMEDIATE_IW_IB:
		instr->immediate[0] = read_instr_word ( state );
		if ( imm_mode == ATTR_IMMEDIATE_IW_IB ) {
			instr->immediate[1] = read_instr_byte ( state );
		}
		break;
		
	case ATTR_IMMEDIATE_BR_OFF8:
		instr->immediate[0] = read_instr_byte ( state );
		break;
		
	case ATTR_IMMEDIATE_BR_OFF16:
		instr->immediate[0] = read_instr_word ( state );
		break;
		
	default:
		Match_failure ( "decode_immediate\n" );
	}
}

static void
verify_decode ( struct decode_state_t *state )
{
	ASSERT ( state != NULL );

	/* [DEBUG] only support the operand size override of 'out' and 'mov_ed_gd' instructions. */
	if ( state->instr.opsize_override ) {
		if ( ! ( ( state->instr.opcode == 0xed ) ||
			 ( state->instr.opcode == 0xef ) ||
			 ( state->instr.opcode == 0xc7 ) ||
			 ( state->instr.opcode == 0x6d ) ||
			 ( state->instr.opcode == 0x6f ) ||
			 ( state->instr.opcode == 0x3b ) ||
			 ( state->instr.opcode == 0xa3 ) ||
			 ( state->instr.opcode == 0xa5 ) ||
			 ( state->instr.opcode == 0x39 ) ||
			 ( ( state->instr.opcode == 0x83 ) && ( state->instr.reg == 7 ) ) || 
			 ( state->instr.opcode == 0x89 ) ||
			 ( state->instr.opcode == 0x0fb6 ) ) ) {
			Print ( stderr, "unexpected operand size override: opcode = %#x, eip=#x\n", state->instr.opcode, state->eip );
		}
	}

	if ( state->instr.repne_repnz ) {
		ASSERT ( ( state->instr.opcode == 0xa6 ) ||
			 ( state->instr.opcode == 0xa7 ) ||
			 ( state->instr.opcode == 0xae ) );
	}

	if ( state->instr.rep_repe_repz ) {
		if ( ! ( ( state->instr.opcode == 0xa4 ) || 
			 ( state->instr.opcode == 0xa5 ) || 
			 ( state->instr.opcode == 0xab ) ||
			 ( state->instr.opcode == 0x6c ) ||
			 ( state->instr.opcode == 0x6d ) ||
			 ( state->instr.opcode == 0x6e ) ||
			 ( state->instr.opcode == 0x6f ) ||
			 ( state->instr.opcode == 0xaf ) ||
			 
			 ( state->instr.opcode == 0xa6 ) ||
			 ( state->instr.opcode == 0xa7 ) ||
			 ( state->instr.opcode == 0xae )  ) ) {
			Print ( stderr, "unexpected rep: opcode = %#x\n", state->instr.opcode );
		}
	}
}
   
struct instruction_t
decode_instruction ( bit32u_t eip )
{
	static bool_t is_inited = FALSE;
	struct decode_state_t state;
	bool_t b;

	if ( ! is_inited ) {
		init_opcode_info ( );
		is_inited = TRUE;
	}

	init_decode_state ( &state, eip );

	b = decode_opcode ( &state );
	if ( ! b ) {
		state.instr.opcode = -1; 
		return state.instr;
	}
   
	if ( opcode_info[state.instr.opcode].attr & ATTR_ANOTHER ) {
		decode_reg_addr_mode_specifier ( &state );
		set_execute ( &state );
	} else {
		struct opcode_info_t *x = &opcode_info[state.instr.opcode];
		state.instr.name = x->name;
		state.instr.execute = x->execute;
		state.instr.maccess = x->maccess;
		state.instr.is_sensitive = ( x->attr & ATTR_SENSITIVE ) ? TRUE : FALSE;
		state.instr.opcode_reg = SUB_BIT ( state.val, 0, 3 ); /* ??? */
	}

	decode_immediate ( &state );

	verify_decode ( &state ); /* [DEBUG] */

	return state.instr;
}

unsigned long int vm_time[2], mon_time [2];

static void
start_profile2 ( struct mon_t *mon )
{
	struct stat_t *stat = &mon->stat;

	bool_t b = stat->comm_counter_flag;
	bool_t bb = stat->halt_counter_flag;

	if ( mon->cpuid == 0 ) { 
		Print_color ( stdout, CYAN, "Profiling starts.\n" ); 
	}
	
	Get_utime_and_stime ( Getpid (), &mon_time[0], &mon_time[1] );
	Get_utime_and_stime ( mon->pid,  &vm_time[0], &vm_time[1] );

	Stat_init ( stat );
	start_time_counter ( &stat->vmm_counter );
	start_time_counter ( &stat->shandler_counter );
	if ( b ) {
		start_time_counter ( &stat->comm_counter );
	}
	if ( bb ) {
		start_time_counter ( &stat->halt_counter );
	}
}

static void
stop_profile2 ( struct mon_t *mon )
{
	struct stat_t *stat = &mon->stat;
	FILE *fp;
	double vmm, comm, hlt, guest;
	int i;
	
	if ( mon->cpuid == 0 ) { 
		Print_color ( stdout, CYAN, "Profiling ends.\n" ); 
	}

	stop_time_counter ( &mon->stat.vmm_counter );

	if ( mon->stat.halt_counter_flag ) {
		stop_time_counter ( &mon->stat.halt_counter );
		start_time_counter ( &mon->stat.halt_counter );
	}
	if ( mon->stat.comm_counter_flag ) {
		stop_time_counter ( &mon->stat.comm_counter );
		start_time_counter ( &mon->stat.comm_counter );
	}

	guest = time_counter_to_sec ( &stat->guest_counter );
	vmm = time_counter_to_sec ( &stat->vmm_counter );
	comm = time_counter_to_sec ( &stat->comm_counter );
	hlt = time_counter_to_sec ( &stat->halt_counter );
	
	fp = Fopen_fmt ( "w+", "%s/profile%d", 
			 Getenv ( "HOME" ), mon->cpuid );

	/*
	Print ( fp, 
		"%f\t" "%f\t" "%f\t" "%f\n" ,
		time_counter_to_sec ( &stat->guest_counter ),
		comm,
		vmm - comm - hlt, 
		hlt );
	*/

	{
		unsigned long int v[2], m[2];
		
		Get_utime_and_stime ( Getpid (), &m[0], &m[1] );
		Get_utime_and_stime ( mon->pid,  &v[0], &v[1] );
		
		Print ( fp, "mon = %d\t" "vm = %d (from /proc)\n" ,
			( ( m[0] - mon_time[0] ) + ( m[1] - mon_time[1] ) ),
			( ( v[0] - vm_time[0] )  + ( v[1] - vm_time[1] ) ) );

		Print ( fp, "mon = %f\t" "vm = %f\n", vmm - comm - hlt, guest );

		Print ( fp, 
			"\t" "kernel = %f\t" "user = %f\n",
			time_counter_to_sec ( &stat->guest_kernel_counter ),
			time_counter_to_sec ( &stat->guest_user_counter ) );

		Print ( fp, "# of irets = %d\n", stat->nr_irets );
		Print ( fp, "# of pit interrupts = %d\n", stat->nr_pit_interrupts );
		Print ( fp, "# of traps = %d\n", stat->nr_traps );
		Print ( fp, "# of other signals = %d\n", stat->nr_other_sigs );
		Print ( fp, "# of instr emu = %d\n", stat->nr_instr_emu );
	}
	fclose (fp);
	
	if ( stat->nr_fetch_requests >= FETCH_HISOTRY_SIZE ) {
		Fatal_failure ( "stop_profile2\n" );
	}

	fp = Fopen_fmt ( "w+", "%s/memfoot%d", Getenv ( "HOME" ), mon->cpuid );
	for ( i = 0; i < (int)stat->nr_fetch_requests; i++ ) {
		struct fetch_history_t *h = &stat->fetch_history[i];

		Print ( fp,
			"%f\t" "%f\t" 
			"%s\t" "%s\t" "%#x\t" "%#x\t" "%#x\n", 
			count_to_sec ( h->elapsed_time_count ),
			count_to_sec ( h->comm_time_count ), 

			( ( h->kind == MEM_ACCESS_READ ) ? "read" : "write" ), 
			( h->is_paddr_access ? "paddr" : "vaddr" ), 

			h->vaddr,
			h->eip,
			h->esp );
	}
	fclose (fp);
	Stat_print ( stderr, stat );
}

void
start_or_stop_profile2 ( struct mon_t *mon, bool_t start_flag )
{
	if ( start_flag ) {
		start_profile2 ( mon );
	} else {
		stop_profile2 ( mon );
	}
}

#ifdef ENABLE_MP

static void
send_start_or_stop_profile_request ( struct mon_t *mon )
{
	struct msg_t *msg;

	msg = Msg_create3 ( MSG_KIND_STAT_REQUEST );
	Comm_send ( mon->comm, msg, BSP_CPUID );
	Msg_destroy ( msg );	
}

static void
bcast_stat_request_ack ( struct mon_t *mon, bool_t start_flag )
{
	struct msg_t *msg;

	msg = Msg_create3 ( MSG_KIND_STAT_REQUEST_ACK, start_flag );
	Comm_bcast ( mon->comm, msg );
	Msg_destroy ( msg );	
}

#else /* ! ENABLE_MP */

static void
send_start_or_stop_profile_request ( struct mon_t *mon )
{
}

static void
bcast_stat_request_ack ( struct mon_t *mon, bool_t start_flag )
{
}

#endif /* ENABLE_MP */

void
start_or_stop_profile ( struct mon_t *mon  )
{
	static bool_t start_flag = TRUE;

	bcast_stat_request_ack ( mon, start_flag );
	start_or_stop_profile2 ( mon, start_flag );

	start_flag = ! start_flag;
}

