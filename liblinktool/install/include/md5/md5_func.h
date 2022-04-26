#ifndef __MD5_FUNC_H_
#define __MD5_FUNC_H_

// function declare
int Compute_string_md5(unsigned char *dest_str, unsigned int dest_len, char *md5_str);
int Compute_file_md5(const char *file_path, char *md5_str);
#endif
