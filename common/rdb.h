int RDBInitNetworkConfiguration(char *ip_address_str, char *subnet_mask_str, char *gateway_str, int *EthernetLinkMode);
int RDBLoadKernel(void);
int RDBLoadIOPRP(void **buffer, int *size);
void RDBInitConsole(void);
int RDBBootKernel(void);
