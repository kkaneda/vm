#include "vmm/mon/mon.h"
#include <string.h>


enum { 
	PCI_ADDRESS_SPACE_IO 	= 0x01,
	PCI_COMMAND		= 0x04,
	PCI_ROM_SLOT 		= 0x06,
};

static struct pci_dev_t *
PciDev_create ( const char *name )
{
	struct pci_dev_t *x;
	int i;

	ASSERT ( name != NULL );

	x = Malloct ( struct pci_dev_t );
	x->name = Strdup ( name );

	for ( i = 0; i < NR_PCI_DEV_CONFIG; i++ ) {
		x->config[i] = 0;
	}

	return x;
}

static bit32u_t
PciDev_read_config ( struct pci_dev_t *x, bit8u_t addr, size_t len )
{
	int i;
	bit32u_t ret;

	ret = 0;
	for ( i = 0; i < len; i++ ) {
		ret |= ( x->config[ addr + i ] << ( i * 8 ));
	}

/*
	if ( ( addr >= 0x41 ) && ( 0x41 < addr + len ) && ( strcmp ( x->name, "PIIX3 IDE" ) == 0 ) ) {
		Print_color ( stdout, CYAN, "read_config: addr=%#x, ret = %#x, %#x\n", addr, ret, x->config[addr ]  );
	}

	if ( ( addr >= 0x43 ) && ( 0x43 < addr + len ) && ( strcmp ( x->name, "PIIX3 IDE" ) == 0 ) ) {
		Print_color ( stdout, CYAN, "read_config: addr=%#x, ret = %#x, %#x\n", addr, ret, x->config[addr ]  );
	}

	if ( ( addr >= 0x20 ) && ( 0x20 < addr + len ) && ( strcmp ( x->name, "PIIX3 IDE" ) == 0 ) ) {
		Print_color ( stdout, CYAN, "read_config: addr=%#x, len=%#x, ret = %#x, %#x\n", 
			      addr, len, ret, x->config [ addr ]  );
	}
*/
	return ret;
}

static bool_t
__is_modifiable1 ( bit32u_t addr )
{
	bool_t ret = FALSE;

	switch ( addr ) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0e:
	case 0x10 ... 0x27: /* base */
	case 0x30 ... 0x33: /* rom */
	case 0x3d:
		ret = FALSE;
                break;
	default:
		ret = TRUE;
                break;
	}	
	return ret;
}

static bool_t
__is_modifiable2 ( bit32u_t addr )
{
	bool_t ret = FALSE;

	switch ( addr ) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0e:
	case 0x38 ... 0x3b: /* rom */
	case 0x3d:
		ret = FALSE;
                break;
	default:
		ret = TRUE;
                break;
	}
	
	return ret;
}

static bool_t
is_modifiable ( struct pci_dev_t *x, bit32u_t addr )
{
	bool_t ret = FALSE;
		
        /* default read/write accesses */
        switch ( x->config[0x0e] ) {
        case 0x00:
        case 0x80:
		ret = __is_modifiable1 ( addr );
		break;
		
        default:
        case 0x01:
		ret = __is_modifiable2 ( addr );
		break;
        }
	
	return ret;
}

#if 0
static void
__pci_update_mappings(PCIDevice *d)
{
	r = &d->io_regions[i];
	if (i == PCI_ROM_SLOT) {
		config_ofs = 0x30;
	} else {
		config_ofs = 0x10 + i * 4;
	}
	if (r->size != 0) {
		if (r->type & PCI_ADDRESS_SPACE_IO) {
			if (cmd & PCI_COMMAND_IO) {
				new_addr = le32_to_cpu(*(uint32_t *)(d->config + 
								     config_ofs));
				new_addr = new_addr & ~(r->size - 1);
				last_addr = new_addr + r->size - 1;
				/* NOTE: we have only 64K ioports on PC */
				if (last_addr <= new_addr || new_addr == 0 ||
				    last_addr >= 0x10000) {
					new_addr = -1;
				}
			} else {
				new_addr = -1;
			}
		} else {
			if (cmd & PCI_COMMAND_MEMORY) {
				new_addr = le32_to_cpu(*(uint32_t *)(d->config + 
							     config_ofs));
				/* the ROM slot has a specific enable bit */
				if (i == PCI_ROM_SLOT && !(new_addr & 1))
					goto no_mem_map;
				new_addr = new_addr & ~(r->size - 1);
				last_addr = new_addr + r->size - 1;
				/* NOTE: we do not support wrapping */
				/* XXX: as we cannot support really dynamic
				   mappings, we handle specific values as invalid
				   mappings. */
				if (last_addr <= new_addr || new_addr == 0 ||
				    last_addr == -1) {
					new_addr = -1;
				}
			} else {
			no_mem_map:
				new_addr = -1;
			}
		}
		/* now do the real mapping */
		if (new_addr != r->addr) {
			if (r->addr != -1) {
				if (r->type & PCI_ADDRESS_SPACE_IO) {
					int class;
					/* NOTE: specific hack for IDE in PC case:
					   only one byte must be mapped. */
					class = d->config[0x0a] | (d->config[0x0b] << 8);
					if (class == 0x0101 && r->size == 4) {
						isa_unassign_ioport(r->addr + 2, 1);
					} else {
						isa_unassign_ioport(r->addr, r->size);
					}
				} else {
					cpu_register_physical_memory(r->addr + pci_mem_base, 
								     r->size, 
								     IO_MEM_UNASSIGNED);
				}
			}
			r->addr = new_addr;
			if (r->addr != -1) {
				r->map_func(d, i, r->addr, r->size, r->type);
			}
		}
	}
}

static void
pci_update_mappings(PCIDevice *d)
{
	PCIIORegion *r;
	int cmd, i;
	uint32_t last_addr, new_addr, config_ofs;
	
	cmd = le16_to_cpu(*(uint16_t *)(d->config + PCI_COMMAND));
	for(i = 0; i < PCI_NUM_REGIONS; i++) {
		__pci_update_mapping ( );
	}
}
#endif


static void
PciDev_write_config ( struct pci_dev_t *x, bit8u_t addr, bit32u_t val, size_t len )
{

	int i;

/*
	if ( ( addr >= 0x41 ) && ( 0x41 < addr + len ) && ( strcmp ( x->name, "PIIX3 IDE" ) == 0 ) ) {
		Print_color ( stdout, CYAN, "write_config: addr = %#x, val = %#x\n",  addr, val );
	}

	if ( ( addr >= 0x43 ) && ( 0x43 < addr + len ) && ( strcmp ( x->name, "PIIX3 IDE" ) == 0 ) ) {
		Print_color ( stdout, CYAN, "write_config: addr = %#x, val = %#x\n",  addr, val );
	}

	if ( ( addr >= 0x20 ) && ( 0x20 < addr + len ) && ( strcmp ( x->name, "PIIX3 IDE" ) == 0 ) ) {
		Print_color ( stdout, CYAN, "write_config: addr = %#x, val = %#x (%d,%d,%d,%d)\n",
			      addr, val,
			      is_modifiable ( x, addr ),
			      is_modifiable ( x, addr + 1 ),
			      is_modifiable ( x, addr + 2 ),
			      is_modifiable ( x, addr + 3 ) );
	}
*/

	if ( ( len == 4 ) &&
	     ( ( ( addr >= 0x10 ) && ( addr < 0x10 + 4 * 6 ) ) || 
	       ( ( addr >= 0x30 ) && ( addr < 0x34 ) ) ) ) {

		int reg;

		reg = ( addr >= 0x30 ) ?  PCI_ROM_SLOT : ( ( addr - 0x10 ) >> 2 );

		if ( reg == 4 ) { // in io_regions
			
			const bit32u_t IO_REGION_SIZE = 0x10;

			/* compute the stored value */
			if ( reg == PCI_ROM_SLOT ) {
				/* keep ROM enable bit */
				val &= ( ~ ( IO_REGION_SIZE - 1 ) ) | 1;
			} else {
				val &= ~ ( IO_REGION_SIZE - 1 );
				val |= PCI_ADDRESS_SPACE_IO;
			}
		
			for ( i = 0; i < len; i++ ) {
				x->config [ addr + i ] = SUB_BIT ( val, i*8, 8 );
			}
#if 0
			pci_update_mappings(d);
#endif
			return;
		}
	}



	for ( i = 0; i < len; i++ ) {
		bit32u_t p;
		
		p = addr + i;
		if ( is_modifiable ( x, p ) ) {
			x->config [ p ] = SUB_BIT ( val, i*8, 8 );
		}
	}


#if 0
	{
		bit32u_t end;

		enum { PCI_COMMAND = 0x04 }; /* 16 bits */

		end = addr + len;
		if ( ( end > PCI_COMMAND ) &&  ( addr < PCI_COMMAND + 2 ) ) {
			update_mappings(d);
		}
	}
#endif
}

static struct pci_dev_t *
create_i440fx ( void )
{
	struct pci_dev_t *x;

	x = PciDev_create ( "i440FX" );

	x->config[0x00] = 0x86; // vendor_id
	x->config[0x01] = 0x80;
	x->config[0x02] = 0x37; // device_id
	x->config[0x03] = 0x12;
	x->config[0x08] = 0x02; // revision
	x->config[0x0a] = 0x00; // class_sub = host2pci
	x->config[0x0b] = 0x06; // class_base = PCI_bridge
	x->config[0x0e] = 0x00; // header_type

	return x;
}

static struct pci_dev_t *
create_ide ( void )
{
	struct pci_dev_t *x;

#if 0
	x = PciDev_create ( "IDE" );

	x->config[0x00] = 0x86; // Intel
	x->config[0x01] = 0x80;
	x->config[0x02] = 0x00; // fake
	x->config[0x03] = 0x01; // fake
	x->config[0x0a] = 0x01; // class_sub = PCI_IDE
	x->config[0x0b] = 0x01; // class_base = PCI_mass_storage
	x->config[0x0e] = 0x80; // header_type = PCI_multifunction, generic
	
	x->config[0x2c] = 0x86; // subsys vendor
	x->config[0x2d] = 0x80; // subsys vendor
	x->config[0x2e] = 0x00; // fake
	x->config[0x2f] = 0x01; // fake
	x->config[0x3d] = 0x01; // interrupt on pin 1
#else

	x = PciDev_create ( "PIIX3 IDE" );

	x->config[0x00] = 0x86; // Intel
	x->config[0x01] = 0x80;
	x->config[0x02] = 0x10;
	x->config[0x03] = 0x70;
	x->config[0x0a] = 0x01; // class_sub = PCI_IDE
	x->config[0x0b] = 0x01; // class_base = PCI_mass_storage
	x->config[0x0e] = 0x00; // header_type
			
#endif

        // enable IDE0
	x->config[0x40] = 0x00; 
	x->config[0x41] = 0x80; 

        // enable IDE1
	x->config[0x42] = 0x00; 
	x->config[0x43] = 0x80; 

	return x;
}

static void
piix3_reset ( struct pci_dev_t *x )
{
	x->config[0x04] = 0x07; // master, memory and I/O
	x->config[0x05] = 0x00;
	x->config[0x06] = 0x00;
	x->config[0x07] = 0x02; // PCI_status_devsel_medium
	x->config[0x4c] = 0x4d;
	x->config[0x4e] = 0x03;
	x->config[0x4f] = 0x00;
	x->config[0x60] = 0x80;
	x->config[0x69] = 0x02;
	x->config[0x70] = 0x80;
	x->config[0x76] = 0x0c;
	x->config[0x77] = 0x0c;
	x->config[0x78] = 0x02;
	x->config[0x79] = 0x00;
	x->config[0x80] = 0x00;
	x->config[0x82] = 0x00;
 	x->config[0xa0] = 0x08;
	x->config[0xa0] = 0x08;
	x->config[0xa2] = 0x00;
	x->config[0xa3] = 0x00;
	x->config[0xa4] = 0x00;
	x->config[0xa5] = 0x00;
	x->config[0xa6] = 0x00;
	x->config[0xa7] = 0x00;
	x->config[0xa8] = 0x0f;
	x->config[0xaa] = 0x00;
	x->config[0xab] = 0x00;
	x->config[0xac] = 0x00;
	x->config[0xae] = 0x00;
}

static struct pci_dev_t *
create_piix3 ( void )
{
	struct pci_dev_t *x;

	x = PciDev_create ( "PIIX3" );

	x->config[0x00] = 0x86; // Intel
	x->config[0x01] = 0x80;
	x->config[0x02] = 0x00; // 82371SB PIIX3 PCI-to-ISA bridge (Step A1)
	x->config[0x03] = 0x70;
	x->config[0x0a] = 0x01; // class_sub = PCI_ISA
	x->config[0x0b] = 0x06; // class_base = PCI_bridge
	x->config[0x0e] = 0x80; // header_type = PCI_multifunction, generic
	
	piix3_reset ( x );

	return x;
}

/************************************************/

static void
register_device ( struct pci_t *x, struct pci_dev_t *dev, int devfn )
{
	ASSERT ( x != NULL );
	ASSERT ( dev != NULL );
	ASSERT ( ( devfn >= 0 ) && ( devfn <= MAX_PCI_DEVS ) );

	x->devs[devfn] = dev;
}

static void 
set_io_region_addr ( struct pci_dev_t *x, int region_num, int type, bit32u_t addr )
{
	{
		bit8u_t ofs;
		
		ofs = ( region_num == PCI_ROM_SLOT ) ? 0x30 : ( 0x10 + region_num * 4 );
		PciDev_write_config ( x, ofs, addr, 4 );
	}
	
	{ /* enable memory mappings */
		bit16u_t cmd;
		
		cmd = PciDev_read_config ( x, PCI_COMMAND, 2 );
		if ( region_num == PCI_ROM_SLOT ) {
			cmd |= 2;
		} else if ( type & PCI_ADDRESS_SPACE_IO ) {
			cmd |= 1;
		} else {
			cmd |= 2;
		}
		
//		Print_color ( stdout, RED, "set_io_region_addr: ofs=%#x, addr=%#x\n", ofs, addr );
		PciDev_write_config ( x, PCI_COMMAND, cmd, 2 );
	}
}

void
Pci_init ( struct pci_t *x )
{
	int i;
	struct pci_dev_t *i440fx, *ide, *piix3;

	ASSERT ( x != NULL );

	x->config_reg = 0;

	for ( i = 0; i < MAX_PCI_DEVS; i++ ) {
		x->devs[i] = NULL;
	}
	
	i440fx = create_i440fx ( );
	register_device ( x, i440fx, 0 );

	ide = create_ide ( );
	register_device ( x, ide, 8 );

#if 0
	set_io_region_addr ( ide, 0, PCI_ADDRESS_SPACE_IO, 0x1f0 );
	set_io_region_addr ( ide, 1, PCI_ADDRESS_SPACE_IO, 0x3f4 );
	set_io_region_addr ( ide, 2, PCI_ADDRESS_SPACE_IO, 0x170 );
	set_io_region_addr ( ide, 3, PCI_ADDRESS_SPACE_IO, 0x374 );
#else
//	set_io_region_addr ( ide, 4, PCI_ADDRESS_SPACE_IO, 0x374 );
	{
		const bit32u_t BIOS_IO_ADDR = 0xc000;
		const bit32u_t IO_REGION_SIZE = 0x10;
		bit32u_t paddr;

		paddr = ( BIOS_IO_ADDR + IO_REGION_SIZE - 1 ) & ( ~( IO_REGION_SIZE - 1 ) );
		set_io_region_addr ( ide, 4, PCI_ADDRESS_SPACE_IO, paddr );
	}
	 
#endif


	piix3 = create_piix3 ( );
	register_device ( x, piix3, 16 );
}


/*****************/

static int 
get_nr_pci_devs ( struct pci_t *x )
{
	int i;
	int n = 0;
	for ( i = 0; i < MAX_PCI_DEVS; i++ ) {
		if ( x->devs[i] != NULL ) { 
			n++; 
		}
	}
	return n;
}

void
Pci_pack ( struct pci_t *x, int fd )
{
	int i;

	Bit32u_pack ( x->config_reg, fd );
	Bit32u_pack ( ( bit32u_t ) get_nr_pci_devs ( x ), fd );

	for ( i = 0; i < MAX_PCI_DEVS; i++ ) {
		struct pci_dev_t *p = x->devs[i];

		if ( p == NULL ) {
			continue;
		}

		Bit32u_pack ( ( bit32u_t ) i, fd );
		Bit8uArray_pack ( p->config, NR_PCI_DEV_CONFIG, fd );
	}
}

void
Pci_unpack ( struct pci_t *x, int fd )
{
	int i, n;

	x->config_reg = Bit32u_unpack ( fd );
	n = ( int ) Bit32u_unpack ( fd );

	for ( i = 0; i < n; i++ ) {
		int j = ( int ) Bit32u_unpack ( fd );
		struct pci_dev_t *p = x->devs[j];

		assert ( p != NULL ); /* [TODO] */

		Bit8uArray_unpack ( p->config, NR_PCI_DEV_CONFIG, fd );
	}	
}

/*****************/

struct pci_addr_t {
	int 	bus_num;
	int 	devfn;
	struct pci_dev_t *dev;
	bit8u_t	config_addr;
	bool_t  is_valid;
};

static struct pci_addr_t 
parse_config_reg ( struct pci_t *x, bit16u_t addr )
{
	struct pci_addr_t pci_addr;

	ASSERT ( x != NULL );
	
	pci_addr.bus_num = SUB_BIT ( x->config_reg, 16, 8 );

	pci_addr.devfn = SUB_BIT ( x->config_reg, 8, 8 );
	pci_addr.dev = x->devs [ pci_addr.devfn ];

	pci_addr.config_addr = ( BIT_ALIGN ( SUB_BIT ( x->config_reg, 0, 8 ), 2 ) | 
				 SUB_BIT ( addr, 0, 2 ) );

	pci_addr.is_valid = ( ( TEST_BIT ( x->config_reg, 31 ) ) &&
			      ( SUB_BIT ( x->config_reg, 0, 2 ) == 0 ) && 
			      ( pci_addr.bus_num == 0 ) &&
			      ( pci_addr.dev != NULL ) );

/*
	Print ( stdout,
		"\t\t" "bus=%x, devfn=%x, dev=%p, config_addr=%#x, is_valid=%d\n",
		pci_addr.bus_num,
		pci_addr.devfn,
		pci_addr.dev,
		pci_addr.config_addr,
		pci_addr.is_valid );
*/

	return pci_addr;
}

/*****************/

static bit32u_t
get_invalid_val ( size_t len )
{
	bit32u_t ret = 0;
	
	switch ( len ) {
	case 1:  ret = 0xff; break;
	case 2:  ret = 0xffff; break;
	case 4:  ret = 0xffffffff; break;
	default: Match_failure ( "get_invalid_value\n" );
	}
	return ret;
}

static bit32u_t
read_data ( struct pci_t *x, bit16u_t addr, size_t len )
{
	struct pci_addr_t pci_addr; 

	pci_addr = parse_config_reg ( x, addr );

	if ( ! pci_addr.is_valid ) {
		return get_invalid_val ( len );
	}

	return PciDev_read_config ( pci_addr.dev, pci_addr.config_addr, len );
}

bit32u_t
Pci_read ( struct pci_t *x, bit16u_t addr, size_t len )
{
	bit32u_t ret = 0;

	switch ( addr ) {
	case 0xcf8:
		ret = SUB_BIT ( x->config_reg, 0, len * 8 );
		break;

	case 0xcfa:
		ret = 0; /* [TODO] */
		break;

	case 0x0cfc:
	case 0x0cfd:
	case 0x0cfe:
	case 0x0cff:
		ret = read_data ( x, addr, len );
		break;
	default:
		Match_failure ( "Pci_read: addr=%#x\n", addr );
	}

//	Print_color ( stdout, GREEN, "PCI read : addr=%#x, val=%#x, len=%#x\n", addr, ret, len );
	
	return ret;
}

/*****************/

static void
write_data ( struct pci_t *x, bit16u_t addr, bit32u_t val, size_t len )
{
	struct pci_addr_t pci_addr; 

	pci_addr = parse_config_reg ( x, addr );

	if ( ! pci_addr.is_valid ) {
		return;
	}

	PciDev_write_config ( pci_addr.dev, pci_addr.config_addr, val, len );
}

void
Pci_write ( struct pci_t *x, bit16u_t addr, bit32u_t val, size_t len )
{
	assert ( x != NULL );

//	Print_color ( stdout, GREEN, "PCI write: addr=%#x, val=%#x, len=%#x\n", addr, val, len );

	switch ( addr ) {
	case 0xcf8: {
		int n = len * 8;
		if ( len == 4 ) {
			x->config_reg = val; 
		} else {
			x->config_reg = ( BIT_ALIGN ( x->config_reg, n ) |
					  SUB_BIT ( val, 0, n ) );
		}
		break;
	}

	case 0xcfa: 
	case 0xcfb:
//		Print_color ( stdout, GREEN, "\t" "ignore\n" );
		break;

	case 0xcfc:
	case 0xcfd:
	case 0xcfe:
	case 0xcff:
		write_data ( x, addr, val, len );
		break;

	default:
		Match_failure ( "Pci_write: addr=%#x\n", addr );
	}
}
