

#include <linux/font.h>
#include <linux/string.h>
#include <linux/semaphore.h>

#include <mach/mt6516_typedefs.h>

#include "mtkfb_console.h"

// ---------------------------------------------------------------------------

typedef struct
{
    struct semaphore sem;

    unsigned char *fb_addr;
    unsigned int fb_width;
    unsigned int fb_height;
    unsigned int fb_bpp;
    unsigned int fg_color;
    unsigned int bg_color;
    unsigned int rows;
    unsigned int cols;
    unsigned int cursor_row;
    unsigned int cursor_col;
} MFC_CONTEXT;

// ---------------------------------------------------------------------------

#define MFC_WIDTH           (ctxt->fb_width)
#define MFC_HEIGHT          (ctxt->fb_height)
#define MFC_BPP             (ctxt->fb_bpp)
#define MFC_PITCH           (MFC_WIDTH * MFC_BPP)

#define MFC_FG_COLOR        (ctxt->fg_color)
#define MFC_BG_COLOR        (ctxt->bg_color)

#define MFC_FONT            font_vga_8x16
#define MFC_FONT_WIDTH      (MFC_FONT.width)
#define MFC_FONT_HEIGHT     (MFC_FONT.height)
#define MFC_FONT_DATA       (MFC_FONT.data)

#define MFC_ROW_SIZE        (MFC_FONT_HEIGHT * MFC_PITCH)
#define MFC_ROW_FIRST       ((BYTE*)(ctxt->fb_addr))
#define MFC_ROW_SECOND      (MFC_ROW_FIRST + MFC_ROW_SIZE)
#define MFC_ROW_LAST        (MFC_ROW_FIRST + MFC_SIZE - MFC_ROW_SIZE)
#define MFC_SIZE            (MFC_ROW_SIZE * ctxt->rows)
#define MFC_SCROLL_SIZE     (MFC_SIZE - MFC_ROW_SIZE)

#define MAKE_TWO_RGB565_COLOR(high, low)  (((low) << 16) | (high))

#define MFC_LOCK()                                                          \
    do {                                                                    \
        if (down_interruptible(&ctxt->sem)) {                               \
            printk("[MFC] ERROR: Can't get semaphore in %s()\n",            \
                   __FUNCTION__);                                           \
            ASSERT(0);                                                      \
            return MFC_STATUS_LOCK_FAIL;                                    \
        }                                                                   \
    } while (0)
    
#define MFC_UNLOCK()                                                        \
    do {                                                                    \
        up(&ctxt->sem);                                                     \
    } while (0)


// ---------------------------------------------------------------------------


static void _MFC_DrawChar(MFC_CONTEXT *ctxt, UINT32 x, UINT32 y, char c)
{
    BYTE ch = *((BYTE*)&c);
	const BYTE *cdat;
    BYTE *dest;
	INT32 rows, offset;

    int font_draw_table16[4];

    ASSERT(x <= (MFC_WIDTH - MFC_FONT_WIDTH));
    ASSERT(y <= (MFC_HEIGHT - MFC_FONT_HEIGHT));

    offset = y * MFC_PITCH + x * MFC_BPP;
    dest = (MFC_ROW_FIRST + offset);

    switch (MFC_BPP) {
    case 2:
        font_draw_table16[0] = MAKE_TWO_RGB565_COLOR(MFC_BG_COLOR, MFC_BG_COLOR);
        font_draw_table16[1] = MAKE_TWO_RGB565_COLOR(MFC_BG_COLOR, MFC_FG_COLOR);
        font_draw_table16[2] = MAKE_TWO_RGB565_COLOR(MFC_FG_COLOR, MFC_BG_COLOR);
        font_draw_table16[3] = MAKE_TWO_RGB565_COLOR(MFC_FG_COLOR, MFC_FG_COLOR);

        cdat = (const BYTE*)MFC_FONT_DATA + ch * MFC_FONT_HEIGHT;
        
        for (rows = MFC_FONT_HEIGHT; rows--; dest += MFC_PITCH)
        {
            BYTE bits = *cdat++;
        
            ((UINT32*)dest)[0] = font_draw_table16[bits >> 6];
            ((UINT32*)dest)[1] = font_draw_table16[bits >> 4 & 3];
            ((UINT32*)dest)[2] = font_draw_table16[bits >> 2 & 3];
            ((UINT32*)dest)[3] = font_draw_table16[bits & 3];
        }
        break;

    default:
        ASSERT(0);
    }
}


static void _MFC_ScrollUp(MFC_CONTEXT *ctxt)
{
    const UINT32 BG_COLOR = MAKE_TWO_RGB565_COLOR(MFC_BG_COLOR, MFC_BG_COLOR);
    
    UINT32 *ptr = (UINT32 *)MFC_ROW_LAST;
    int i = MFC_ROW_SIZE / sizeof(UINT32);

    memcpy(MFC_ROW_FIRST, MFC_ROW_SECOND, MFC_SCROLL_SIZE);
    
    while(--i >= 0) {
        *ptr ++ = BG_COLOR;
    }
}


static void _MFC_Newline(MFC_CONTEXT *ctxt)
{
    ++ ctxt->cursor_row;
    ctxt->cursor_col = 0;

    /* Check if we need to scroll the terminal */
    if (ctxt->cursor_row >= ctxt->rows) {
        /* Scroll everything up */
        _MFC_ScrollUp(ctxt);

        /* Decrement row number */
        -- ctxt->cursor_row;
    }
}


#define CHECK_NEWLINE()                     \
    do {                                    \
        if (ctxt->cursor_col >= ctxt->cols) \
            _MFC_Newline(ctxt);             \
    } while(0)

static void _MFC_Putc(MFC_CONTEXT *ctxt, const char c)
{
    CHECK_NEWLINE();

    switch (c) {
    case '\n':		/* next line */
        _MFC_Newline(ctxt);
        break;

    case '\r':		/* carriage return */
        ctxt->cursor_col = 0;
        break;

    case '\t':		/* tab 8 */
        ctxt->cursor_col += 8;
        ctxt->cursor_col &= ~0x0007;
        CHECK_NEWLINE();
        break;

    default:		/* draw the char */
        _MFC_DrawChar(ctxt,
                      ctxt->cursor_col * MFC_FONT_WIDTH,
                      ctxt->cursor_row * MFC_FONT_HEIGHT,
                      c);
        ++ ctxt->cursor_col;
        CHECK_NEWLINE();
    }
}

// ---------------------------------------------------------------------------

MFC_STATUS MFC_Open(MFC_HANDLE *handle,
                    void *fb_addr,
                    unsigned int fb_width,
                    unsigned int fb_height,
                    unsigned int fb_bpp,
                    unsigned int fg_color,
                    unsigned int bg_color)
{
    MFC_CONTEXT *ctxt = NULL;

    if (NULL == handle || NULL == fb_addr) 
        return MFC_STATUS_INVALID_ARGUMENT;

    if (fb_bpp != 2)
        return MFC_STATUS_NOT_IMPLEMENTED;  // only support RGB565

    ctxt = kzalloc(sizeof(MFC_CONTEXT), GFP_KERNEL);
    if (!ctxt) return MFC_STATUS_OUT_OF_MEMORY;

    init_MUTEX(&ctxt->sem);
    ctxt->fb_addr   = fb_addr;
    ctxt->fb_width  = fb_width;
    ctxt->fb_height = fb_height;
    ctxt->fb_bpp    = fb_bpp;
    ctxt->fg_color  = fg_color;
    ctxt->bg_color  = bg_color;
    ctxt->rows      = fb_height / MFC_FONT_HEIGHT;
    ctxt->cols      = fb_width / MFC_FONT_WIDTH;

    *handle = ctxt;

    return MFC_STATUS_OK;
}


MFC_STATUS MFC_Close(MFC_HANDLE handle)
{
    if (!handle)
        return MFC_STATUS_INVALID_ARGUMENT;

    kfree(handle);

    return MFC_STATUS_OK;
}


MFC_STATUS MFC_ResetCursor(MFC_HANDLE handle)
{
    MFC_CONTEXT *ctxt = (MFC_CONTEXT *)handle;

    if (!ctxt) 
        return MFC_STATUS_INVALID_ARGUMENT;

    MFC_LOCK();
    ctxt->cursor_row = ctxt->cursor_col = 0;
    MFC_UNLOCK();

    return MFC_STATUS_OK;
}


MFC_STATUS MFC_Print(MFC_HANDLE handle, const char *str)
{
    MFC_CONTEXT *ctxt = (MFC_CONTEXT *)handle;
	int count = 0;

    if (!ctxt || !str) 
        return MFC_STATUS_INVALID_ARGUMENT;

    MFC_LOCK();

    count = strlen(str);

	while (count--)
		_MFC_Putc(ctxt, *str++);

    MFC_UNLOCK();

    return MFC_STATUS_OK;
}

