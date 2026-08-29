#include "ProvisioningStorage.h"
#include <FlashLayout.h>
#include <string.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif
namespace mindflayer { namespace provisioning {
using namespace mindflayer::flashlayout;
static const uint8_t RECORD_MAGIC[4]={'M','F','R','1'}; alignas(4) static const uint8_t COMMIT[4]={'M','F','P','C'}; alignas(4) static uint8_t io[RECORD_IO_SIZE];
static uint32_t addressFor(Copy c){return c==COPY_A?PROVISIONING_A:c==COPY_B?PROVISIONING_B:0;}
static bool owned(Copy c,uint32_t address,size_t size){uint32_t start=addressFor(c);return start&&address>=start&&size<=SECTOR_SIZE&&address+size>=address&&address+size<=start+SECTOR_SIZE;}
static uint32_t get32(const uint8_t*p){return((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void put32(uint8_t*p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static size_t aligned(size_t n){return(n+3)&~size_t(3);}
bool generationNewer(uint32_t candidate,uint32_t reference){return candidate!=reference&&(uint32_t)(candidate-reference)<0x80000000u;}
bool ProvisioningStore::readRecord(Copy copy,Provisioning& output,uint32_t& generation,bool requireCommit){
  uint32_t address=addressFor(copy);if(!owned(copy,address,aligned(RECORD_HEADER_SIZE))||!backend_.read(address,io,aligned(RECORD_HEADER_SIZE)))return false;
  if(memcmp(io,RECORD_MAGIC,4)||io[4]!=RECORD_VERSION)return false;
  generation=get32(io+5);size_t payload=((size_t)io[9]<<8)|io[10];if(!payload||payload>MAX_PAYLOAD_SIZE)return false;
  size_t dataSize=RECORD_HEADER_SIZE+payload+RECORD_CRC_SIZE,readSize=aligned(dataSize);if(readSize>sizeof(io)||!owned(copy,address,readSize)||!backend_.read(address,io,readSize))return false;
  if(get32(io+RECORD_HEADER_SIZE+payload)!=crc32(io,RECORD_HEADER_SIZE+payload))return false;
  if(requireCommit){alignas(4) uint8_t marker[4];uint32_t markerAddress=address+SECTOR_SIZE-4;if(!owned(copy,markerAddress,4)||!backend_.read(markerAddress,marker,4)||memcmp(marker,COMMIT,4))return false;}
  return decodePayload(io+RECORD_HEADER_SIZE,payload,output);
}
bool ProvisioningStore::inspect(Copy c,Provisioning&p,uint32_t&g){return readRecord(c,p,g);}
bool ProvisioningStore::load(Provisioning& output,Selection* selection){
  static Provisioning b;uint32_t ga=0,gb=0;bool va=readRecord(COPY_A,output,ga),vb=readRecord(COPY_B,b,gb);if(!va&&!vb){if(selection)*selection={NO_COPY,0};return false;}
  Copy chosen=va&&(!vb||generationNewer(ga,gb))?COPY_A:COPY_B;if(chosen==COPY_B)output=b;uint32_t generation=chosen==COPY_A?ga:gb;if(selection)*selection={chosen,generation};return true;
}
bool ProvisioningStore::writeEnvelope(const uint8_t* envelope,size_t size,Selection* result){
  static Provisioning scratch;if(!decodeEnvelope(envelope,size,scratch))return false;size_t payloadSize=((size_t)envelope[5]<<8)|envelope[6];const uint8_t*payload=envelope+ENVELOPE_HEADER_SIZE;
  Selection selected;bool hasCurrent=load(scratch,&selected);Copy target=!hasCurrent?COPY_A:selected.copy==COPY_A?COPY_B:COPY_A;uint32_t generation=hasCurrent?selected.generation+1:1,address=addressFor(target),sector=address/SECTOR_SIZE;
  if(!owned(target,address,SECTOR_SIZE)||(sector!=PROVISIONING_A/SECTOR_SIZE&&sector!=PROVISIONING_B/SECTOR_SIZE))return false;
  memset(io,0xff,sizeof(io));memcpy(io,RECORD_MAGIC,4);io[4]=RECORD_VERSION;put32(io+5,generation);io[9]=payloadSize>>8;io[10]=payloadSize;memcpy(io+RECORD_HEADER_SIZE,payload,payloadSize);put32(io+RECORD_HEADER_SIZE+payloadSize,crc32(io,RECORD_HEADER_SIZE+payloadSize));
  size_t dataSize=aligned(RECORD_HEADER_SIZE+payloadSize+RECORD_CRC_SIZE);if(!owned(target,address,dataSize)||!backend_.eraseSector(sector)||!backend_.write(address,io,dataSize))return false;
  uint32_t checked;if(!readRecord(target,scratch,checked,false)||checked!=generation)return false;uint32_t commitAddress=address+SECTOR_SIZE-4;
  if(!owned(target,commitAddress,4)||!backend_.write(commitAddress,COMMIT,4)||!readRecord(target,scratch,checked,true)||checked!=generation)return false;
  if (result) *result = {target, generation};
  return true;
}
#ifdef ARDUINO
class EspFlashBackend:public FlashBackend{
 static bool withinOwnedSector(uint32_t a,size_t n){
  if(n>SECTOR_SIZE||a+n<a)return false;
  bool inA=a>=PROVISIONING_A&&a+n<=PROVISIONING_A+SECTOR_SIZE;
  bool inB=a>=PROVISIONING_B&&a+n<=PROVISIONING_B+SECTOR_SIZE;
  return inA||inB;
 }
 public:bool read(uint32_t a,void*out,size_t n)override{return ESP.getFlashChipRealSize()>=FLASH_SIZE&&withinOwnedSector(a,n)&&a%4==0&&n%4==0&&ESP.flashRead(a,(uint32_t*)out,n);}
 bool eraseSector(uint32_t s)override{return ESP.getFlashChipRealSize()>=FLASH_SIZE&&(s==PROVISIONING_A/SECTOR_SIZE||s==PROVISIONING_B/SECTOR_SIZE)&&ESP.flashEraseSector(s);}
 bool write(uint32_t a,const void*d,size_t n)override{return ESP.getFlashChipRealSize()>=FLASH_SIZE&&withinOwnedSector(a,n)&&a%4==0&&n%4==0&&ESP.flashWrite(a,(uint32_t*)d,n);}
};static EspFlashBackend flash;static ProvisioningStore store(flash);
bool loadStored(Provisioning&p,Selection*s){return store.load(p,s);}bool storeAtomically(const uint8_t*e,size_t n,Selection*s){return store.writeEnvelope(e,n,s);}
#endif
} }
