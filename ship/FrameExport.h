// Local RGBA transport. Called only on SDL's GL render thread, before swap.
#include <vector>
extern "C" void CompositeExportFrame(SDL_Window* win) {
 const char* base=std::getenv("COMPOSITE_FRAME");if(!base)return;
 constexpr size_t capacity=64+2560*1440*4;
 static uint8_t* mem=nullptr;static bool failed=false;static int fd=-1;static std::vector<uint8_t> pixels;
 if(failed)return;
 if(!mem){
  fd=open(base,O_RDWR|O_CREAT|O_NOFOLLOW,0600);struct stat st{};
  if(fd<0||fstat(fd,&st)||st.st_uid!=getuid()||!S_ISREG(st.st_mode)||flock(fd,LOCK_EX|LOCK_NB)||ftruncate(fd,capacity)){if(fd>=0)close(fd);failed=true;return;}
  auto p=mmap(nullptr,capacity,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);if(p==MAP_FAILED){close(fd);failed=true;return;}
  mem=(uint8_t*)p;std::memset(mem,0,64);
 }
 int w,h;SDL_GL_GetDrawableSize(win,&w,&h);if(w<=0||h<=0||w>2560||h>1440)return;
 // GLX defers default backbuffer allocation across window resizes until a swap.
 // Reading the stale drawable can fault in NVIDIA's driver. Present once per
 // size change, then capture the next fully rendered frame at that size.
 static int drawableWidth=0,drawableHeight=0;
 if(w!=drawableWidth||h!=drawableHeight){SDL_GL_SwapWindow(win);drawableWidth=w;drawableHeight=h;return;}
 using Read=void(*)(int,int,int,int,unsigned,unsigned,void*);
 using Get=void(*)(unsigned,int*);using Bind=void(*)(unsigned,unsigned);using Pixel=void(*)(unsigned,int);
 static auto read=(Read)SDL_GL_GetProcAddress("glReadPixels");static auto get=(Get)SDL_GL_GetProcAddress("glGetIntegerv");
 static auto bind=(Bind)SDL_GL_GetProcAddress("glBindFramebuffer");static auto pixel=(Pixel)SDL_GL_GetProcAddress("glPixelStorei");
 if(!read||!get||!bind||!pixel)return;
 int old=0,alignment=0;get(0x8CAA,&old);get(0x0D05,&alignment);bind(0x8CA8,0);pixel(0x0D05,1);
 pixels.resize((size_t)w*h*4);read(0,0,w,h,0x1908,0x1401,pixels.data());bind(0x8CA8,old);pixel(0x0D05,alignment);
 auto seq=composite::acquire(mem);composite::release(mem,seq+1);__atomic_thread_fence(__ATOMIC_SEQ_CST);
 uint32_t header[]={0x46524D31,(uint32_t)w,(uint32_t)h,(uint32_t)getpid()};std::memcpy(mem+4,header,sizeof(header));
 std::memcpy(mem+20,&renderedCamera,sizeof(renderedCamera));
 for(int y=0;y<h;y++)std::memcpy(mem+64+(size_t)y*w*4,pixels.data()+(size_t)y*w*4,(size_t)w*4);
 composite::release(mem,seq+2);
}
extern "C" int CompositeHideNativeHud(){return std::getenv("COMPOSITE_FRAME")!=nullptr;}
