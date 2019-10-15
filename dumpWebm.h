#ifndef __DUMP_WEBM__
#define __DUMP_WEBM__
void webm_init();
void webm_deinit();
void webm_write_frame(const unsigned char *data, unsigned int dataLen, bool keyframe);
#endif
