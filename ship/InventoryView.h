// Minecraft assets are read from the user's already-installed client jar.
// This view is rendered by 2ship; it does not claim to be Minecraft's GPU renderer.
class StoryWindow : public Ship::GuiWindow {
 bool texturesLoaded=false,wasMenu=false;
 int page=0;
 double swingUntil=0;
 uint32_t lastControls=0;
public:
 StoryWindow():GuiWindow("gComposite.Inventory",true,"Composite inventory"){}
 void InitElement() override{} void UpdateElement() override{} void DrawElement() override{}
 void Draw() override{
  if(std::getenv("COMPOSITE_FRAME")){
   StoryView v;{std::lock_guard<std::mutex> lock(viewMutex);v=storyView;}
   if(v.visible&&v.connected&&(v.port.flags&16)){
    auto vp=ImGui::GetMainViewport();auto dl=ImGui::GetForegroundDrawList();
    const char* hint="Song notes: arrow keys = C buttons   |   Space = A";
    const float size=std::clamp(vp->Size.y/36.0f,18.0f,32.0f);
    ImVec2 pos{vp->Pos.x+24,vp->Pos.y+24};
    dl->AddRectFilled({pos.x-8,pos.y-6},{pos.x+size*32,pos.y+size+8},IM_COL32(0,0,0,215));
    dl->AddText(nullptr,size,pos,IM_COL32_WHITE,hint);
   }
   return;
  }
  auto gui=std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
  auto menu=gui->GetMenu();
  if(hideNativeMenu.exchange(false)&&menu)menu->Hide();
  bool menuOpen=menu&&menu->IsVisible();nativeMenuOpen=menuOpen;
  if(wasMenu&&!menuOpen)recapture=true;
  if(!wasMenu&&menuOpen)releaseMouse=true;
  wasMenu=menuOpen;
  StoryView v;{std::lock_guard<std::mutex> lock(viewMutex);v=storyView;}
  if(!v.visible||menuOpen)return;
  if(!texturesLoaded){
   const char* home=std::getenv("SHIP_HOME");
   for(auto name:{"hotbar","selection","dirt","steve","inventory"}){
    auto texture=std::make_shared<Ship::GuiTexture>();texture->Data=nullptr;
    std::string path=std::string(home?home:".")+"/ui/"+name+".png";
    texture->Data=stbi_load(path.c_str(),&texture->Metadata.Width,&texture->Metadata.Height,nullptr,4);
    if(!texture->Data){spdlog::error("[Composite] Missing local UI texture: {}",path);return;}
    texture->DataSize=texture->Metadata.Width*texture->Metadata.Height*4;
    gui->LoadTextureFromResource(std::string("composite/")+name,texture);
   }
   texturesLoaded=true;
  }
  auto vp=ImGui::GetMainViewport();auto dl=ImGui::GetForegroundDrawList();
  float scale=std::clamp(std::floor(vp->Size.y/360.0f),2.0f,4.0f);
  auto tex=[&](const char* n){return gui->GetTextureByName(std::string("composite/")+n);};
  auto textAt=[&](ImVec2 p,float size,const std::string& text,ImU32 color=IM_COL32_WHITE){
   dl->AddText(nullptr,size,ImVec2(p.x+scale,p.y+scale),IM_COL32(20,20,20,230),text.c_str());dl->AddText(nullptr,size,p,color,text.c_str());
  };
  auto cube=[&](ImVec2 center,float size){
   float w=size*.5f,h=size*.28f;
   ImVec2 top{center.x,center.y-h*1.5f},left{center.x-w,center.y-h*.5f},right{center.x+w,center.y-h*.5f},mid{center.x,center.y+h*.5f};
   ImVec2 bottom{center.x,center.y+h*2},lb{center.x-w,center.y+h},rb{center.x+w,center.y+h};
   dl->AddImageQuad(tex("dirt"),top,right,mid,left,{0,0},{1,0},{1,1},{0,1},IM_COL32(255,255,255,255));
   dl->AddImageQuad(tex("dirt"),left,mid,bottom,lb,{0,0},{1,0},{1,1},{0,1},IM_COL32(180,180,180,255));
   dl->AddImageQuad(tex("dirt"),mid,right,rb,bottom,{0,0},{1,0},{1,1},{0,1},IM_COL32(220,220,220,255));
  };
  auto bindingItem=[&](uint32_t binding)->int{return binding<48?v.port.items[binding]:binding==254?(int)v.port.sword:-1;};
  auto icon=[&](uint32_t binding,ImVec2 p,float cell){
   if(binding==253){cube({p.x+cell*.5f,p.y+cell*.4f},cell*.85f);textAt({p.x+cell*.48f,p.y+cell*.64f},cell*.52f,std::to_string(v.blocks.remaining));}
   else {int id=bindingItem(binding);if(id>=0&&id<131)dl->AddImage(gui->GetTextureByName((const char*)gItemIcons[id]),p,{p.x+cell,p.y+cell});}
  };
  if(!v.connected){textAt({vp->Pos.x+20,vp->Pos.y+40},20,"Waiting for the Minecraft Fabric process...");return;}
  if((v.port.flags&14)&&!v.mc.inventoryOpen)return;
  uint32_t selected=v.mc.hotbar[std::min(v.mc.selected,8u)];
  auto pressed=v.mc.controls&~lastControls;lastControls=v.mc.controls;
  if(pressed&(32|128))swingUntil=ImGui::GetTime()+.25;
  // A textured Steve right arm and the selected held block in first person.
  if(v.mc.thirdPerson==0&&!v.mc.inventoryOpen){
   float s=vp->Size.y/720.0f;
   float swing=std::max(0.0,swingUntil-ImGui::GetTime())/.25;
   float cx=vp->Pos.x+vp->Size.x*.78f-std::sin(swing*3.14159f)*55*s;
   float cy=vp->Pos.y+vp->Size.y*.93f+std::sin(swing*3.14159f)*40*s;
   ImVec2 a{cx-35*s,cy-180*s},b{cx+10*s,cy-198*s},c{cx+160*s,cy+60*s},d{cx+75*s,cy+100*s};
   ImVec2 e{cx+48*s,cy-170*s},f{cx+215*s,cy+72*s};
   dl->AddImageQuad(tex("steve"),a,b,c,d,{44.f/64,32.f/64},{48.f/64,32.f/64},{48.f/64,20.f/64},{44.f/64,20.f/64},IM_COL32_WHITE);
   dl->AddImageQuad(tex("steve"),b,e,f,c,{48.f/64,32.f/64},{52.f/64,32.f/64},{52.f/64,20.f/64},{48.f/64,20.f/64},IM_COL32(185,185,185,255));
   dl->AddImageQuad(tex("steve"),a,{cx+5*s,cy-217*s},e,b,{44.f/64,16.f/64},{48.f/64,16.f/64},{48.f/64,20.f/64},{44.f/64,20.f/64},IM_COL32(235,235,235,255));
   if(selected==253&&v.blocks.remaining)cube({cx-15*s,cy-230*s},165*s);
   else if(bindingItem(selected)>=0&&bindingItem(selected)<131){auto id=bindingItem(selected);dl->AddImage(gui->GetTextureByName((const char*)gItemIcons[id]),{cx-90*s,cy-300*s},{cx+75*s,cy-135*s});}
  }
  if(!v.mc.inventoryOpen){
   ImVec2 center{vp->Pos.x+vp->Size.x/2,vp->Pos.y+vp->Size.y/2};
   dl->AddLine({center.x-4*scale,center.y},{center.x+4*scale,center.y},IM_COL32_WHITE,scale);
   dl->AddLine({center.x,center.y-4*scale},{center.x,center.y+4*scale},IM_COL32_WHITE,scale);
  }
  float barX=vp->Pos.x+(vp->Size.x-182*scale)/2,barY=vp->Pos.y+vp->Size.y-24*scale;
  dl->AddImage(tex("hotbar"),{barX,barY},{barX+182*scale,barY+22*scale});
  dl->AddImage(tex("selection"),{barX+((float)v.mc.selected*20-1)*scale,barY-scale},{barX+(v.mc.selected*20+23)*scale,barY+23*scale});
  for(int h=0;h<9;h++)icon(v.mc.hotbar[h],{barX+(h*20+3)*scale,barY+3*scale},16*scale);
  std::string label=selected==253?"Dirt":selected==254?"Attack / sword":selected<48?"Zelda item":"Empty hand";
  float tw=ImGui::CalcTextSize(label.c_str()).x;
  textAt({vp->Pos.x+vp->Size.x/2-tw*scale/2,barY-14*scale},12*scale,label);
  if(!v.captured&&!v.mc.inventoryOpen)textAt({vp->Pos.x+20,vp->Pos.y+40},18,"Click to play  |  E inventory  |  F5 view");
  if(!v.mc.inventoryOpen)return;
  dl->AddRectFilled(vp->Pos,{vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y},IM_COL32(0,0,0,160));
  float u=std::min({scale,(vp->Size.y-36)/210.0f,(vp->Size.x-36)/200.0f});
  float x=vp->Pos.x+(vp->Size.x-196*u)/2,y=vp->Pos.y+(vp->Size.y-202*u)/2;
  auto point=[&](float a,float b){return ImVec2{x+a*u,y+b*u};};
  auto panel=[&](float a,float b,float w,float h){
   dl->AddRectFilled(point(a,b),point(a+w,b+h),IM_COL32(198,198,198,255));
   dl->AddLine(point(a,b),point(a+w,b),IM_COL32_WHITE,2*u);dl->AddLine(point(a,b),point(a,b+h),IM_COL32_WHITE,2*u);
   dl->AddLine(point(a+w,b),point(a+w,b+h),IM_COL32(55,55,55,255),2*u);dl->AddLine(point(a,b+h),point(a+w,b+h),IM_COL32(55,55,55,255),2*u);
  };
  panel(0,0,196,202);
  auto labelAt=[&](float a,float b,const char* label,float size=8){dl->AddText(nullptr,size*u,point(a,b),IM_COL32(55,55,55,255),label);};
  labelAt(9,8,"Inventory",11);
  labelAt(9,25,"16 dirt blocks for building");labelAt(9,36,"RMB place  /  LMB break  /  F5 view");
  labelAt(9,50,"Select a hotbar slot, then click an item.");
  auto clickBox=[&](ImVec2 lo,ImVec2 hi){return ImGui::IsMouseHoveringRect(lo,hi,false)&&ImGui::IsMouseClicked(ImGuiMouseButton_Left);};
  panel(9,64,85,15);panel(101,64,86,15);
  labelAt(14,68,"Supplies / items");labelAt(106,68,"More items / masks");
  if(clickBox(point(9,64),point(94,79)))page=0;
  if(clickBox(point(101,64),point(187,79)))page=1;
  auto slot=[&](float a,float b,uint32_t binding,bool highlight){
   auto lo=point(a,b),hi=point(a+18,b+18);
   dl->AddRectFilled(lo,hi,IM_COL32(139,139,139,255));
   dl->AddLine(lo,{hi.x,lo.y},IM_COL32(55,55,55,255),u);dl->AddLine(lo,{lo.x,hi.y},IM_COL32(55,55,55,255),u);
   dl->AddLine({lo.x,hi.y},hi,IM_COL32_WHITE,u);dl->AddLine({hi.x,lo.y},hi,IM_COL32_WHITE,u);
   icon(binding,point(a+1,b+1),16*u);
   if(highlight)dl->AddRect(lo,hi,IM_COL32_WHITE,0,0,2*u);
   if(ImGui::IsMouseHoveringRect(lo,hi,false))dl->AddRectFilled(lo,hi,IM_COL32(255,255,255,65));
   return clickBox(lo,hi);
  };
  for(int i=0;i<27;i++){
   int n=page*27+i;uint32_t binding=n==0?253:n==1?254:n-2<48?n-2:255;
   if(binding<48&&v.port.items[binding]==255)binding=255;
   if(slot(17+(i%9)*18,86+(i/9)*18,binding,false)&&binding!=255)uiCommand=(1u<<16)|binding;
  }
  labelAt(17,148,"Hotbar");
  for(int h=0;h<9;h++)if(slot(17+h*18,160,v.mc.hotbar[h],v.mc.selected==(uint32_t)h))uiCommand=(2u<<16)|h;
  labelAt(9,186,"E / Esc close       Blocks reset on scene change");
 }
};
