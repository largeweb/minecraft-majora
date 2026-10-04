import local.composite.Shared;
import java.nio.*;import java.nio.file.*;
public class SharedTest {
 public static void main(String[] a)throws Exception{
  try(var s=new Shared(Path.of(a[0]))){
   var p=s.snapshot(Shared.PORT,56);
   if(p.getInt(0)!=123||p.getFloat(20)!=40||p.getFloat(24)!=-80||p.getInt(8)!=17)throw new AssertionError("C++ -> Java layout");
   var r=s.collide(123,40,-80,120,2,-3,4);
   if(r==null||r[0]!=2||r[1]!=0||r[2]!=4)throw new AssertionError("collision reply");
   var sp=s.snapshot(512,100);if(sp.getInt(0)!=3||(sp.get(55)&255)!=49||sp.get(62)!=12||sp.getInt(84)!=7||sp.getInt(96)!=-2)throw new AssertionError("story layout");
   var layer=s.snapshot(1024,208);if(layer.getInt(0)!=123||layer.getInt(4)!=2||layer.getInt(8)!=14||layer.getInt(12)!=1||layer.getFloat(28)!=40||layer.getFloat(32)!=80||layer.getFloat(36)!=-120)throw new AssertionError("multi-block layout");
   var sm=ByteBuffer.allocate(60).order(ByteOrder.LITTLE_ENDIAN);sm.putInt(0,123);sm.putInt(12,1);sm.putInt(52,253);sm.putInt(56,7);s.publish(768,sm);
   var out=ByteBuffer.allocate(52).order(ByteOrder.LITTLE_ENDIAN);out.putInt(0,123);out.putInt(4,42);out.putInt(32,1);out.putFloat(40,80);s.publish(out);
  }System.out.println("PASS: C++/Java shared-memory layout and collision round trip");
 }
}
