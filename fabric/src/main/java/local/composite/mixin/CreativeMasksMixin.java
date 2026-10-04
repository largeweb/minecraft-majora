package local.composite.mixin;
import local.composite.*;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.item.*;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.*;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
@Mixin(CreativeModeTab.class)
public class CreativeMasksMixin {
 @Inject(method="buildContents",at=@At("TAIL"))
 private void composite$masks(CreativeModeTab.ItemDisplayParameters parameters,CallbackInfo ci){
  if(!Passthrough.ENABLED)return;
  var tab=(CreativeModeTab)(Object)this;
  var id=BuiltInRegistries.CREATIVE_MODE_TAB.getKey(tab);
  if(id==null||!id.toString().equals("minecraft:tools_and_utilities"))return;
  for(int slot=24;slot<48;slot++){
   tab.getDisplayItems().add(NativeAvatar.item(slot));
   tab.getSearchTabDisplayItems().add(NativeAvatar.item(slot));
  }
 }
}
