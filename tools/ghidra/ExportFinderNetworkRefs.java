// @category Finder
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
public class ExportFinderNetworkRefs extends GhidraScript {
    public void run() throws Exception {
        File out=new File(getScriptArgs()[0]);out.mkdirs();
        Set<Function> callers=new LinkedHashSet<>();
        try(PrintWriter w=new PrintWriter(new File(out,"references.tsv"),StandardCharsets.UTF_8)) {
            for(long a:new long[]{0x1340f0L,0x1acae8L,0x1b1eecL,0x7daacL,0x1d2260L}) {
                var refs=getReferencesTo(toAddr(a));
                for(var r:refs) {
                    Function f=getFunctionContaining(r.getFromAddress());
                    w.println(Long.toHexString(a)+"\t"+r.getFromAddress()+"\t"+(f==null?"data":f.getName(true)));
                    if(f!=null && a==0x1340f0L)callers.add(f);
                }
            }
        }
        DecompInterface d=new DecompInterface();d.openProgram(currentProgram);
        try {
            for(Function f:callers) {
                var r=d.decompileFunction(f,60,monitor);
                try(PrintWriter w=new PrintWriter(new File(out,f.getEntryPoint()+".c"),StandardCharsets.UTF_8)) {
                    w.println("/* "+f.getName(true)+" */");w.print(r.getDecompiledFunction().getC());
                }
            }
        } finally {d.dispose();}
    }
}
