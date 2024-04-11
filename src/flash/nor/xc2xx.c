// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 *   Copyright (C) 2024 by XHSC Semiconductor                              *
 ***************************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
 
#include "imp.h"
#include <helper/binarybuffer.h>
#include <target/algorithm.h>
#include <target/armv7m.h>
#include <target/cortex_m.h>



#define CM_PFU0_BASE                                        (0x40070000U)
#define CM_PFU1_BASE                                        (0x40070400U)
#define CM_PFU2_BASE                                        (0x40070800U)
#define CM_PFU3_BASE                                        (0x40070C00U)
#define PFU_WMPR_OFFSET                                     (0x4)
#define PFU_RMR_OFFSET                                      (0x8)
#define PFU_WMR_OFFSET                                      (0xc)
#define PFU_OSR_OFFSET                                      (0x10U)
#define PFU_FSR_OFFSET                                      (0x20U)
#define PFU_FSCR_OFFSET                                     (0x24U)
#define PFU_IER_OFFSET                                      (0x28U)
#define PFU_OAR_OFFSET                                      (0x30U)
#define PFU_PDR0_OFFSET                                     (0x40U)
#define PFU_PDR1_OFFSET                                     (0x44U)
#define PFU_PDR2_OFFSET                                     (0x48U)
#define PFU_PDR3_OFFSET                                     (0x4cU)
#define PFU_ECCCR_OFFSET                                    (0x50U)
#define PFU_ECCDR_OFFSET                                    (0x54U)
#define PFU_ERR0_OFFSET                                     (0x58U)
#define PFU_ERR1_OFFSET                                     (0x5cU)
#define PFU_EIR0_OFFSET                                     (0x60U)
#define PFU_EIR1_OFFSET                                     (0x64U)
#define PFU_EIR2_OFFSET                                     (0x68U)
#define PFU_EIR3_OFFSET                                     (0x6cU)
#define PFU_EIR4_OFFSET                                     (0x70U)
#define PFU_SWER0_OFFSET                                    (0x80U)
#define PFU_SWER1_OFFSET                                    (0x84U)
#define PFU_SWER2_OFFSET                                    (0x8cU)
#define PFU_SWER3_OFFSET                                    (0x90U)

#define FLASH_WRITE_UNPROTECTED_VALUE1                      (0x01234567U)
#define FLASH_WRITE_UNPROTECTED_VALUE2                      (0xFEDCBA98U)

#define PFU_FSR_RDY                                         (0x00000100U)
#define PFU_WMR_WDWM                                        (0x80000000U)
#define PFU_OSR_OPST                                        (0x00000001U)
#define FLASH_SMART_SECTOR_ERASE_MODE                       (3U)
#define FLASH_SMART_SINGLE_PROGRAM_MODE2                    (6U)   

#define PFU_CMD_MASS_ERASE                                  (2U)
#define PFU_BLOCK_SIZE                                      (0x200000U)
#define PFU0_ADDR                                           (0x10000000U)
#define PFU1_ADDR                                           (0x10200000U)
#define PFU2_ADDR                                           (0x14000000U)
#define PFU3_ADDR                                           (0x14200000U)

#define PFU_MAP_MODE_REG                                    (0x4002703CU)

/*************************************************************************/
volatile uint32_t *pAddr_SC_UCA_FDMS = (volatile uint32_t *)PFU_MAP_MODE_REG;

#define FLASH_ERASE_TIMEOUT  100000  /*timeout count*/

const int PFU_CTRL_BASE[4] ={CM_PFU0_BASE, CM_PFU1_BASE, CM_PFU2_BASE, CM_PFU3_BASE}; 
static unsigned int dual_bank_probed = 0U;
/*Command flash bank xc2xx <base> <size> 0 0 <target>*/
FLASH_BANK_COMMAND_HANDLER(xc2xx_flash_bank_command)
{
    LOG_INFO("This is calling xc2xx_flash_bank_commad");
    bank->driver_priv = NULL;
 
    return ERROR_OK;
}

static inline unsigned int get_pfu_instance(unsigned int addr)
{   unsigned int instance = 0U;
    if ((addr >= PFU0_ADDR) && (addr < (PFU1_ADDR + PFU_BLOCK_SIZE)))
    {
        instance = 0U;
    }
    if ((addr >= PFU1_ADDR) && (addr < (PFU2_ADDR + PFU_BLOCK_SIZE)))
    {
        instance = 1U;
    }
    if ((addr >= PFU2_ADDR) && (addr < (PFU2_ADDR + PFU_BLOCK_SIZE)))
    {
        instance = 2U;
    }
    if ((addr >= PFU3_ADDR) && (addr < (PFU3_ADDR + PFU_BLOCK_SIZE)))
    {
        instance = 3U;
    }
    
    return instance;

}
static inline int xc2xx_write_protection_enable(struct flash_bank *bank, unsigned int instance)
{    
    LOG_DEBUG("Enable PFU register write protection enable");
    struct target *target = bank->target;
    int retval = ERROR_OK;
    retval = target_write_u32(target, CM_PFU0_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE1);
    if(retval != ERROR_OK){
      return retval;
    }
    
    retval = target_write_u32(target, CM_PFU0_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE2);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU1_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE1);
    if(retval != ERROR_OK){
      return retval;
    }
    
    retval = target_write_u32(target, CM_PFU1_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE2);
    if(retval != ERROR_OK){
      return retval;
    }
	  
    retval = target_write_u32(target, CM_PFU2_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE1);
    if(retval != ERROR_OK){
      return retval;
    }
    
    retval = target_write_u32(target, CM_PFU2_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE2);
    if(retval != ERROR_OK){
      return retval;
    }
	  
    retval = target_write_u32(target, CM_PFU3_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE1);
    if(retval != ERROR_OK){
      return retval;
    }
    
    retval = target_write_u32(target, CM_PFU3_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE2);
    if(retval != ERROR_OK){
      return retval;
    }
	  
    return retval;

}


static inline int xc2xx_write_protection_disable(struct flash_bank *bank, unsigned int instance)
{    
    LOG_DEBUG("Enable PFU register write protection disable");
    struct target *target = bank->target;
    int retval = ERROR_OK;
    retval =  target_write_u32(target, CM_PFU0_BASE + PFU_WMR_OFFSET,PFU_WMR_WDWM);
    if(retval != ERROR_OK){
      return retval;
    }
    retval =  target_write_u32(target, CM_PFU1_BASE + PFU_WMR_OFFSET,PFU_WMR_WDWM);
    if(retval != ERROR_OK){
      return retval;
    }
    retval =  target_write_u32(target, CM_PFU2_BASE + PFU_WMR_OFFSET,PFU_WMR_WDWM);
    if(retval != ERROR_OK){
      return retval;
    }
    retval =  target_write_u32(target, CM_PFU3_BASE + PFU_WMR_OFFSET,PFU_WMR_WDWM);
    if(retval != ERROR_OK){
      return retval;	  
    }
	
    return retval;
}
static inline int xc2xx_get_flash_status(struct target *target, uint32_t *status, unsigned int addr)
{    
    struct target *target = bank->target;
    LOG_DEBUG("Get flash busy status");


    return target_read_u32(target,PFU_CTRL_BASE[get_pfu_instance(addr + (uint32_t)bank->base)] + PFU_FSR_OFFSET, status);
}

static inline int xc2xx_sector_write_protection_enable(struct flash_bank *bank, unsigned int instance)
{    
    LOG_DEBUG("Enable PFU sector write protection disable");

    struct target *target = bank->target;
    int retval = ERROR_OK;
    retval = target_write_u32(target, CM_PFU0_BASE + PFU_SWER0_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU0_BASE + PFU_SWER1_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU0_BASE + PFU_SWER2_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU0_BASE + PFU_SWER3_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }

    retval = target_write_u32(target, CM_PFU1_BASE + PFU_SWER0_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU1_BASE + PFU_SWER1_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU1_BASE + PFU_SWER2_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU1_BASE + PFU_SWER3_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }	

    retval = target_write_u32(target, CM_PFU2_BASE + PFU_SWER0_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU2_BASE + PFU_SWER1_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU2_BASE + PFU_SWER2_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU2_BASE + PFU_SWER3_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }

    retval = target_write_u32(target, CM_PFU3_BASE + PFU_SWER0_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU3_BASE + PFU_SWER1_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU3_BASE + PFU_SWER2_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }
    retval = target_write_u32(target, CM_PFU3_BASE + PFU_SWER3_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }	

	return retval;
}

static int xc2xx_wait_status_busy(struct flash_bank *bank, int timeout, unsigned int addr)
{
    uint32_t status;
    int retval = ERROR_OK;
    struct target *target = bank->target;
    for(;;){
        retval = xc2xx_get_flash_status(bank,&status, addr);
        if(retval != ERROR_OK){
            return retval;
        }
        if((status & PFU_FSR_RDY)==PFU_FSR_RDY){
            return ERROR_OK;
        }
        if(timeout-- <= 0){
            LOG_DEBUG("Timed out waiting for flash: 0x%04x", status);
            return ERROR_FAIL;
        }
        alive_sleep(10);
    }
 
    return retval;
}
 
static int xc2xx_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    struct target *target = bank->target;
 
    LOG_DEBUG("xc2xx erase: %d - %d", first, last);
 
    if(target->state != TARGET_HALTED){
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }
 
 
    int retval = ERROR_OK; 
    retval = xc2xx_write_protection_enable(bank,0U);
    if(retval != ERROR_OK){
	    return retval;
	}
	
	retval = xc2xx_sector_write_protection_enable(bank,0);
    if(retval != ERROR_OK){
	    return retval;
	}
	
    for(unsigned int i = first ; i <= last; ++i){

        /*flash memory page erase*/
        retval = target_write_u32(target,  PFU_CTRL_BASE[get_pfu_instance(bank->sectors[i].offset + (uint32_t)bank->base)] + PFU_WMR_OFFSET, FLASH_SMART_SECTOR_ERASE_MODE);
        if(retval != ERROR_OK){
            return retval;
        }
		
        retval = target_write_u32(target,  PFU_CTRL_BASE[get_pfu_instance(bank->sectors[i].offset + (uint32_t)bank->base)] + PFU_OAR_OFFSET, bank->sectors[i].offset + (uint32_t)bank->base);
        if(retval != ERROR_OK){
            return retval;
        }

        retval = target_write_u32(target,  PFU_CTRL_BASE[get_pfu_instance(bank->sectors[i].offset + (uint32_t)bank->base)] + PFU_OSR_OFFSET, PFU_OSR_OPST);
        if(retval != ERROR_OK){
            return retval;
        }
 
        //wait
        retval = xc2xx_wait_status_busy(bank,FLASH_ERASE_TIMEOUT,bank->sectors[i].offset + (uint32_t)bank->base);
        if(retval != ERROR_OK){
            return retval;
        }
 
        LOG_DEBUG("xc2xx erased page %d", i);
        bank->sectors[i].is_erased = 1;
    }
 
    return ERROR_OK;
}
 
static int xc2xx_protect(struct flash_bank *bank, int set, unsigned int first, unsigned int last)
{
    return ERROR_FLASH_OPER_UNSUPPORTED;
}
 
static int xc2xx_write(struct flash_bank *bank, const uint8_t *buffer,uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
 
    LOG_DEBUG("xc2xx flash write: 0x%x 0x%x", offset, count);
 
    if(target->state != TARGET_HALTED){
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }
	
    int retval = ERROR_OK; 
    retval = xc2xx_write_protection_enable(bank,0U);
    if(retval != ERROR_OK){
	    return retval;
	}
	
	retval = xc2xx_sector_write_protection_enable(bank,0);
    if(retval != ERROR_OK){
	    return retval;
	}
	
	
    if(offset & 0x03){
        LOG_ERROR("offset 0x%x required 4-byte alignment", offset);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }
    if(count & 0x3){
        LOG_ERROR("size 0x%x required 4-byte alignment", count);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }
 
    uint32_t addr = offset;
    for(uint32_t i = 0; i < count; i += 16)
    {
        uint32_t word0 = (buffer[i]   << 0)  |
                         (buffer[i+1] << 8)  |
                         (buffer[i+2] << 16) |
                         (buffer[i+3] << 24);

        uint32_t word1 = (buffer[i+4]   << 0)  |
                         (buffer[i+4+1] << 8)  |
                         (buffer[i+4+2] << 16) |
                         (buffer[i+4+3] << 24);

        uint32_t word2 = (buffer[i+8]   << 0)  |
                         (buffer[i+8+1] << 8)  |
                         (buffer[i+8+2] << 16) |
                         (buffer[i+8+3] << 24);

        uint32_t word3 = (buffer[i+12]   << 0)  |
                         (buffer[i+12+1] << 8)  |
                         (buffer[i+12+2] << 16) |
                         (buffer[i+12+3] << 24);
						 
        LOG_DEBUG("xc2xx flash write word 0x%x 0x%x 0x%08x", i,    addr,      word0);
        LOG_DEBUG("xc2xx flash write word 0x%x 0x%x 0x%08x", i+1u, addr+4u,   word1);
        LOG_DEBUG("xc2xx flash write word 0x%x 0x%x 0x%08x", i+2u, addr+8u,   word2);
        LOG_DEBUG("xc2xx flash write word 0x%x 0x%x 0x%08x", i+3u, addr+0xcu, word3);
 
        // flash memory word program
		
        LOG_INFO("addr=%x",(addr+ (uint32_t)bank->base));
        retval = target_write_u32(target, PFU_CTRL_BASE[get_pfu_instance(addr+ (uint32_t)bank->base)] + PFU_WMR_OFFSET, FLASH_SMART_SINGLE_PROGRAM_MODE2);
        if(retval != ERROR_OK){
            return retval;
        }
		
        retval = target_write_u32(target, PFU_CTRL_BASE[get_pfu_instance(addr+ (uint32_t)bank->base)] + PFU_OAR_OFFSET, addr);
        if(retval != ERROR_OK){
            return retval;
        }

        retval = target_write_u32(target, PFU_CTRL_BASE[get_pfu_instance(addr+ (uint32_t)bank->base)] + PFU_PDR0_OFFSET, word0);
        if(retval != ERROR_OK){
            return retval;
        }

        retval = target_write_u32(target, PFU_CTRL_BASE[get_pfu_instance(addr+ (uint32_t)bank->base)] + PFU_PDR1_OFFSET, word1);
        if(retval != ERROR_OK){
            return retval;
        }

        retval = target_write_u32(target, PFU_CTRL_BASE[get_pfu_instance(addr+ (uint32_t)bank->base)] + PFU_PDR2_OFFSET, word2);
        if(retval != ERROR_OK){
            return retval;
        }

        retval = target_write_u32(target, PFU_CTRL_BASE[get_pfu_instance(addr+ (uint32_t)bank->base)] + PFU_PDR3_OFFSET, word3);
        if(retval != ERROR_OK){
            return retval;
        }


        retval = target_write_u32(target, PFU_CTRL_BASE[get_pfu_instance(addr+ (uint32_t)bank->base)] + PFU_OSR_OFFSET,PFU_OSR_OPST);
        if(retval != ERROR_OK){
            return retval;
        }


        // wait
        retval = xc2xx_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, addr);
        if (retval != ERROR_OK)
            return retval;
        addr += 16u;
    }
 
    LOG_DEBUG("xc2xx flash write success");
    return ERROR_OK;
}
 
static int xc2xx_probe(struct flash_bank *bank)
{
    LOG_INFO("This is calling xc2xx_probe 0x%x bank number",bank->bank_number);
    int sector_size = 0x4000;
    bank-> size = 0x400000;
    int num_sectors = bank->size / sector_size;
 

 
    if(bank->sectors){
        free(bank->sectors);
    }
 
    if(dual_bank_probed == 0)
    {
      bank->base = 0x10000000;
      dual_bank_probed = 1;
    }
    else
    {
      bank->base = 0x14000000;
      dual_bank_probed = 0;
    }
    bank->num_sectors = num_sectors;
    bank->sectors = malloc(sizeof(struct flash_sector) * num_sectors);
 
    for(int i = 0; i < num_sectors; ++i){
        bank->sectors[i].offset + (uint32_t)bank->base = i * sector_size;
        bank->sectors[i].size = sector_size;
        bank->sectors[i].is_erased = -1;
        bank->sectors[i].is_protected = 0;
    }

    LOG_INFO("xc2xxx probe: %d sectors, 0x%x sectors bytes, 0x%x total, 0x%x base address", num_sectors, sector_size, bank->size, (uint32_t)bank->base);
     
    return ERROR_OK;
}
 
static int xc2xx_auto_probe(struct flash_bank * bank)
{
    LOG_INFO("This is calling xc2xx_auto_probe");
    return xc2xx_probe(bank);
}
 
static int xc2xx_protect_check(struct flash_bank *bank)
{
    return ERROR_OK;
}
 
static int xc2xx_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    command_print_sameline(cmd, "xc2xx");
    return ERROR_OK;
}
 
static int xc2xx_mass_erase(struct flash_bank *bank, unsigned int bank_id)
{
    struct target *target = bank->target;
 
    if (target->state != TARGET_HALTED) 
    {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }
    LOG_INFO("mass erase operation on going...");
    int retval = ERROR_OK;
    retval = xc2xx_write_protection_enable(bank, 0U);
    if(retval != ERROR_OK)
    {
	return retval;
    }
	
    retval = xc2xx_sector_write_protection_enable(bank,0);
    if(retval != ERROR_OK)
    {
	return retval;
    }

    if(bank_id == 0)
    {
	    // flash memory mass erase
	    retval = target_write_u32(target, CM_PFU0_BASE + PFU_WMR_OFFSET, PFU_CMD_MASS_ERASE);
	    if (retval != ERROR_OK)
	    {
		return retval;
	    }
	    retval = target_write_u32(target, CM_PFU0_BASE + PFU_OAR_OFFSET, PFU0_ADDR);
	    if(retval != ERROR_OK)
	    {
		return retval;
	    }
		
	    retval = target_write_u32(target, CM_PFU0_BASE + PFU_OSR_OFFSET,PFU_OSR_OPST);
	    if(retval != ERROR_OK)
	    {
		return retval;
	    }
		

	    retval = xc2xx_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, PFU0_ADDR);
	    if(retval != ERROR_OK)
	    {
		return retval;
	    }


	    retval = target_write_u32(target, CM_PFU1_BASE + PFU_WMR_OFFSET, PFU_CMD_MASS_ERASE);
	    if (retval != ERROR_OK)
	    {
		return retval;

	    }
	    
	    retval = target_write_u32(target, CM_PFU1_BASE + PFU_OAR_OFFSET, PFU1_ADDR);
	    if(retval != ERROR_OK)
	    {
		return retval;
	    }

	    retval = target_write_u32(target, CM_PFU1_BASE + PFU_OSR_OFFSET,PFU_OSR_OPST);
	    if(retval != ERROR_OK)
	    {
	    
		return retval;
	    }
		
	    retval = xc2xx_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, PFU1_ADDR);
	    if(retval != ERROR_OK)
	    {
		return retval;
	    }
    }
    
    if(bank_id == 1)
    {

	    retval = target_write_u32(target, CM_PFU2_BASE + PFU_WMR_OFFSET, PFU_CMD_MASS_ERASE);
	    if (retval != ERROR_OK)
	    {
		return retval;
	    }
		
	    retval = target_write_u32(target, CM_PFU2_BASE + PFU_OAR_OFFSET, PFU2_ADDR);
	    if(retval != ERROR_OK)
	    {
	   	return retval;
	    }
		

	    retval = target_write_u32(target, CM_PFU2_BASE + PFU_OSR_OFFSET,PFU_OSR_OPST);

	    if(retval != ERROR_OK)
	    {
		return retval;
	    }
		
		
	    
	    retval = xc2xx_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, PFU2_ADDR);
	    if (retval != ERROR_OK)
	    {
		return retval;
	    }

	    retval = target_write_u32(target, CM_PFU3_BASE + PFU_WMR_OFFSET, PFU_CMD_MASS_ERASE);
	    if(retval != ERROR_OK)
	    {
		return retval;
	    }
	    
	    retval = target_write_u32(target, CM_PFU3_BASE + PFU_OAR_OFFSET, PFU3_ADDR);
	    if(retval != ERROR_OK)
	    {
		return retval;
	    }
		
	    retval = target_write_u32(target, CM_PFU3_BASE + PFU_OSR_OFFSET,PFU_OSR_OPST);
	    if(retval != ERROR_OK)
	    {
		return retval; 
	    }
		

	    retval = xc2xx_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, PFU3_ADDR);
	    if (retval != ERROR_OK)
	    {
		return retval;
	    }
    }
 
    LOG_INFO("mass erase operation done");
     
    return ERROR_OK;
}
 
COMMAND_HANDLER(xc2xx_handle_mass_erase_command)
{
    if(CMD_ARGC != 1 )
        return ERROR_COMMAND_SYNTAX_ERROR;
    unsigned int bank_id = 0;
     
    COMMAND_PARSE_NUMBER(u32,CMD_ARGV[0],bank_id);
    if((bank_id != 0) && (bank_id != 1))
        return ERROR_COMMAND_SYNTAX_ERROR;    
    
    LOG_INFO("Start to do mass erase");
    struct flash_bank *bank;
    int retval = CALL_COMMAND_HANDLER(flash_command_get_bank,0, &bank);
    if(retval != ERROR_OK){
        return retval;
    }
 
    retval = xc2xx_mass_erase(bank,bank_id);
    if (retval == ERROR_OK) {
        // set all sectors as erased
        unsigned int i;
        for (i = 0; i < bank->num_sectors; i++)
            bank->sectors[i].is_erased = 1;
 
        LOG_INFO("mass erase success");
    } else {
        LOG_INFO("mass erase fail");
    }
 
    return ERROR_OK;
}

 
static const struct command_registration xc2xx_exec_command_handlers[] = {
    {
        .name = "mass_erase",
        .handler = xc2xx_handle_mass_erase_command,
        .mode = COMMAND_EXEC,
        .usage = "bank_id",
        .help = "Erase all flash banks",
    },
    
    COMMAND_REGISTRATION_DONE
};
 
static const struct command_registration xc2xx_command_handlers[] = {
    {
        .name = "xc2xx",
        .mode = COMMAND_ANY,
        .help = "xc2xx flash command group",
        .usage = "",
        .chain = xc2xx_exec_command_handlers,
    },
    COMMAND_REGISTRATION_DONE
};
 
const struct flash_driver xc2xx_flash = {
    .name = "xc2xx",
    .commands = xc2xx_command_handlers,
    .flash_bank_command = xc2xx_flash_bank_command,
    .erase = xc2xx_erase,
    .protect = xc2xx_protect,
    .write = xc2xx_write,
    .read = default_flash_read,
    .probe = xc2xx_probe,
    .auto_probe = xc2xx_auto_probe,
    .erase_check = default_flash_blank_check,
    .protect_check = xc2xx_protect_check,
    .info = xc2xx_info,
};

