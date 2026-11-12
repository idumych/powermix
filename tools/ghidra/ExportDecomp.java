// Decompile every function and write one .c file per class (namespace) to
// <outdir>/<program>/, plus an index of address -> name.
// Script argument: output directory.
//@category BeOS

import java.io.File;
import java.io.PrintWriter;
import java.util.Map;
import java.util.TreeMap;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.Namespace;

public class ExportDecomp extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File out = new File(args.length > 0 ? args[0] : "decomp", currentProgram.getName());
		out.mkdirs();

		DecompInterface ifc = new DecompInterface();
		ifc.setOptions(new DecompileOptions());
		ifc.openProgram(currentProgram);

		Map<String, StringBuilder> files = new TreeMap<>();
		StringBuilder index = new StringBuilder();
		FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
		int n = 0, failed = 0;
		while (it.hasNext() && !monitor.isCancelled()) {
			Function f = it.next();
			if (f.isThunk() || f.isExternal())
				continue;
			Namespace ns = f.getParentNamespace();
			String key = ns.isGlobal() ? "_global" : ns.getName(true).replace("::", "__");
			key = key.replaceAll("[^A-Za-z0-9_]", "_");
			StringBuilder sb = files.computeIfAbsent(key, k -> new StringBuilder());

			DecompileResults r = ifc.decompileFunction(f, 120, monitor);
			sb.append("// ").append(f.getEntryPoint()).append("  ")
				.append(f.getName(true)).append('\n');
			if (r != null && r.decompileCompleted()) {
				sb.append(r.getDecompiledFunction().getC()).append('\n');
			} else {
				failed++;
				sb.append("// decompilation failed: ")
					.append(r == null ? "null" : r.getErrorMessage()).append("\n\n");
			}
			index.append(f.getEntryPoint()).append('\t').append(key).append('\t')
				.append(f.getSignature().getPrototypeString()).append('\n');
			if (++n % 500 == 0)
				println(currentProgram.getName() + ": " + n + " functions");
		}
		ifc.dispose();

		for (Map.Entry<String, StringBuilder> e : files.entrySet()) {
			try (PrintWriter w = new PrintWriter(new File(out, e.getKey() + ".c"), "UTF-8")) {
				w.print(e.getValue());
			}
		}
		try (PrintWriter w = new PrintWriter(new File(out, "_index.tsv"), "UTF-8")) {
			w.print(index);
		}
		println(currentProgram.getName() + ": exported " + n + " functions in "
			+ files.size() + " files, " + failed + " failed");
	}
}
