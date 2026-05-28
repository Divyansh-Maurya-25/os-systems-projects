#ifndef __SERIAL_H__
#define __SERIAL_H__

/*
 * Project 2 — Parallel Text Compressor (TA-safe pthread-only version)
 * Group Members:
 *   Divyansh Maurya
 *   Arjan Subedi
 *   Ayush Poudel
 *   Aidan Lauser
 *
 * Description:
 *   Public header exposing the compress_directory() function
 *   used by main.c. The implementation is in serial.c.
 */

void compress_directory(char *directory_name);

#endif
