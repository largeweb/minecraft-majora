#include "Protocol.h"
#include "DirtTexture.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include <SDL2/SDL.h>
#include <libultraship/libultraship.h>
#include <fast/Fast3dGui.h>
#include <ship/window/gui/resource/GuiTexture.h>
#include <mutex>
#include <vector>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include "2s2h/BenJsonConversions.hpp"
extern "C" {
#include "functions.h"
#include "variables.h"
#include "padutils.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
void func_8088ACE0(EnArrow*, PlayState*);
#include "assets/objects/gameplay_keep/gameplay_keep.h"
extern TexturePtr gItemIcons[131];
void func_80834DB8(Player*, PlayerAnimationHeader*, f32, PlayState*);
}

namespace {
using namespace composite;
uint8_t* shared=nullptr;
int descriptor=-1;
Port port{};
Minecraft mc{};
BlockLayer blocks{};
int dirtBg[MAX_BLOCKS];
bool active=false,sceneReady=false,wantStart=false;
std::atomic<bool> captured{false},recapture{false},nativeMenuOpen{false};
uint32_t lastTick=0,lastReply=0;
uint64_t lastMinecraft=0;
u16 frozenTime=0;
std::atomic<uint32_t> buttons{0};
// Audio polls outside the gameplay input path. Publish one coherent, expiring
// snapshot rather than letting the audio thread read mutable PlayState memory.
std::atomic<uint64_t> instrumentInput{0};
std::atomic<int32_t> mouseX{0},mouseY{0};
std::atomic<bool> toggle{false},releaseMouse{false};
SDL_Window* window=nullptr;
bool story=false;
struct NativeCameraFrame {uint32_t epoch;float x,y,z,yaw,pitch,fov;uint32_t valid,sequence,scene;float light;};
static_assert(sizeof(NativeCameraFrame)==44);
NativeCameraFrame renderedCamera{};
StoryPort storyPort{};
StoryMinecraft storyMC{};
std::atomic<int32_t> wheel{0};
std::atomic<uint32_t> uiCommand{0};
std::atomic<bool> inventoryShown{false},hideNativeMenu{false};
void storyPump();
void storyCollider();
bool blockTarget(int bg,CollisionPoly* poly);
bool storyCameraFree();
void storyGuiInit();
void storyMenuInput();
void storyStop();
bool world();bool clockTown();
void saveCheckpoint(){
 if(!world()||!clockTown()||gPlayState->msgCtx.msgLength||Player_InCsMode(gPlayState)||!CHECK_QUEST_ITEM(QUEST_SONG_HEALING))return;
 try{
  Play_SaveCycleSceneFlags(gPlayState);
  json j={{"format",1},{"save",gSaveContext.save}};
  std::ofstream out("composite-checkpoint.json.new");out<<j.dump(2);out.close();
  if(!out)throw std::runtime_error("checkpoint write failed");
  std::filesystem::rename("composite-checkpoint.json.new","composite-checkpoint.json");
  spdlog::info("[Composite] Clock Town checkpoint saved after Song of Healing");
 }catch(const std::exception& e){spdlog::error("[Composite] Checkpoint: {}",e.what());}
}

bool clockTown(){return gPlayState && gPlayState->sceneId>=SCENE_TOWN && gPlayState->sceneId<=SCENE_ALLEY;}
bool world(){return sceneReady && gPlayState && GET_PLAYER(gPlayState);}
bool live(){return active && mc.epoch==port.epoch && lastMinecraft && SDL_GetTicks64()-lastMinecraft<1000;}
void capture(bool on){
 if(std::getenv("COMPOSITE_FRAME"))on=false;
 captured=on;SDL_SetRelativeMouseMode(on?SDL_TRUE:SDL_FALSE);
 if(!on)buttons=0;
}
int events(void*,SDL_Event* e){
 if(e->type==SDL_KEYDOWN && !e->key.repeat){
  if(e->key.keysym.scancode==SDL_SCANCODE_F8)toggle=true;
  if(e->key.keysym.scancode==SDL_SCANCODE_ESCAPE){if(inventoryShown)hideNativeMenu=true;else releaseMouse=true;}
 }
 if(e->type==SDL_WINDOWEVENT && e->window.event==SDL_WINDOWEVENT_FOCUS_LOST)releaseMouse=true;
 if(story&&active&&!inventoryShown&&!nativeMenuOpen){
  if(e->type==SDL_WINDOWEVENT&&e->window.event==SDL_WINDOWEVENT_FOCUS_GAINED)recapture=true;
  if(!captured&&((e->type==SDL_KEYDOWN&&e->key.keysym.scancode!=SDL_SCANCODE_ESCAPE)||e->type==SDL_MOUSEBUTTONDOWN))recapture=true;
 }
 if(story&&active&&nativeMenuOpen&&!inventoryShown)return 1;
 if(!captured && !recapture && !(story && (inventoryShown||!active)))return 1;
 if(e->type==SDL_MOUSEWHEEL)wheel+=e->wheel.y;
 if(e->type==SDL_MOUSEMOTION){mouseX+=e->motion.xrel;mouseY+=e->motion.yrel;}
 uint32_t bit=0;
 if(e->type==SDL_KEYDOWN||e->type==SDL_KEYUP){
  switch(e->key.keysym.scancode){
   case SDL_SCANCODE_W:bit=1;break;case SDL_SCANCODE_S:bit=2;break;
   case SDL_SCANCODE_A:bit=4;break;case SDL_SCANCODE_D:bit=8;break;
   case SDL_SCANCODE_SPACE:bit=16;break;case SDL_SCANCODE_E:bit=64;break;
   case SDL_SCANCODE_LSHIFT:bit=1<<17;break;case SDL_SCANCODE_LCTRL:bit=1<<18;break;
   case SDL_SCANCODE_RETURN:bit=1<<19;break;case SDL_SCANCODE_ESCAPE:bit=1<<20;break;
   case SDL_SCANCODE_F5:bit=1<<21;break;case SDL_SCANCODE_TAB:bit=1<<22;break;
   case SDL_SCANCODE_UP:bit=1<<23;break;case SDL_SCANCODE_DOWN:bit=1<<24;break;
   case SDL_SCANCODE_LEFT:bit=1<<25;break;case SDL_SCANCODE_RIGHT:bit=1<<26;break;
   default:break;
  }
  if(e->key.keysym.scancode>=SDL_SCANCODE_1 && e->key.keysym.scancode<=SDL_SCANCODE_9)bit=1u<<(8+e->key.keysym.scancode-SDL_SCANCODE_1);
  if(e->type==SDL_KEYDOWN)buttons.fetch_or(bit);else buttons.fetch_and(~bit);
 }
 if((e->type==SDL_MOUSEBUTTONDOWN||e->type==SDL_MOUSEBUTTONUP)&&(e->button.button==SDL_BUTTON_LEFT||e->button.button==SDL_BUTTON_RIGHT)){
  uint32_t b=e->button.button==SDL_BUTTON_LEFT?32:128;
  if(e->type==SDL_MOUSEBUTTONDOWN)buttons.fetch_or(b);else buttons.fetch_and(~b);
 }
 return 1;
}
void stop(){if(story)storyStop();active=false;port.active=0;capture(false);mc.block=0;lastMinecraft=0;}
void start(){
 if(!world())return;
 if(!story&&!clockTown()){
  gPlayState->nextEntrance=ENTRANCE(SOUTH_CLOCK_TOWN,0);gSaveContext.nextCutsceneIndex=0;
  gPlayState->transitionTrigger=TRANS_TRIGGER_START;gPlayState->transitionType=TRANS_TYPE_FADE_BLACK;
  wantStart=true;return;
 }
 auto p=GET_PLAYER(gPlayState);port.epoch++;port.active=story?2:1;
 port.x=p->actor.world.pos.x;port.y=p->actor.world.pos.y;port.z=p->actor.world.pos.z;
 mc={};blocks={};storyMC={};inventoryShown=false;lastMinecraft=0;lastTick=0;active=true;wantStart=false;
 frozenTime=gSaveContext.save.time;capture(true);
 spdlog::info("[Composite] Waiting for Minecraft; F8/ESC releases capture. Epoch {}",port.epoch);
}

// Sweep the Minecraft player capsule against the port's actual collision world.
// Substeps prevent tunnelling at low render rates. The one block is resolved by
// Minecraft's VoxelShape code before and after this world query.
Reply collide(const Request& r){
 if(!world()||!active||r.epoch!=port.epoch)return {0,0,0};
 const float vals[]={r.x,r.y,r.z,r.dx,r.dy,r.dz};
 for(float f:vals)if(!std::isfinite(f)||std::abs(f)>100000)return {0,0,0};
 if(std::abs(r.dx)>200||std::abs(r.dy)>200||std::abs(r.dz)>200)return {0,0,0};
 auto* actor=&GET_PLAYER(gPlayState)->actor;
 auto* player=GET_PLAYER(gPlayState);
 float height=72,radius=12;
 if(std::getenv("COMPOSITE_FRAME")&&player->ageProperties){
  height=player->ageProperties->ceilingCheckHeight;radius=player->ageProperties->wallCheckRadius;
  float h;std::memcpy(&h,shared+44,4);if(std::isfinite(h)&&h>=8&&h<=height)height=h;
 }
 // Minecraft already resolves its exact VoxelShapes. Do not resolve mirrored
 // full cubes a second time as Zelda cylinders (this blocked corners/steps).
 struct CubeQueryGuard {
  bool enabled;
  CubeQueryGuard(bool value):enabled(value){if(enabled)for(uint32_t j=0;j<blocks.count;j++)if(dirtBg[j]!=BG_ACTOR_MAX)DynaPoly_DisableCollision(gPlayState,&gPlayState->colCtx.dyna,dirtBg[j]);}
  ~CubeQueryGuard(){if(enabled)for(uint32_t j=0;j<blocks.count;j++)if(dirtBg[j]!=BG_ACTOR_MAX)DynaPoly_EnableCollision(gPlayState,&gPlayState->colCtx.dyna,dirtBg[j]);}
 } cubes(std::getenv("COMPOSITE_FRAME")!=nullptr);
 Vec3f cur{r.x,r.y,r.z};
 int steps=std::clamp((int)std::ceil(std::max({std::abs(r.dx),std::abs(r.dy),std::abs(r.dz)})/8),1,32);
 for(int i=0;i<steps;i++){
  Vec3f next{cur.x+r.dx/steps,cur.y+r.dy/steps,cur.z+r.dz/steps},out=next;
  CollisionPoly* wall=nullptr;int bg=0;
  BgCheck_EntitySphVsWall3(&gPlayState->colCtx,&out,&next,&cur,radius,&wall,&bg,actor,std::min(26.8f,height-2.0f));
  // Also test near the head, so low archways aren't traversable upright.
  Vec3f oldHead{cur.x,cur.y+std::max(0.0f,height-20),cur.z},newHead{out.x,out.y+std::max(0.0f,height-20),out.z},head=newHead;
  BgCheck_EntitySphVsWall3(&gPlayState->colCtx,&head,&newHead,&oldHead,radius,&wall,&bg,actor,20.0f);
  out.x=head.x;out.z=head.z;
  float floorY=-32000;
  for(auto offset: {Vec3f{0,0,0},Vec3f{9,0,9},Vec3f{-9,0,9},Vec3f{9,0,-9},Vec3f{-9,0,-9}}){
   Vec3f ray{out.x+offset.x,std::max(cur.y,out.y)+24,out.z+offset.z};
   CollisionPoly* floor=nullptr;
   float h=BgCheck_EntityRaycastFloor5(&gPlayState->colCtx,&floor,&bg,actor,&ray);
   if(h<=cur.y+24.01f)floorY=std::max(floorY,h);
  }
  if(out.y<floorY && r.dy<=0)out.y=floorY;
  float ceilingY=out.y;CollisionPoly* ceiling=nullptr;
  if(BgCheck_EntityCheckCeiling(&gPlayState->colCtx,&ceilingY,&out,height,&ceiling,&bg,actor))out.y=std::min(out.y,ceilingY);
  if(floorY<-31000 && out.y<cur.y-20)return {0,0,0};
  cur=out;
 }
 return {cur.x-r.x,cur.y-r.y,cur.z-r.z};
}
void serviceCamera(){
 if(!shared||!std::getenv("COMPOSITE_FRAME")||!world())return;
 auto serial=acquire(shared+1344);if(serial==acquire(shared+1376))return;
 uint32_t epoch;Vec3f eye,end;std::memcpy(&epoch,shared+1348,4);std::memcpy(&eye,shared+1352,12);std::memcpy(&end,shared+1364,12);
 if(serial!=acquire(shared+1344))return;
 bool valid=epoch==port.epoch;for(float f:{eye.x,eye.y,eye.z,end.x,end.y,end.z})if(!std::isfinite(f)||std::abs(f)>100000)valid=false;
 if(valid){
  Vec3f hit{};CollisionPoly* poly=nullptr;int bg=0;
  if(BgCheck_EntityLineTest1(&gPlayState->colCtx,&eye,&end,&hit,&poly,true,true,true,true,&bg)){
   float dx=eye.x-hit.x,dy=eye.y-hit.y,dz=eye.z-hit.z,len=std::sqrt(dx*dx+dy*dy+dz*dz);
   if(len>0.1f)end={hit.x+dx/len*8,hit.y+dy/len*8,hit.z+dz/len*8};else end=eye;
  }
 }else end=eye;
 std::memcpy(shared+1380,&end,12);release(shared+1376,serial);
}
void service(){
 serviceCamera();
 auto serial=acquire(shared+REQUEST);
 if(serial==lastReply)return;
 Request r{};std::memcpy(&r,shared+REQUEST+4,sizeof(r));
 if(serial!=acquire(shared+REQUEST))return;
 Reply result=collide(r);
 std::memcpy(shared+REPLY+4,&result,sizeof(result));
 release(shared+REPLY,serial);lastReply=serial;
}
void target(){
 port.target=0;
 if(!live())return;
 constexpr float radians=3.14159265358979323846f/180;
 float yaw=mc.yaw*radians,pitch=mc.pitch*radians;
 Vec3f eye{mc.x,(story&&!std::getenv("COMPOSITE_FRAME")?port.y+50:mc.y+(GET_PLAYER(gPlayState)->ageProperties?GET_PLAYER(gPlayState)->ageProperties->ceilingCheckHeight*.85f:34.0f)),mc.z};
 Vec3f end{eye.x-std::sin(yaw)*std::cos(pitch)*200,eye.y-std::sin(pitch)*200,eye.z+std::cos(yaw)*std::cos(pitch)*200};
 Vec3f hit{};CollisionPoly* poly=nullptr;int bg=0;
 if(!BgCheck_EntityLineTest1(&gPlayState->colCtx,&eye,&end,&hit,&poly,true,true,true,true,&bg))return;
 if(story&&blockTarget(bg,poly))return;
 // Otherwise place on upward-facing world surfaces, snapped in X/Z.
 if(!poly||poly->normal.y<16000)return;
 float x=std::floor(hit.x/SCALE)*SCALE,z=std::floor(hit.z/SCALE)*SCALE;
 Vec3f ray{x+20,hit.y+25,z+20};CollisionPoly* floor=nullptr;
 float y=BgCheck_EntityRaycastFloor5(&gPlayState->colCtx,&floor,&bg,&GET_PLAYER(gPlayState)->actor,&ray);
 if(y<-31000||std::abs(y-hit.y)>24)return;
 port.target=1;port.tx=x;port.ty=y;port.tz=z;
}
void publishEnemies(){
 if(!std::getenv("COMPOSITE_FRAME")||!live()||!world())return;
 struct Enemy {int32_t id;float x,y,z;uint32_t health;};struct Enemies{uint32_t epoch,count;Enemy entries[7];} out{};
 out.epoch=port.epoch;std::vector<Actor*> enemies;
 for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].first;a;a=a->next)if(a->update)enemies.push_back(a);
 auto distance=[](Actor* a){float x=a->world.pos.x-mc.x,z=a->world.pos.z-mc.z;return x*x+z*z;};
 std::sort(enemies.begin(),enemies.end(),[&](Actor* a,Actor* b){return distance(a)<distance(b);});
 for(auto* a:enemies){if(out.count==7)break;out.entries[out.count++]={a->id,a->world.pos.x,a->world.pos.y,a->world.pos.z,a->colChkInfo.health};}
 write(shared,1888,out);
}
void publishFloor(){
 if(!std::getenv("COMPOSITE_FRAME")||!live()||!world()||port.frame%4)return;
 struct Floors {uint32_t epoch;int32_t x,z;float y[81];} f{};
 f.epoch=port.epoch;f.x=(int)std::floor(mc.x/40)-4;f.z=(int)std::floor(mc.z/40)-4;
 for(int i=0;i<81;i++){
  Vec3f ray{(f.x+i%9)*40.0f+20,mc.y+80,(f.z+i/9)*40.0f+20};CollisionPoly* poly=nullptr;int bg=0;
  f.y[i]=BgCheck_EntityRaycastFloor5(&gPlayState->colCtx,&poly,&bg,&GET_PLAYER(gPlayState)->actor,&ray);
  // Never turn a user-placed cube into an invisible permanent floor.
  for(int j=0;j<MAX_BLOCKS;j++)if(dirtBg[j]!=BG_ACTOR_MAX&&bg==dirtBg[j])f.y[i]=-32000;
 }
 write(shared,1536,f);
}
void pump(){
 if(!shared)return;
 if(!window){window=SDL_GetKeyboardFocus();if(window&&story)SDL_SetWindowTitle(window,std::getenv("COMPOSITE_FRAME")?"Zelda frame source — Native Passthrough":(std::getenv("COMPOSITE_TEST_INPUT")||std::getenv("COMPOSITE_PLAYTEST"))?"Composite Playtest — Minecraft controls":"Minecraft x Majora — E inventory, F5 view");}
 if(story&&(!world()||gSaveContext.gameMode!=GAMEMODE_NORMAL))storyMenuInput();
 if(releaseMouse.exchange(false))capture(false);
 if(recapture.exchange(false)&&active&&!inventoryShown&&!nativeMenuOpen)capture(true);
 if(toggle.exchange(false)){
  if(story&&active){capture(true);hideNativeMenu=true;}
  else if(active && !captured)capture(true);
  else if(active)stop();else start();
 }
 if(wantStart&&world()&&(story||clockTown())&&gSaveContext.gameMode==GAMEMODE_NORMAL&&gPlayState->transitionTrigger==TRANS_TRIGGER_OFF)start();
 // Explicitly enabled integration-test input; reserved bytes are ignored normally.
 if(std::getenv("COMPOSITE_TEST_INPUT") && acquire(shared+448)==0x54455354){
  buttons=acquire(shared+452);mouseX=(int32_t)acquire(shared+456);mouseY=(int32_t)acquire(shared+460);
  captured=true;
 }
 if(active && (!world()||(!story&&!clockTown())))stop();
 service();
 if(active){
  Minecraft candidate{};
  if(read(shared,MC,candidate)&&candidate.epoch==port.epoch && candidate.tick!=lastTick){
   bool valid=true;
   for(float f:{candidate.x,candidate.y,candidate.z,candidate.yaw,candidate.pitch,candidate.bx,candidate.by,candidate.bz})if(!std::isfinite(f)||std::abs(f)>100000)valid=false;
   if(valid){mc=candidate;lastTick=mc.tick;lastMinecraft=SDL_GetTicks64();}
  }
  if(!story){
  gSaveContext.save.time=frozenTime;
  if(gPlayState->msgCtx.msgLength && gPlayState->msgCtx.msgMode!=MSGMODE_TEXT_CLOSING)Message_CloseTextbox(gPlayState);
  if(live()){
   auto* p=GET_PLAYER(gPlayState);p->actor.prevPos=p->actor.world.pos;
   p->actor.world.pos={mc.x,mc.y,mc.z};p->actor.velocity={0,0,0};p->actor.speed=0;
   p->actor.shape.rot.y=(s16)(-mc.yaw*(32768.0f/180));p->actor.world.rot.y=p->actor.shape.rot.y;
   p->actor.focus.pos={mc.x,mc.y+(GET_PLAYER(gPlayState)->ageProperties?GET_PLAYER(gPlayState)->ageProperties->ceilingCheckHeight*.85f:34.0f),mc.z};
  }
  }else storyPump();
  target();
 }
 port.active=active?(story?2:1):0;port.buttons=(captured||inventoryShown)?buttons.load():0;
 port.mouseX=mouseX;port.mouseY=mouseY;port.scene=world()?gPlayState->sceneId:0xFFFFFFFF;
 release(shared+16,buttons.load());release(shared+20,gSaveContext.gameMode);
 release(shared+32,captured?1:0);release(shared+36,nativeMenuOpen?1:0);
 if(gGameState){release(shared+24,gGameState->input[0].press.button);release(shared+28,gGameState->input[0].cur.button);}
 publishFloor();publishEnemies();
 port.frame++;write(shared,PORT,port);
}
void camera(Camera* c){
 if(!live()||!world()||c!=GET_ACTIVE_CAM(gPlayState))return;
 if(story&&!storyCameraFree())return;
 float y=mc.yaw*(3.14159265f/180),p=mc.pitch*(3.14159265f/180);
 if(std::getenv("COMPOSITE_FRAME")){
  struct NativeCamera {uint32_t epoch;float x,y,z,yaw,pitch,fov;uint32_t valid;} v{};
  if(read(shared,1280,v)&&v.epoch==port.epoch&&v.valid&&std::isfinite(v.fov)&&v.fov>=20&&v.fov<=150){
   bool valid=true;for(float f:{v.x,v.y,v.z,v.yaw,v.pitch})if(!std::isfinite(f)||std::abs(f)>100000)valid=false;
   if(valid){
    float ya=v.yaw*(3.14159265f/180),pi=v.pitch*(3.14159265f/180);
    c->eye={v.x,v.y,v.z};c->eyeNext=c->eye;
    c->at={v.x-std::sin(ya)*std::cos(pi)*100,v.y-std::sin(pi)*100,v.z+std::cos(ya)*std::cos(pi)*100};
    c->up={0,1,0};c->fov=v.fov;c->roll=0;gPlayState->view.fovy=v.fov;
    View_LookAt(&gPlayState->view,&c->eye,&c->at,&c->up);return;
   }
  }
 }
 if(story){
  c->at={port.x,port.y+(std::getenv("COMPOSITE_FRAME")?64.8f:50.0f),port.z};
  float reach=std::getenv("COMPOSITE_FRAME")?160.0f:110.0f;
  float distance=storyMC.thirdPerson==2?-reach:storyMC.thirdPerson?reach:0.0f;
  c->eye={c->at.x+std::sin(y)*std::cos(p)*distance,c->at.y+std::sin(p)*distance,c->at.z-std::cos(y)*std::cos(p)*distance};
  if(distance){Vec3f hit{};CollisionPoly* poly=nullptr;int bg=0;
   if(BgCheck_EntityLineTest1(&gPlayState->colCtx,&c->at,&c->eye,&hit,&poly,true,true,true,true,&bg)){
    c->eye={hit.x+(c->at.x-hit.x)*.08f,hit.y+(c->at.y-hit.y)*.08f,hit.z+(c->at.z-hit.z)*.08f};
   }
  }
  c->inputDir.y=c->camDir.y=(s16)(-mc.yaw*(32768.0f/180));
 }else c->eye={mc.x,mc.y+(GET_PLAYER(gPlayState)->ageProperties?GET_PLAYER(gPlayState)->ageProperties->ceilingCheckHeight*.85f:34.0f),mc.z};
 c->eyeNext=c->eye;
 float sign=story&&storyMC.thirdPerson==2?-1.0f:1.0f;
 c->at={c->eye.x-std::sin(y)*std::cos(p)*100*sign,c->eye.y-std::sin(p)*100*sign,c->eye.z+std::cos(y)*std::cos(p)*100*sign};
 c->up={0,1,0};c->fov=70;c->roll=0;
 gPlayState->view.fovy=70;View_LookAt(&gPlayState->view,&c->eye,&c->at,&c->up);
}
#include "NativeCombat.h"
#include "StoryHook.h"
} // namespace
void compositeDraw(){
 using namespace composite;
 if(!live()||!world()||std::getenv("COMPOSITE_FRAME"))return;
 OPEN_DISPS(gPlayState->state.gfxCtx);
 int count=story?(int)blocks.count:(mc.block?1:0);
 for(int blockIndex=0;blockIndex<count;blockIndex++){
  // This is the one-block Minecraft layer, composed with the port view/projection
  // and Z buffer (not a second window or a screen-space image).
  Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);
  gDPPipeSync(POLY_OPA_DISP++);
  gDPSetCycleType(POLY_OPA_DISP++,G_CYC_1CYCLE);
  gDPSetTextureLUT(POLY_OPA_DISP++,G_TT_NONE);
  gDPSetAlphaCompare(POLY_OPA_DISP++,G_AC_NONE);
  gSPClearGeometryMode(POLY_OPA_DISP++,G_LIGHTING|G_CULL_BOTH|G_FOG|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
  gSPSetGeometryMode(POLY_OPA_DISP++,G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
  gDPSetRenderMode(POLY_OPA_DISP++,G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);
  gDPSetCombineMode(POLY_OPA_DISP++,G_CC_MODULATERGBA,G_CC_MODULATERGBA);
  gDPSetTextureFilter(POLY_OPA_DISP++,G_TF_POINT);
  gSPTexture(POLY_OPA_DISP++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
  gDPLoadTextureBlock(POLY_OPA_DISP++,(uintptr_t)dirtTexture,G_IM_FMT_RGBA,G_IM_SIZ_16b,16,16,0,G_TX_WRAP,G_TX_WRAP,4,4,G_TX_NOLOD,G_TX_NOLOD);
  auto pos=story?blocks.positions[blockIndex]:BlockPos{mc.bx,mc.by,mc.bz};
  Matrix_Translate(pos.x,pos.y,pos.z,MTXMODE_NEW);
  MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++,gPlayState->state.gfxCtx);
  static const int corners[6][4][3]={
   {{0,0,0},{40,0,0},{40,40,0},{0,40,0}},{{40,0,40},{0,0,40},{0,40,40},{40,40,40}},
   {{0,0,40},{0,0,0},{0,40,0},{0,40,40}},{{40,0,0},{40,0,40},{40,40,40},{40,40,0}},
   {{0,40,0},{40,40,0},{40,40,40},{0,40,40}},{{0,0,40},{40,0,40},{40,0,0},{0,0,0}}};
  auto* vertices=(Vtx*)GRAPH_ALLOC(gPlayState->state.gfxCtx,24*sizeof(Vtx));
  for(int face=0;face<6;face++)for(int v=0;v<4;v++){
   auto& a=vertices[face*4+v];std::memset(&a,0,sizeof(a));
   for(int k=0;k<3;k++)a.v.ob[k]=corners[face][v][k];
   a.v.tc[0]=(v==1||v==2)?512:0;a.v.tc[1]=v<2?512:0;
   a.v.cn[0]=a.v.cn[1]=a.v.cn[2]=face==4?255:face==5?135:200;a.v.cn[3]=255;
  }
  gSPVertex(POLY_OPA_DISP++,(uintptr_t)vertices,24,0);
  for(int f=0;f<6;f++){int v=f*4;gSP2Triangles(POLY_OPA_DISP++,v,v+1,v+2,0,v,v+2,v+3,0);}
  gDPPipeSync(POLY_OPA_DISP++);gDPSetTextureFilter(POLY_OPA_DISP++,G_TF_BILERP);
 }
 if(!story){
 // Small crosshair in the port's logical 320x240 viewport.
 gDPPipeSync(OVERLAY_DISP++);gDPSetCycleType(OVERLAY_DISP++,G_CYC_FILL);
 gDPSetRenderMode(OVERLAY_DISP++,G_RM_NOOP,G_RM_NOOP2);
 gDPSetFillColor(OVERLAY_DISP++,0xFFFFFFFF);
 gDPFillRectangle(OVERLAY_DISP++,157,120,163,120);gDPFillRectangle(OVERLAY_DISP++,160,117,160,123);
 gDPPipeSync(OVERLAY_DISP++);gDPSetCycleType(OVERLAY_DISP++,G_CYC_1CYCLE);
 }
 CLOSE_DISPS(gPlayState->state.gfxCtx);
}
namespace {
void init(){
 const char* path=std::getenv("COMPOSITE_SHM");if(!path)return;
 descriptor=open(path,O_RDWR|O_CREAT|O_NOFOLLOW,0600);
 if(descriptor<0 || flock(descriptor,LOCK_EX|LOCK_NB)!=0){spdlog::error("[Composite] Shared memory already in use or unavailable");if(std::getenv("COMPOSITE_FRAME"))std::exit(73);return;}
 struct stat st{};fstat(descriptor,&st);
 if(st.st_uid!=getuid()||!S_ISREG(st.st_mode)||ftruncate(descriptor,SIZE)!=0){spdlog::error("[Composite] Unsafe shared memory file");return;}
 shared=(uint8_t*)mmap(nullptr,SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,descriptor,0);
 if(shared==MAP_FAILED){shared=nullptr;return;}
 story=std::getenv("COMPOSITE_STORY")!=nullptr;
 if(std::getenv("COMPOSITE_FRAME")){CVarSetInteger("gVsyncEnabled",0);CVarSetInteger("gInterpolationFPS",60);}
 if(story)storyGuiInit();
 port.epoch=(uint32_t)getpid();
 std::memset(shared,0,SIZE);release(shared,MAGIC);release(shared+4,VERSION);
 SDL_AddEventWatch(events,nullptr);
 auto* gi=GameInteractor::Instance;
 gi->RegisterGameHook<GameInteractor::OnConsoleLogoUpdate>([]{
  static bool booted=false;
  if(booted||!std::getenv("COMPOSITE_BOOT"))return;
  booted=true;Rand_Seed(osGetTime());Sram_InitNewSave();
  bool lesson=std::getenv("COMPOSITE_START")&&std::strcmp(std::getenv("COMPOSITE_START"),"lesson")==0;
  bool restored=false;
  if(!lesson&&std::filesystem::exists("composite-checkpoint.json")){
   try{std::ifstream in("composite-checkpoint.json");json j;in>>j;
    if(j.at("format")!=1)throw std::runtime_error("unknown checkpoint format");
    j.at("save").get_to(gSaveContext.save);restored=true;
    spdlog::info("[Composite] Resumed local Clock Town checkpoint");
   }catch(const std::exception& e){spdlog::error("[Composite] Cannot resume checkpoint: {}",e.what());std::exit(74);}
  }
  gSaveContext.fileNum=0;gSaveContext.gameMode=GAMEMODE_NORMAL;
  gSaveContext.save.entrance=lesson?ENTRANCE(CLOCK_TOWER_INTERIOR,3):std::getenv("COMPOSITE_START")&&std::strcmp(std::getenv("COMPOSITE_START"),"field")==0?ENTRANCE(TERMINA_FIELD,0):ENTRANCE(SOUTH_CLOCK_TOWN,0);
  gSaveContext.save.cutsceneIndex=0;gSaveContext.nextCutsceneIndex=0;
  if(!restored){gSaveContext.save.day=1;gSaveContext.save.time=CLOCK_TIME(12,0);}
  if(lesson)gSaveContext.save.saveInfo.inventory.items[SLOT_OCARINA]=ITEM_OCARINA_OF_TIME;
  gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;gSaveContext.save.hasTatl=true;
  gSaveContext.save.saveInfo.playerData.threeDayResetCount=1;
  SET_WEEKEVENTREG(WEEKEVENTREG_59_04);SET_WEEKEVENTREG(WEEKEVENTREG_31_04);
  gSaveContext.respawnFlag=0;gSaveContext.seqId=NA_BGM_DISABLED;gSaveContext.ambienceId=AMBIENCE_ID_DISABLED;
  gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance=0xFFFF;
  gSaveContext.showTitleCard=false;
  STOP_GAMESTATE(gGameState);SET_NEXT_GAMESTATE(gGameState,Play_Init,sizeof(PlayState));
  wantStart=true;
 });
 gi->RegisterGameHook<GameInteractor::OnSceneInit>([](s8,s8){sceneReady=true;if(story)wantStart=true;});
 gi->RegisterGameHook<GameInteractor::OnPlayDestroy>([]{if(story){mc.block=0;storyCollider();}sceneReady=false;stop();});
 gi->RegisterGameHook<GameInteractor::OnGameStateMainStart>(pump);
 gi->RegisterGameHook<GameInteractor::OnPassPlayerInputs>([](Input* i){if(active&&!story){std::memset(i,0,sizeof(*i));}});
 gi->RegisterGameHook<GameInteractor::OnPassPlayerInputs>([](Input*){
  if(std::getenv("COMPOSITE_FRAME")&&live()&&world()&&storyCameraFree()){
   auto* p=GET_PLAYER(gPlayState);
   p->actor.world.rot.y=p->actor.shape.rot.y=p->yaw=(s16)(-mc.yaw*(32768.0f/180));
   if(mc.grounded)p->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
   else p->actor.bgCheckFlags&=~BGCHECKFLAG_GROUND;
  }
 });
 gi->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]{
  static bool checkpointed=false;
  if(!checkpointed&&std::getenv("COMPOSITE_FRAME")&&world()&&clockTown()&&CHECK_QUEST_ITEM(QUEST_SONG_HEALING)&&!gPlayState->msgCtx.msgLength&&!Player_InCsMode(gPlayState)){
   saveCheckpoint();checkpointed=std::filesystem::exists("composite-checkpoint.json");
  }
 });
 gi->RegisterGameHook<GameInteractor::ShouldActorUpdate>([](Actor* a,bool* should){if(active&&!story&&world()&&a->id==ACTOR_PLAYER)*should=false;});
 gi->RegisterGameHook<GameInteractor::ShouldActorDraw>([](Actor* a,bool* should){if(live()&&(!story||(!storyMC.thirdPerson&&storyCameraFree()))&&a->id==ACTOR_PLAYER)*should=false;});
 gi->RegisterGameHook<GameInteractor::AfterCameraUpdate>(camera);
 gi->RegisterGameHook<GameInteractor::OnActorUpdate>([](Actor* a){
  if(a->id==ACTOR_PLAYER&&std::getenv("COMPOSITE_FRAME")&&live()&&storyCameraFree()){
   a->world.pos={mc.x,mc.y,mc.z};
  }
 });
 gi->RegisterGameHook<GameInteractor::OnActorUpdate>(nativeCombat);
 gi->RegisterGameHook<GameInteractor::OnPlayDrawWorldEnd>(compositeDraw);
 spdlog::info("[Composite] Bridge ready: {}",story?"story passthrough; open a normal save":"Clock Town sandbox; F8 activates");
}
static RegisterShipInitFunc registration(init);
}

extern "C" bool CompositePollCollision(){
 if(!shared||!std::getenv("COMPOSITE_FRAME"))return false;
 release(shared+56,acquire(shared+56)+1);service();return true;
}
extern "C" int CompositeOcarinaInput(u16* pad){
 const auto snapshot=instrumentInput.load(std::memory_order_acquire);
 if(!snapshot)return 0; // Ordinary 2ship input when no composite session owns it.
 const uint64_t stamp=snapshot>>16;
 *pad=SDL_GetTicks64()-stamp<250?(u16)snapshot:0;
 if(shared&&std::getenv("COMPOSITE_TEST_INPUT"))release(shared+2040,*pad);
 return 1;
}
#include "FrameExport.h"

// Late-latch the world view for every displayed (including interpolated) frame.
// Actor animation still interpolates normally; camera motion is never blended
// against an older camera independently of Minecraft's block geometry.
void CompositeLateCamera(std::unordered_map<Mtx*,MtxF>& replacements){
 renderedCamera={};
 if(!shared||!std::getenv("COMPOSITE_FRAME")||!world())return;
 auto& lights=gPlayState->envCtx.lightSettings;
 float brightness=0;
 for(int i=0;i<3;i++)brightness+=std::min(255,(int)lights.ambientColor[i]+(int)lights.light1Color[i]);
 renderedCamera.light=std::clamp(brightness/(3*255.0f),.12f,1.0f);
 if(!live()||!storyCameraFree())return;
 struct RequestCamera {uint32_t epoch;float x,y,z,yaw,pitch,fov;uint32_t valid;} v{};
 if(!read(shared,1280,v)||v.epoch!=port.epoch||!v.valid||v.fov<20||v.fov>150)return;
 for(float value:{v.x,v.y,v.z,v.yaw,v.pitch,v.fov})if(!std::isfinite(value)||std::abs(value)>100000)return;
 auto& view=gPlayState->view;if(!view.viewingPtr||!view.projectionPtr)return;
 const float yaw=v.yaw*(3.14159265f/180),pitch=v.pitch*(3.14159265f/180);
 MtxF look{},projection{};u16 norm;
 guLookAtF((float(*)[4])&look,v.x,v.y,v.z,v.x-std::sin(yaw)*std::cos(pitch)*100,v.y-std::sin(pitch)*100,v.z+std::cos(yaw)*std::cos(pitch)*100,0,1,0);
 guPerspectiveF((float(*)[4])&projection,&norm,v.fov,(float)(view.viewport.rightX-view.viewport.leftX)/(view.viewport.bottomY-view.viewport.topY),view.zNear,view.zFar,view.scale);
 replacements[view.viewingPtr]=look;replacements[view.projectionPtr]=projection;
 renderedCamera={v.epoch,v.x,v.y,v.z,v.yaw,v.pitch,v.fov,1,acquire(shared+1280),port.scene,renderedCamera.light};
}
