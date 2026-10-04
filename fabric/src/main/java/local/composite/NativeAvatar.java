package local.composite;
import java.util.UUID;
import net.minecraft.client.Minecraft;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.*;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.component.CustomData;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
/** Zelda owns form, mask ownership and transformation; Minecraft owns controls. */
public final class NativeAvatar {
 private static volatile UUID owner;
 private static Entity attachedPlayer;
 private static volatile float radius=14,height=40,eye=34;
 private static Shared shared;private static int epoch,serial,cooldown;
 private static final String[] NAMES={"Postman's Hat","All-Night Mask","Blast Mask","Stone Mask","Great Fairy's Mask","Deku Mask","Keaton Mask","Bremen Mask","Bunny Hood","Don Gero's Mask","Mask of Scents","Goron Mask","Romani's Mask","Circus Leader's Mask","Kafei's Mask","Couple's Mask","Mask of Truth","Zora Mask","Kamaro's Mask","Gibdo Mask","Garo's Mask","Captain's Hat","Giant's Mask","Fierce Deity's Mask"};
 public static boolean owns(Entity entity){return Passthrough.active()&&entity instanceof Player&&entity.getUUID().equals(owner);}
 public static EntityDimensions dimensions(Entity entity,Pose pose){
  if(!owns(entity))return null;
  float h=height,e=eye;
  if(pose==Pose.CROUCHING){h*=.8f;e*=.8f;}
  if(pose==Pose.SWIMMING||pose==Pose.FALL_FLYING||pose==Pose.SPIN_ATTACK){h=Math.min(h,24);e=h*.8f;}
  return EntityDimensions.scalable(radius*2/40,h/40).withEyeHeight(e/40);
 }
 public static int slot(ItemStack stack){
  var data=stack.get(DataComponents.CUSTOM_DATA);if(data==null)return -1;
  int slot=data.copyTag().getIntOr("composite_mask_slot",-1);
  return slot>=24&&slot<48?slot:-1;
 }
 public static ItemStack item(int slot){
  if(slot<24||slot>=48)throw new IllegalArgumentException("Invalid mask slot");
  var stack=new ItemStack(Items.PAPER);var tag=new CompoundTag();tag.putInt("composite_mask_slot",slot);
  stack.set(DataComponents.CUSTOM_DATA,CustomData.of(tag));
  stack.set(DataComponents.CUSTOM_NAME,Component.literal(NAMES[slot-24]));
  stack.set(DataComponents.ITEM_MODEL,Identifier.fromNamespaceAndPath("composite","mask_"+slot));
  stack.set(DataComponents.LORE,new net.minecraft.world.item.component.ItemLore(java.util.List.of(Component.literal("Majora’s Mask — right-click to equip; earn in Zelda first"))));
  stack.set(DataComponents.MAX_STACK_SIZE,1);return stack;
 }
 public static void tick(Minecraft mc,Shared shm,int currentEpoch,java.nio.ByteBuffer story){
  boolean newPlayer=attachedPlayer!=mc.player||epoch!=currentEpoch;attachedPlayer=mc.player;shared=shm;epoch=currentEpoch;owner=mc.player.getUUID();if(cooldown>0)cooldown--;
  var avatar=shm.snapshot(640,28);
  if(avatar!=null&&avatar.getInt(0)==epoch){
   float r=avatar.getFloat(8),h=avatar.getFloat(12),e=avatar.getFloat(16);
   if(Float.isFinite(r)&&Float.isFinite(h)&&Float.isFinite(e)&&r>=5&&r<=40&&h>=12&&h<=120&&e>0&&e<=h){
    if(newPlayer||radius!=r||height!=h||eye!=e){radius=r;height=h;eye=e;mc.player.refreshDimensions();var uuid=owner;var server=mc.getSingleplayerServer();
     server.execute(()->{var sp=server.getPlayerList().getPlayer(uuid);if(sp!=null)sp.refreshDimensions();});}
   }
  }
  // Mask tokens are ordinary, disposable inventory items. Creative supplies replacements;
  // native ownership is checked only when equipping, never by rewriting the inventory.
 }
 public static boolean use(Minecraft mc){
  if(!Passthrough.active()||mc.player==null||shared==null||slot(mc.player.getMainHandItem())<0)return false;
  if(!Passthrough.interactive()||cooldown>0)return true;
  int slot=slot(mc.player.getMainHandItem());var story=shared.snapshot(512,100);
  if(story==null||Byte.toUnsignedInt(story.get(8+slot))==255){mc.player.sendSystemMessage(Component.literal("This mask must first be earned in Zelda."));return true;}
  var request=Passthrough.buffer(12);request.putInt(epoch).putInt(++serial).putInt(slot);shared.publish(896,request);cooldown=20;return true;
 }
}
