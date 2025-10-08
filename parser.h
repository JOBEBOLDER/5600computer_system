/*
 * file:        parser.c
 * description: skeleton code for simple shell
 *
 * Peter Desnoyers, Northeastern CS5600 Fall 2023
 */

/* standard include file protection: 
*/
#ifndef __PARSER_H__
#define __PARSER_H__

/* function declarations: 
*/
int parse(const char *line, int argc_max, char **argv, char *buf, int buf_len);

#endif

