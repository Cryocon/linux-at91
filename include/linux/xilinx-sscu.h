#ifndef _XILINX_SSCU
#define _XILINX_SSCU
struct xsscu_data {
	char *name;
	unsigned int clk;
	unsigned int sout;
	unsigned int init_b;
	unsigned int prog_b;
	unsigned int done;
};

enum {
	XSSCU_STATE_IDLE,
	XSSCU_STATE_UPLOADING,
	XSSCU_STATE_UPLOAD_DONE,
	XSSCU_STATE_DISABLED,
	XSSCU_STATE_PROG_ERROR,
};

struct xsscu_device_data {
	struct xsscu_data *pdata;
	int open;
	int state;
	char *read_ptr;
	char msg_buffer[128];
};
#endif
