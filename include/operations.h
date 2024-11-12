#include "raspiraw.h"

void update_regs(const struct sensor_def *sensor, struct mode_def *mode, int hflip, int vflip, int exposure, int gain);

void send_regs(int fd, const struct sensor_def *sensor, const struct sensor_regs *regs, int num_regs);

void modRegBit(struct mode_def *mode, uint16_t reg, int bit, int value, enum operation op);

void modReg(struct mode_def *mode, uint16_t reg, int startBit, int endBit, int value, enum operation op);
