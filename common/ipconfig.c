#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>

#include <netman.h>

#include "ipconfig.h"

static char *GetNextToken(char *line, char delimiter){
	char *field_end, *result;
	static char *current_line=NULL;

	result=NULL;

	if(line!=NULL){
		current_line=line;
	}

	while(*current_line==delimiter) current_line++;
	if(current_line[0]!='\0'){
		if((field_end=strchr(current_line, delimiter))==NULL){
			field_end=&current_line[strlen(current_line)];
		}

		*field_end='\0';

		if(current_line[1]!='\0'){	//Test to see if there is another token after this one.
			result=current_line;
			current_line=field_end+1;
		}
		else current_line=NULL;
	}
	else current_line=NULL;

	return result;
}

int ParseNetAddr(const char *address, unsigned char *octlets){
	int result;
	char address_copy[16], *octlet;
	unsigned char i;

	if(strlen(address)<16){
		strcpy(address_copy, address);

		if((octlet=strtok(address_copy, "."))!=NULL){
			result=0;

			octlets[0]=strtoul(octlet, NULL, 10);
			for(i=1; i<4; i++){
				if((octlet=strtok(NULL, "."))==NULL){
					result=EINVAL;
					break;
				}
				else octlets[i]=strtoul(octlet, NULL, 10);
			}
		}
		else result=EINVAL;
	}
	else result=EINVAL;


	return result;
}

int ParseConfig(const char *path, char *ip_address, char *subnet_mask, char *gateway, int *LinkMode){
	FILE *fd;
	int result, size;
	char *FileBuffer, *line, *field;
	unsigned int i, DataLineNum;

	*LinkMode=-1;
	if((fd=fopen(path, "r"))!=NULL){
		fseek(fd, 0, SEEK_END);
		size=ftell(fd);
		rewind(fd);
		if((FileBuffer=malloc(size+1))!=NULL){
			if(fread(FileBuffer, 1, size, fd)==size){
				FileBuffer[size]='\0';

				if((line=strtok(FileBuffer, "\r\n"))!=NULL){
					result=EINVAL;
					DataLineNum=0;
					do{
						i=0;
						while(line[i]==' ') i++;
						if(line[i]!='#' && line[i]!='\0'){
							if(DataLineNum==0){
								if((field=GetNextToken(line, ' '))!=NULL){
									strncpy(ip_address, field, 15);
									ip_address[15]='\0';
									if((field=GetNextToken(NULL, ' '))!=NULL){
										strncpy(subnet_mask, field, 15);
										subnet_mask[15]='\0';
										if((field=GetNextToken(NULL, ' '))!=NULL){
											strncpy(gateway, field, 15);
											gateway[15]='\0';
											result=0;
											DataLineNum++;
										}
									}
								}
							}
							else if(DataLineNum==1){
								if(!strcmp(line, "AUTO")){
									*LinkMode=-1;
									DataLineNum++;
								}
								else if(!strcmp(line, "100MFDX")){
									*LinkMode=NETMAN_NETIF_ETH_LINK_MODE_100M_FDX;
									DataLineNum++;
								}
								else if(!strcmp(line, "100MHDX")){
									*LinkMode=NETMAN_NETIF_ETH_LINK_MODE_100M_HDX;
									DataLineNum++;
								}
								else if(!strcmp(line, "10MFDX")){
									*LinkMode=NETMAN_NETIF_ETH_LINK_MODE_10M_FDX;
									DataLineNum++;
								}
								else if(!strcmp(line, "10MHDX")){
									*LinkMode=NETMAN_NETIF_ETH_LINK_MODE_10M_HDX;
									DataLineNum++;
								}
							}
						}
					}while((line=strtok(NULL, "\r\n"))!=NULL);
				}
				else result=EINVAL;
			}
			else result=EIO;

			free(FileBuffer);
		}
		else result=ENOMEM;

		fclose(fd);
	}
	else result=ENOENT;

	return result;
}
