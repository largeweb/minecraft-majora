package local.composite;
import java.lang.reflect.*;import net.minecraft.world.phys.*;
public class CollisionTest {
 static void near(double a,double b){if(Math.abs(a-b)>1e-8)throw new AssertionError(a+" != "+b);}
 public static void main(String[] args)throws Exception{
  Field f=Bridge.class.getDeclaredField("block");f.setAccessible(true);f.set(null,new AABB(0,0,0,1,1,1));
  // Stop at the near side, land on the top, hit the underside, slide along side.
  near(Bridge.clip(new AABB(-1,0,0.2,-0.4,1.8,0.8),new Vec3(1,0,0)).x,0.4);
  near(Bridge.clip(new AABB(0.2,2,0.2,0.8,3.8,0.8),new Vec3(0,-2,0)).y,-1);
  near(Bridge.clip(new AABB(0.2,-2,0.2,0.8,-0.2,0.8),new Vec3(0,1,0)).y,0.2);
  var slide=Bridge.clip(new AABB(-1,0,0.2,-0.4,1.8,0.8),new Vec3(1,0,0.2));near(slide.x,0.4);near(slide.z,0.2);
  f.set(null,null);near(Bridge.clip(new AABB(-1,0,0.2,-0.4,1.8,0.8),new Vec3(1,0,0)).x,1);
  System.out.println("PASS: Minecraft VoxelShape side/top/underside/slide/break collision");
 }
}
