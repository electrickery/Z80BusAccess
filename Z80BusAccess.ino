/*
 * Z80 bus access
 */
 
#include "Z80pins.h"

#define SERIALBUFSIZE         90
char serialBuffer[SERIALBUFSIZE];
byte setBufPointer = 0;

#define LED 13

uint16_t repeatRate = 1 < 9;
#define RECORDSIZE 32
#define DATARECORDTYPE 0

#define DUMPPAGE 0x0100
unsigned int lastEndAddress = 0;
unsigned int addressOffset = 0;

void setup() {

  Serial.begin(9600);
  Serial.println("Z80busAccess v0.1");
  
  pinMode(LED, OUTPUT);
  
  pinMode(Z80INT,   INPUT);
  pinMode(Z80NMI,   INPUT);
  pinMode(Z80WAIT,  INPUT);
  pinMode(Z80BUSRQ, INPUT);
  pinMode(Z80RESET, INPUT); 
  DDRA  = 0X00; // address LSB input
  DDRC  = 0X00; // address MSB input
  DDRF  = 0X00; // signals input
  DDRL  = 0X00; // data bus input  
  triStating();
}

void loop() {
  commandCollector();
}

void triStating() {
  DDRA  = 0X00; // address LSB input
  DDRC  = 0X00; // address MSB input
  DDRL  = 0X00; // data bus input
  DDRK  = 0X00; // all control out bits input
  digitalWrite(Z80BUSRQ, HIGH);           // give up bus access
  digitalWrite(LED, HIGH);
}

void activeState() {
  pinMode(Z80BUSRQ, OUTPUT);              // request for bus access
  digitalWrite(Z80BUSRQ, LOW);
  while(digitalRead(Z80BUSAK) == HIGH) {  // wait for CPU response. 
    digitalWrite(LED, !digitalRead(LED));
    Serial.print("Z80BUSAK: ");
    Serial.println(digitalRead(Z80BUSAK));
  }
  digitalWrite(LED, LOW);
  DDRA  = 0XFF; // address LSB output
  DDRC  = 0XFF; // address MSB output
  PORTK = 0XFF; // control out bits high
  DDRK  = 0X0F; // control out bits (WR,RD,MREQ,IORQ) output,(M1,RFSH,HALT,BUSACK) input
  DDRL  = 0X00; // data bus input
  PORTL = 0XFF; // data bus pull ups
}

void dataBusReadMode() {
  activeState();
  DDRL  = 0x00;  // read mode
}

void dataBusWriteMode() {
  activeState();
  DDRL  = 0xFF;  // write mode
}

void clearSerialBuffer() {
  byte i;
  for (i = 0; i < SERIALBUFSIZE; i++) {
    serialBuffer[i] = 0;
  }
}

void commandCollector() {
  if (Serial.available() > 0) {
    int inByte = Serial.read();
    switch(inByte) {
    case '.':
//    case '\r':
    case '\n':
      commandInterpreter();
      clearSerialBuffer();
      setBufPointer = 0;
      break;
    case '\r':
      break;  // ignore carriage return
    default:
      serialBuffer[setBufPointer] = inByte;
      setBufPointer++;
      if (setBufPointer >= SERIALBUFSIZE) {
        Serial.println("Serial buffer overflow. Cleanup.");
        clearSerialBuffer();
        setBufPointer = 0;
      }
    }
  }
}

void commandInterpreter() {
  byte bufByte = serialBuffer[0];
  
  switch(bufByte) {
    case 'C':
    case 'c':
      calcChecksum();
      break;
    case 'D':  // dump memory
    case 'd':
      dumpMemory();
      break;
    case 'F':  // dump memory
    case 'f':
      setValue();
      break;
    case 'I':
    case 'i':
      generateDataRecords();
      generateEndRecord();
      break;
    case 'T':  // 
      testBus();
      break;
    case 'H':  // help
    case 'h':
    case '?':  // help
//      Serial.println("F?:");
      usage();
      break; 
    case ':':  // hex-intel record
      hexIntelInterpreter();
      break;
    default:
      Serial.print(bufByte);
      Serial.print(" ");
      Serial.println("unsupported");
  }
  return;
}

void testBus() {
  while (1) {
    pinMode(Z80BUSRQ, OUTPUT);
    digitalWrite(Z80BUSRQ, !digitalRead(Z80BUSRQ));
    Serial.print("BUSACK: ");
    Serial.println(digitalRead(Z80BUSAK));
    delay(200);
    if (Serial.available() > 0) {
      return;
    }
  }
}

void usage() {
  Serial.println("-- Z80busAccess v0.1 command set --");
  Serial.println("Cssss-eeee       - Calculate checksum from ssss to eeee");
  Serial.println("D[ssss[-eeee]|+] - Dump memory from ssss to eeee (default 256 bytes)");
  Serial.println("Fssss-eeee:v     - fill a memory range with a value");
  Serial.println("Issss-eeee       - Generate hex intel data records");
  Serial.println(":ssaaaatthhhh...hhcc - accepts hex intel record");
  Serial.println("H                - This help text");
  Serial.println(";ssss-eeee       - Generate hex intel data records");
  Serial.println("E                - Generate hex intel end record");
}

void dumpMemory() {
  unsigned int startAddress;
  unsigned int endAddress;
  bool repeatMode = 0;
  if (setBufPointer == 1 ) {
    startAddress = lastEndAddress;
    lastEndAddress = endAddress;
    endAddress = startAddress + DUMPPAGE;
  } else if (setBufPointer == 2 && serialBuffer[1] == '+') {
    startAddress = lastEndAddress - DUMPPAGE;
    lastEndAddress = endAddress;
    endAddress = startAddress + DUMPPAGE;
    repeatMode = 1;
  } else if (setBufPointer == 5) {
    startAddress = get16BitValue(1);
    lastEndAddress = endAddress;
    endAddress = startAddress + DUMPPAGE;
  } else if (setBufPointer == 10) {
    startAddress = get16BitValue(1);
    endAddress = get16BitValue(6);
    lastEndAddress = endAddress;
  } else {
    Serial.println("unsupported"); 
    return;
  }
  unsigned char asChars[17];
  unsigned char *asCharsP = &asChars[0];
  unsigned char positionOnLine;
  asChars[16] = 0;
  do {
    // show range
    printWord(startAddress);
    Serial.print("-");
    printWord(endAddress - 1);
    Serial.println();
    unsigned int i, data;
    dataBusReadMode();
    for (i = startAddress; i < endAddress; i++) {
      positionOnLine = i & 0x0F;
      if (positionOnLine == 0) {
        printWord(i);   // Address at start of line
        Serial.print(": ");
      }
      data = readByte(i);
      printByte(data);   // actual value in hex
      // fill an array with the ASCII part of the line
      asChars[positionOnLine] = (data >= ' ' && data <= '~') ? data : '.';
      if ((i & 0x03) == 0x03) Serial.print(" ");
      if ((i & 0x0F) == 0x0F) {
        Serial.print (" ");
        printString(asCharsP); // print the ASCII part
        Serial.println("");
      }
    }
    Serial.println();
    delay(repeatRate);
    if (Serial.available() > 0) {
      clearSerialBuffer();
      setBufPointer = 0;
      repeatMode = 0;
      triStating();
      return;
    }
  } while (repeatMode);
  triStating();
}

/*
 * :ccrraaaadddd....ddss
 * || | |   |         |
 * || | |   |         checksum on all bytes, starting with byte count
 * || | |   data byte
 * || | address
 * || record type; 00 is data record, 01 is end record (end of file)
 * |byte count, usually 10 (16) or 20 (32)
 * start character
 * 
 */
void hexIntelInterpreter() {
  unsigned int count;
  unsigned int sumCheck = 0;
  count  = getNibble(serialBuffer[1]) * (1 << 4);
  count += getNibble(serialBuffer[2]);
  sumCheck += count;
  unsigned int addressLSB, addressMSB, baseAddress;
  addressMSB  = get8BitValue(3);
  addressLSB  = get8BitValue(5);
  sumCheck += addressMSB;
  sumCheck += addressLSB;
  baseAddress = (addressMSB << 8) + addressLSB;
//  printWord(baseAddress - addressOffset);
//  Serial.println();
  unsigned int recordType;
  recordType  = get8BitValue(7);
  if (recordType == 1) { // End of file record type
    triStating();
    return;
  }
  if (recordType != 0) { // ignore all but data records
    return; 
  }
  sumCheck +=  recordType;
  unsigned int i, sbOffset;
  byte value;
  dataBusWriteMode();
  for (i = 0; i < count; i++) {
    sbOffset = (i * 2) + 9;
    value     = get8BitValue(sbOffset); 
    sumCheck +=  value;
    writeByte(baseAddress + i - addressOffset, value);
  }
  triStating();
  unsigned sumCheckValue;
  sbOffset += 2;
  sumCheckValue  = get8BitValue(sbOffset);
  sumCheck      += sumCheckValue;
  sumCheck &= 0xFF;
  if (sumCheck == 0) {
    printWord(baseAddress - addressOffset);
    Serial.println();
  } else {
    Serial.print("Sumcheck incorrect: ");
    printByte(sumCheck); 
  }
}

void generateDataRecords() {
  unsigned int startAddress;
  unsigned int endAddress;
  startAddress = get16BitValue(1);
  endAddress   = get16BitValue(6);
  printWord(startAddress);
  Serial.print("-");
  printWord(endAddress);
  Serial.println();

  unsigned int i, j;
  unsigned char addressMSB, addressLSB, data;
  unsigned char sumCheckCount = 0;

  dataBusReadMode();  
  for (i = startAddress; i < endAddress; i += RECORDSIZE) {
    sumCheckCount = 0;
    Serial.print(":");
    printByte(RECORDSIZE);  
    sumCheckCount -= RECORDSIZE;
    addressMSB = i >> 8;
    addressLSB = i & 0xFF;
    printByte(addressMSB);
    printByte(addressLSB);
    sumCheckCount -= addressMSB;
    sumCheckCount -= addressLSB;
    printByte(DATARECORDTYPE);
    sumCheckCount -= DATARECORDTYPE;
    for (j = 0; j < RECORDSIZE; j++) {
      data = readByte(i + j);
      printByte(data);
      sumCheckCount -= data;
    }
    printByte(sumCheckCount);
    Serial.println();
  }
  triStating();
}

void generateEndRecord() {
  Serial.println(":00000001FF");
}

void setValue() {
  unsigned int startAddress;
  unsigned int endAddress;
  byte value;
  startAddress = get16BitValue(1);
  endAddress   = get16BitValue(6);
  value        = get8BitValue(11);
  
  dataBusWriteMode();
  unsigned int i;
  for (i = startAddress; i <= endAddress; i++) {
    writeByte(i, value);
  }
  Serial.print("S:");
  printWord(startAddress);
  Serial.print("-");
  printWord(endAddress);
  Serial.print(" with ");
  printByte(value);
  Serial.println();
  triStating();
}

void calcChecksum() {
  unsigned int startAddress;
  unsigned int endAddress;
  unsigned char value;
  // Cssss eeee
  startAddress  = get16BitValue(1);
  endAddress  = get16BitValue(6);

  Serial.print("Checksum block ");
  Serial.print(startAddress, HEX);
  Serial.print("h - ");
  Serial.print(endAddress, HEX);
  Serial.print("h : ");
  dataBusReadMode();
  Serial.print(blockChecksum(startAddress,endAddress), HEX);
  triStating();
  Serial.println("h");
}

unsigned int get16BitValue(byte index) {
  byte i = index;
  unsigned address;
  address  = getNibble(serialBuffer[i++]) * (1 << 12);
  address += getNibble(serialBuffer[i++]) * (1 << 8);
  address += getNibble(serialBuffer[i++]) * (1 << 4);
  address += getNibble(serialBuffer[i++]);
  return address;
}

byte get8BitValue(byte index) {
  byte i = index;
  byte data;
  data  = getNibble(serialBuffer[i++]) * (1 << 4);
  data += getNibble(serialBuffer[i++]);
  return data;
}

void printByte(unsigned char data) {
  unsigned char dataMSN = data >> 4;
  unsigned char dataLSN = data & 0x0F;
  Serial.print(dataMSN, HEX);
  Serial.print(dataLSN, HEX);
}

void printWord(unsigned int data) {
  printByte(data >> 8);
  printByte(data & 0xFF);
}

void writeByte(unsigned int address, unsigned int value) {
  unsigned int addressLSB = address & 0xFF;
  unsigned int addressMSB = address >> 8;
  dataBusWriteMode();
  PORTA = addressLSB;
  PORTC = addressMSB;
  PORTL = value;
  digitalWrite(Z80MREQ, LOW);
  digitalWrite(Z80WR,   LOW);
  delayMicroseconds(10);
  digitalWrite(Z80WR,   HIGH);
  digitalWrite(Z80MREQ, HIGH);
  dataBusReadMode();
}

unsigned int readByte(unsigned int address) {
  unsigned int data = 0;
  unsigned int addressLSB = address & 0xFF;
  unsigned int addressMSB = address >> 8;
  dataBusReadMode();
  PORTL = 0xFF; // enable pull ups
  PORTA = addressLSB;
  PORTC = addressMSB;
  digitalWrite(Z80MREQ, LOW);
  digitalWrite(Z80RD,   LOW);
  delayMicroseconds(10);
  data = PINL;
  digitalWrite(Z80RD,   HIGH);
  digitalWrite(Z80MREQ, HIGH); 
  return data; 
}

void printString(unsigned char *asCharP) {
  unsigned char i = 0;
  while(asCharP[i] != 0) {
    Serial.write(asCharP[i]); 
//    Serial.print(asCharP[i], HEX);
    i++;
  }
}

int getNibble(unsigned char myChar) {
  int nibble = myChar;
  if (nibble > 'F') nibble -= ' ';  // lower to upper case
  nibble -= '0';
  if (nibble > 9) nibble -= 7; // offset 9+1 - A
  return nibble;
}

unsigned int blockChecksum(unsigned long startAddress, unsigned long endAddress)
{
  unsigned long checksum = 0;
  for (unsigned long i=startAddress; i<=endAddress; i++) {
    checksum += readByte(i);  
  }
  return checksum;
}
