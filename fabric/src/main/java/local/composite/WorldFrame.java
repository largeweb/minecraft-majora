package local.composite;
import java.nio.*;
import java.nio.file.*;
import java.nio.channels.FileChannel;
import java.lang.invoke.*;
import com.mojang.blaze3d.pipeline.TextureTarget;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.renderpearl.api.GpuFormat;
import net.minecraft.client.Minecraft;
/** A single coherent source image and its actual render camera, latched before culling. */
public final class WorldFrame {
 static final VarHandle INT=MethodHandles.byteBufferViewVarHandle(int[].class,ByteOrder.LITTLE_ENDIAN);
 static MappedByteBuffer map;static FileChannel file;static TextureTarget texture;
 static ByteBuffer pixels,staging;static int sequence,producer,width,height,uploaded=-1;static long changed;
 static FrameCamera camera;static float light=1;
 public record FrameCamera(int epoch,float x,float y,float z,float yaw,float pitch,float fov,int valid,int request,int scene){}
 public static float light(){return light;}
 public static int sequence(){return sequence;}
 public static FrameCamera prepare(){
  if(!Passthrough.active())return null;
  try{
   if(map==null){var path=Path.of(System.getProperty("composite.shm")+".rgba");if(!Files.exists(path))return null;file=FileChannel.open(path,StandardOpenOption.READ);map=file.map(FileChannel.MapMode.READ_ONLY,0,file.size());map.order(ByteOrder.LITTLE_ENDIAN);}
   int seq=(int)INT.getAcquire(map,0),w=map.getInt(8),h=map.getInt(12),pid=map.getInt(16);
   if(map.getInt(4)!=0x46524D31||w<1||h<1||w>2560||h>1440)return null;
   if((seq!=sequence||pid!=producer)&&(seq&1)==0){
    int size=w*h*4;if(staging==null||staging.capacity()!=size)staging=ByteBuffer.allocateDirect(size);
    var pose=new FrameCamera(map.getInt(20),map.getFloat(24),map.getFloat(28),map.getFloat(32),map.getFloat(36),map.getFloat(40),map.getFloat(44),map.getInt(48),map.getInt(52),map.getInt(56));
    float brightness=map.getFloat(60);
    staging.clear();staging.put(map.slice(64,size));staging.flip();VarHandle.acquireFence();
    if(seq==(int)INT.getAcquire(map,0)){
     var old=pixels;pixels=staging;staging=old;camera=pose;
     if(Float.isFinite(brightness)&&brightness>=.1f&&brightness<=1)light=brightness;
     if(pid!=producer)uploaded=-1;
     sequence=seq;producer=pid;width=w;height=h;changed=System.nanoTime();
    }
   }
   return camera!=null&&System.nanoTime()-changed<250_000_000L?camera:null;
  }catch(Exception e){throw new IllegalStateException("Zelda framebuffer snapshot failed",e);}
 }
 public static boolean draw(){
  if(!Passthrough.active()||pixels==null||System.nanoTime()-changed>2_000_000_000L)return false;
  if(uploaded!=sequence){
   if(texture==null||texture.width!=width||texture.height!=height){if(texture!=null)texture.destroyBuffers();texture=new TextureTarget("Zelda live world",width,height,GpuFormat.RGBA8_UNORM,null);}
   var encoder=RenderSystem.getDevice().createCommandEncoder();encoder.writeToTexture(texture.getColorTexture(),pixels,0,0,0,0,width,height);encoder.submit();uploaded=sequence;
  }
  var target=Minecraft.getInstance().gameRenderer.mainRenderTarget();
  texture.blitAndBlendToTexture(target.getColorTextureView(),null);
  var encoder=RenderSystem.getDevice().createCommandEncoder();encoder.clearDepthTexture(target.getDepthTexture(),0.0);encoder.submit();return true;
 }
}
