#include "raspiraw.h"
#include "RegOperation.h"

void send_regs(int fd, const struct sensor_def *sensor, const struct sensor_regs *regs, int num_regs)
{
	int i;
	for (i=0; i<num_regs; i++)
	{
		if (regs[i].reg == 0xFFFF)
		{
			if (ioctl(fd, I2C_SLAVE_FORCE, regs[i].data) < 0)
			{
				vcos_log_error("Failed to set I2C address to %02X", regs[i].data);
			}
		}
		else if (regs[i].reg == 0xFFFE)
		{
			vcos_sleep(regs[i].data);
		}
		else
		{
			if (sensor->i2c_addressing == 1)
			{
				unsigned char msg[3] = {regs[i].reg, regs[i].data & 0xFF };
				int len = 2;

				if (sensor->i2c_data_size == 2)
				{
					msg[1] = (regs[i].data>>8) & 0xFF;
					msg[2] = regs[i].data & 0xFF;
					len = 3;
				}
				if (write(fd, msg, len) != len)
				{
					vcos_log_error("Failed to write register index %d (%02X val %02X)", i, regs[i].reg, regs[i].data);
				}
			}
			else
			{
				unsigned char msg[4] = {regs[i].reg>>8, regs[i].reg, regs[i].data};
				int len = 3;

				if (sensor->i2c_data_size == 2)
				{
					msg[2] = regs[i].data >> 8;
					msg[3] = regs[i].data;
					len = 4;
				}
				if (write(fd, msg, len) != len)
				{
					vcos_log_error("Failed to write register index %d", i);
				}
			}
		}
	}
}

void modRegBit(struct mode_def *mode, uint16_t reg, int bit, int value, enum operation op)
{
	int i = 0;
	uint16_t val;
	while(i < mode->num_regs && mode->regs[i].reg != reg) i++;
	if (i == mode->num_regs) {
		vcos_log_error("Reg: %04X not found!\n", reg);
		return;
	}
	val = mode->regs[i].data;

	switch(op)
	{
		case EQUAL:
			val = (val | (1 << bit)) & (~( (1 << bit) ^ (value << bit) ));
			break;
		case SET:
			val = val | (1 << bit);
			break;
		case CLEAR:
			val = val & ~(1 << bit);
			break;
		case XOR:
			val = val ^ (value << bit);
			break;
	}
	mode->regs[i].data = val;
}

void modReg(struct mode_def *mode, uint16_t reg, int startBit, int endBit, int value, enum operation op)
{
	int i;
	for(i = startBit; i <= endBit; i++) {
		modRegBit(mode, reg, i, value >> i & 1, op);
	}
}

void update_regs(const struct sensor_def *sensor, struct mode_def *mode, int hflip, int vflip, int exposure, int gain)
{
	if (sensor->vflip_reg)
	{
		modRegBit(mode, sensor->vflip_reg, sensor->vflip_reg_bit, vflip, XOR);
		if (vflip && !sensor->flips_dont_change_bayer_order)
			mode->order ^= 2;
	}

	if (sensor->hflip_reg)
	{
		modRegBit(mode, sensor->hflip_reg, sensor->hflip_reg_bit, hflip, XOR);
		if (hflip && !sensor->flips_dont_change_bayer_order)
			mode->order ^= 1;
	}

	if (sensor->exposure_reg && exposure != -1)
	{
		if (exposure < 0 || exposure >= (1<<sensor->exposure_reg_num_bits))
		{
			vcos_log_error("Invalid exposure:%d, exposure range is 0 to %u!\n",
						exposure, (1<<sensor->exposure_reg_num_bits)-1);
		}
		else
		{
			uint8_t val;
			int i, j=sensor->exposure_reg_num_bits-1;
			int num_regs = (sensor->exposure_reg_num_bits+7)>>3;

			for(i=0; i<num_regs; i++, j-=8)
			{
				val = (exposure >> (j&~7)) & 0xFF;
				modReg(mode, sensor->exposure_reg+i, 0, j&0x7, val, EQUAL);
				vcos_log_error("Set exposure %04X to %02X", sensor->exposure_reg+i, val);
			}
		}
	}
	if (sensor->vts_reg && exposure != -1 && exposure >= mode->min_vts)
	{
		if (exposure < 0 || exposure >= (1<<sensor->vts_reg_num_bits))
		{
			vcos_log_error("Invalid exposure:%d, vts range is 0 to %u!\n",
						exposure, (1<<sensor->vts_reg_num_bits)-1);
		}
		else
		{
			uint8_t val;
			int i, j=sensor->vts_reg_num_bits-1;
			int num_regs = (sensor->vts_reg_num_bits+7)>>3;

			for(i = 0; i<num_regs; i++, j-=8)
			{
				val = (exposure >> (j&~7)) & 0xFF;
				modReg(mode, sensor->vts_reg+i, 0, j&0x7, val, EQUAL);
				vcos_log_error("Set vts %04X to %02X", sensor->vts_reg+i, val);
			}
		}
	}
	if (sensor->gain_reg && gain != -1)
	{
		if (gain < 0 || gain >= (1<<sensor->gain_reg_num_bits))
		{
			vcos_log_error("Invalid gain:%d, gain range is 0 to %u\n",
						gain, (1<<sensor->gain_reg_num_bits)-1);
		}
		else
		{
			uint8_t val;
			int i, j=sensor->gain_reg_num_bits-1;
			int num_regs = (sensor->gain_reg_num_bits+7)>>3;

			for(i = 0; i<num_regs; i++, j-=8)
			{
				val = (gain >> (j&~7)) & 0xFF;
				modReg(mode, sensor->gain_reg+i, 0, j&0x7, val, EQUAL);
				vcos_log_error("Set gain %04X to %02X", sensor->gain_reg+i, val);
			}
		}
	}
}
