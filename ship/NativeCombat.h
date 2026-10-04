// Included in the bridge namespace. All actor/collider changes stay on the game thread.
struct CombatEvent { uint32_t epoch,sequence,kind;float x,y,z,dx,dy,dz,power; };
static_assert(sizeof(CombatEvent)==40);
ColliderCylinder nativeSword[3]{};
bool nativeSwordReady=false;
uint32_t lastCombat=0;
void nativeCombat(Actor* actor){
 if(!shared||!std::getenv("COMPOSITE_FRAME")||!live()||!world()||actor!=&GET_PLAYER(gPlayState)->actor)return;
 if(nativeSwordReady){
  bool hit=false;for(auto& sword:nativeSword){hit|=(sword.base.atFlags&AT_HIT)!=0;sword.base.atFlags&=~(AT_HIT|AT_BOUNCED);}
  if(hit)release(shared+1480,acquire(shared+1480)+1);
 }
 CombatEvent e{};if(!read(shared,1408,e)||e.epoch!=port.epoch||e.sequence==lastCombat)return;
 lastCombat=e.sequence;release(shared+1472,e.sequence);
 if(!storyCameraFree())return;
 for(float f:{e.x,e.y,e.z,e.dx,e.dy,e.dz,e.power})if(!std::isfinite(f)||std::abs(f)>100000)return;
 if(std::abs(e.x-mc.x)>100||std::abs(e.y-mc.y)>100||std::abs(e.z-mc.z)>100)return;
 if(e.kind==3){
  float magnitude=std::sqrt(e.dx*e.dx+e.dy*e.dy+e.dz*e.dz);if(magnitude<.05f||magnitude>10)return;
  s16 yaw=Math_Atan2S(e.dx,e.dz),pitch=Math_Atan2S(-e.dy,std::sqrt(e.dx*e.dx+e.dz*e.dz));
  auto* a=(EnArrow*)Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_ARROW,e.x,e.y,e.z,pitch,yaw,0,ARROW_TYPE_NORMAL);
  if(a){
   // Enter native free flight directly; normal setup expects Link to have held a bow.
   a->actionFunc=func_8088ACE0;a->unk_228=a->actor.world.pos;a->unk_260=40;
   Actor_SetSpeeds(&a->actor,std::clamp(magnitude*40.0f,10.0f,150.0f));
   release(shared+1484,acquire(shared+1484)+1);Player_PlaySfx(GET_PLAYER(gPlayState),NA_SE_IT_ARROW_SHOT);
  }
 }else if(e.kind==1||e.kind==2){
  if(!nativeSwordReady){
   ColliderCylinderInit init={
    {COL_MATERIAL_HIT0,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},
    {ELEM_MATERIAL_UNK0,{DMG_SWORD,0,1},{0,0,0},ATELEM_ON|ATELEM_SFX_NORMAL,ACELEM_NONE,OCELEM_NONE},
    {22,56,0,{0,0,0}}};
   for(auto& sword:nativeSword){Collider_InitAndSetCylinder(gPlayState,&sword,actor,&init);}
   nativeSwordReady=true;
  }
  // Overlapping samples cover close contact through the full reach; Zelda uses
  // the maximum AC damage, so overlapping contacts do not multiply damage.
  Vec3f start{e.x,e.y,e.z},end{e.x+e.dx*110,e.y+e.dy*110,e.z+e.dz*110},hit{};
  float reach=85;CollisionPoly* poly=nullptr;int bg=0;
  if(BgCheck_EntityLineTest1(&gPlayState->colCtx,&start,&end,&hit,&poly,true,true,true,true,&bg)){
   float dx=hit.x-e.x,dy=hit.y-e.y,dz=hit.z-e.z;reach=std::min(reach,std::sqrt(dx*dx+dy*dy+dz*dz)-24);
  }
  if(reach>0)for(int i=0;i<3;i++){
   float distance=std::min(20.0f+i*32.5f,reach);auto& sword=nativeSword[i];
   sword.dim.pos={(s16)(e.x+e.dx*distance),(s16)(e.y+e.dy*distance-36),(s16)(e.z+e.dz*distance)};
   sword.elem.atDmgInfo.damage=e.kind==2?4:1;
   sword.base.atFlags=AT_ON|AT_TYPE_PLAYER;sword.elem.atElemFlags&=~ATELEM_HIT;
   CollisionCheck_SetAT(gPlayState,&gPlayState->colChkCtx,&sword.base);
   if(distance==reach)break;
  }
  release(shared+1476,acquire(shared+1476)+1);
 }
}
