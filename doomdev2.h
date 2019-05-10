#ifndef DOOMDEV2_H
#define DOOMDEV2_H

#ifdef __KERNEL__
#include <linux/kernel.h>
#else
#include <stdint.h>
#endif

#include <linux/ioctl.h>

/* /dev/doom* ioctls.  */

struct doomdev2_ioctl_create_surface {
	uint16_t width;
	uint16_t height;
};

struct doomdev2_ioctl_create_buffer {
	uint32_t size;
};

struct doomdev2_ioctl_setup {
	int32_t surf_dst_fd;
	int32_t surf_src_fd;
	int32_t texture_fd;
	int32_t flat_fd;
	int32_t colormap_fd;
	int32_t translation_fd;
	int32_t tranmap_fd;
};

#define DOOMDEV2_IOCTL_CREATE_SURFACE _IOW('D', 0x00, struct doomdev2_ioctl_create_surface)
#define DOOMDEV2_IOCTL_CREATE_BUFFER _IOW('D', 0x01, struct doomdev2_ioctl_create_buffer)
#define DOOMDEV2_IOCTL_SETUP _IOW('D', 0x02, struct doomdev2_ioctl_setup)

enum doomdev2_cmd_type {
	DOOMDEV2_CMD_TYPE_COPY_RECT = 0,
	DOOMDEV2_CMD_TYPE_FILL_RECT = 1,
	DOOMDEV2_CMD_TYPE_DRAW_LINE = 2,
	DOOMDEV2_CMD_TYPE_DRAW_BACKGROUND = 3,
	DOOMDEV2_CMD_TYPE_DRAW_COLUMN = 4,
	DOOMDEV2_CMD_TYPE_DRAW_SPAN = 5,
	DOOMDEV2_CMD_TYPE_DRAW_FUZZ = 6,
};

#define DOOMDEV2_CMD_FLAGS_TRANSLATE	0x01
#define DOOMDEV2_CMD_FLAGS_COLORMAP	0x02
#define DOOMDEV2_CMD_FLAGS_TRANMAP	0x04

struct doomdev2_cmd_copy_rect {
	uint8_t type;
	uint8_t _pad[3];
	uint16_t width;
	uint16_t height;
	uint16_t pos_dst_x;
	uint16_t pos_dst_y;
	uint16_t pos_src_x;
	uint16_t pos_src_y;
	uint32_t _pad2[4];
};

struct doomdev2_cmd_fill_rect {
	uint8_t type;
	uint8_t fill_color;
	uint8_t _pad[2];
	uint16_t width;
	uint16_t height;
	uint16_t pos_x;
	uint16_t pos_y;
	uint32_t _pad2[5];
};

struct doomdev2_cmd_draw_line {
	uint8_t type;
	uint8_t fill_color;
	uint8_t _pad[2];
	uint16_t pos_a_x;
	uint16_t pos_a_y;
	uint16_t pos_b_x;
	uint16_t pos_b_y;
	uint32_t _pad2[5];
};

struct doomdev2_cmd_draw_background {
	uint8_t type;
	uint8_t _pad;
	uint16_t flat_idx;
	uint16_t width;
	uint16_t height;
	uint16_t pos_x;
	uint16_t pos_y;
	uint32_t _pad2[5];
};

struct doomdev2_cmd_draw_column {
	uint8_t type;
	uint8_t flags;
	uint16_t pos_x;
	uint16_t pos_a_y;
	uint16_t pos_b_y;
	uint16_t colormap_idx;
	uint16_t translation_idx;
	uint16_t texture_height;
	uint16_t _pad;
	uint32_t texture_offset;
	uint32_t ustart;
	uint32_t ustep;
	uint32_t _pad2;
};

struct doomdev2_cmd_draw_span {
	uint8_t type;
	uint8_t flags;
	uint16_t pos_y;
	uint16_t pos_a_x;
	uint16_t pos_b_x;
	uint16_t colormap_idx;
	uint16_t translation_idx;
	uint16_t flat_idx;
	uint16_t _pad;
	uint32_t ustart;
	uint32_t vstart;
	uint32_t ustep;
	uint32_t vstep;
};

struct doomdev2_cmd_draw_fuzz {
	uint8_t type;
	uint8_t fuzz_pos;
	uint16_t pos_x;
	uint16_t pos_a_y;
	uint16_t pos_b_y;
	uint16_t fuzz_start;
	uint16_t fuzz_end;
	uint16_t colormap_idx;
	uint16_t _pad[9];
};

struct doomdev2_cmd {
	union {
		uint8_t type;
		struct doomdev2_cmd_copy_rect copy_rect;
		struct doomdev2_cmd_fill_rect fill_rect;
		struct doomdev2_cmd_draw_line draw_line;
		struct doomdev2_cmd_draw_background draw_background;
		struct doomdev2_cmd_draw_column draw_column;
		struct doomdev2_cmd_draw_span draw_span;
		struct doomdev2_cmd_draw_fuzz draw_fuzz;
	};
};

_Static_assert(sizeof (struct doomdev2_cmd_copy_rect) == 32, "cmd size mismatch");
_Static_assert(sizeof (struct doomdev2_cmd_fill_rect) == 32, "cmd size mismatch");
_Static_assert(sizeof (struct doomdev2_cmd_draw_line) == 32, "cmd size mismatch");
_Static_assert(sizeof (struct doomdev2_cmd_draw_background) == 32, "cmd size mismatch");
_Static_assert(sizeof (struct doomdev2_cmd_draw_column) == 32, "cmd size mismatch");
_Static_assert(sizeof (struct doomdev2_cmd_draw_span) == 32, "cmd size mismatch");
_Static_assert(sizeof (struct doomdev2_cmd_draw_fuzz) == 32, "cmd size mismatch");
_Static_assert(sizeof (struct doomdev2_cmd) == 32, "cmd size mismatch");

#endif
