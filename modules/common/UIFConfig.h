struct UIFBootConfiguration
{
    u8 BootupMcSlot;
    struct
    {
        u8 noAuto;
        u16 conf;
    } smap;
    u8 ip_address[4];
    u8 subnet_mask[4];
    u8 gateway[4];
};
