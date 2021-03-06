

#include <mach/mt6516_typedefs.h>

#include "jpeg_drv_6516_reg.h"


kal_uint32 _jpeg_enc_int_status = 0;


void jpeg_isr_enc_lisr(void)
{
    _jpeg_enc_int_status = REG_JPEG_ENC_INTERRUPT_STATUS;

    /// clear the interrupt status register
    REG_JPEG_ENC_INTERRUPT_STATUS = 0;
}

void jpeg_drv_enc_start(void)
{
//   REG_JPEG_ENC_CTRL &= ~(JPEG_ENC_CTRL_CONT_SHOOT_BIT | JPEG_ENC_CTRL_CONT_SHOOT_ADDR_SW_BIT);
   REG_JPEG_ENC_CTRL |= (JPEG_ENC_CTRL_IN_EN_BIT | JPEG_ENC_CTRL_ENABLE_BIT);
}

void jpeg_drv_enc_reset(void)
{
   REG_JPEG_ENC_RST = 0;
   REG_JPEG_ENC_RST = 1;

   _jpeg_enc_int_status = 0;
}


void jpeg_drv_enc_set_dst_buffer_info(kal_uint32 addr, kal_uint32 stallOffset, kal_uint32 dstIndex)
{

    if(dstIndex == 0)
    {
        REG_JPEG_ENC_DEST_ADDR1  = (kal_uint32)addr;
        REG_JPEG_ENC_STALL_ADDR1 = ((kal_uint32)addr + stallOffset) & 0xFFFFFFFC;
    }
    else
    {
        REG_JPEG_ENC_DEST_ADDR2  = (kal_uint32)addr;
        REG_JPEG_ENC_STALL_ADDR2 = ((kal_uint32)addr + stallOffset) & 0xFFFFFFFC;
    }

}

void jpeg_drv_enc_set_file_format(kal_uint32 fileFormat)
{
   REG_JPEG_ENC_CTRL &= ~(JPEG_ENC_CTRL_FILE_FORMAT_BIT);
   
   if (fileFormat)
   {
      REG_JPEG_ENC_CTRL |= JPEG_DEV_ENC_FILE_FORMAT_JFIF_EXIF;
   }
}

void jpeg_drv_enc_set_mode(kal_uint32 mode, kal_uint32 frames)
{
    switch (mode)
    {
        case 0:
            REG_JPEG_ENC_FRAME_NUMBER = 0;
            REG_JPEG_ENC_CTRL &= ~JPEG_ENC_CTRL_CONT_SHOOT_BIT;            
            break;
        case 1:
            REG_JPEG_ENC_FRAME_NUMBER = frames;
            REG_JPEG_ENC_CTRL |= JPEG_ENC_CTRL_CONT_SHOOT_BIT;
            REG_JPEG_ENC_CTRL &= ~JPEG_ENC_CTRL_CONT_SHOOT_ADDR_SW_BIT;
            break;
        case 2:
            REG_JPEG_ENC_FRAME_NUMBER = frames;
            REG_JPEG_ENC_CTRL |= JPEG_ENC_CTRL_CONT_SHOOT_BIT;
            REG_JPEG_ENC_CTRL |= JPEG_ENC_CTRL_CONT_SHOOT_ADDR_SW_BIT;
            break;
    }
}

void jpeg_drv_enc_set_quality(kal_uint32 quality)
{
    REG_JPEG_ENC_QUALITY = quality;
}


kal_uint32 jpeg_drv_enc_set_sample_format_related(kal_uint32 width, kal_uint32 height, kal_uint32 samplingFormat)
{   
   REG_JPEG_ENC_CTRL &= ~(JPEG_ENC_CTRL_YUV_BIT | JPEG_ENC_CTRL_GRAY_BIT);

   switch (samplingFormat)
   {
   case 422:
      REG_JPEG_ENC_CTRL |= JPEG_DRV_ENC_YUV422;
      REG_JPEG_ENC_BLK_NUM = ((width + 15) >> 4) * ((height + 7) >> 3) * 4 - 1;
      break;

   case 420:
      REG_JPEG_ENC_CTRL |= JPEG_DRV_ENC_YUV420;
      REG_JPEG_ENC_BLK_NUM = ((width + 15) >> 4) * ((height + 15) >> 4) * 6 - 1;
      break;
      
   case 411:
      REG_JPEG_ENC_CTRL |= JPEG_DRV_ENC_YUV411;
      REG_JPEG_ENC_BLK_NUM = ((width + 31) >> 5) * ((height + 7) >> 3) * 6 - 1;
      break;

   case 400:
      REG_JPEG_ENC_CTRL |= JPEG_DRV_ENC_YUV400;
      REG_JPEG_ENC_BLK_NUM = ((width + 7) >> 3) * ((height + 7) >> 3) - 1;
      break;

   default:
      return 1;
   }
   return 0;
}


kal_uint32 jpeg_drv_enc_query_required_memory(kal_uint32 *intMemory, kal_uint32 *extMemory)
{
   /// JPEG HW encoder need no memory.

   *intMemory = 0;
   *extMemory = 0;
   
   return 1;
}


kal_uint32 jpeg_drv_enc_get_dma_addr(kal_uint8 index)
{
    if(index == 0)
        return REG_JPEG_ENC_DMA_ADDR1;

    return REG_JPEG_ENC_DMA_ADDR2;
}

kal_uint32 jpeg_drv_enc_get_dst_addr(kal_uint8 index)
{
    if(index == 0)
        return REG_JPEG_ENC_DEST_ADDR1;

    return REG_JPEG_ENC_DEST_ADDR2;
}

kal_uint32 jpeg_drv_enc_get_file_size(kal_uint8 index)
{
    return jpeg_drv_enc_get_dma_addr(index) - jpeg_drv_enc_get_dst_addr(index);
}

kal_uint32 jpeg_drv_enc_get_result(kal_uint32 *fileSize)
{
    *fileSize = jpeg_drv_enc_get_file_size(0);

    if(_jpeg_enc_int_status & JPEG_DRV_ENC_INT_STATUS_DONE)
    {
        return 0;
    }
    else if(_jpeg_enc_int_status & JPEG_DRV_ENC_INT_STATUS_STALL)
    {
        return 1;
    }

    return 2;
}

kal_uint8 jpeg_drv_enc_get_current_frame(void)
{
    return REG_JPEG_ENC_CURR_FRAME_CNT & 1;
}

//#endif /// #if defined(__DRV_GRAPHICS_JPEG_3351_SERIES__) && defined(__MTK_TARGET__)
