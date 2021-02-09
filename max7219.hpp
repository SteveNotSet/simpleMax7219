#include "./charset.h"
#include <SPI.h>
//#define DIN 13 //GPIO13,D7
//#define CLK 14 //GPIO14,D5
#define CS 16 //GPIO16,D0

class maxPic{
	public:
		uint8_t data[32];	//图像数据
		uint8_t buffer[32];	//用于滚动字幕等操作的图像数据缓冲区
		//uint32_t width;
		//uint32_t height;	//以后再实现可变大小
		void moveLeft(uint8_t distance);
		void moveUp(uint8_t distance);
		void moveDown(uint8_t distance);
		void putChar(const void* charset, const char character, uint32_t pos);
		void putChar(const char character, uint32_t pos);
		uint64_t putString(const void* charset, String text, uint32_t pos, uint8_t charWidth);
		void clearData(void);
		void clearBuffer(void);
		void dataToBuffer(void);
		void bufferToData(void);
		void dataToBuffer(maxPic fromData);
		void bufferToData(maxPic fromBuffer);
		void joinData(maxPic fromData, char method);
};

void maxPic::moveLeft(uint8_t distance){
	for(char i=0;i<24;i++){
		data[i] = (data[i] << distance) | (data[i+8] >> (8-distance));			//前3块左移
	}
	for(char i=24;i<32;i++){
		data[i] = (data[i] << distance) | (buffer[i-24] >> (8-distance));										//第4块左移
	}
	for(char i=0;i<32;i++){
		buffer[i] = (buffer[i] << distance) | (buffer[i+8] >> (8-distance));	//缓冲区4块左移
	}
}

void maxPic::moveUp(uint8_t distance){
	for(char i=0;i<32;i++){
		if(i%8>=(8-distance)){	//图像最下面distance行
			data[i]=buffer[i-8+distance];
		}else{					//图像上面行
			data[i]=data[i+distance];
		}
	}
	for(char i=0;i<32;i++){
		if(i%8>=(8-distance)){	//缓冲区最下面distance行
			buffer[i]=0;
		}else{					//缓冲区上面行
			buffer[i]=buffer[i+distance];
		}
	}
}

void maxPic::moveDown(uint8_t distance){
	moveUp(8-distance);
}

void maxPic::dataToBuffer(void){
	for(char i=0;i<32;i++){
		buffer[i]=data[i];
	}
}

void maxPic::bufferToData(void){
	for(char i=0;i<32;i++){
		data[i]=buffer[i];
	}
}

void maxPic::dataToBuffer(maxPic fromData){
	for(char i=0;i<32;i++){
		buffer[i]=fromData.data[i];
	}
}

void maxPic::bufferToData(maxPic fromBuffer){
	for(char i=0;i<32;i++){
		data[i]=fromBuffer.buffer[i];
	}
}

void maxPic::putChar(const void* charset/*字符集在flash中的位置*/, const char character, uint32_t pos){
	char posBlock = pos >> 3;	//获取块位置
	char posDetail = pos % 8;	//获取偏移
	unsigned char picData;		//暂存字库数据
	if(posBlock>=3){
		for(char j=0;j<8;j++){
			picData = pgm_read_byte(charset+8*character-0x0100+j);
			data[8*posBlock+j] |= picData >> posDetail;
		}
	}else if(posBlock<3){
		for(char j=0;j<8;j++){
			picData = pgm_read_byte(charset+8*character-0x0100+j);
			data[8*posBlock+j] |= picData >> posDetail;
			data[8*posBlock+j+8] |= picData << (8-posDetail);
		}
	}
}

void maxPic::putChar(const char character, uint32_t pos){
	char posBlock = pos >> 3;	//获取块位置
	char posDetail = pos % 8;	//获取偏移
	unsigned char picData;		//暂存字库数据
	if(posBlock>=3){
		for(char j=0;j<8;j++){
			picData = pgm_read_byte(bigFont+8*character-0x0100+j);
			data[8*posBlock+j] |= picData >> posDetail;
		}
	}else if(posBlock<3){
		for(char j=0;j<8;j++){
			picData = pgm_read_byte(bigFont+8*character-0x0100+j);
			data[8*posBlock+j] |= picData >> posDetail;
			data[8*posBlock+j+8] |= picData << (8-posDetail);
		}
	}
}

uint64_t maxPic::putString(const void* charset, String text, uint32_t pos, uint8_t charWidth){
	uint64_t i;
	for(i=0;i<text.length();i++){
		putChar(charset, text[i], pos+charWidth*i);
	}
	return pos+(i+1)*charWidth;
}

void maxPic::clearData(void){
	for(char i=0;i<32;i++){
		data[i]=0;
	}
}

void maxPic::clearBuffer(void){
	for(char i=0;i<32;i++){
		buffer[i]=0;
	}
}

void maxPic::joinData(maxPic fromData, char method){
	for(char i=0;i<32;i++){
		switch(method){
			case 0:
				data[i]|=fromData.data[i];
				break;
			case 1:
				data[i]&=fromData.data[i];
				break;
			case 2:
				data[i]^=fromData.data[i];
				break;
		}
	}
}

//MAX7219驱动函数
void matrixInit() {
	const unsigned char operatingCode[5][2]={
		{0x09, 0x00},	//DECODEMODE
		{0x0a, 0x00},	//INTENSITY
		{0x0b, 0x07},	//SCANLIMIT
		{0x0c, 0x01},	//SHUTDOWN
		{0x0f, 0x00}	//DISPLAYTEST
	};
	for(char j=0;j<5;j++){
		digitalWrite(CS, LOW);
		for(char i=0;i<4;i++){
			SPI.transfer(operatingCode[j][0]);
			SPI.transfer(operatingCode[j][1]);
		}
		digitalWrite(CS, HIGH);
	}
}

void refresh(maxPic data) {
	for(char i=0;i<8;i++){
		digitalWrite(CS, LOW);	//片选
		for(char j=0;j<4;j++){
			SPI.transfer(i+1);	//写入数码管编号
			SPI.transfer(data.data[j*8+i]);//写入数据
		}
		digitalWrite(CS, HIGH);
	}
}

void scrollText(String text, uint32_t Delay, char charWidth, const void* charset, maxPic& data, bool doRefresh){
	for(long i=0;i<text.length();i++){	//逐字滚出
		for(char j=0;j<8;j++){
			data.buffer[j]=pgm_read_byte(charset+8*text[i]-0x0100+j);	//将字符读入缓冲区
		}
		for(char j=0;j<charWidth;j++){	//左移
			data.moveLeft(1);
			if(doRefresh==true){refresh(data);}
			delay(Delay);
		}
	}
}

void pageTurnUp(maxPic screen1, maxPic screen2){
	const uint16_t delayPx[8] = {//每一行的延时时间(ms)，用来实现非线性动画
		/*250,104,79,67,67,79,104,250*//*125,52,40,33,33,40,52,125*/50,21,15,13,13,15,21,50
	};
	screen1.clearBuffer();
	screen1.dataToBuffer(screen2);
	refresh(screen1);
	for(uint8_t i=0;i<8;i++){
		screen1.moveUp(1);
		delay(delayPx[i]);
		refresh(screen1);
	}
}
