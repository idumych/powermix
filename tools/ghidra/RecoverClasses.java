// Recover Gobe's class layouts and fill in the placeholder structs the
// demangler created for them (/Demangler/<Class>), which is what `this` points
// to in every method. Run after FixThiscall (needs `this`) and before export.
//
// Every function is decompiled once; from the p-code:
//  - size:    `__builtin_new(N)` whose result is passed as `this` to a
//             constructor of C  ->  sizeof(C) = N;
//  - base:    a constructor of C calling a constructor of B on its own `this`
//             (offset 0)  ->  B is C's primary base;
//  - members: constructor calls of X on `this + K` in C's constructors  ->  an
//             X object (member or secondary base) at K;
//  - vptr:    a constructor of C storing C's vtable address at `this + K`;
//  - fields:  Ghidra's FillOutStructureHelper on `this` in each method of C;
//             all methods' offsets/types are merged per class.
//
// Structs are built bases first: base at 0, vtable pointer (if the class
// introduces it), member objects, then fields named field_0x<off>. Fields
// inside the base or a member object are dropped (they belong to those
// classes). Be API classes (from BeAPI_gcc2.gdt) are used as bases but never
// changed.
//
// Script arguments (optional): directory for <program>.classes.tsv, a summary
// per class (size, base, vptr, field count, evidence); and a .gdt path to
// save the filled structs to (used for the main app, so the parts can apply
// its layouts via ApplyBeTypes instead of rebuilding them from partial views).
//@category BeOS

import java.io.File;
import java.io.PrintWriter;
import java.util.*;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.util.FillOutStructureHelper;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.pcode.*;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.Symbol;

public class RecoverClasses extends GhidraScript {

	static class ClassInfo {
		final String name;
		final Map<Long, Integer> sizes = new TreeMap<>();      // size -> votes (as long key)
		final Map<String, Integer> bases = new HashMap<>();    // base -> votes
		final Map<Long, String> members = new TreeMap<>();     // offset -> class
		final Map<Long, Integer> vptrs = new TreeMap<>();      // offset -> votes
		final Map<Long, Map<String, Object[]>> fields = new TreeMap<>(); // off -> type path -> {DataType, votes}
		int methods;
		ClassInfo(String n) { name = n; }
	}

	private final Map<String, ClassInfo> classes = new TreeMap<>();
	private DataTypeManager dtm;

	private ClassInfo info(String n) {
		return classes.computeIfAbsent(n, ClassInfo::new);
	}

	@Override
	protected void run() throws Exception {
		dtm = currentProgram.getDataTypeManager();
		DecompInterface ifc = new DecompInterface();
		DecompileOptions opts = new DecompileOptions();
		ifc.setOptions(opts);
		ifc.openProgram(currentProgram);
		FillOutStructureHelper fill = new FillOutStructureHelper(currentProgram, monitor);

		int n = 0;
		FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
		while (it.hasNext() && !monitor.isCancelled()) {
			Function f = it.next();
			if (f.isThunk())
				continue;
			DecompileResults r = ifc.decompileFunction(f, 60, monitor);
			if (r == null || !r.decompileCompleted() || r.getHighFunction() == null)
				continue;
			HighFunction hf = r.getHighFunction();
			collectSizes(hf);

			String cls = className(f);
			if (cls != null && ("__thiscall".equals(f.getCallingConventionName())
					|| hasThisParam(f))) {
				ClassInfo ci = info(cls);
				ci.methods++;
				HighVariable self = thisVar(hf);
				if (self != null) {
					if (f.getName().equals(cls))
						collectCtorFacts(ci, hf, self);
					collectFields(ci, f, self, fill, ifc);
				}
			}
			if (++n % 1000 == 0)
				println(currentProgram.getName() + ": analyzed " + n + " functions");
		}
		ifc.dispose();

		int built = buildStructs();
		writeSummary();
		String[] args = getScriptArgs();
		if (args.length > 1)
			exportStructs(new File(args[1]));
		println(currentProgram.getName() + ": " + classes.size() + " classes seen, "
			+ built + " structs filled");
	}

	// ---- fact collection -------------------------------------------------

	private static String className(Function f) {
		Namespace ns = f.getParentNamespace();
		if (ns == null || ns.isGlobal() || f.isExternal())
			return null;
		return ns.getName();
	}

	/** Struct-returning methods have an explicit `this` after `__return`. */
	private static boolean hasThisParam(Function f) {
		for (Parameter p : f.getParameters())
			if ("this".equals(p.getName()))
				return true;
		return false;
	}

	private static HighVariable thisVar(HighFunction hf) {
		LocalSymbolMap lsm = hf.getLocalSymbolMap();
		if (lsm.getNumParams() == 0)
			return null;
		for (int i = 0; i < lsm.getNumParams(); i++) {
			HighSymbol s = lsm.getParamSymbol(i);
			if (s != null && "this".equals(s.getName()))
				return s.getHighVariable();
		}
		HighSymbol s = lsm.getParamSymbol(0);
		if (s == null || "__return".equals(s.getName()))
			return null;
		return s.getHighVariable();
	}

	/** __builtin_new(N) feeding a constructor's `this`. */
	private void collectSizes(HighFunction hf) {
		Iterator<PcodeOpAST> ops = hf.getPcodeOps();
		while (ops.hasNext()) {
			PcodeOpAST op = ops.next();
			if (op.getOpcode() != PcodeOp.CALL || op.getNumInputs() < 2)
				continue;
			Function callee = getFunctionAt(op.getInput(0).getAddress());
			if (callee == null || !isCtor(callee))
				continue;
			Varnode self = strip(op.getInput(1));
			PcodeOp def = self.getDef();
			if (def == null || def.getOpcode() != PcodeOp.CALL || def.getNumInputs() < 2)
				continue;
			Function alloc = getFunctionAt(def.getInput(0).getAddress());
			if (alloc == null || !alloc.getName().equals("__builtin_new"))
				continue;
			Varnode size = def.getInput(1);
			if (size.isConstant())
				vote(info(callee.getParentNamespace().getName()).sizes, size.getOffset());
		}
	}

	private void collectCtorFacts(ClassInfo ci, HighFunction hf, HighVariable self) {
		// vtables of other classes stored by this constructor: offset -> class
		Map<Long, Set<String>> foreignVtables = new HashMap<>();
		Set<Long> ownVtableSlots = new HashSet<>();
		Iterator<PcodeOpAST> ops = hf.getPcodeOps();
		while (ops.hasNext()) {
			PcodeOpAST op = ops.next();
			if (op.getOpcode() == PcodeOp.CALL && op.getNumInputs() >= 2) {
				Function callee = getFunctionAt(op.getInput(0).getAddress());
				if (callee == null || !isCtor(callee))
					continue;
				long off = offsetFrom(op.getInput(1), self);
				String other = callee.getParentNamespace().getName();
				if (off == 0 && !other.equals(ci.name))
					vote(ci.bases, other);
				else if (off > 0)
					ci.members.put(off, other);
			} else if (op.getOpcode() == PcodeOp.STORE && op.getNumInputs() >= 3) {
				long off = offsetFrom(op.getInput(1), self);
				long addr = constantAddress(op.getInput(2));
				if (off < 0 || addr < 0)
					continue;
				Address a = toAddr(addr);
				Symbol sym = getSymbolAt(a);
				if (sym == null)
					continue;
				if (isVtableOf(sym.getName(), ci.name)) {
					vote(ci.vptrs, off);
					ownVtableSlots.add(off);
				} else if (sym.getName().endsWith("_virtual_table")) {
					String other = sym.getName().substring(0,
						sym.getName().length() - "_virtual_table".length());
					foreignVtables.computeIfAbsent(off, k -> new HashSet<>()).add(other);
				}
			}
		}
		// An inlined base constructor stores the base's vtable into the slot
		// this class then overwrites with its own (e.g. TMICNElement storing
		// TRibbonElement's vtable at +0x28 first).
		for (Long slot : ownVtableSlots)
			for (String other : foreignVtables.getOrDefault(slot, Collections.emptySet()))
				if (!other.equals(ci.name) && !other.contains("_"))
					vote(ci.bases, other);
	}

	private void collectFields(ClassInfo ci, Function f, HighVariable self,
			FillOutStructureHelper fill, DecompInterface ifc) {
		try {
			fill.processStructure(self, f, true, false, ifc);
		} catch (Exception e) {
			return;
		}
		NoisyStructureBuilder map = fill.getComponentMap();
		if (map == null)
			return;
		Iterator<Map.Entry<Long, DataType>> it = map.iterator();
		while (it.hasNext()) {
			Map.Entry<Long, DataType> e = it.next();
			DataType dt = e.getValue();
			if (dt == null)
				continue;
			Map<String, Object[]> byType = ci.fields.computeIfAbsent(e.getKey(), k -> new HashMap<>());
			Object[] slot = byType.computeIfAbsent(dt.getPathName(), k -> new Object[] { dt, 0 });
			slot[1] = (Integer) slot[1] + 1;
		}
	}

	private static boolean isCtor(Function f) {
		Namespace ns = f.getParentNamespace();
		return ns != null && !ns.isGlobal() && f.getName().equals(ns.getName());
	}

	private static boolean isVtableOf(String sym, String cls) {
		return sym.equals(cls + "_virtual_table") || sym.equals("_vt." + cls.length() + cls)
			|| sym.endsWith("::" + cls + "_virtual_table")
			|| sym.equals("vtable") || sym.equals(cls + "::vtable");
	}

	/** Offset of v from self (self, self + c, casts/copies), or -1. */
	private static long offsetFrom(Varnode v, HighVariable self) {
		long off = 0;
		for (int depth = 0; depth < 8 && v != null; depth++) {
			if (v.getHigh() == self)
				return off;
			PcodeOp d = v.getDef();
			if (d == null)
				return -1;
			switch (d.getOpcode()) {
				case PcodeOp.COPY:
				case PcodeOp.CAST:
					v = d.getInput(0);
					break;
				case PcodeOp.INT_ADD:
				case PcodeOp.PTRSUB:
					if (d.getInput(1).isConstant()) {
						off += d.getInput(1).getOffset();
						v = d.getInput(0);
					} else if (d.getInput(0).isConstant()) {
						off += d.getInput(0).getOffset();
						v = d.getInput(1);
					} else
						return -1;
					break;
				case PcodeOp.PTRADD:
					// base + index * element size (placeholder structs are 1 byte)
					if (d.getInput(1).isConstant() && d.getInput(2).isConstant()) {
						off += d.getInput(1).getOffset() * d.getInput(2).getOffset();
						v = d.getInput(0);
					} else
						return -1;
					break;
				default:
					return -1;
			}
		}
		return -1;
	}

	/** A constant, or an address expression like PTRSUB(0, addr), or -1. */
	private static long constantAddress(Varnode v) {
		for (int i = 0; i < 4 && v != null; i++) {
			if (v.isConstant())
				return v.getOffset();
			PcodeOp d = v.getDef();
			if (d == null)
				return -1;
			int oc = d.getOpcode();
			if (oc == PcodeOp.COPY || oc == PcodeOp.CAST)
				v = d.getInput(0);
			else if (oc == PcodeOp.PTRSUB && d.getInput(0).isConstant()
					&& d.getInput(0).getOffset() == 0 && d.getInput(1).isConstant())
				return d.getInput(1).getOffset();
			else
				return -1;
		}
		return -1;
	}

	private static Varnode strip(Varnode v) {
		for (int i = 0; i < 4 && v.getDef() != null; i++) {
			int oc = v.getDef().getOpcode();
			if (oc != PcodeOp.CAST && oc != PcodeOp.COPY)
				break;
			v = v.getDef().getInput(0);
		}
		return v;
	}

	private static <K> void vote(Map<K, Integer> m, K k) {
		m.merge(k, 1, Integer::sum);
	}

	private static <K> K best(Map<K, Integer> m) {
		K bk = null;
		int bv = 0;
		for (Map.Entry<K, Integer> e : m.entrySet())
			if (e.getValue() > bv) {
				bk = e.getKey();
				bv = e.getValue();
			}
		return bk;
	}

	// ---- struct building -------------------------------------------------

	private final Set<String> done = new HashSet<>();
	private final Set<String> inProgress = new HashSet<>();
	private final Map<String, String> notes = new HashMap<>();

	private int buildStructs() {
		int built = 0;
		int tx = currentProgram.startTransaction("Recover classes");
		try {
			for (String c : classes.keySet())
				if (build(c))
					built++;
		} finally {
			currentProgram.endTransaction(tx, true);
		}
		return built;
	}

	/** The struct `this` points to for class c (placeholder or Be API type). */
	private Structure classStruct(String c) {
		List<DataType> found = new ArrayList<>();
		dtm.findDataTypes(c, found);
		Structure be = null, other = null;
		for (DataType dt : found) {
			if (!(dt instanceof Structure))
				continue;
			String p = dt.getCategoryPath().getPath();
			if (p.startsWith("/BeAPI"))
				be = (Structure) dt;
			else if (p.equals("/Demangler") || p.startsWith("/Demangler/"))
				other = (Structure) dt;
		}
		return be != null ? be : other;
	}

	private boolean build(String c) {
		if (done.contains(c) || inProgress.contains(c))
			return false;
		ClassInfo ci = classes.get(c);
		Structure s = classStruct(c);
		// only fill the demangler's empty placeholders: Be API types replaced
		// by ApplyBeTypes keep the placeholder's /Demangler path
		if (ci == null || s == null || !isPlaceholder(s))
			return false;
		inProgress.add(c);
		StringBuilder note = new StringBuilder();

		String base = best(ci.bases);
		Structure baseStruct = null;
		if (base != null) {
			build(base);
			baseStruct = classStruct(base);
			if (baseStruct != null && (baseStruct.isNotYetDefined() || baseStruct.getLength() <= 1))
				baseStruct = null;
		}
		int baseSize = baseStruct == null ? 0 : baseStruct.getLength();

		Long size = best(ci.sizes);
		Long vptr = best(ci.vptrs);
		boolean inheritsVptr = baseStruct != null && hasVptr(baseStruct);

		long end = baseSize;
		if (vptr != null && !inheritsVptr)
			end = Math.max(end, vptr + 4);
		for (Map.Entry<Long, String> m : ci.members.entrySet()) {
			Structure ms = classStruct(m.getValue());
			if (ms != null && !ms.isNotYetDefined() && ms.getLength() > 1)
				end = Math.max(end, m.getKey() + ms.getLength());
		}
		for (Map.Entry<Long, Map<String, Object[]>> e : ci.fields.entrySet()) {
			DataType dt = pick(e.getValue());
			if (dt != null)
				end = Math.max(end, e.getKey() + Math.max(1, dt.getLength()));
		}
		long total = size != null ? size : end;
		if (size != null && end > size)
			note.append("fields past new() size ").append(end).append(" > ").append(size).append("; ");
		if (total <= 0) {
			inProgress.remove(c);
			return false;
		}

		s.deleteAll();
		s.setPackingEnabled(false);
		int have = s.isZeroLength() ? 0 : s.getLength();
		if (have < total)
			s.growStructure((int) (total - have));
		BitSet used = new BitSet();

		if (baseStruct != null && baseSize <= total) {
			put(s, 0, baseStruct, "super_" + base, null, used);
		}
		if (vptr != null && !inheritsVptr && vptr + 4 <= total) {
			put(s, vptr.intValue(), PointerDataType.dataType, "_vptr", "gcc2 vtable pointer", used);
		}
		for (Map.Entry<Long, String> m : ci.members.entrySet()) {
			build(m.getValue());
			Structure ms = classStruct(m.getValue());
			if (ms == null || ms.isNotYetDefined() || ms.getLength() <= 1)
				continue;
			if (m.getKey() + ms.getLength() > total)
				continue;
			put(s, m.getKey().intValue(), ms, "m" + m.getValue() + "_0x"
				+ Long.toHexString(m.getKey()), "constructed in ctor", used);
		}
		int nf = 0;
		for (Map.Entry<Long, Map<String, Object[]>> e : ci.fields.entrySet()) {
			long off = e.getKey();
			DataType dt = pick(e.getValue());
			if (dt == null)
				continue;
			int len = Math.max(1, dt.getLength());
			if (off < baseSize || off + len > total)
				continue;
			if (put(s, (int) off, dt, "field_0x" + Long.toHexString(off), null, used))
				nf++;
		}

		note.insert(0, "size=" + (size != null ? size : "?(" + total + ")") + " base="
			+ (base == null ? "-" : base + (baseStruct == null ? "(unknown)" : ""))
			+ " vptr=" + (vptr == null ? "-" : "0x" + Long.toHexString(vptr)
				+ (inheritsVptr ? "(inherited)" : ""))
			+ " members=" + ci.members.size() + " fields=" + nf + " methods=" + ci.methods + "; ");
		notes.put(c, note.toString());
		inProgress.remove(c);
		done.add(c);
		return true;
	}

	/** Place dt at off unless it overlaps something placed already. */
	private static boolean put(Structure s, int off, DataType dt, String name, String comment,
			BitSet used) {
		int len = Math.max(1, dt.getLength());
		if (used.get(off, off + len).cardinality() > 0)
			return false;
		try {
			s.replaceAtOffset(off, dt, len, name, comment);
			used.set(off, off + len);
			return true;
		} catch (Exception e) {
			return false;
		}
	}

	private static boolean isPlaceholder(Structure s) {
		return s.isNotYetDefined() || s.isZeroLength() || s.getLength() <= 1;
	}

	private static boolean hasVptr(Structure s) {
		for (DataTypeComponent c : s.getDefinedComponents()) {
			String n = c.getFieldName();
			if (n == null)
				continue;
			if (n.startsWith("_vptr"))
				return true;
			if (n.startsWith("super_") && c.getDataType() instanceof Structure
					&& hasVptr((Structure) c.getDataType()))
				return true;
		}
		return false;
	}

	/**
	 * By-value composites from the field collector are only trusted when fully
	 * defined and not one of the classes being rebuilt: Gobe classes are still
	 * 1-byte placeholders during collection, and embedding one at that size
	 * breaks the struct once the class is filled ("field does not fit").
	 * Embedded Gobe objects come from the constructor analysis instead.
	 */
	private boolean usableFieldType(DataType dt) {
		DataType base = dt;
		while (base instanceof TypeDef)
			base = ((TypeDef) base).getBaseDataType();
		while (base instanceof Array)
			base = ((Array) base).getDataType();
		if (!(base instanceof Composite))
			return true;
		if (base.isNotYetDefined() || base.getLength() <= 1)
			return false;
		return !classes.containsKey(base.getName());
	}

	/** Most specific, most frequent usable type seen at one offset. */
	private DataType pick(Map<String, Object[]> byType) {
		DataType best = null;
		int bestScore = -1;
		for (Object[] slot : byType.values()) {
			DataType dt = (DataType) slot[0];
			if (!usableFieldType(dt))
				continue;
			int votes = (Integer) slot[1];
			int spec = specificity(dt);
			int score = spec * 1000 + votes;
			if (score > bestScore) {
				best = dt;
				bestScore = score;
			}
		}
		return best;
	}

	private static int specificity(DataType dt) {
		if (dt instanceof Pointer) {
			DataType to = ((Pointer) dt).getDataType();
			if (to instanceof Structure && !to.isNotYetDefined())
				return 5;
			if (to != null && !(to instanceof Undefined) && !(to instanceof DefaultDataType))
				return 4;
			return 3;
		}
		if (dt instanceof Undefined || dt instanceof DefaultDataType)
			return 0;
		if (dt instanceof AbstractIntegerDataType)
			return 1;
		return 2;
	}

	private void exportStructs(File out) throws Exception {
		if (out.exists())
			out.delete();
		FileDataTypeManager archive = FileDataTypeManager.createFileArchive(out);
		int tx = archive.startTransaction("Gobe app classes");
		int n = 0;
		try {
			for (String c : done) {
				Structure s = classStruct(c);
				if (s != null) {
					archive.resolve(s, DataTypeConflictHandler.REPLACE_HANDLER);
					n++;
				}
			}
		} finally {
			archive.endTransaction(tx, true);
		}
		archive.save();
		archive.close();
		println(currentProgram.getName() + ": saved " + n + " class structs to " + out);
	}

	private void writeSummary() throws Exception {
		String[] args = getScriptArgs();
		if (args.length == 0)
			return;
		File dir = new File(args[0]);
		dir.mkdirs();
		try (PrintWriter w = new PrintWriter(new File(dir,
				currentProgram.getName() + ".classes.tsv"), "UTF-8")) {
			w.println("class\tsummary");
			for (String c : classes.keySet())
				w.println(c + "\t" + notes.getOrDefault(c, "not built"));
		}
	}
}
