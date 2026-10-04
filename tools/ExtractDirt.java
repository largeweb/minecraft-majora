import java.util.zip.*;import java.nio.file.*;import javax.imageio.ImageIO;
public class ExtractDirt {
 public static void main(String[] args)throws Exception{
  try(var zip=new ZipFile(args[0])){
   var img=ImageIO.read(zip.getInputStream(zip.getEntry("assets/minecraft/textures/block/dirt.png")));
   if(img.getWidth()!=16||img.getHeight()!=16)throw new Exception("Expected 16x16 dirt texture");
   StringBuilder s=new StringBuilder("// Extracted locally from the user's installed Minecraft. Do not redistribute.\n#pragma once\nalignas(8) static const unsigned char dirtTexture[] = {\n");
   for(int y=0;y<16;y++)for(int x=0;x<16;x++){
    int p=img.getRGB(x,y),v=(((p>>19)&31)<<11)|(((p>>11)&31)<<6)|(((p>>3)&31)<<1)|1;
    s.append((v>>8)&255).append(',').append(v&255).append(',');
   }s.append("\n};\n");Files.writeString(Path.of(args[1]),s);
  }
 }
}
