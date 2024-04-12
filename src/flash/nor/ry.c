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
#include <stdio.h>
#include <string.h>


#define CM_PFU0_BASE                                        (0x40070000U)
#define CM_PFU1_BASE                                        (0x40070400U)
#define CM_PFU2_BASE                                        (0x40070800U)
#define CM_PFU3_BASE                                        (0x40070C00U)
#define CM_DFU0_BASE                                        (0x40078000U)
#define CM_DFU1_BASE                                        (0x40078400U)

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
#define PFU_FSR_SPWP                                        (0x00000200U)
#define PFU_WMR_WDWM                                        (0x80000000U)
#define PFU_OSR_OPST                                        (0x00000001U)
#define FLASH_SMART_SECTOR_ERASE_MODE                       (3U)
#define FLASH_SMART_SINGLE_PROGRAM_MODE2                    (6U)   
#define FLASH_SMART_SERIES_PROGRAM_MODE1                    (0xCU)   

#define PFU_CMD_MASS_ERASE                                  (2U)
#define PFU_BLOCK_SIZE                                      (0x200000U)
#define DFU_BLOCK_SIZE                                      (0x20000U)
#define DFU_SCS_BLOCK_SIZE                                  (0x4000U)

#define PFU0_ADDR_SINGLE                                    (0x10000000U)
#define PFU1_ADDR_SINGLE                                    (0x10200000U)
#define PFU2_ADDR_SINGLE                                    (0x10400000U)
#define PFU3_ADDR_SINGLE                                    (0x10600000U)


#define PFU0_ADDR_MAP_A                                     (0x10000000U)
#define PFU1_ADDR_MAP_A                                     (0x10200000U)
#define PFU2_ADDR_MAP_A                                     (0x14000000U)
#define PFU3_ADDR_MAP_A                                     (0x14200000U)

#define PFU0_ADDR_MAP_B                                     (0x14000000U)
#define PFU1_ADDR_MAP_B                                     (0x14200000U)
#define PFU2_ADDR_MAP_B                                     (0x10000000U)
#define PFU3_ADDR_MAP_B                                     (0x10200000U)


#define DFU0_ADDR_SINGLE                                    (0x40400000u)
#define DFU1_ADDR_SINGLE                                    (0x40420000u)



#define DFU0_ADDR_MAP_A                                     (0x40400000u)
#define DFU1_ADDR_MAP_A                                     (0x40800000u)

#define DFU0_ADDR_MAP_B                                     (0x40800000u)
#define DFU1_ADDR_MAP_B                                     (0x40400000u)

#define CM_DFU0_SCS_BASE                                    (0x40C00000U)



#define FLASH_SC_MODE_SET                                   (0x4002703CU)
#define PFLASH_TYPE_BIT                                     (0x10000000U)
#define DFLASH_TYPE_BIT                                     (0x40000000U)

/*************************************************************************/

#define FLASH_ERASE_TIMEOUT  100000  /*timeout count*/

const int PFU_CTRL_BASE[4] ={CM_PFU0_BASE, CM_PFU1_BASE, CM_PFU2_BASE, CM_PFU3_BASE}; 
const int DFU_CTRL_BASE[2] ={CM_DFU0_BASE, CM_DFU1_BASE}; 

const char *flash_mode[] = {"FLASH_SINGLE_MAP", "FLASH_DOUBLE_A_MAP","FLASH_DOUBLE_B_MAP"};

typedef enum 
{
    FLASH_SINGLE_MAP = 0,
    FLASH_DOUBLE_A_MAP,
    FLASH_DOUBLE_B_MAP    
} en_flash_mapping_mode_t;

typedef enum
{
    FLASH_P_FLASH = 0,
    FLASH_D_FLASH,
    FLASH_NONE_FLASH,
} en_flash_type_t;


static int FLASH_GetMapMode(struct target *target, en_flash_type_t eFlashType, en_flash_mapping_mode_t *flash_map_mode)
{

    int retval = ERROR_OK;
    uint32_t FLASH_Red_SCS = 0x0;
    retval = target_read_u32(target,FLASH_SC_MODE_SET, &FLASH_Red_SCS);
    if(retval != ERROR_OK)
        return ERROR_FAIL;

    en_flash_mapping_mode_t eRet = FLASH_SINGLE_MAP;


    if(FLASH_P_FLASH == eFlashType)
    {
        if(!(FLASH_Red_SCS & 0x1))
        {
           eRet = FLASH_SINGLE_MAP;
        }
        else
        { 
          if((FLASH_Red_SCS & 0x4))
          {
             eRet = FLASH_DOUBLE_A_MAP;
          }
          else
          {
             eRet = FLASH_DOUBLE_B_MAP;
          }
        }
    }
    

    if(FLASH_D_FLASH == eFlashType)
    {
        if(!(FLASH_Red_SCS & 0x2))
        {
           eRet = FLASH_SINGLE_MAP;
        }
        else
        { 
          if((FLASH_Red_SCS & 0x8))
          {
             eRet = FLASH_DOUBLE_A_MAP;  
          }
          else
          {
             eRet = FLASH_DOUBLE_B_MAP; 
          }
        }
    }
    
    *flash_map_mode = eRet;
    return ERROR_OK;
}


/*Command flash bank ry <base> <size> 0 0 <target>*/
FLASH_BANK_COMMAND_HANDLER(ry_flash_bank_command)
{
    LOG_INFO("This is calling ry_flash_bank_commad");
    bank->driver_priv = NULL;
 
    return ERROR_OK;
}

static inline unsigned int get_pdfu_instance(struct target *target, unsigned int addr)
{   unsigned int instance = 0U;
    int retval = ERROR_OK;
    en_flash_type_t eFlashType = FLASH_P_FLASH;

    if (addr & PFLASH_TYPE_BIT)
    {
        eFlashType = FLASH_P_FLASH;
    }
    else if (addr & DFLASH_TYPE_BIT)
    {
        eFlashType = FLASH_D_FLASH;
    }
    else
    {
        LOG_ERROR("Invalid P/DFLASH address for this device");
        return ERROR_FAIL;
    }
    
    en_flash_mapping_mode_t eRet =  FLASH_SINGLE_MAP;
    retval = FLASH_GetMapMode(target, eFlashType, (en_flash_mapping_mode_t*)&eRet);
    if(retval!=ERROR_OK)
        return ERROR_FAIL;
    
    if(eFlashType == FLASH_P_FLASH)
    {

        if(eRet == FLASH_DOUBLE_B_MAP)
        {
            if ((addr >= PFU0_ADDR_MAP_B) && (addr < (PFU0_ADDR_MAP_B + PFU_BLOCK_SIZE)))
            {
                instance = 0U;
            }
            else if ((addr >= PFU1_ADDR_MAP_B) && (addr < (PFU1_ADDR_MAP_B + PFU_BLOCK_SIZE)))
            {
                instance = 1U;
            }
            else if ((addr >= PFU2_ADDR_MAP_B) && (addr < (PFU2_ADDR_MAP_B + PFU_BLOCK_SIZE)))
            {
                instance = 2U;
            }
            else if ((addr >= PFU3_ADDR_MAP_B) && (addr < (PFU3_ADDR_MAP_B + PFU_BLOCK_SIZE)))
            {
                instance = 3U;
            }
            else
            {
                LOG_ERROR("Invalid P/DFLASH address for this device");
                return ERROR_FAIL;          
            }
        }

        if(eRet == FLASH_DOUBLE_A_MAP)
        {
            if ((addr >= PFU0_ADDR_MAP_A) && (addr < (PFU0_ADDR_MAP_A + PFU_BLOCK_SIZE)))
            {
                instance = 0U;
            }
            else if ((addr >= PFU1_ADDR_MAP_A) && (addr < (PFU1_ADDR_MAP_A + PFU_BLOCK_SIZE)))
            {
                instance = 1U;
            }
            else if ((addr >= PFU2_ADDR_MAP_A) && (addr < (PFU2_ADDR_MAP_A + PFU_BLOCK_SIZE)))
            {
                instance = 2U;
            }
            else if ((addr >= PFU3_ADDR_MAP_A) && (addr < (PFU3_ADDR_MAP_A + PFU_BLOCK_SIZE)))
            {
                instance = 3U;
            }
            else
            {
                LOG_ERROR("Invalid P/DFLASH address for this device");
                return ERROR_FAIL;          
            }
        }

        if(eRet == FLASH_SINGLE_MAP)
        {
            if ((addr >= PFU0_ADDR_SINGLE) && (addr < (PFU0_ADDR_SINGLE + PFU_BLOCK_SIZE)))
            {
                instance = 0U;
            }
            else if ((addr >= PFU1_ADDR_SINGLE) && (addr < (PFU1_ADDR_SINGLE + PFU_BLOCK_SIZE)))
            {
                instance = 1U;
            }
            else if ((addr >= PFU2_ADDR_SINGLE) && (addr < (PFU2_ADDR_SINGLE + PFU_BLOCK_SIZE)))
            {
                instance = 2U;
            }
            else if ((addr >= PFU3_ADDR_SINGLE) && (addr < (PFU3_ADDR_SINGLE + PFU_BLOCK_SIZE)))
            {
                instance = 3U;
            }
            else
            {
                LOG_ERROR("Invalid P/DFLASH address for this device");
                return ERROR_FAIL;          
            }
                
        }
    }
    

    if(eFlashType == FLASH_D_FLASH)
    {
        if ((addr >= CM_DFU0_SCS_BASE) && (addr < (CM_DFU0_SCS_BASE + DFU_SCS_BLOCK_SIZE)))
        {
            return 0U;
        } 

        if(eRet == FLASH_DOUBLE_B_MAP)
        {
            if ((addr >= DFU0_ADDR_MAP_B) && (addr < (DFU0_ADDR_MAP_B + DFU_BLOCK_SIZE)))
            {
                instance = 0U;
            }
            else if ((addr >= DFU1_ADDR_MAP_B) && (addr < (DFU1_ADDR_MAP_B + DFU_BLOCK_SIZE)))
            {
                instance = 1U;
            }
            else
            {
                LOG_ERROR("Invalid P/DFLASH address for this device");
                return ERROR_FAIL;          
            }
        }

        if(eRet == FLASH_DOUBLE_A_MAP)
        {
            if ((addr >= DFU0_ADDR_MAP_A) && (addr < (DFU0_ADDR_MAP_A + DFU_BLOCK_SIZE)))
            {
                instance = 0U;
            }
            else if ((addr >= DFU1_ADDR_MAP_A) && (addr < (DFU1_ADDR_MAP_A + DFU_BLOCK_SIZE)))
            {
                instance = 1U;
            }
            else
            {
                LOG_ERROR("Invalid P/DFLASH address for this device");
                return ERROR_FAIL;          
            }
        }

        if(eRet == FLASH_SINGLE_MAP)
        {
            if ((addr >= DFU0_ADDR_SINGLE) && (addr < (DFU0_ADDR_SINGLE + DFU_BLOCK_SIZE)))
            {
                instance = 0U;
            }
            else if ((addr >= DFU1_ADDR_SINGLE) && (addr < (DFU1_ADDR_SINGLE + DFU_BLOCK_SIZE)))
            {
                instance = 1U;
            }
            else
            {
                LOG_ERROR("Invalid P/DFLASH address for this device");
                return ERROR_FAIL;          
            }
                
        } 
    }
    
    LOG_DEBUG("FLASH instance %d", instance);

    return instance;


}
static inline int ry_write_protection_enable(struct flash_bank *bank, unsigned int instance)
{    
    LOG_DEBUG("Enable D/PFU register write protection enable");
    struct target *target = bank->target;
    int retval = ERROR_OK;

    // DFLASH   
    retval = target_write_u32(target, CM_DFU0_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE1);
    if(retval != ERROR_OK){
      return retval;
    }
    
    retval = target_write_u32(target, CM_DFU0_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE2);
    if(retval != ERROR_OK){
      return retval;
    }

    retval = target_write_u32(target, CM_DFU1_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE1);
    if(retval != ERROR_OK){
      return retval;
    }
    
    retval = target_write_u32(target, CM_DFU1_BASE + PFU_WMPR_OFFSET,FLASH_WRITE_UNPROTECTED_VALUE2);
    if(retval != ERROR_OK){
      return retval;
    }

    // PFLASH
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


static inline int ry_write_protection_disable(struct flash_bank *bank, unsigned int instance)
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
    
    //DFLASH
    retval =  target_write_u32(target, CM_DFU0_BASE + PFU_WMR_OFFSET,PFU_WMR_WDWM);
    if(retval != ERROR_OK){
      return retval;
    }
    retval =  target_write_u32(target, CM_DFU1_BASE + PFU_WMR_OFFSET,PFU_WMR_WDWM);
    if(retval != ERROR_OK){
      return retval;      
    }

    return retval;
}
static inline int ry_get_flash_status(struct flash_bank *bank, uint32_t *status, unsigned int instance)
{    
    struct target *target = bank->target;
    LOG_DEBUG("Get flash busy status");
    if (bank->base & PFLASH_TYPE_BIT)
    {
        return target_read_u32(target,PFU_CTRL_BASE[instance] + PFU_FSR_OFFSET, status);
    }
    if (bank->base & DFLASH_TYPE_BIT)
    {
        return target_read_u32(target,DFU_CTRL_BASE[instance] + PFU_FSR_OFFSET, status);
    }

    return ERROR_OK; 

}

static inline int ry_sector_write_protection_enable(struct flash_bank *bank, unsigned int instance)
{    
    LOG_DEBUG("Enable PFU sector write protection disable");

    struct target *target = bank->target;
    int retval = ERROR_OK;

    // PFLASH
    
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

    // DFLASH
    retval = target_write_u32(target, CM_DFU0_BASE + PFU_SWER0_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }  

    retval = target_write_u32(target, CM_DFU1_BASE + PFU_SWER0_OFFSET,0xFFFFFFFF);
    if(retval != ERROR_OK){
      return retval;
    }  

    return retval;
}

static int ry_wait_status_busy(struct flash_bank *bank, int timeout, unsigned int instance)
{
    uint32_t status;
    int retval = ERROR_OK;
    for(;;){
        retval = ry_get_flash_status(bank,&status, instance);
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

static int ry_wait_status_write_permit(struct flash_bank *bank, int timeout, unsigned int instance)
{
    uint32_t status;
    int retval = ERROR_OK;
    for(;;){
        retval = ry_get_flash_status(bank,&status, instance);
        if(retval != ERROR_OK){
            return retval;
        }
        if((status & PFU_FSR_SPWP) == PFU_FSR_SPWP){
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

 
static int ry_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    struct target *target = bank->target;
 
    LOG_DEBUG("ry erase: %d - %d", first, last);
 
    if(target->state != TARGET_HALTED){
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }
 
 
    int retval = ERROR_OK; 
    retval = ry_write_protection_enable(bank,0U);
    if(retval != ERROR_OK){
        return retval;
    }
    
    retval = ry_sector_write_protection_enable(bank,0);
    if(retval != ERROR_OK){
        return retval;
    }
    


    // PFLASH
    if (bank->base & PFLASH_TYPE_BIT)
    {
        LOG_INFO("This is PFLASH erasing...");

        for(unsigned int i = first ; i <= last; ++i)
        {

            /*flash memory page erase*/
            retval = target_write_u32(target,  PFU_CTRL_BASE[get_pdfu_instance(target,(bank->sectors[i].offset + (uint32_t)bank->base))] + PFU_WMR_OFFSET, FLASH_SMART_SECTOR_ERASE_MODE);
            if(retval != ERROR_OK){
                return retval;
            }
            
            retval = target_write_u32(target,  PFU_CTRL_BASE[get_pdfu_instance(target,(bank->sectors[i].offset + (uint32_t)bank->base))] + PFU_OAR_OFFSET, bank->sectors[i].offset + (uint32_t)bank->base);
            if(retval != ERROR_OK){
                return retval;
            }

            retval = target_write_u32(target,  PFU_CTRL_BASE[get_pdfu_instance(target,(bank->sectors[i].offset + (uint32_t)bank->base))] + PFU_OSR_OFFSET, PFU_OSR_OPST);
            if(retval != ERROR_OK){
                return retval;
            }
    
            //wait
            retval = ry_wait_status_busy(bank,FLASH_ERASE_TIMEOUT,get_pdfu_instance(target,(bank->sectors[i].offset + (uint32_t)bank->base)));
            if(retval != ERROR_OK){
                return retval;
            }
    
            LOG_DEBUG("ry erased page %d", i);
            bank->sectors[i].is_erased = 1;
        }
    }

    // DFLASH
    if (bank->base & DFLASH_TYPE_BIT)
    {
        LOG_INFO("This is DFLASH erasing...");
        for(unsigned int i = first ; i <= last; ++i)
        {

            /*flash memory page erase*/
            retval = target_write_u32(target,  DFU_CTRL_BASE[get_pdfu_instance(target,(bank->sectors[i].offset + (uint32_t)bank->base))] + PFU_WMR_OFFSET, FLASH_SMART_SECTOR_ERASE_MODE);
            if(retval != ERROR_OK){
                return retval;
            }
            
            retval = target_write_u32(target,  DFU_CTRL_BASE[get_pdfu_instance(target,(bank->sectors[i].offset + (uint32_t)bank->base))] + PFU_OAR_OFFSET, bank->sectors[i].offset + (uint32_t)bank->base);
            if(retval != ERROR_OK){
                return retval;
            }

            retval = target_write_u32(target,  DFU_CTRL_BASE[get_pdfu_instance(target,(bank->sectors[i].offset + (uint32_t)bank->base))] + PFU_OSR_OFFSET, PFU_OSR_OPST);
            if(retval != ERROR_OK){
                return retval;
            }
    
            //wait
            retval = ry_wait_status_busy(bank,FLASH_ERASE_TIMEOUT,get_pdfu_instance(target,(bank->sectors[i].offset + (uint32_t)bank->base)));
            if(retval != ERROR_OK){
                return retval;
            }
    
            LOG_DEBUG("ry erased page %d", i);
            bank->sectors[i].is_erased = 1;
        }
    }

    return ERROR_OK;
}
 
static int ry_protect(struct flash_bank *bank, int set, unsigned int first, unsigned int last)
{
    return ERROR_FLASH_OPER_UNSUPPORTED;
}
 
static int ry_write(struct flash_bank *bank, const uint8_t *buffer,uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
 
    LOG_DEBUG("ry flash write: 0x%x 0x%x", offset, count);
 
    if(target->state != TARGET_HALTED){
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }
    
    int retval = ERROR_OK; 
    retval = ry_write_protection_enable(bank,0U);
    if(retval != ERROR_OK){
        return retval;
    }
    
    retval = ry_sector_write_protection_enable(bank,0);
    if(retval != ERROR_OK){
        return retval;
    }
    
    
    if(offset & 0x3){
        LOG_ERROR("offset 0x%x required 4-byte alignment", offset);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }
    if(count & 0x3){
        LOG_ERROR("size 0x%x required 4-byte alignment", count);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }
 
    uint32_t addr = offset;

    if (bank->base & PFLASH_TYPE_BIT)
    {
        LOG_INFO("This is PFLASH writing...");

        for(uint32_t i = 0; i < count; i += 128)
        {


            int j = ((i *100U) / count ) + 1U;
            const char *lable = "|/-\\";
            printf("[%d%%][%c]\r", j , lable[j%4]);
            fflush(stdout);
            
            // flash memory word program

            retval = target_write_u32(target, PFU_CTRL_BASE[get_pdfu_instance(target,(addr + (uint32_t)bank->base))] + PFU_WMR_OFFSET, FLASH_SMART_SERIES_PROGRAM_MODE1);
            if(retval != ERROR_OK){
                return retval;
            }

            retval = target_write_u32(target, PFU_CTRL_BASE[get_pdfu_instance(target,(addr + (uint32_t)bank->base))] + PFU_OAR_OFFSET, (addr + (uint32_t)bank->base));
            if(retval != ERROR_OK){
                return retval;
            }


            for(uint32_t u = 0; u < 128; u += 16)
            {
                retval = ry_wait_status_write_permit(bank, FLASH_ERASE_TIMEOUT, get_pdfu_instance(target,(addr+ (uint32_t)bank->base)));
                if (retval != ERROR_OK){
                    return retval;

                }

                for(uint32_t k = 0; k < 16; k += 4)
                {
                    uint32_t word = (buffer[i+u+k]   << 0)  |
                                    (buffer[i+u+k+1] << 8)  |
                                    (buffer[i+u+k+2] << 16) |
                                    (buffer[i+u+k+3] << 24);

                    retval = target_write_u32(target, PFU_CTRL_BASE[get_pdfu_instance(target,(addr + (uint32_t)bank->base))] + PFU_PDR0_OFFSET + k, word);
                    if(retval != ERROR_OK){
                        return retval;
                    }

                }

                retval = target_write_u32(target, PFU_CTRL_BASE[get_pdfu_instance(target,(addr+ (uint32_t)bank->base))] + PFU_OSR_OFFSET,PFU_OSR_OPST);
                if(retval != ERROR_OK){
                    return retval;
                }

            }

            // wait
            retval = ry_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, get_pdfu_instance(target,(addr+ (uint32_t)bank->base)));
            if (retval != ERROR_OK)
                return retval;
            addr += 128u;
        }
 
    }

    if (bank->base & DFLASH_TYPE_BIT)
    {
        LOG_INFO("This is DFLASH writing...");

        for(uint32_t i = 0; i < count; i += 4)
        {

            int j = ((i *100U) / count ) + 1U;
            const char *lable = "|/-\\";
            printf("[%d%%][%c]\r", j , lable[j%4]);
            fflush(stdout);

            uint32_t word0 = (buffer[i]   << 0)  |
                             (buffer[i+1] << 8)  |
                             (buffer[i+2] << 16) |
                             (buffer[i+3] << 24);
                            
            LOG_DEBUG("ry flash write word 0x%x 0x%x 0x%08x", i,  addr,   word0);
    
            // flash memory word program

            retval = target_write_u32(target, DFU_CTRL_BASE[get_pdfu_instance(target,(addr + (uint32_t)bank->base))] + PFU_WMR_OFFSET, FLASH_SMART_SINGLE_PROGRAM_MODE2);
            if(retval != ERROR_OK){
                return retval;
            }

            retval = target_write_u32(target, DFU_CTRL_BASE[get_pdfu_instance(target,(addr + (uint32_t)bank->base))] + PFU_OAR_OFFSET, (addr + (uint32_t)bank->base));
            if(retval != ERROR_OK){
                return retval;
            }

            retval = target_write_u32(target, DFU_CTRL_BASE[get_pdfu_instance(target,(addr + (uint32_t)bank->base))] + PFU_PDR0_OFFSET, word0);
            if(retval != ERROR_OK){
                return retval;
            }

            retval = target_write_u32(target, DFU_CTRL_BASE[get_pdfu_instance(target,(addr+ (uint32_t)bank->base))] + PFU_OSR_OFFSET,PFU_OSR_OPST);
            if(retval != ERROR_OK){
                return retval;
            }

            // wait
            retval = ry_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, get_pdfu_instance(target,(addr+ (uint32_t)bank->base)));
            if (retval != ERROR_OK)
                return retval;
            addr += 4u;
        }
    }


    LOG_DEBUG("ry flash write success");
    return ERROR_OK;
}
 
static int ry_probe(struct flash_bank *bank)
{

    LOG_DEBUG("This is calling ry_probe 0x%x bank number",bank->bank_number);

    int sector_size = 0x0;
    bank-> size = 0x0;
    int num_sectors  = 0 ;

    if(bank->sectors){
        free(bank->sectors);
    }
 
    if(bank->bank_number == 0)
    {
      bank->bank_number = 0;  
      bank->base = 0x10000000;
      sector_size = 0x4000;
      bank-> size = 0x400000;
      num_sectors = bank->size / sector_size;

    }
    else if(bank->bank_number == 1)
    {
      bank->bank_number = 1;  
      bank->base = 0x14000000;
      sector_size = 0x4000;
      bank-> size = 0x400000;
      num_sectors = bank->size / sector_size;
    }
    else if(bank->bank_number == 2)
    {
      bank->bank_number = 2;  
      bank->base = 0x40400000;
      sector_size = 0x1000;
      bank-> size = 0x20000;
      num_sectors = bank->size / sector_size;
    }
    else if(bank->bank_number == 3)
    {
      bank->bank_number = 3;  
      bank->base = 0x40800000;
      sector_size = 0x1000;
      bank-> size = 0x20000;
      num_sectors = bank->size / sector_size;
    }
    
    else if(bank->bank_number == 4)
    {
      bank->bank_number = 4;  
      bank->base = 0x40c00000;
      sector_size = 0x1000;
      bank-> size = 0x4000;
      num_sectors = bank->size / sector_size;
    }

    bank->num_sectors = num_sectors;
    bank->sectors = malloc(sizeof(struct flash_sector) * num_sectors);
 
    for(int i = 0; i < num_sectors; ++i){
        bank->sectors[i].offset = i * sector_size;
        bank->sectors[i].size = sector_size;
        bank->sectors[i].is_erased = -1;
        bank->sectors[i].is_protected = 0;
    }

    LOG_INFO("ryx probe: %d sectors, 0x%x sectors bytes, 0x%x total, 0x%x base address", num_sectors, sector_size, bank->size, (uint32_t)bank->base);
     
    return ERROR_OK;
}
 
static int ry_auto_probe(struct flash_bank * bank)
{
    LOG_DEBUG("This is calling ry_auto_probe");
    return ry_probe(bank);
}
 
static int ry_protect_check(struct flash_bank *bank)
{
    return ERROR_OK;
}
 
static int ry_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    command_print_sameline(cmd, "ry");
    return ERROR_OK;
}
 
static int ry_mass_erase(struct flash_bank *bank, unsigned int bank_id)
{
    struct target *target = bank->target;
 
    if (target->state != TARGET_HALTED) 
    {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }
    LOG_INFO("mass erase operation on going...");
    int retval = ERROR_OK;
    retval = ry_write_protection_enable(bank, 0U);
    if(retval != ERROR_OK)
    {
    return retval;
    }
    
    retval = ry_sector_write_protection_enable(bank,0);
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
        retval = target_write_u32(target, CM_PFU0_BASE + PFU_OAR_OFFSET, PFU0_ADDR_SINGLE);
        if(retval != ERROR_OK)
        {
            return retval;
        }
        
        retval = target_write_u32(target, CM_PFU0_BASE + PFU_OSR_OFFSET,PFU_OSR_OPST);
        if(retval != ERROR_OK)
        {
            return retval;
        }
        

        retval = ry_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, 0);
        if(retval != ERROR_OK)
        {
            return retval;
        }


        retval = target_write_u32(target, CM_PFU1_BASE + PFU_WMR_OFFSET, PFU_CMD_MASS_ERASE);
        if (retval != ERROR_OK)
        {
            return retval;

        }
        
        retval = target_write_u32(target, CM_PFU1_BASE + PFU_OAR_OFFSET, PFU0_ADDR_SINGLE);
        if(retval != ERROR_OK)
        {
            return retval;
        }

        retval = target_write_u32(target, CM_PFU1_BASE + PFU_OSR_OFFSET,PFU_OSR_OPST);
        if(retval != ERROR_OK)
        {
        
            return retval;
        }
        
        retval = ry_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, 1);
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
        
        retval = target_write_u32(target, CM_PFU2_BASE + PFU_OAR_OFFSET, PFU0_ADDR_SINGLE);
        if(retval != ERROR_OK)
        {
            return retval;
        }
        

        retval = target_write_u32(target, CM_PFU2_BASE + PFU_OSR_OFFSET,PFU_OSR_OPST);

        if(retval != ERROR_OK)
        {
            return retval;
        }
        
        
        
        retval = ry_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, 2);
        if (retval != ERROR_OK)
        {
            return retval;
        }

        retval = target_write_u32(target, CM_PFU3_BASE + PFU_WMR_OFFSET, PFU_CMD_MASS_ERASE);
        if(retval != ERROR_OK)
        {
            return retval;
        }
        
        retval = target_write_u32(target, CM_PFU3_BASE + PFU_OAR_OFFSET, PFU0_ADDR_SINGLE);
        if(retval != ERROR_OK)
        {
            return retval;
        }
        
        retval = target_write_u32(target, CM_PFU3_BASE + PFU_OSR_OFFSET,PFU_OSR_OPST);
        if(retval != ERROR_OK)
        {
            return retval; 
        }
        

        retval = ry_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, 3);
        if (retval != ERROR_OK)
        {
            return retval;
        }
    }
 

    if(bank_id == 2)
    {

        retval = target_write_u32(target, CM_DFU0_BASE + PFU_WMR_OFFSET, PFU_CMD_MASS_ERASE);
        if (retval != ERROR_OK)
        {
            return retval;
        }
        
        retval = target_write_u32(target, CM_DFU0_BASE + PFU_OAR_OFFSET, DFU0_ADDR_MAP_A);
        if(retval != ERROR_OK)
        {
           return retval;
        }
        

        retval = target_write_u32(target, CM_DFU0_BASE + PFU_OSR_OFFSET,PFU_OSR_OPST);

        if(retval != ERROR_OK)
        {
            return retval;
        }
       
        retval = ry_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, 0);
        if (retval != ERROR_OK)
        {
            return retval;
        }
    }

    if(bank_id == 3)
    {

        retval = target_write_u32(target, CM_DFU1_BASE + PFU_WMR_OFFSET, PFU_CMD_MASS_ERASE);
        if (retval != ERROR_OK)
        {
            return retval;
        }
        
        retval = target_write_u32(target, CM_DFU1_BASE + PFU_OAR_OFFSET, DFU1_ADDR_MAP_A);
        if(retval != ERROR_OK)
        {
            return retval;
        }
        

        retval = target_write_u32(target, CM_DFU1_BASE + PFU_OSR_OFFSET,PFU_OSR_OPST);

        if(retval != ERROR_OK)
        {
            return retval;
        }

        retval = ry_wait_status_busy(bank, FLASH_ERASE_TIMEOUT, 1);
        if (retval != ERROR_OK)
        {
            return retval;
        }
        
    }

    LOG_INFO("mass erase operation done");
     
    return ERROR_OK;
}
 
COMMAND_HANDLER(ry_handle_mass_erase_command)
{
    if(CMD_ARGC != 1 )
        return ERROR_COMMAND_SYNTAX_ERROR;
    unsigned int bank_id = 0;
     
    COMMAND_PARSE_NUMBER(u32,CMD_ARGV[0],bank_id);
    if((bank_id != 0) && (bank_id != 1) && (bank_id != 2) && (bank_id != 3))
        return ERROR_COMMAND_SYNTAX_ERROR;    
    
    LOG_INFO("Start to do mass erase");
    struct flash_bank *bank;
    int retval = CALL_COMMAND_HANDLER(flash_command_get_bank,0, &bank);
    if(retval != ERROR_OK){
        return retval;
    }
 
    retval = ry_mass_erase(bank,bank_id);
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

 
static const struct command_registration ry_exec_command_handlers[] = {
    {
        .name = "mass_erase",
        .handler = ry_handle_mass_erase_command,
        .mode = COMMAND_EXEC,
        .usage = "bank_id",
        .help = "Erase all flash banks",
    },
    
    COMMAND_REGISTRATION_DONE
};
 
static const struct command_registration ry_command_handlers[] = {
    {
        .name = "ry",
        .mode = COMMAND_ANY,
        .help = "ry flash command group",
        .usage = "",
        .chain = ry_exec_command_handlers,
    },
    COMMAND_REGISTRATION_DONE
};
 
const struct flash_driver ry_flash = {
    .name = "ry",
    .commands = ry_command_handlers,
    .flash_bank_command = ry_flash_bank_command,
    .erase = ry_erase,
    .protect = ry_protect,
    .write = ry_write,
    .read = default_flash_read,
    .probe = ry_probe,
    .auto_probe = ry_auto_probe,
    .erase_check = default_flash_blank_check,
    .protect_check = ry_protect_check,
    .info = ry_info,
};

