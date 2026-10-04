#include "../ship/Protocol.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cassert>
#include <chrono>
#include <thread>
int main(int argc,char**argv){
 using namespace composite;
 int fd=open(argv[1],O_CREAT|O_RDWR|O_TRUNC,0600);assert(fd>=0);assert(ftruncate(fd,SIZE)==0);
 auto* m=(uint8_t*)mmap(nullptr,SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);assert(m!=MAP_FAILED);
 memset(m,0,SIZE);release(m,MAGIC);release(m+4,VERSION);
 Port p{};p.epoch=123;p.x=40;p.y=-80;p.z=120;p.buttons=17;write(m,PORT,p);
 StoryPort sp{};sp.flags=3;sp.items[47]=49;sp.ammo[6]=12;sp.commandSeq=7;sp.wheel=-2;write(m,STORY_PORT,sp);
 BlockLayer layer{};layer.epoch=123;layer.count=2;layer.remaining=14;layer.aimed=1;layer.positions[1]={40,80,-120};write(m,BLOCKS,layer);
 for(int i=0;i<5000;i++){
  auto q=acquire(m+REQUEST);if(q){
   Request r{};memcpy(&r,m+REQUEST+4,sizeof(r));assert(r.epoch==123&&r.x==40&&r.dy==-3);
   Reply result{r.dx,0,r.dz};memcpy(m+REPLY+4,&result,sizeof(result));release(m+REPLY,q);
   Minecraft mc{};
   for(int j=0;j<5000;j++){if(read(m,MC,mc)&&mc.tick==42){assert(mc.epoch==123&&mc.bx==80&&mc.block==1);StoryMinecraft sm{};assert(read(m,STORY_MC,sm)&&sm.epoch==123&&sm.hotbar[8]==253&&sm.inventoryOpen==1&&sm.commandAck==7);return 0;}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
   return 2;
  }std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }return 3;
}
