// Headless Ghidra script: decompile every function and list string cross-references.
// Usage (analyzeHeadless ... -postScript DumpAll.java <out dir>)
// Writes <out>/decomp.c (all functions) and <out>/strings.txt (string -> referencing functions).
//@category licoop

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.data.StringDataInstance;
import java.io.*;

public class DumpAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args.length > 0 ? args[0] : ".");
        out.mkdirs();

        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);
        FunctionManager fm = currentProgram.getFunctionManager();
        int n = 0, failed = 0;
        try (PrintWriter w = new PrintWriter(new BufferedWriter(new FileWriter(new File(out, "decomp.c"))))) {
            for (Function f : fm.getFunctions(true)) {
                if (monitor.isCancelled()) break;
                if (f.isThunk() || f.isExternal()) continue;
                DecompileResults r = dec.decompileFunction(f, 60, monitor);
                w.println("// ==== " + f.getName() + " @ " + f.getEntryPoint());
                if (r != null && r.decompileCompleted()) {
                    w.println(r.getDecompiledFunction().getC());
                } else {
                    w.println("// decompile failed");
                    failed++;
                }
                n++;
            }
        }
        println("decompiled " + n + " functions, failed " + failed);

        try (PrintWriter w = new PrintWriter(new BufferedWriter(new FileWriter(new File(out, "strings.txt"))))) {
            Listing listing = currentProgram.getListing();
            for (Data d : listing.getDefinedData(true)) {
                StringDataInstance s = StringDataInstance.getStringDataInstance(d);
                if (s == StringDataInstance.NULL_INSTANCE) continue;
                String v = s.getStringValue();
                if (v == null || v.length() < 3) continue;
                StringBuilder refs = new StringBuilder();
                for (Reference ref : getReferencesTo(d.getAddress())) {
                    Function f = fm.getFunctionContaining(ref.getFromAddress());
                    refs.append(' ').append(f != null ? f.getName() : ref.getFromAddress().toString());
                }
                w.println(d.getAddress() + "\t" + v.replace("\n", "\\n") + "\t" + refs);
            }
        }
        println("strings written");
    }
}
