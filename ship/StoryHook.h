// Included in CompositeBridge's private namespace; all gameplay writes run on the game thread.
struct StoryView { bool visible=false,connected=false,captured=false; StoryPort port{}; StoryMinecraft mc{}; BlockLayer blocks{}; };
std::mutex viewMutex;
StoryView storyView;
uint32_t previousControls=0;
u16 previousPad=0;
uint32_t maskRequestSeen=0;
int maskInputTicks=0;
DynaPolyActor dirtActors[MAX_BLOCKS]{};
bool dirtInitialized=false;
Vec3s dirtVertices[8]={{0,0,0},{40,0,0},{40,40,0},{0,40,0},{0,0,40},{40,0,40},{40,40,40},{0,40,40}};
CollisionPoly dirtPolys[12]{};
SurfaceType dirtSurface[1]{};
BgCamInfo dirtCam[1]{};
CollisionHeader dirtCollision{};
void storyCollider(){
 if(!dirtInitialized){std::fill(std::begin(dirtBg),std::end(dirtBg),BG_ACTOR_MAX);dirtInitialized=true;}
 if(!gPlayState)return;
 for(int i=0;i<MAX_BLOCKS;i++)if(i>=(int)blocks.count&&dirtBg[i]!=BG_ACTOR_MAX){DynaPoly_DeleteBgActor(gPlayState,&gPlayState->colCtx.dyna,dirtBg[i]);dirtBg[i]=BG_ACTOR_MAX;}
 if(!blocks.count)return;
 static const int faces[6][4]={{0,3,2,1},{4,5,6,7},{0,4,7,3},{1,2,6,5},{3,7,6,2},{0,1,5,4}};
 static const Vec3s normals[6]={{0,0,-32767},{0,0,32767},{-32767,0,0},{32767,0,0},{0,32767,0},{0,-32767,0}};
 for(int f=0;f<6;f++)for(int t=0;t<2;t++){
  auto& p=dirtPolys[f*2+t];p={};p.vtxData[0]=faces[f][0];p.vtxData[1]=faces[f][t?2:1];p.vtxData[2]=faces[f][t?3:2];p.normal=normals[f];p.dist=(f==1||f==3||f==4)?-40:0;
 }
 dirtCollision={};dirtCollision.minBounds={0,0,0};dirtCollision.maxBounds={40,40,40};
 dirtCollision.numVertices=8;dirtCollision.vtxList=dirtVertices;dirtCollision.numPolygons=12;dirtCollision.polyList=dirtPolys;
 dirtCollision.surfaceTypeList=dirtSurface;dirtCollision.bgCamList=dirtCam;
 for(uint32_t i=0;i<blocks.count;i++){
  auto& actor=dirtActors[i];auto pos=blocks.positions[i];
  if(dirtBg[i]!=BG_ACTOR_MAX){actor.actor.world.pos={pos.x,pos.y,pos.z};continue;}
  actor={};DynaPolyActor_Init(&actor,DYNA_TRANSFORM_POS);
  actor.actor.id=-1;actor.actor.scale={1,1,1};actor.actor.world.pos={pos.x,pos.y,pos.z};
  actor.actor.update=[](Actor*,PlayState*){};
  dirtBg[i]=DynaPoly_SetBgActor(gPlayState,&gPlayState->colCtx.dyna,&actor.actor,&dirtCollision);actor.bgId=dirtBg[i];
 }
}
bool blockTarget(int bg,CollisionPoly* poly){
 if(!poly)return false;
 for(uint32_t i=0;i<blocks.count;i++)if(dirtBg[i]==bg){
  auto p=blocks.positions[i];
  if(std::abs((int)poly->normal.x)>16000)p.x+=poly->normal.x>0?40:-40;
  else if(std::abs((int)poly->normal.y)>16000)p.y+=poly->normal.y>0?40:-40;
  else p.z+=poly->normal.z>0?40:-40;
  port.target=1;port.tx=p.x;port.ty=p.y;port.tz=p.z;return true;
 }
 return false;
}

bool storyCameraFree(){
 return world()&&!Player_InCsMode(gPlayState)&&!gPlayState->msgCtx.msgLength&&!gPlayState->pauseCtx.state&&
  gPlayState->activeCamId==CAM_ID_MAIN&&!GET_PLAYER(gPlayState)->focusActor&&!(storyMC.controls&(1<<18));
}
void storyPump(){
 auto* p=GET_PLAYER(gPlayState);
 // Opt-in integration fixture, never enabled by either user launcher.
 if(std::getenv("COMPOSITE_TEST_INPUT")&&std::getenv("COMPOSITE_TEST_SONG")&&live()){
  static bool started=false;
  if(!started||acquire(shared+448)==0x534F4E47){started=true;release(shared+448,0);Message_DisplayOcarinaStaff(gPlayState,OCARINA_ACTION_PROMPT_HEALING);}
  release(shared+2044,(gPlayState->msgCtx.msgMode<<16)|gPlayState->msgCtx.ocarinaMode);
 }
 if(std::getenv("COMPOSITE_FRAME")&&live()&&storyCameraFree()){
  p->actor.prevPos=p->actor.world.pos;p->actor.world.pos={mc.x,mc.y,mc.z};
  p->actor.velocity={0,0,0};p->fallStartHeight=(s16)mc.y;p->fallDistance=0;p->actor.shape.rot.y=(s16)(-mc.yaw*(32768.0f/180));
 }
 port.x=p->actor.world.pos.x;port.y=p->actor.world.pos.y;port.z=p->actor.world.pos.z;
 storyPort.flags=1;
 if(gPlayState->msgCtx.msgLength)storyPort.flags|=2;
 if(gPlayState->pauseCtx.state)storyPort.flags|=4;
 if(Player_InCsMode(gPlayState))storyPort.flags|=8;
 if(gPlayState->msgCtx.msgMode>=MSGMODE_OCARINA_STARTING&&gPlayState->msgCtx.msgMode<=MSGMODE_26)storyPort.flags|=16;
 storyPort.yaw=-(float)p->actor.shape.rot.y*(180.0f/32768);
 std::memcpy(storyPort.items,gSaveContext.save.saveInfo.inventory.items,48);
 std::memcpy(storyPort.ammo,gSaveContext.save.saveInfo.inventory.ammo,24);
 storyPort.sword=GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_B);storyPort.wheel=wheel;
 auto command=uiCommand.exchange(0);
 if(command){storyPort.commandSeq++;storyPort.commandKind=command>>16;storyPort.commandValue=command&65535;}
 write(shared,STORY_PORT,storyPort);
 if(std::getenv("COMPOSITE_FRAME")&&p->ageProperties){
  struct AvatarState {uint32_t epoch,form;float radius,height,eye;uint32_t mask,flags;} avatar{
   port.epoch,(uint32_t)p->transformation,p->ageProperties->wallCheckRadius,
   p->ageProperties->ceilingCheckHeight,p->ageProperties->ceilingCheckHeight*.85f,
   (uint32_t)p->currentMask,storyPort.flags};
  write(shared,640,avatar);
 }
 StoryMinecraft next{};
 if(read(shared,STORY_MC,next)&&next.epoch==port.epoch&&next.selected<9)storyMC=next;
 bool open=live()&&storyMC.inventoryOpen;
 if(open!=inventoryShown){inventoryShown=open;auto held=buttons.load();capture(!open);buttons=held;}
 uint32_t b=live()?storyMC.controls:0,press=b&~previousControls;previousControls=b;
 u16 pad=0;
 bool nativeUI=(storyPort.flags&6)!=0;
 if(b&32)pad|=BTN_B;
 if(b&(1<<17))pad|=BTN_R;
 if(b&(1<<18))pad|=BTN_Z;
 if(b&(1<<19))pad|=BTN_A;
 if(b&(1<<20))pad|=BTN_B;
 if(b&(1<<22))pad|=BTN_START;
 if(b&(1<<23))pad|=BTN_CUP;
 if(b&(1<<24))pad|=BTN_CDOWN;
 if(b&(1<<25))pad|=BTN_CLEFT;
 if(b&(1<<26))pad|=BTN_CRIGHT;
 if((b&16)&&nativeUI)pad|=BTN_A;
 if((press&(1<<27))&&storyCameraFree()&&INV_CONTENT(ITEM_OCARINA_OF_TIME)==ITEM_OCARINA_OF_TIME){
  SET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_C_LEFT,ITEM_OCARINA_OF_TIME);
  SET_CUR_FORM_BTN_SLOT(EQUIP_SLOT_C_LEFT,SLOT_OCARINA);
  Interface_LoadItemIcon(gPlayState,EQUIP_SLOT_C_LEFT);pad|=BTN_CLEFT;
 }
 if(std::getenv("COMPOSITE_FRAME")){
  struct MaskRequest {uint32_t epoch,serial,slot;} request{};
  if(read(shared,896,request)&&request.epoch==port.epoch&&request.serial&&request.serial!=maskRequestSeen){
   maskRequestSeen=request.serial;release(shared+924,request.serial);
   if(storyCameraFree()&&request.slot>=SLOT_MASK_POSTMAN&&request.slot<=SLOT_MASK_FIERCE_DEITY&&
      storyPort.items[request.slot]>=ITEM_MASK_DEKU&&storyPort.items[request.slot]<=ITEM_MASK_GIANT){
    SET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_C_LEFT,storyPort.items[request.slot]);
    SET_CUR_FORM_BTN_SLOT(EQUIP_SLOT_C_LEFT,request.slot);
    Interface_LoadItemIcon(gPlayState,EQUIP_SLOT_C_LEFT);maskInputTicks=6;
   }
  }
 }
 if(maskInputTicks>0){
  --maskInputTicks;
  if(maskInputTicks>0&&maskInputTicks<=3&&storyCameraFree())pad|=BTN_CLEFT;
 }
 instrumentInput.store((SDL_GetTicks64()<<16)|pad,std::memory_order_release);
 // Right click prioritizes context interaction. Enter always supplies native A.
 if(b&128){
  uint32_t slot=storyMC.hotbar[storyMC.selected];
  if(nativeUI||slot>=48||p->talkActor||p->heldActor)pad|=BTN_A;
  else if(storyPort.items[slot]!=ITEM_NONE){
   bool changed=GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_C_LEFT)!=storyPort.items[slot];
   SET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_C_LEFT,storyPort.items[slot]);
   SET_CUR_FORM_BTN_SLOT(EQUIP_SLOT_C_LEFT,slot);
   if(changed)Interface_LoadItemIcon(gPlayState,EQUIP_SLOT_C_LEFT);
   pad|=BTN_CLEFT;
  }
 }
 auto& input=gPlayState->state.input[0];
 input.prev=input.cur;input.prev.button=previousPad;
 input.cur.button=pad;input.cur.stick_x=((b&8)?80:0)-((b&4)?80:0);input.cur.stick_y=((b&1)?80:0)-((b&2)?80:0);
 // Native stick updates drive Link's gait and wall/door interaction; Minecraft
 // remains the position authority after the native actor update.
 if(std::getenv("COMPOSITE_FRAME")&&!storyCameraFree()&&!nativeUI)input.cur.stick_x=input.cur.stick_y=0;
 if(input.cur.stick_x&&input.cur.stick_y){input.cur.stick_x=input.cur.stick_x*71/100;input.cur.stick_y=input.cur.stick_y*71/100;}
 input.press.button=pad&~previousPad;input.rel.button=previousPad&~pad;
 input.press.stick_x=input.cur.stick_x-input.prev.stick_x;input.press.stick_y=input.cur.stick_y-input.prev.stick_y;
 PadUtils_UpdateRelXY(&input);previousPad=pad;
 if((press&16)&&storyCameraFree()&&(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND)&&p->actor.depthInWater<10&&
    !p->heldActor&&!(p->stateFlags1&(PLAYER_STATE1_DEAD|PLAYER_STATE1_CARRYING_ACTOR|PLAYER_STATE1_200000|PLAYER_STATE1_8000000))){
  func_80834DB8(p,(PlayerAnimationHeader*)&gPlayerAnim_link_normal_jump,9.0f,gPlayState);
 }
 BlockLayer nextBlocks{};
 if(live()&&read(shared,BLOCKS,nextBlocks)&&nextBlocks.epoch==port.epoch&&nextBlocks.count<=MAX_BLOCKS&&nextBlocks.remaining<=MAX_BLOCKS){
  bool valid=true;for(uint32_t i=0;i<nextBlocks.count;i++)for(float f:{nextBlocks.positions[i].x,nextBlocks.positions[i].y,nextBlocks.positions[i].z})if(!std::isfinite(f)||std::abs(f)>100000)valid=false;
  if(valid)blocks=nextBlocks;
 }
 storyCollider();
 std::lock_guard<std::mutex> lock(viewMutex);storyView={active,live(),captured.load(),storyPort,storyMC,blocks};
}
#include "InventoryView.h"
void storyStop(){instrumentInput.store(0,std::memory_order_release);nativeSwordReady=false;lastCombat=0;mc.block=0;blocks={};storyCollider();previousControls=0;previousPad=0;maskRequestSeen=0;maskInputTicks=0;std::lock_guard<std::mutex> lock(viewMutex);storyView.visible=false;}
void storyGuiInit(){CVarSetInteger("gWindows.ModMenu",0);CVarSetInteger("gWindows.Menu",0);auto menu=Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetMenu();if(menu)menu->Hide();Ship::Context::GetRawInstance()->GetWindow()->GetGui()->AddGuiWindow(std::make_shared<StoryWindow>());}

void storyMenuInput(){
 if(!gGameState)return;
 auto b=buttons.load();
 auto& input=gGameState->input[0];
 static u16 last=0;
 u16 pad=0;
 if(b&((1<<19)|(1<<22)))pad|=BTN_START;
 if(b&16)pad|=BTN_A;
 if(b&(1<<20))pad|=BTN_B;
 input.prev=input.cur;input.prev.button=last;input.cur.button=pad;
 input.cur.stick_x=((b&(8|(1<<26)))?80:0)-((b&(4|(1<<25)))?80:0);
 input.cur.stick_y=((b&(1|(1<<23)))?80:0)-((b&(2|(1<<24)))?80:0);
 input.press.button=pad&~last;input.rel.button=last&~pad;
 input.press.stick_x=input.cur.stick_x-input.prev.stick_x;input.press.stick_y=input.cur.stick_y-input.prev.stick_y;
 PadUtils_UpdateRelXY(&input);last=pad;
}
