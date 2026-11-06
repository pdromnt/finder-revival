// Export named UI functions and selected decompilations for the Finder project.
// @category Finder
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

public class ExportFinderUi extends GhidraScript {
    public void run() throws Exception {
        File out = new File(getScriptArgs()[0]);
        out.mkdirs();
        Set<Long> wanted = new LinkedHashSet<>(Arrays.asList(
            0x35858L, 0x135d40L, 0x129a3cL, 0x1e61b0L, 0xfb168L
        ));
        if (getScriptArgs().length > 1) {
            wanted.clear();
            for (int i = 1; i < getScriptArgs().length; i++)
                wanted.add(Long.parseUnsignedLong(getScriptArgs()[i].replace("0x", ""), 16));
        }
        try (PrintWriter names = new PrintWriter(new File(out, "functions.tsv"), StandardCharsets.UTF_8)) {
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            while (it.hasNext()) {
                Function f = it.next();
                names.println(f.getEntryPoint() + "\t" + f.getBody().getNumAddresses() + "\t" + f.getName(true));
                String n = f.getName(true).toLowerCase();
                if (getScriptArgs().length == 1 && (n.contains("setupui") || n.contains("stylesheet") || n.contains("qinitresources")))
                    if (!f.isExternal() && f.getBody().getNumAddresses() > 32)
                        wanted.add(f.getEntryPoint().getOffset());
            }
        }
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        try {
            for (long addr : wanted) {
                monitor.checkCancelled();
                Function f = currentProgram.getFunctionManager().getFunctionAt(toAddr(addr));
                if (f == null) { println("No function at " + Long.toHexString(addr)); continue; }
                DecompileResults r = decomp.decompileFunction(f, 90, monitor);
                String text = r.decompileCompleted() ? r.getDecompiledFunction().getC() : r.getErrorMessage();
                File target = new File(out, Long.toHexString(addr) + ".c");
                try (PrintWriter w = new PrintWriter(target, StandardCharsets.UTF_8)) {
                    w.println("/* " + f.getName(true) + " @ " + f.getEntryPoint() + " */");
                    w.print(text);
                }
                println("Exported " + f.getName(true) + " to " + target);
            }
        } finally { decomp.dispose(); }
    }
}
