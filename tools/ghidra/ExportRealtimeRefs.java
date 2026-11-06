// @category Finder
import ghidra.app.script.GhidraScript;
import java.io.*;
import java.nio.charset.StandardCharsets;
public class ExportRealtimeRefs extends GhidraScript {
 public void run() throws Exception {
  try(PrintWriter out=new PrintWriter(new File(getScriptArgs()[0]),StandardCharsets.UTF_8)) {
   for(long address:new long[]{0x1a85ccL,0x1a882cL}) {
    out.println("REFERENCES "+Long.toHexString(address));
    for(var reference:getReferencesTo(toAddr(address))) {
     var f=getFunctionContaining(reference.getFromAddress());
     out.println(reference.getFromAddress()+"\t"+(f==null?"data":f.getName(true)));
    }
   }
  }
 }
}
