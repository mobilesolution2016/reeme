struct PckHeader
{
	uint16_t			cmdId;
	uint16_t			flags;
	uint32_t			crc32;
	uint32_t			bodyLeng;
	uint32_t			decompLeng;
} ;

enum PckCommands {
	kCmdRequest = 1,
};

enum PckFlags {

};