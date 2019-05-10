#ifndef HARDDOOM2_H
#define HARDDOOM2_H

/* Section 1: PCI ids. */

#define HARDDOOM2_VENDOR_ID				0x0666
#define HARDDOOM2_DEVICE_ID				0x1994

/* Section 2: MMIO registers.  */

/* Section 2.1: main control area.  */

/* Enables active units of the device.  TLB is passive and doesn't have
 * an enable (disable XY or TEX instead).  FIFOs also don't have enables
 * -- disable the source and/or destination unit instead.  */
#define HARDDOOM2_ENABLE				0x0000
#define HARDDOOM2_ENABLE_CMD_FETCH			0x00000001
#define HARDDOOM2_ENABLE_CMD_SEND			0x00000002
#define HARDDOOM2_ENABLE_FE				0x00000004
#define HARDDOOM2_ENABLE_XY				0x00000008
#define HARDDOOM2_ENABLE_TEX				0x00000010
#define HARDDOOM2_ENABLE_FLAT				0x00000020
#define HARDDOOM2_ENABLE_FUZZ				0x00000040
#define HARDDOOM2_ENABLE_SR				0x00000080
#define HARDDOOM2_ENABLE_OG				0x00000100
#define HARDDOOM2_ENABLE_SW				0x00000200
#define HARDDOOM2_ENABLE_ALL				0x000003ff
/* Status of device units -- 1 means they have work to do.  */
#define HARDDOOM2_STATUS				0x0004
#define HARDDOOM2_STATUS_CMD_FETCH			0x00000001
#define HARDDOOM2_STATUS_FE				0x00000004
#define HARDDOOM2_STATUS_XY				0x00000008
#define HARDDOOM2_STATUS_TEX				0x00000010
#define HARDDOOM2_STATUS_FLAT				0x00000020
#define HARDDOOM2_STATUS_FUZZ				0x00000040
#define HARDDOOM2_STATUS_SR				0x00000080
#define HARDDOOM2_STATUS_OG				0x00000100
#define HARDDOOM2_STATUS_SW				0x00000200
#define HARDDOOM2_STATUS_FIFO_FECMD			0x00010000
#define HARDDOOM2_STATUS_FIFO_XYCMD			0x00020000
#define HARDDOOM2_STATUS_FIFO_TEXCMD			0x00040000
#define HARDDOOM2_STATUS_FIFO_FLATCMD			0x00080000
#define HARDDOOM2_STATUS_FIFO_FUZZCMD			0x00100000
#define HARDDOOM2_STATUS_FIFO_OGCMD			0x00200000
#define HARDDOOM2_STATUS_FIFO_SWCMD			0x00400000
#define HARDDOOM2_STATUS_FIFO_XYOUTR			0x01000000
#define HARDDOOM2_STATUS_FIFO_XYOUTW			0x02000000
#define HARDDOOM2_STATUS_FIFO_SROUT			0x04000000
#define HARDDOOM2_STATUS_FIFO_TEXOUT			0x08000000
#define HARDDOOM2_STATUS_FIFO_FLATOUT			0x10000000
#define HARDDOOM2_STATUS_FIFO_FUZZOUT			0x20000000
#define HARDDOOM2_STATUS_FIFO_OGOUT			0x40000000
#define HARDDOOM2_STATUS_FIFO_XYSYNC			0x80000000
/* The reset register.  Punching 1 will clear all pending work.  There is
 * no reset for CMD_FETCH (initialize CMD_*_PTR instead).  */
#define HARDDOOM2_RESET					0x0004
#define HARDDOOM2_RESET_FE				0x00000004
#define HARDDOOM2_RESET_XY				0x00000008
#define HARDDOOM2_RESET_TEX				0x00000010
#define HARDDOOM2_RESET_FLAT				0x00000020
#define HARDDOOM2_RESET_FUZZ				0x00000040
#define HARDDOOM2_RESET_SR				0x00000080
#define HARDDOOM2_RESET_OG				0x00000100
#define HARDDOOM2_RESET_SW				0x00000200
#define HARDDOOM2_RESET_STATS				0x00000400
#define HARDDOOM2_RESET_TLB				0x00000800
#define HARDDOOM2_RESET_TEX_CACHE			0x00001000
#define HARDDOOM2_RESET_FLAT_CACHE			0x00002000
#define HARDDOOM2_RESET_SW_CACHE			0x00004000
#define HARDDOOM2_RESET_FIFO_FECMD			0x00010000
#define HARDDOOM2_RESET_FIFO_XYCMD			0x00020000
#define HARDDOOM2_RESET_FIFO_TEXCMD			0x00040000
#define HARDDOOM2_RESET_FIFO_FLATCMD			0x00080000
#define HARDDOOM2_RESET_FIFO_FUZZCMD			0x00100000
#define HARDDOOM2_RESET_FIFO_OGCMD			0x00200000
#define HARDDOOM2_RESET_FIFO_SWCMD			0x00400000
#define HARDDOOM2_RESET_FIFO_XYOUTR			0x01000000
#define HARDDOOM2_RESET_FIFO_XYOUTW			0x02000000
#define HARDDOOM2_RESET_FIFO_SROUT			0x04000000
#define HARDDOOM2_RESET_FIFO_TEXOUT			0x08000000
#define HARDDOOM2_RESET_FIFO_FLATOUT			0x10000000
#define HARDDOOM2_RESET_FIFO_FUZZOUT			0x20000000
#define HARDDOOM2_RESET_FIFO_OGOUT			0x40000000
#define HARDDOOM2_RESET_FIFO_XYSYNC			0x80000000
#define HARDDOOM2_RESET_ALL				0xff7f7ffc
/* Interrupt status.  */
#define HARDDOOM2_INTR					0x0008
#define HARDDOOM2_INTR_FENCE				0x00000001
#define HARDDOOM2_INTR_PONG_SYNC			0x00000002
#define HARDDOOM2_INTR_PONG_ASYNC			0x00000004
#define HARDDOOM2_INTR_FE_ERROR				0x00000010
#define HARDDOOM2_INTR_CMD_OVERFLOW			0x00000020
#define HARDDOOM2_INTR_SURF_DST_OVERFLOW		0x00000040
#define HARDDOOM2_INTR_SURF_SRC_OVERFLOW		0x00000080
#define HARDDOOM2_INTR_PAGE_FAULT(i)			(0x00000100 << (i))
#define HARDDOOM2_INTR_PAGE_FAULT_CMD			0x00000100
#define HARDDOOM2_INTR_PAGE_FAULT_SURF_DST		0x00000200
#define HARDDOOM2_INTR_PAGE_FAULT_SURF_SRC		0x00000400
#define HARDDOOM2_INTR_PAGE_FAULT_TEXTURE		0x00000800
#define HARDDOOM2_INTR_PAGE_FAULT_FLAT			0x00001000
#define HARDDOOM2_INTR_PAGE_FAULT_TRANSLATION		0x00002000
#define HARDDOOM2_INTR_PAGE_FAULT_COLORMAP		0x00004000
#define HARDDOOM2_INTR_PAGE_FAULT_TRANMAP		0x00008000
#define HARDDOOM2_INTR_MASK				0x0000fff7
/* And enable (same bitfields).  */
#define HARDDOOM2_INTR_ENABLE				0x000c
/* The counter of processed FENCE commands.  */
#define HARDDOOM2_FENCE_COUNTER				0x0010
/* The value that will trigger a FENCE interrupt when reached by FENCE_COUNTER.  */
#define HARDDOOM2_FENCE_WAIT				0x0014


/* Section 2.2: CMD -- command processor.  */

/* Direct command submission (goes to FIFO bypassing CMD_FETCH).  */
#define HARDDOOM2_CMD_SEND(i)				(0x0040 + (i) * 4)
#define HARDDOOM2_CMD_SEND_SIZE				8
/* The page table pointer (writes will be passed through to TLB).  */
#define HARDDOOM2_CMD_PT				0x0060
/* The command buffer size, in one-command units (max 0x20000 commands, or 4MiB).  */
#define HARDDOOM2_CMD_SIZE				0x0064
#define HARDDOOM2_CMD_SIZE_MASK				0x0003ffff
/* Command read index -- whenever not equal to CMD_WRITE_IDX, CMD_FETCH will
 * fetch command from here and increment.  */
#define HARDDOOM2_CMD_READ_IDX				0x0068
/* Command write index -- CMD_FETCH halts when it hits this index.  */
#define HARDDOOM2_CMD_WRITE_IDX				0x006c
#define HARDDOOM2_CMD_IDX_MASK				0x0001ffff
/* Read-only number of free slots in FIFO -- how many commands can be sent by
 * CMD_SEND immediately.  */
#define HARDDOOM2_CMD_FREE				0x0070


/* Section 2.3: TLB.  Called by the FE, XY and TEX units to translate virtual
 * addresses to physical.  */

/* The 8 buffers.  */
#define HARDDOOM2_TLB_IDX_CMD				0
#define HARDDOOM2_TLB_IDX_SURF_DST			1
#define HARDDOOM2_TLB_IDX_SURF_SRC			2
#define HARDDOOM2_TLB_IDX_TEXTURE			3
#define HARDDOOM2_TLB_IDX_FLAT				4
#define HARDDOOM2_TLB_IDX_TRANSLATION			5
#define HARDDOOM2_TLB_IDX_COLORMAP			6
#define HARDDOOM2_TLB_IDX_TRANMAP			7
#define HARDDOOM2_TLB_NUM				8

/* The current page table for each buffer.  */
#define HARDDOOM2_TLB_PT(i)				(0x0080 + (i) * 4)
#define HARDDOOM2_TLB_PT_CMD				0x0080
#define HARDDOOM2_TLB_PT_SURF_DST			0x0084
#define HARDDOOM2_TLB_PT_SURF_SRC			0x0088
#define HARDDOOM2_TLB_PT_TEXTURE			0x008c
#define HARDDOOM2_TLB_PT_FLAT				0x0090
#define HARDDOOM2_TLB_PT_TRANSLATION			0x0094
#define HARDDOOM2_TLB_PT_COLORMAP			0x0098
#define HARDDOOM2_TLB_PT_TRANMAP			0x009c
/* The single-entry TLBs, one for each paged buffer.  Bits 0, 1 and 12-31 are taken straight
 * from the PTE.  Bit 2 is the valid bit.  */
#define HARDDOOM2_TLB_ENTRY(i)				(0x00a0 + (i) * 4)
#define HARDDOOM2_TLB_ENTRY_CMD				0x00a0
#define HARDDOOM2_TLB_ENTRY_SURF_DST			0x00a4
#define HARDDOOM2_TLB_ENTRY_SURF_SRC			0x00a8
#define HARDDOOM2_TLB_ENTRY_TEXTURE			0x00ac
#define HARDDOOM2_TLB_ENTRY_FLAT			0x00b0
#define HARDDOOM2_TLB_ENTRY_TRANSLATION			0x00b4
#define HARDDOOM2_TLB_ENTRY_COLORMAP			0x00b8
#define HARDDOOM2_TLB_ENTRY_TRANMAP			0x00bc
#define HARDDOOM2_TLB_ENTRY_VALID			0x00000004
#define HARDDOOM2_TLB_ENTRY_MASK			0xfffffff7
/* The last virtual address looked up in the TLB -- useful in case of
 * page faults.  */
#define HARDDOOM2_TLB_VADDR(i)				(0x00c0 + (i) * 4)
#define HARDDOOM2_TLB_VADDR_CMD				0x00c0
#define HARDDOOM2_TLB_VADDR_SURF_DST			0x00c4
#define HARDDOOM2_TLB_VADDR_SURF_SRC			0x00c8
#define HARDDOOM2_TLB_VADDR_TEXTURE			0x00cc
#define HARDDOOM2_TLB_VADDR_FLAT			0x00d0
#define HARDDOOM2_TLB_VADDR_TRANSLATION			0x00d4
#define HARDDOOM2_TLB_VADDR_COLORMAP			0x00d8
#define HARDDOOM2_TLB_VADDR_TRANMAP			0x00dc
#define HARDDOOM2_TLB_VADDR_MASK			0x003fffc0
#define HARDDOOM2_TLB_PTE_TAG_MASK			0x000003ff
#define HARDDOOM2_TLB_PTE_TAG_SHIFT			12


/* Section 2.4: FE -- the Front End unit.  Its task is to digest driver
 * commands into simple commands for individual blocks (XY, TEX, FLAT,
 * FUZZ, OG).  Since this is quite a complicated task, it is a microcoded engine.  */

/* Front End microcode window.  Any access to _DATA will access the code RAM
 * cell selected by _ADDR and bump _ADDR by one.  Each cell is one 30-bit
 * instruction.  */
#define HARDDOOM2_FE_CODE_ADDR				0x0100
#define HARDDOOM2_FE_CODE_WINDOW			0x0104
#define HARDDOOM2_FE_CODE_SIZE				0x00001000
/* The FE data memory window -- behaves like the code window.  Used by
 * the microcode to store data arrays.  */
#define HARDDOOM2_FE_DATA_ADDR				0x0108
#define HARDDOOM2_FE_DATA_WINDOW			0x010c
#define HARDDOOM2_FE_DATA_SIZE				0x00000200
/* Front End error reporting -- when the microcode detects an error in the
 * command stream, it triggers the FE_ERROR interrupt, writes the offending
 * command to _CMD, and writes the error code to _CODE.  */
#define HARDDOOM2_FE_ERROR_CODE				0x0110
/* Unknown command type.  */
#define HARDDOOM2_FE_ERROR_CODE_RESERVED_TYPE		0x00000000
/* Known command type with non-0 value in reserved bits.  */
#define HARDDOOM2_FE_ERROR_CODE_RESERVED_BITS		0x00000001
/* SURF_*_WIDTH == 0.  */
#define HARDDOOM2_FE_ERROR_CODE_SURF_WIDTH_ZERO		0x00000002
/* SURF_*_WIDTH > 2048.  */
#define HARDDOOM2_FE_ERROR_CODE_SURF_WIDTH_OVF		0x00000003
/* DRAW_COLUMN Y_A > Y_B.  */
#define HARDDOOM2_FE_ERROR_CODE_DRAW_COLUMN_REV		0x00000004
/* DRAW_COLUMN Y_A > Y_B or out of fuzz bonds.  */
#define HARDDOOM2_FE_ERROR_CODE_DRAW_FUZZ_REV		0x00000005
/* DRAW_SPAN X_A > X_B.  */
#define HARDDOOM2_FE_ERROR_CODE_DRAW_SPAN_REV		0x00000006
#define HARDDOOM2_FE_ERROR_CODE_MASK			0x00000fff
/* The FE state -- microcode program counter and "waiting for FIFO" flag.  */
#define HARDDOOM2_FE_STATE				0x0114
#define HARDDOOM2_FE_STATE_PC_MASK			0x00000fff
#define HARDDOOM2_FE_STATE_WAIT_FIFO			0x00001000
#define HARDDOOM2_FE_STATE_MASK				0x00001fff
/* The FE registers, for use as variables by the microcode.  */
#define HARDDOOM2_FE_REG(i)				(0x0180 + (i) * 4)
#define HARDDOOM2_FE_REG_NUM				0x20


/* Section 2.5: FIFO -- various internal command and data queues of the device.  */

/* Every FIFO has internal state consisting of a state register and the FIFO data array.
 * There are 2**X entries, indexed by (X+1)-bit indices (each entry is visible
 * under two indices).  Bits 0..X of state is read pointer (index of the next entry to
 * be read by destination), 16..(16+X) is write pointer (index of the next entry to be
 * written by source).  FIFO is empty iff read == write, full iff read ==
 * write ^ (1 << X).  Situations where ((write - read) % 2 ** (X + 1)) > (2 ** X)
 * are illegal and won't be reached in proper operation of the device.
 * Every FIFO also has window registers that can access the FIFO data.  When read,
 * it reads from READ_PTR, and increments it.  When written, writes to WRITE_PTR, and
 * increments it.  If this causes a FIFO overflow/underflow, so be it.  */

/* The SR -> OG FIFO.  Each element is a 64-pixel block.  */
#define HARDDOOM2_FIFO_SROUT_DATA_WINDOW		0x0200
#define HARDDOOM2_FIFO_SROUT_STATE			0x0320
#define HARDDOOM2_FIFO_SROUT_SIZE			0x00000020
/* The FLAT -> OG FIFO.  Each element is a 64-pixel block.  */
#define HARDDOOM2_FIFO_FLATOUT_DATA_WINDOW		0x0240
#define HARDDOOM2_FIFO_FLATOUT_STATE			0x0324
#define HARDDOOM2_FIFO_FLATOUT_SIZE			0x00000020
/* The TEX -> OG FIFO.  Each element is a 64-pixel block + 64-bit mask.  */
#define HARDDOOM2_FIFO_TEXOUT_DATA_WINDOW		0x0280
#define HARDDOOM2_FIFO_TEXOUT_MASK_WINDOW		0x0300
#define HARDDOOM2_FIFO_TEXOUT_STATE			0x0328
#define HARDDOOM2_FIFO_TEXOUT_SIZE			0x00000020
/* The OG -> SW FIFO.  Each element is a 64-pixel block + 64-bit mask.  */
#define HARDDOOM2_FIFO_OGOUT_DATA_WINDOW		0x02c0
#define HARDDOOM2_FIFO_OGOUT_MASK_WINDOW		0x0308
#define HARDDOOM2_FIFO_OGOUT_STATE			0x032c
#define HARDDOOM2_FIFO_OGOUT_SIZE			0x00000020
/* The FUZZ -> OG FIFO.  Each element is a 64-bit mask.  */
#define HARDDOOM2_FIFO_FUZZOUT_MASK_WINDOW		0x0310
#define HARDDOOM2_FIFO_FUZZOUT_STATE			0x0330
#define HARDDOOM2_FIFO_FUZZOUT_SIZE			0x00000020

/* The XY -> SR FIFO (with physical addresses to be read by SR and sent
 * to OG for processing).  Each element is a 64-bit address.  */
#define HARDDOOM2_FIFO_XYOUTR_DATA_WINDOW		0x0340
#define HARDDOOM2_FIFO_XYOUTR_STATE			0x0350
#define HARDDOOM2_FIFO_XYOUTR_SIZE			0x00000080
/* The XY -> SW FIFO (with physical addresses to be written by SW with data
 * prepared by OG).  Each element is a 64-bit address.  */
#define HARDDOOM2_FIFO_XYOUTW_DATA_WINDOW		0x0348
#define HARDDOOM2_FIFO_XYOUTW_STATE			0x0354
#define HARDDOOM2_FIFO_XYOUTW_SIZE			0x00000080

/* The SW -> XY pseudo-FIFO used for INTERLOCKs.  There is no actual data
 * payload being transmitted -- the FIFO state is simply the number of
 * INTERLOCKs sent by SW and not yet received by XY.  */
#define HARDDOOM2_FIFO_XYSYNC_STATE			0x0358
#define HARDDOOM2_FIFO_XYSYNC_STATE_MASK		0x000000ff

/* The CMD -> FE FIFO.  Each element is an 8-word command.  */
#define HARDDOOM2_FIFO_FECMD_STATE			0x035c
#define HARDDOOM2_FIFO_FECMD_DATA_WINDOW		0x0360
#define HARDDOOM2_FIFO_FECMD_SIZE			0x00000200

/* The per-unit command FIFOs.  Each element consists of a command type and data.  */

/* FE -> XY.  */
#define HARDDOOM2_FIFO_XYCMD_CMD_WINDOW			0x0380
#define HARDDOOM2_FIFO_XYCMD_DATA_WINDOW		0x0384
#define HARDDOOM2_FIFO_XYCMD_STATE			0x0388
#define HARDDOOM2_FIFO_XYCMD_SIZE			0x00000080
/* FE -> TEX.  */
#define HARDDOOM2_FIFO_TEXCMD_CMD_WINDOW		0x0390
#define HARDDOOM2_FIFO_TEXCMD_DATA_WINDOW		0x0394
#define HARDDOOM2_FIFO_TEXCMD_STATE			0x0398
#define HARDDOOM2_FIFO_TEXCMD_SIZE			0x00000080
/* FE -> FUZZ.  */
#define HARDDOOM2_FIFO_FUZZCMD_CMD_WINDOW		0x03a0
#define HARDDOOM2_FIFO_FUZZCMD_DATA_WINDOW		0x03a4
#define HARDDOOM2_FIFO_FUZZCMD_STATE			0x03a8
#define HARDDOOM2_FIFO_FUZZCMD_SIZE			0x00000080
/* FE -> FLAT.  */
#define HARDDOOM2_FIFO_FLATCMD_CMD_WINDOW		0x03b0
#define HARDDOOM2_FIFO_FLATCMD_DATA_WINDOW		0x03b4
#define HARDDOOM2_FIFO_FLATCMD_STATE			0x03b8
#define HARDDOOM2_FIFO_FLATCMD_SIZE			0x00000080
/* FE -> OG.  */
#define HARDDOOM2_FIFO_OGCMD_CMD_WINDOW			0x03c0
#define HARDDOOM2_FIFO_OGCMD_DATA_WINDOW		0x03c4
#define HARDDOOM2_FIFO_OGCMD_STATE			0x03c8
#define HARDDOOM2_FIFO_OGCMD_SIZE			0x00000080
/* OG -> SW.  */
#define HARDDOOM2_FIFO_SWCMD_CMD_WINDOW			0x03d0
#define HARDDOOM2_FIFO_SWCMD_DATA_WINDOW		0x03d4
#define HARDDOOM2_FIFO_SWCMD_STATE			0x03d8
#define HARDDOOM2_FIFO_SWCMD_SIZE			0x00000080


/* Section 2.6: STATS.  */

/* The indices are listed below.  */
#define HARDDOOM2_STATS(i)				(0x0400 + (i) * 4)
#define HARDDOOM2_STATS_NUM				0x60

/* The FE stats (counted by microcode).  */
/* A COPY_RECT command was processed as a series of horizontally drawn lines.  */
#define HARDDOOM2_STAT_FE_COPY_RECT_HORIZONTAL		0x00
/* A line was drawn as part of the above.  */
#define HARDDOOM2_STAT_FE_COPY_RECT_LINE		0x01
/* A COPY_RECT command was processed as a vertical series of blocks.  */
#define HARDDOOM2_STAT_FE_COPY_RECT_VERTICAL		0x02
/* Like the above 3, but for FILL_RECT.  */
#define HARDDOOM2_STAT_FE_FILL_RECT_HORIZONTAL		0x03
#define HARDDOOM2_STAT_FE_FILL_RECT_LINE		0x04
#define HARDDOOM2_STAT_FE_FILL_RECT_VERTICAL		0x05
/* A DRAW_BACKGROUND command was processed.  */
#define HARDDOOM2_STAT_FE_DRAW_BACKGROUND		0x06
/* A line was drawn as part of DRAW_BACKGROUND.  */
#define HARDDOOM2_STAT_FE_DRAW_BACKGROUND_LINE		0x07
/* A DRAW_LINE command was processed as mostly-horizontal.  */
#define HARDDOOM2_STAT_FE_DRAW_LINE_HORIZONTAL		0x08
/* Ditto, mostly-vertical.  */
#define HARDDOOM2_STAT_FE_DRAW_LINE_VERTICAL		0x09
/* A chunk of pixels (X×1 rectangle) was drawn for
 * a mostly-horizontal line.  */
#define HARDDOOM2_STAT_FE_DRAW_LINE_H_CHUNK		0x0a
/* A chunk of pixels (1×X rectangle) was drawn for
 * a mostly-vertical line.  */
#define HARDDOOM2_STAT_FE_DRAW_LINE_V_CHUNK		0x0b
/* A pixel was drawn for a mostly-horizontal line.  */
#define HARDDOOM2_STAT_FE_DRAW_LINE_H_PIXEL		0x0c
/* A pixel was drawn for a mostly-vertical line.  */
#define HARDDOOM2_STAT_FE_DRAW_LINE_V_PIXEL		0x0d
/* A batch of DRAW_COLUMN commands was processed.  */
#define HARDDOOM2_STAT_FE_DRAW_COLUMN_BATCH		0x0e
/* A batch of DRAW_FUZZ commands was processed.  */
#define HARDDOOM2_STAT_FE_DRAW_FUZZ_BATCH		0x0f
/* A DRAW_COLUMN command was processed.  */
#define HARDDOOM2_STAT_FE_DRAW_COLUMN			0x10
/* A DRAW_FUZZ command was processed.  */
#define HARDDOOM2_STAT_FE_DRAW_FUZZ			0x11
/* A vertical chunk of a DRAW_COLUMN batch was processed.  */
#define HARDDOOM2_STAT_FE_DRAW_COLUMN_CHUNK		0x12
/* A vertical chunk of a DRAW_FUZZ batch was processed.  */
#define HARDDOOM2_STAT_FE_DRAW_FUZZ_CHUNK		0x13
/* A command with PING_ASYNC was processed.  */
#define HARDDOOM2_STAT_FE_PING_ASYNC			0x14
/* The current flat was reloaded.  */
#define HARDDOOM2_STAT_FE_LOAD_FLAT			0x15
/* The current translation was reloaded.  */
#define HARDDOOM2_STAT_FE_LOAD_TRANSLATION		0x16
/* The current colormap was reloaded.  */
#define HARDDOOM2_STAT_FE_LOAD_COLORMAP			0x17
/* A batch was terminated because of mismatched colormap index.  */
#define HARDDOOM2_STAT_FE_BATCH_END_MISMATCH_COLORMAP	0x18
/* A batch was terminated because of mismatched texture dimensions.  */
#define HARDDOOM2_STAT_FE_BATCH_END_MISMATCH_TEX_DIMS	0x19
/* A batch was terminated because a different command type was submitted.  */
#define HARDDOOM2_STAT_FE_BATCH_END_MISMATCH_CMD	0x1a
/* A batch was terminated because of mismatched fuzz area.  */
#define HARDDOOM2_STAT_FE_BATCH_END_MISMATCH_FUZZ	0x1b
/* A batch was terminated because maximum batch size was reached.  */
#define HARDDOOM2_STAT_FE_BATCH_END_SIZE		0x1c
/* A batch was terminated because X/Y coordinates went in an unexpected direction.  */
#define HARDDOOM2_STAT_FE_BATCH_END_XY			0x1d
/* A batch was terminated because of a synchronization flag.  */
#define HARDDOOM2_STAT_FE_BATCH_END_SYNC		0x1e
/* A DRAW_SPAN command was processed.  */
#define HARDDOOM2_STAT_FE_DRAW_SPAN			0x1f

/* TLB statistics.  */
/* TLB hits and misses.  */
#define HARDDOOM2_STAT_TLB_HIT(i)			(0x20 + (i))
#define HARDDOOM2_STAT_TLB_CMD_HIT			0x20
#define HARDDOOM2_STAT_TLB_SURF_DST_HIT			0x21
#define HARDDOOM2_STAT_TLB_SURF_SRC_HIT			0x22
#define HARDDOOM2_STAT_TLB_TEXTURE_HIT			0x23
#define HARDDOOM2_STAT_TLB_FLAT_HIT			0x24
#define HARDDOOM2_STAT_TLB_TRANSLATION_HIT		0x25
#define HARDDOOM2_STAT_TLB_COLORMAP_HIT			0x26
#define HARDDOOM2_STAT_TLB_TRANMAP_HIT			0x27
#define HARDDOOM2_STAT_TLB_MISS(i)			(0x28 + (i))
#define HARDDOOM2_STAT_TLB_CMD_MISS			0x28
#define HARDDOOM2_STAT_TLB_SURF_DST_MISS		0x29
#define HARDDOOM2_STAT_TLB_SURF_SRC_MISS		0x2a
#define HARDDOOM2_STAT_TLB_TEXTURE_MISS			0x2b
#define HARDDOOM2_STAT_TLB_FLAT_MISS			0x2c
#define HARDDOOM2_STAT_TLB_TRANSLATION_MISS		0x2d
#define HARDDOOM2_STAT_TLB_COLORMAP_MISS		0x2e
#define HARDDOOM2_STAT_TLB_TRANMAP_MISS			0x2f
/* Active PT change events.  */
#define HARDDOOM2_STAT_TLB_CHANGE(i)			(0x30 + (i))
#define HARDDOOM2_STAT_TLB_CMD_CHANGE			0x30
#define HARDDOOM2_STAT_TLB_SURF_DST_CHANGE		0x31
#define HARDDOOM2_STAT_TLB_SURF_SRC_CHANGE		0x32
#define HARDDOOM2_STAT_TLB_TEXTURE_CHANGE		0x33
#define HARDDOOM2_STAT_TLB_FLAT_CHANGE			0x34
#define HARDDOOM2_STAT_TLB_TRANSLATION_CHANGE		0x35
#define HARDDOOM2_STAT_TLB_COLORMAP_CHANGE		0x36
#define HARDDOOM2_STAT_TLB_TRANMAP_CHANGE		0x37

/* TEX statistics.  */
/* A texture cache hit on the currently textured pixel.  */
#define HARDDOOM2_STAT_TEX_CACHE_HIT			0x38
/* A texture cache hit on a speculative pixel (causing it to be pre-textured).  */
#define HARDDOOM2_STAT_TEX_CACHE_SPEC_HIT		0x39
/* A texture cache miss on the currently textured pixel (ie. a cache fill).  */
#define HARDDOOM2_STAT_TEX_CACHE_MISS			0x3a
/* A texture cache miss on a speculative pixel (no cache fill).  */
#define HARDDOOM2_STAT_TEX_CACHE_SPEC_MISS		0x3b

/* FLAT statistics.  */
/* A block was sent to OG by the DRAW_SPAN command.  */
#define HARDDOOM2_STAT_FLAT_SPAN_BLOCK			0x3c
/* A pixel was textured by the DRAW_SPAN command.  */
#define HARDDOOM2_STAT_FLAT_SPAN_PIXEL			0x3d
/* A flat cache hit.  */
#define HARDDOOM2_STAT_FLAT_CACHE_HIT			0x3e
/* A flat cache miss (and fill).  */
#define HARDDOOM2_STAT_FLAT_CACHE_MISS			0x3f

/* OG statistics.  */
/* A block was sent to SW by the DRAW_BUF_* command.  */
#define HARDDOOM2_STAT_OG_DRAW_BUF_BLOCK		0x40
/* A pixel was sent to SW by the DRAW_BUF_* command.  */
#define HARDDOOM2_STAT_OG_DRAW_BUF_PIXEL		0x41
/* A block was sent to SW by the COPY_* command.  */
#define HARDDOOM2_STAT_OG_COPY_BLOCK			0x42
/* A pixel was sent to SW by the COPY_* command.  */
#define HARDDOOM2_STAT_OG_COPY_PIXEL			0x43
/* A block was processed by the translation color map.  */
#define HARDDOOM2_STAT_OG_TRANSLATION_BLOCK		0x44
/* A block was processed by the main color map.  */
#define HARDDOOM2_STAT_OG_COLORMAP_BLOCK		0x45
/* A pixel was processed by the translation color map.  */
#define HARDDOOM2_STAT_OG_TRANSLATION_PIXEL		0x46
/* A pixel was processed by the main color map.  */
#define HARDDOOM2_STAT_OG_COLORMAP_PIXEL		0x47
/* A pixel was sent to SW by the DRAW_FUZZ command.  */
#define HARDDOOM2_STAT_OG_FUZZ_PIXEL			0x48

/* SW statistics.  */
/* A contiguous group of pixels was written to memory by SW.  */
#define HARDDOOM2_STAT_SW_XFER				0x49
/* A FENCE command was processed by SW.  */
#define HARDDOOM2_STAT_SW_FENCE				0x4a
/* A FENCE command was processed by SW.  */
#define HARDDOOM2_STAT_SW_PING_SYNC			0x4b
/* A block was processed by the transparency map.  */
#define HARDDOOM2_STAT_SW_TRANMAP_BLOCK			0x4c
/* A pixel was processed by the transparancy map.  */
#define HARDDOOM2_STAT_SW_TRANMAP_PIXEL			0x4d
/* A transparency map cache hit.  */
#define HARDDOOM2_STAT_SW_TRANMAP_HIT			0x4e
/* A transparency map cache miss (and fill).  */
#define HARDDOOM2_STAT_SW_TRANMAP_MISS			0x4f

/* Various FIFO flow statistics.  */
/* A command was sent to FE (from CMD_SEND or CMD_FETCH).  */
#define HARDDOOM2_STAT_FIFO_FECMD			0x50
/* A command was sent to XY by FE.  */
#define HARDDOOM2_STAT_FIFO_XYCMD			0x51
/* A command was sent to TEX by FE.  */
#define HARDDOOM2_STAT_FIFO_TEXCMD			0x52
/* A command was sent to FLAT by FE.  */
#define HARDDOOM2_STAT_FIFO_FLATCMD			0x53
/* A command was sent to FUZZ by FE.  */
#define HARDDOOM2_STAT_FIFO_FUZZCMD			0x54
/* A command was sent to OG by FE.  */
#define HARDDOOM2_STAT_FIFO_OGCMD			0x55
/* A command was sent to SW by OG.  */
#define HARDDOOM2_STAT_FIFO_SWCMD			0x56
/* An INTERLOCK command was processed by.  */
#define HARDDOOM2_STAT_FIFO_XYSYNC			0x57
/* A block of pixels was read from a surface by SR and sent to OG.  */
#define HARDDOOM2_STAT_FIFO_SROUT			0x58
/* A block of pixels was textured by TEX and sent to OG.  */
#define HARDDOOM2_STAT_FIFO_TEXOUT			0x59
/* A block of pixels was read or textured by FLAT and sent to OG.  */
#define HARDDOOM2_STAT_FIFO_FLATOUT			0x5a
/* A block mask was prepared by FUZZ and sent to OG.  */
#define HARDDOOM2_STAT_FIFO_FUZZOUT			0x5b
/* A block of pixels was prepared by OG and sent to SW to be written to a surface.  */
#define HARDDOOM2_STAT_FIFO_OGOUT			0x5c
/* A pixel was textured by TEX and sent to OG.  */
#define HARDDOOM2_STAT_FIFO_TEXOUT_PIXEL		0x5d
/* A pixel was prepared by OG and sent to SW to be written to a surface.  */
#define HARDDOOM2_STAT_FIFO_OGOUT_PIXEL			0x5e

/* A FENCE interrupt was triggered.  */
#define HARDDOOM2_STAT_FENCE_INTR			0x5f


/* Section 2.7: XY -- coordinate translation unit.  Its responsibility
 * is to translate (X, Y) coordinates in the surfaces into physical
 * addresses and to send them to SR and SW units.  Since SR and SW
 * operate on 64-pixel blocks, the X coordinates here are 5-bit and
 * are counted in blocks.  Widths are likewise 6-bit and counted in
 * blocks.  The FE unit requests translation in horizontal
 * or vertical batches, by giving the starting (X, Y) coordinate and
 * width or height of the batch.  The XY unit can have two batches
 * active at a moment -- one for the source surface and one for the
 * destination surface.  Also, the XY unit handles one half of the INTERLOCK
 * command (by listening on the SW2XY interface for receipt of the other
 * half by SW and blocking all operations until then).  */

/* Current XY unit state: surface widths and active command types, if any.  */
#define HARDDOOM2_XY_STATE				0x0600
#define HARDDOOM2_XY_STATE_SURF_DST_WIDTH_MASK		0x0000003f
#define HARDDOOM2_XY_STATE_SURF_DST_WIDTH_SHIFT		0
#define HARDDOOM2_XY_STATE_EXTR_SURF_DST_WIDTH(v)	((v) & 0x3f)
#define HARDDOOM2_XY_STATE_SURF_SRC_WIDTH_MASK		0x00003f00
#define HARDDOOM2_XY_STATE_SURF_SRC_WIDTH_SHIFT		8
#define HARDDOOM2_XY_STATE_EXTR_SURF_SRC_WIDTH(v)	((v) >> 8 & 0x3f)
#define HARDDOOM2_XY_STATE_PENDING_CMD_TYPE_MASK	0x000f0000
#define HARDDOOM2_XY_STATE_PENDING_CMD_TYPE_SHIFT	16
#define HARDDOOM2_XY_STATE_EXTR_PENDING_CMD_TYPE(v)	((v) >> 16 & 0xf)
#define HARDDOOM2_XY_STATE_DST_CMD_TYPE_MASK		0x00f00000
#define HARDDOOM2_XY_STATE_DST_CMD_TYPE_SHIFT		20
#define HARDDOOM2_XY_STATE_EXTR_DST_CMD_TYPE(v)		((v) >> 20 & 0xf)
#define HARDDOOM2_XY_STATE_SRC_CMD_TYPE_MASK		0x0f000000
#define HARDDOOM2_XY_STATE_SRC_CMD_TYPE_SHIFT		24
#define HARDDOOM2_XY_STATE_EXTR_SRC_CMD_TYPE(v)		((v) >> 24 & 0xf)
#define HARDDOOM2_XY_STATE_MASK				0x0fff3f3f
/* The data for the currently pending command.  */
#define HARDDOOM2_XY_PENDING_DATA			0x0604

/* The destination address command currently being executed.  */
#define HARDDOOM2_XY_DST_DATA				0x0608
/* The source address command currently being executed.  */
#define HARDDOOM2_XY_SRC_DATA				0x060c


/* Section 2.8: SR -- the Surface Read unit.  It takes physical addresses
 * from the XY unit, reads full blocks from them, and then submits them
 * to the OG unit for processing.  Stateless.  Used to read source data
 * for COPY_RECT and to read current data from the destination framebuffer
 * to be modified by the FUZZ effect.  */


/* Section 2.9: TEX -- the texture unit.  Can handle 64 DRAW_COLUMN commands
 * concurrently, merging them to a single stream of textured blocks.
 * To optimize cache usage, when a pixel is textured, up to 0x10 pixels
 * below it are speculatively textured as well if the required texels
 * are on the currently active cache line.  */

/* A copy of the last submitted TEXTURE_DIMS command.  */
#define HARDDOOM2_TEX_DIMS				0x0800
#define HARDDOOM2_TEX_DIMS_MASK				0xffffffff
/* The last submitted USTART command (will be copied to TEX_COLUMN_STATE
 * by START_COLUMN).  */
#define HARDDOOM2_TEX_USTART				0x0804
/* The last submitted USTEP command (will be copied to TEX_COLUMN_STEP
 * by START_COLUMN).  */
#define HARDDOOM2_TEX_USTEP				0x0808
/* The DRAW_TEX command in progress (if any).  */
#define HARDDOOM2_TEX_DRAW				0x080c
/* The number of blocks left to texture (non-0 means busy unit).  */
#define HARDDOOM2_TEX_DRAW_LENGTH_MASK			0x00000fff
#define HARDDOOM2_TEX_DRAW_EXTR_LENGTH(v)		((v) & 0x00000fff)
/* The index of the next pixel to be textured in the current block.  */
#define HARDDOOM2_TEX_DRAW_X_MASK			0x0003f000
#define HARDDOOM2_TEX_DRAW_X_SHIFT			12
#define HARDDOOM2_TEX_DRAW_EXTR_X(v)			((v) >> 12 & 0x3f)
#define HARDDOOM2_TEX_DRAW_MASK				0x0003ffff
/* The mask of currently active columns (64-bit register).  */
#define HARDDOOM2_TEX_MASK				0x0810
/* The texture cache -- a single 64-byte line.  */
#define HARDDOOM2_TEX_CACHE_STATE			0x0818
#define HARDDOOM2_TEX_CACHE_STATE_TAG_MASK		0x0000ffff
#define HARDDOOM2_TEX_CACHE_STATE_VALID			0x00010000
/* The position of the current block in the speculative texturing buffers.  */
#define HARDDOOM2_TEX_CACHE_STATE_SPEC_POS_MASK		0x00f00000
#define HARDDOOM2_TEX_CACHE_STATE_SPEC_POS_SHIFT	20
#define HARDDOOM2_TEX_CACHE_STATE_MASK			0x00f1ffff
#define HARDDOOM2_TEX_CACHE				0x0840
#define HARDDOOM2_TEX_CACHE_SIZE			64
/* The starting texture offset for each column, and the number of
 * speculatively textured pixels available in the buffer.  */
#define HARDDOOM2_TEX_COLUMN_STATE(i)			(0x0900 + (i) * 4)
#define HARDDOOM2_TEX_COLUMN_STATE_OFFSET_MASK		0x003fffff
#define HARDDOOM2_TEX_COLUMN_STATE_SPEC_MASK		0x07c00000
#define HARDDOOM2_TEX_COLUMN_STATE_SPEC_SHIFT		22
#define HARDDOOM2_TEX_COLUMN_STATE_MASK			0x07ffffff
/* The current texture coordinate for each column.  */
#define HARDDOOM2_TEX_COLUMN_COORD(i)			(0x0a00 + (i) * 4)
/* The coordinate step for each column.  */
#define HARDDOOM2_TEX_COLUMN_STEP(i)			(0x0b00 + (i) * 4)
/* The speculatively textured pixels for each column.  */
#define HARDDOOM2_TEX_COLUMN_SPEC_DATA(i)		(0x0c00 + (i) * 0x10)
#define HARDDOOM2_TEX_COLUMN_SPEC_DATA_SIZE		0x10

#define HARDDOOM2_TEX_OFFSET_MASK			0x003fffff


/* Section 2.10: FLAT.  Gets flat coordinates and pixel count from the FE unit,
 * reads the texels from the flat, and outputs textured pixels to the OG unit.
 * The FE can request a single horizontal span at a time.  Also, can bypass
 * raw blocks from the flat directly to the OG (used for DRAW_BACKGROUND).  */

/* The current U and V coordinates (updated as pixels are textured).  */
#define HARDDOOM2_FLAT_UCOORD				0x1000
#define HARDDOOM2_FLAT_VCOORD				0x1004
/* The USTEP and VSTEP params are stored here.  */
#define HARDDOOM2_FLAT_USTEP				0x1008
#define HARDDOOM2_FLAT_VSTEP				0x100c
#define HARDDOOM2_FLAT_COORD_MASK			0x003fffff
/* The address of the current flat, shifted right by 8.  */
#define HARDDOOM2_FLAT_ADDR				0x1010
#define HARDDOOM2_FLAT_ADDR_MASK			0xfffffff0
/* The DRAW_SPAN command in progress (if any).  */
#define HARDDOOM2_FLAT_DRAW				0x1014
/* The number of pixels left to texture (unit busy if non-0).  */
#define HARDDOOM2_FLAT_DRAW_LENGTH_MASK			0x00000fff
#define HARDDOOM2_FLAT_DRAW_EXTR_LENGTH(v)		((v) & 0x00000fff)
/* The index of the next pixel to be textured in the current block.  */
#define HARDDOOM2_FLAT_DRAW_X_MASK			0x0003f000
#define HARDDOOM2_FLAT_DRAW_X_SHIFT			12
#define HARDDOOM2_FLAT_DRAW_EXTR_X(v)			((v) >> 12 & 0x3f)
#define HARDDOOM2_FLAT_DRAW_MASK			0x0003ffff
/* The READ_FLAT command in progress (if any).  */
#define HARDDOOM2_FLAT_READ				0x1018
/* Number of blocks left to read.  */
#define HARDDOOM2_FLAT_READ_LENGTH_MASK			0x00000fff
#define HARDDOOM2_FLAT_READ_EXTR_LENGTH(v)		((v) & 0x00000fff)
/* The V coordinate of the next block to read.  */
#define HARDDOOM2_FLAT_READ_POS_MASK			0x0003f000
#define HARDDOOM2_FLAT_READ_POS_SHIFT			12
#define HARDDOOM2_FLAT_READ_EXTR_POS(v)			((v) >> 12 & 0x3f)
#define HARDDOOM2_FLAT_READ_MASK			0x0003ffff
/* The flat cache state.  It has one 64-byte cache line.  */
#define HARDDOOM2_FLAT_CACHE_STATE			0x101c
#define HARDDOOM2_FLAT_CACHE_STATE_TAG_MASK		0x0000003f
#define HARDDOOM2_FLAT_CACHE_STATE_VALID		0x00000100
#define HARDDOOM2_FLAT_CACHE_STATE_MASK			0x0000013f
#define HARDDOOM2_FLAT_CACHE				0x1040
#define HARDDOOM2_FLAT_CACHE_SIZE			64


/* Section 2.11: FUZZ.  Responsible for generating pixel masks for the FUZZ
 * effect.  Capable of operating on a block of columns at once, with
 * independent fuzz positions for each column.  The FE unit computes
 * and sets the initial fuzz position for each column, then sends FUZZ
 * the number of masks to generate, letting it step the positions while
 * the batch is being drawn.  */

/* The fuzz position for every column (64 single-byte registers).  */
#define HARDDOOM2_FUZZ_POSITION				0x1200
/* The number of blocks left to emit (non-0 means busy unit).  */
#define HARDDOOM2_FUZZ_DRAW				0x1240
#define HARDDOOM2_FUZZ_DRAW_MASK			0x00000fff


/* Section 2.12: OG -- the output gather unit.  Responsible for gathering pixel data from
 * FE, SR, TEX and FLAT, applying effects, preparing write masks, and sending
 * the final pixel data to SW.  The heart of this unit is a 4-block buffer.
 * The buffer blocks are aligned with destination surface blocks -- blocks
 * from SR and FLAT are rotated on input to match the destination alignment.  */

/* The write mask of the block to be rendered (64-bit register).  */
#define HARDDOOM2_OG_MASK				0x1400
/* The FUZZ effect mask received from the FUZZ unit (64-bit register).  */
#define HARDDOOM2_OG_FUZZ_MASK				0x1408
/* The payload of the command currently being processed.  */
#define HARDDOOM2_OG_DATA				0x1410
/* Misc state.  */
#define HARDDOOM2_OG_STATE				0x1414
/* The command type currently being processed.  */
#define HARDDOOM2_OG_STATE_CMD_MASK			0x0000003f
#define HARDDOOM2_OG_STATE_EXTR_CMD(v)			((v) & 0x0000003f)
/* The current position in the buffer (exact interpretation depends
 * on the command).  */
#define HARDDOOM2_OG_STATE_BUF_POS_MASK			0x0000ff00
#define HARDDOOM2_OG_STATE_BUF_POS_SHIFT		8
#define HARDDOOM2_OG_STATE_EXTR_BUF_POS(v)		((v) >> 8 & 0xff)
/* The current state in the command execution state machine.  Every command
 * starts in the INIT state.  The exact interpretation depends on the command.  */
#define HARDDOOM2_OG_STATE_STATE_INIT			0x00000000
#define HARDDOOM2_OG_STATE_STATE_PREFILL		0x00010000
#define HARDDOOM2_OG_STATE_STATE_RUNNING		0x00020000
#define HARDDOOM2_OG_STATE_STATE_FILLED			0x00030000
#define HARDDOOM2_OG_STATE_STATE_MASK			0x00030000
#define HARDDOOM2_OG_STATE_MASK				0x0003ff3f
/* The buffer.  */
#define HARDDOOM2_OG_BUF				0x1500
#define HARDDOOM2_OG_BUF_SIZE				0x00000100
/* The current color maps (they are eagerly read in their entirety when FE
 * requests so).  */
#define HARDDOOM2_OG_TRANSLATION			0x1600
#define HARDDOOM2_OG_COLORMAP				0x1700


/* The SW unit.  Gets final pixel data and write masks from the OG unit,
 * and stores it to addresses received from the XY unit.  Also, as the last
 * unit in the pipeline, handles synchronization commands.  */

/* The currently processed command.  */
#define HARDDOOM2_SW_STATE				0x1800
/* Blocks left to draw.  */
#define HARDDOOM2_SW_STATE_DRAW_MASK			0x00000fff
/* Index of pixel currently being tranmapped.  */
#define HARDDOOM2_SW_STATE_X_MASK			0x0003f000
#define HARDDOOM2_SW_STATE_X_SHIFT			12
#define HARDDOOM2_SW_STATE_EXTR_X(v)			((v) >> 12 & 0x3f)
#define HARDDOOM2_SW_STATE_BLOCK_PENDING		0x00100000
/* Current pending command type (unit busy if non-0).  */
#define HARDDOOM2_SW_STATE_CMD_MASK			0xf0000000
#define HARDDOOM2_SW_STATE_CMD_SHIFT			28
#define HARDDOOM2_SW_STATE_EXTR_CMD(v)			((v) >> 28 & 0xf)
#define HARDDOOM2_SW_STATE_MASK				0xf013ffff
/* The mask of currently pending block.  */
#define HARDDOOM2_SW_MASK				0x1808
/* The physical address of currently pending block.  */
#define HARDDOOM2_SW_ADDR				0x1810

/* The SW cache (for TRANMAP).  */
#define HARDDOOM2_SW_CACHE_STATE(i)			(0x1840 + (i) * 4)
#define HARDDOOM2_SW_CACHE_STATE_TAG_MASK		0x0000003f
#define HARDDOOM2_SW_CACHE_STATE_VALID			0x00000100
#define HARDDOOM2_SW_CACHE_STATE_MASK			0x0000013f
#define HARDDOOM2_SW_CACHE(i)				(0x1c00 + (i) * 0x40)
#define HARDDOOM2_SW_CACHE_LINES			16
#define HARDDOOM2_SW_CACHE_LINE_SIZE			64

/* The data of the currently pending block.  */
#define HARDDOOM2_SW_BUF				0x1880
#define HARDDOOM2_SW_BUF_SIZE				0x40
/* The old data from the framebuffer.  */
#define HARDDOOM2_SW_OLD				0x18c0
#define HARDDOOM2_SW_OLD_SIZE				0x40


/* Section 3: The driver commands.  */

/* Word 0 (common commands).  */
#define HARDDOOM2_CMD_W0(type, flags)			((type) | (flags))
/* V_CopyRect: The usual blit.  */
#define HARDDOOM2_CMD_TYPE_COPY_RECT			0x0
/* V_FillRect: The usual solid fill.  */
#define HARDDOOM2_CMD_TYPE_FILL_RECT			0x1
/* V_DrawLine: The usual solid line. */
#define HARDDOOM2_CMD_TYPE_DRAW_LINE			0x2
/* V_DrawBackground: Fill a rectangle with repeated flat. */
#define HARDDOOM2_CMD_TYPE_DRAW_BACKGROUND		0x3
/* R_DrawColumn. */
#define HARDDOOM2_CMD_TYPE_DRAW_COLUMN			0x4
/* R_DrawColumn with fuzz effect. */
#define HARDDOOM2_CMD_TYPE_DRAW_FUZZ			0x5
/* R_DrawSpan. */
#define HARDDOOM2_CMD_TYPE_DRAW_SPAN			0x6
/* Sets up buffers. */
#define HARDDOOM2_CMD_TYPE_SETUP			0x7
#define HARDDOOM2_CMD_TYPE_MASK				0xf
#define HARDDOOM2_CMD_W0_EXTR_TYPE(word)		((word) & HARDDOOM2_CMD_TYPE_MASK)
/* Block further surface reads until inflight surface writes are complete.  */
#define HARDDOOM2_CMD_FLAG_INTERLOCK			0x00000010
/* Trigger an interrupt now.  */
#define HARDDOOM2_CMD_FLAG_PING_ASYNC			0x00000020
/* Trigger an interrupt once we're done with current work.  */
#define HARDDOOM2_CMD_FLAG_PING_SYNC			0x00000040
/* Bump the sync counter.  */
#define HARDDOOM2_CMD_FLAG_FENCE			0x00000080
/* Use translation colormap if enabled.  */
#define HARDDOOM2_CMD_FLAG_TRANSLATION			0x00000100
/* Use main colormap if enabled.  */
#define HARDDOOM2_CMD_FLAG_COLORMAP			0x00000200
/* Use transparency map if enabled.  */
#define HARDDOOM2_CMD_FLAG_TRANMAP			0x00000400

/* Word 1 (common).  */
#define HARDDOOM2_CMD_W1(tridx, cmidx)			((tridx) | (cmidx) << 16)
/* Translation colormap index.  */
#define HARDDOOM2_CMD_W1_EXTR_TRANSLATION_IDX(word)	((word) & 0x3fff)
/* Main colormap index.  */
#define HARDDOOM2_CMD_W1_EXTR_COLORMAP_IDX(word)	((word) >> 16 & 0x3fff)

/* Word 2 and 3 (common).  */
#define HARDDOOM2_CMD_W2(x, y, flidx)			((x) | (y) << 11 | (flidx) << 22)
#define HARDDOOM2_CMD_W3(x, y)				((x) | (y) << 11)
/* The X coordinate.  */
#define HARDDOOM2_CMD_W2_W3_EXTR_X(word)		((word) & 0x7ff)
/* The Y coordinate.  */
#define HARDDOOM2_CMD_W2_W3_EXTR_Y(word)		((word) >> 11 & 0x7ff)
/* The flat index.  */
#define HARDDOOM2_CMD_W2_EXTR_FLAT_IDX(word)		((word) >> 22 & 0x3ff)

/* Word 4 (common): USTART.  */
/* Word 5 (common): USTEP.  */

/* Word 6 variant A (COPY_RECT, FILL_RECT, DRAW_LINE, FILL_BACKGROUND).  */
#define HARDDOOM2_CMD_W6_A(w, h, col)			((w) | (h) << 12 | (col) << 24)
/* Rectangle width.  */
#define HARDDOOM2_CMD_W6_A_EXTR_WIDTH(word)		((word) & 0xfff)
/* Rectangle height.  */
#define HARDDOOM2_CMD_W6_A_EXTR_HEIGHT(word)		((word) >> 12 & 0xfff)
/* Solid fill color for FILL_RECT and DRAW_LINE. */
#define HARDDOOM2_CMD_W6_A_EXTR_FILL_COLOR(word)	((word) >> 24 & 0xff)

/* Word 6 variant B (DRAW_COLUMN).  */
#define HARDDOOM2_CMD_W6_B(off)				(off)
/* Texture column offset.  */
#define HARDDOOM2_CMD_W6_B_EXTR_TEXTURE_OFFSET(word)	((word) & 0x3fffff)

/* Word 7 variant B (DRAW_COLUMN).  */
#define HARDDOOM2_CMD_W7_B(limit, height)		((limit) | (height) << 16)
/* Texture data limit (address of last valid byte).  */
#define HARDDOOM2_CMD_W7_B_EXTR_TEXTURE_LIMIT(word)	((word) & 0xffff)
/* Texture height for repetition, or 0.  */
#define HARDDOOM2_CMD_W7_B_EXTR_TEXTURE_HEIGHT(word)	((word) >> 16 & 0xffff)

/* Word 6 variant C (DRAW_FUZZ).  */
#define HARDDOOM2_CMD_W6_C(s, e, p)			((s) | (e) << 12 | (p) << 24)
/* Fuzz area start Y coordinate.  */
#define HARDDOOM2_CMD_W6_C_EXTR_FUZZ_START(word)	((word) & 0x7ff)
/* Fuzz area end Y coordinate.  */
#define HARDDOOM2_CMD_W6_C_EXTR_FUZZ_END(word)		((word) >> 12 & 0x7ff)
/* Fuzz position (random seed really).  */
#define HARDDOOM2_CMD_W6_C_EXTR_FUZZ_POS(word)		((word) >> 24 & 0x3f)

/* Word 6 variant D (DRAW_SPAN): VSTART.  */
/* Word 7 variant D (DRAW_SPAN): VSTEP.  */

/* Word 0 (SETUP).  */
#define HARDDOOM2_CMD_W0_SETUP(type, flags, sdwidth, sswidth)	((type) | (flags) | ((sdwidth) >> 6) << 16 | ((sswidth) >> 6) << 24)
#define HARDDOOM2_CMD_FLAG_SETUP_SURF_DST		0x00000200
#define HARDDOOM2_CMD_FLAG_SETUP_SURF_SRC		0x00000400
#define HARDDOOM2_CMD_FLAG_SETUP_TEXTURE		0x00000800
#define HARDDOOM2_CMD_FLAG_SETUP_FLAT			0x00001000
#define HARDDOOM2_CMD_FLAG_SETUP_TRANSLATION		0x00002000
#define HARDDOOM2_CMD_FLAG_SETUP_COLORMAP		0x00004000
#define HARDDOOM2_CMD_FLAG_SETUP_TRANMAP		0x00008000
#define HARDDOOM2_CMD_W0_EXTR_SURF_WIDTH_DST(word)	(((word) >> 16 & 0x3f) << 6)
#define HARDDOOM2_CMD_W0_EXTR_SURF_WIDTH_SRC(word)	(((word) >> 24 & 0x3f) << 6)

/* Word 1 (SETUP): SURF_DST_PT.  */
/* Word 2 (SETUP): SURF_SRC_PT.  */
/* Word 3 (SETUP): TEXTURE_PT.  */
/* Word 4 (SETUP): FLAT_PT.  */
/* Word 5 (SETUP): TRANSLATION_PT.  */
/* Word 6 (SETUP): COLORMAP_PT.  */
/* Word 7 (SETUP): TRANMAP_PT.  */


/* Section 4: internal commands.  */

/* Section 4.1: XYCMD -- XY unit internal commands.  */

#define HARDDOOM2_XYCMD_TYPE_NOP			0x0
/* Straight from SETUP.  */
#define HARDDOOM2_XYCMD_TYPE_SURF_DST_PT		0x1
#define HARDDOOM2_XYCMD_TYPE_SURF_SRC_PT		0x2
#define HARDDOOM2_XYCMD_TYPE_SURF_DST_WIDTH		0x3
#define HARDDOOM2_XYCMD_TYPE_SURF_SRC_WIDTH		0x4
/* One half of the interlock -- makes XY wait for the interlock signal from SW (no payload).  */
#define HARDDOOM2_XYCMD_TYPE_INTERLOCK			0x5
/* Emits a series of addresses for a horizontal write to DST.  */
#define HARDDOOM2_XYCMD_TYPE_WRITE_DST_H		0x6
/* Emits a series of addresses for a vertical write to DST.  */
#define HARDDOOM2_XYCMD_TYPE_WRITE_DST_V		0x7
/* Emits a series of addresses for a horizontal read from SRC.  */
#define HARDDOOM2_XYCMD_TYPE_READ_SRC_H			0x8
/* Emits a series of addresses for a vertical read from SRC.  */
#define HARDDOOM2_XYCMD_TYPE_READ_SRC_V			0x9
/* Emits a series of addresses for a vertical read from DST.  */
#define HARDDOOM2_XYCMD_TYPE_READ_DST_V			0xa
/* Emits a series of addresses for a vertical read and write to DST (sending
 * the same address stream to both SR and SW).  */
#define HARDDOOM2_XYCMD_TYPE_RMW_DST_V			0xb

/* The payload for *_H commands: starting xy position and width (in blocks).  */
#define HARDDOOM2_XYCMD_DATA_H(x, y, w)			(((w) & 0x3f) << 16 | ((y) & 0x7ff) << 5 | ((x) & 0x1f))
/* The payload for *_V commands: starting xy position and height (in pixels).  */
#define HARDDOOM2_XYCMD_DATA_V(x, y, h)			(((h) & 0xfff) << 16 | ((y) & 0x7ff) << 5 | ((x) & 0x1f))

/* For all READ/WRITE/RMW commands -- start X coordinate in blocks.  */
#define HARDDOOM2_XYCMD_DATA_EXTR_X(cmd)		((cmd) & 0x1f)
/* For all READ/WRITE/RMW commands -- start Y coordinate.  */
#define HARDDOOM2_XYCMD_DATA_EXTR_Y(cmd)		((cmd) >> 5 & 0x7ff)
/* For horizontal READ/WRITE commands -- width in blocks.  */
#define HARDDOOM2_XYCMD_DATA_EXTR_WIDTH(cmd)		((cmd) >> 16 & 0x3f)
/* For vertical READ/WRITE commands -- height.  */
#define HARDDOOM2_XYCMD_DATA_EXTR_HEIGHT(cmd)		((cmd) >> 16 & 0xfff)


/* Section 4.2: TEXCMD -- TEX unit internal commands.  */

#define HARDDOOM2_TEXCMD_TYPE_NOP			0x0
/* Straight from SETUP.  */
#define HARDDOOM2_TEXCMD_TYPE_TEXTURE_PT		0x1
/* Contains packed TEXTURE_LIMIT and TEXTURE_HEIGHT, same as DRAW_COLUMN command word 7.  */
#define HARDDOOM2_TEXCMD_TYPE_TEXTURE_DIMS		0x2
/* Straight from the command words.  */
#define HARDDOOM2_TEXCMD_TYPE_USTART			0x3
#define HARDDOOM2_TEXCMD_TYPE_USTEP			0x4
/* Enables given column slot, sets its texture offset, copies the last
 * submitted USTEP value, and initializes current coordinate to the last
 * submitted USTART value.  */
#define HARDDOOM2_TEXCMD_TYPE_START_COLUMN		0x5
/* Disables given column slot.  */
#define HARDDOOM2_TEXCMD_TYPE_END_COLUMN		0x6
/* Emits a given number of blocks to OG.  */
#define HARDDOOM2_TEXCMD_TYPE_DRAW_TEX			0x7

#define HARDDOOM2_TEXCMD_DATA_START_COLUMN(x, offset)	((offset) | (x) << 22)

/* For START_COLUMN.  */
#define HARDDOOM2_TEXCMD_DATA_EXTR_OFFSET(cmd)		((cmd) & 0x3fffff)
#define HARDDOOM2_TEXCMD_DATA_EXTR_START_X(cmd)		((cmd) >> 22 & 0x3f)
/* For END_COLUMN.  */
#define HARDDOOM2_TEXCMD_DATA_EXTR_END_X(cmd)		((cmd) & 0x3f)
/* For DRAW_TEX.  */
#define HARDDOOM2_TEXCMD_DATA_EXTR_DRAW_HEIGHT(cmd)	((cmd) & 0xfff)


/* Section 4.3: FLATCMD -- FLAT unit internal commands.  */

#define HARDDOOM2_FLATCMD_TYPE_NOP			0x0
/* The physical address of current FLAT (translated by FE).  */
#define HARDDOOM2_FLATCMD_TYPE_FLAT_ADDR		0x1
/* Straight from the command words.  */
#define HARDDOOM2_FLATCMD_TYPE_USTART			0x2
#define HARDDOOM2_FLATCMD_TYPE_VSTART			0x3
#define HARDDOOM2_FLATCMD_TYPE_USTEP			0x4
#define HARDDOOM2_FLATCMD_TYPE_VSTEP			0x5
/* Reads N lines of a flat in a loop, starting from the given Y coordinate.  */
#define HARDDOOM2_FLATCMD_TYPE_READ_FLAT		0x6
/* Emits a given number of textured pixels to the OG.  */
#define HARDDOOM2_FLATCMD_TYPE_DRAW_SPAN		0x7

#define HARDDOOM2_FLATCMD_DATA_READ_FLAT(h, y)		((h) | (y) << 12)

#define HARDDOOM2_FLATCMD_DATA_EXTR_READ_HEIGHT(cmd)	((cmd) & 0xfff)
#define HARDDOOM2_FLATCMD_DATA_EXTR_READ_Y(cmd)		((cmd) >> 12 & 0x3f)
#define HARDDOOM2_FLATCMD_DATA_EXTR_DRAW_WIDTH(cmd)	((cmd) & 0xfff)


/* Section 4.4: FUZZCMD -- FUZZ unit internal commands.  */

#define HARDDOOM2_FUZZCMD_TYPE_NOP			0x0
/* Sets the fuzz position for a given column.  */
#define HARDDOOM2_FUZZCMD_TYPE_SET_COLUMN		0x1
/* Emits a given number of block masks to the OG.  */
#define HARDDOOM2_FUZZCMD_TYPE_DRAW_FUZZ		0x2

#define HARDDOOM2_FUZZCMD_DATA_SET_COLUMN(x, p)		((x) << 6 | (p))

#define HARDDOOM2_FUZZCMD_DATA_EXTR_X(cmd)		((cmd) >> 6 & 0x3f)
#define HARDDOOM2_FUZZCMD_DATA_EXTR_POS(cmd)		((cmd) & 0x3f)


/* Section 4.5: OGCMD -- OG unit internal commands.  */

/* Note: The first 5 commands have values shared with SWCMD.  */
#define HARDDOOM2_OGCMD_TYPE_NOP			0x00
/* Tells SW to send the interlock signal on SW2XY (no payload).  */
#define HARDDOOM2_OGCMD_TYPE_INTERLOCK			0x01
/* Bumps fence counter, passed to SW (no payload).  */
#define HARDDOOM2_OGCMD_TYPE_FENCE			0x02
/* Triggers PONG_SYNC interrupt, passed to SW (no payload).  */
#define HARDDOOM2_OGCMD_TYPE_PING			0x03
/* Straight from the SETUP command.  */
#define HARDDOOM2_OGCMD_TYPE_TRANMAP_PT			0x04
/* Fills the buffer with a solid color.  */
#define HARDDOOM2_OGCMD_TYPE_FILL_COLOR			0x05
/* Fetches the color map from the given physical address (shifted by 8).  */
#define HARDDOOM2_OGCMD_TYPE_COLORMAP_ADDR		0x06
/* Fetches the translation map from the given physical address (shifted by 8).  */
#define HARDDOOM2_OGCMD_TYPE_TRANSLATION_ADDR		0x07
/* Draws a horizontal stripe of pixels from buffer.  */
#define HARDDOOM2_OGCMD_TYPE_DRAW_BUF_H			0x08
/* Draws a vertical stripe of pixels from buffer.  */
#define HARDDOOM2_OGCMD_TYPE_DRAW_BUF_V			0x09
/* Draws a horizontal stripe of pixels from SR.  */
#define HARDDOOM2_OGCMD_TYPE_COPY_H			0x0a
/* Draws a vertical stripe of pixels from SR.  */
#define HARDDOOM2_OGCMD_TYPE_COPY_V			0x0b
/* Reads a block from FLAT to the buffer (no payload).  */
#define HARDDOOM2_OGCMD_TYPE_READ_FLAT			0x0c
/* Draws a horizontal stripe of pixels from FLAT.  */
#define HARDDOOM2_OGCMD_TYPE_DRAW_SPAN			0x0d
/* Reads pixels from SR, applies FUZZ effect, draws them.  */
#define HARDDOOM2_OGCMD_TYPE_DRAW_FUZZ			0x0e
/* Prepares for the above (no payload).  */
#define HARDDOOM2_OGCMD_TYPE_INIT_FUZZ			0x0f
/* Flips the given column in FUZZ draw mask.  */
#define HARDDOOM2_OGCMD_TYPE_FUZZ_COLUMN		0x10
/* Draws blocks straight from TEX.  */
#define HARDDOOM2_OGCMD_TYPE_DRAW_TEX			0x11

#define HARDDOOM2_OGCMD_DATA_DRAW_BUF_H(x, w)		((x) | (w) << 6)
#define HARDDOOM2_OGCMD_DATA_DRAW_BUF_V(x, w, h)	((x) | (w) << 6 | (h) << 12)
#define HARDDOOM2_OGCMD_DATA_COPY_H(x, w, o)		((x) | (w) << 6 | (o) << 24)
#define HARDDOOM2_OGCMD_DATA_COPY_V(x, w, h, o)		((x) | (w) << 6 | (h) << 12 | (o) << 24)
#define HARDDOOM2_OGCMD_DATA_DRAW_SPAN(x, w, f)		((x) | (w) << 6 | (f) << 24)
#define HARDDOOM2_OGCMD_DATA_DRAW_FUZZ(h, f)		((h) | (f) << 24)
#define HARDDOOM2_OGCMD_DATA_FUZZ_COLUMN(x)		((x))
#define HARDDOOM2_OGCMD_DATA_DRAW_TEX(h, f)		((h) | (f) << 24)

#define HARDDOOM2_OGCMD_DATA_EXTR_X(cmd)		((cmd) & 0x3f)
#define HARDDOOM2_OGCMD_DATA_EXTR_H_WIDTH(cmd)		((cmd) >> 6 & 0xfff)
#define HARDDOOM2_OGCMD_DATA_EXTR_V_WIDTH(cmd)		((cmd) >> 6 & 0x3f)
#define HARDDOOM2_OGCMD_DATA_EXTR_V_HEIGHT(cmd)		((cmd) >> 12 & 0xfff)
#define HARDDOOM2_OGCMD_DATA_EXTR_SRC_OFFSET(cmd)	((cmd) >> 24 & 0x3f)
#define HARDDOOM2_OGCMD_DATA_EXTR_TF_HEIGHT(cmd)	((cmd) & 0xfff)
#define HARDDOOM2_OGCMD_DATA_EXTR_FLAGS(cmd)		((cmd) >> 24 & 7)

#define HARDDOOM2_OGCMD_FLAG_TRANSLATION		1
#define HARDDOOM2_OGCMD_FLAG_COLORMAP			2
#define HARDDOOM2_OGCMD_FLAG_TRANMAP			4


/* Section 4.6: SWCMD -- SW unit internal commands.  */

#define HARDDOOM2_SWCMD_TYPE_NOP			HARDDOOM2_OGCMD_TYPE_NOP
/* Will send the interlock signal on SW2XY.  */
#define HARDDOOM2_SWCMD_TYPE_INTERLOCK			HARDDOOM2_OGCMD_TYPE_INTERLOCK
/* Bumps fence counter.  */
#define HARDDOOM2_SWCMD_TYPE_FENCE			HARDDOOM2_OGCMD_TYPE_FENCE
/* Triggers PONG_SYNC interrupt.  */
#define HARDDOOM2_SWCMD_TYPE_PING			HARDDOOM2_OGCMD_TYPE_PING
/* Straight from the SETUP command.  */
#define HARDDOOM2_SWCMD_TYPE_TRANMAP_PT			HARDDOOM2_OGCMD_TYPE_TRANMAP_PT
/* Draws a given number of blocks.  */
#define HARDDOOM2_SWCMD_TYPE_DRAW			0x5
/* Draws a given number of transparent blocks.  */
#define HARDDOOM2_SWCMD_TYPE_DRAW_TRANSPARENT		0x6


/* Section 5: Page tables.  */

#define HARDDOOM2_PTE_VALID				0x00000001
#define HARDDOOM2_PTE_WRITABLE				0x00000002
#define HARDDOOM2_PTE_PHYS_MASK				0xfffffff0
#define HARDDOOM2_PTE_PHYS_SHIFT			4
#define HARDDOOM2_PAGE_SHIFT				12
#define HARDDOOM2_PAGE_SIZE				0x1000
#define HARDDOOM2_VIDX_MASK				0x000003ff

/* Section 6: Misc things.  */

#define HARDDOOM2_XY_COORD_MASK				0x7ff
#define HARDDOOM2_COLORMAP_SIZE				0x100
#define HARDDOOM2_FLAT_SIZE				0x1000
#define HARDDOOM2_TRANMAP_SIZE				0x10000
/* The block size in pixels / columns -- the source and destination surfaces
 * are written in blocks of this many pixels, and texture/fuzz column batches
 * are made of this many columns.  Also happens to be the width of a flat.  */
#define HARDDOOM2_BLOCK_SIZE				0x40

#endif
