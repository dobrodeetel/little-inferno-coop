// Headless Ghidra script: list every "<Class>::vftable" with its slots.
// Usage: analyzeHeadless <proj> wog -process WorldOfGoo.exe -noanalysis -postScript DumpVtables.java <out dir>
// Writes <out>/vtables.txt: "Class  +0xOFF  FUN_xxx" per slot.
//@category licoop

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import java.io.*;

public class DumpVtables extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args.length > 0 ? args[0] : ".");
        out.mkdirs();
        int classes = 0;
        try (PrintWriter w = new PrintWriter(new BufferedWriter(new FileWriter(new File(out, "vtables.txt"))))) {
            SymbolIterator it = currentProgram.getSymbolTable().getSymbolIterator("vftable", true);
            while (it.hasNext()) {
                Symbol s = it.next();
                String cls = s.getParentNamespace().getName(true);
                Address a = s.getAddress();
                w.println("== " + cls + " @ " + a);
                for (int i = 0; i < 128; i++) {
                    Address slot = a.add(i * 8L);
                    if (i > 0 && currentProgram.getSymbolTable().getPrimarySymbol(slot) != null
                            && currentProgram.getSymbolTable().getPrimarySymbol(slot).getName().equals("vftable")) break;
                    long ptr = getLong(slot);
                    Address target = toAddr(ptr);
                    Function f = getFunctionAt(target);
                    if (f == null) break;
                    w.println(cls + "\t+0x" + Integer.toHexString(i * 8) + "\t" + f.getName());
                }
                classes++;
            }
        }
        println("vtables: " + classes);
    }
}
