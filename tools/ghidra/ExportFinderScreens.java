// Export the complete named touchscreen surface, not just generated setupUi.
// @category Finder
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

public class ExportFinderScreens extends GhidraScript {
    public void run() throws Exception {
        File out = new File(getScriptArgs()[0]); out.mkdirs();
        List<Function> functions = new ArrayList<>();
        Set<String> classes = new TreeSet<>();
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext()) {
            Function f = it.next();
            if (f.isExternal()) continue;
            functions.add(f);
            String n = f.getName(true);
            int colon = n.indexOf("::");
            if (colon < 0 || n.startsWith("<")) continue;
            if (n.contains("::on_") || n.endsWith("::setupUi")) {
                String c = n.substring(0, colon);
                classes.add(c); if (c.startsWith("Ui_")) classes.add(c.substring(3));
            }
        }
        classes.addAll(Arrays.asList("ShowList", "ProgressDialog", "NoButtonDialog",
            "UpdateConfirmDialog", "ProgressDialogUpdate", "SerialComDialog"));
        DecompInterface decomp = new DecompInterface(); decomp.openProgram(currentProgram);
        try (PrintWriter index = new PrintWriter(new File(out, "index.tsv"), StandardCharsets.UTF_8);
             PrintWriter calls = new PrintWriter(new File(out, "calls.tsv"), StandardCharsets.UTF_8)) {
            index.println("address\tbytes\tclass\tname\tprototype\tdecompiled");
            calls.println("caller_address\tcaller\tcallee_address\tcallee");
            int count = 0;
            for (Function f : functions) {
                monitor.checkCancelled();
                String n = f.getName(true); int colon = n.indexOf("::");
                if (colon < 0 || !classes.contains(n.substring(0, colon))) continue;
                String c = n.substring(0, colon);
                for (Function callee : f.getCalledFunctions(monitor))
                    calls.println(f.getEntryPoint()+"\t"+n+"\t"+callee.getEntryPoint()+"\t"+callee.getName(true));
                // Generated meta-object methods are indexed, but need no source review.
                boolean generated = n.endsWith("::qt_metacall") || n.endsWith("::qt_metacast") ||
                    n.endsWith("::metaObject") || n.endsWith("::tr") || n.endsWith("::trUtf8");
                boolean ok = false;
                if (!generated) {
                    DecompileResults r = decomp.decompileFunction(f, 45, monitor);
                    ok = r.decompileCompleted();
                    try (PrintWriter w = new PrintWriter(new File(out, f.getEntryPoint()+".c"), StandardCharsets.UTF_8)) {
                        w.println("/* "+n+" @ "+f.getEntryPoint()+" */");
                        w.print(ok ? r.getDecompiledFunction().getC() : r.getErrorMessage());
                    }
                }
                index.println(f.getEntryPoint()+"\t"+f.getBody().getNumAddresses()+"\t"+c+"\t"+n+"\t"+f.getPrototypeString(false,false)+"\t"+(generated ? "generated" : ok ? "yes" : "FAILED"));
                index.flush(); calls.flush();
                if (++count % 25 == 0) println("Exported "+count+" screen functions; now "+n);
            }
            println("Complete: "+count+" functions across "+classes.size()+" scopes");
        } finally { decomp.dispose(); }
    }
}
