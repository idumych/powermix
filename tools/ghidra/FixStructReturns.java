// gcc2 returns classes/structs by value through a hidden pointer: the caller
// passes the address of the result as the first stack argument (before
// `this`), the callee fills it, returns it in EAX and pops it (`ret 4`).
// Mangled names carry no return types, so these functions show `this` as
// the hidden pointer and the real `this` as in_stack_0000000c.
//
// Local functions: any function with a `ret 0x4`. The returned type, in order
// of preference, is the class whose constructor is called on the hidden
// pointer; the return type of another struct-returning function it is passed
// to as the result; the class whose methods are called on it (`this`); or,
// for bodies that fill it inline, the class the callers use the returned
// local as (methods/constructors called on the same stack slot).
// Imports: Be API methods listed in known_structret.txt (from Haiku's headers)
// and main-app functions in app_structret.txt (written by decompile_all.sh).
//
// Each gets an explicit signature with custom storage:
//     X *f(X *__return, Class *this, args...)     (this only for members)
// Run after FixThiscall (member/static decided) and before RecoverClasses.
// Script argument (optional): directory for <program>.structret.txt.
//@category BeOS

import java.io.File;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.util.*;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.listing.Function.FunctionUpdateType;
import ghidra.program.model.pcode.*;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;

public class FixStructReturns extends GhidraScript {
	private final Map<String, String> known = new HashMap<>(); // key -> type name
	private final Map<Function, String> found = new LinkedHashMap<>();
	private final Set<Function> candidates = new HashSet<>();

	@Override
	protected void run() throws Exception {
		File dir = getSourceFile().getParentFile().getFile(false);
		for (String list : new String[] { "known_structret.txt", "app_structret.txt" }) {
			File f = new File(dir, list);
			if (!f.exists())
				continue;
			for (String line : Files.readAllLines(f.toPath())) {
				if (line.isBlank() || line.startsWith("#"))
					continue;
				String[] p = line.split("\t");
				// "?" = returns a struct of unknown type: still fix the signature
				if (p.length >= 3)
					known.putIfAbsent(p[0] + "\t" + p[1], p[2]);
			}
		}

		// local functions with `ret 4`
		FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
		while (it.hasNext()) {
			Function f = it.next();
			if (!f.isThunk() && hasRet4(f) && !alreadyFixed(f))
				candidates.add(f);
		}

		DecompInterface ifc = new DecompInterface();
		ifc.openProgram(currentProgram);
		// two passes: the second can use results of the first (f returns g(...))
		for (int pass = 0; pass < 2; pass++) {
			for (Function f : candidates) {
				if (found.containsKey(f) && found.get(f) != null)
					continue;
				found.put(f, returnedClass(f, ifc));
			}
		}
		ifc.dispose();

		int local = 0, typed = 0, imports = 0;
		for (Map.Entry<Function, String> e : found.entrySet()) {
			apply(e.getKey(), e.getValue(), "__thiscall".equals(e.getKey().getCallingConventionName()));
			local++;
			if (e.getValue() != null)
				typed++;
		}

		// imports
		FunctionIterator ext = currentProgram.getFunctionManager().getExternalFunctions();
		while (ext.hasNext()) {
			Function f = ext.next();
			if (alreadyFixed(f))
				continue;
			String t = known.get(key(f));
			if (t == null)
				continue;
			apply(f, t, "__thiscall".equals(f.getCallingConventionName()));
			imports++;
		}

		String[] args = getScriptArgs();
		if (args.length > 0) {
			new File(args[0]).mkdirs();
			try (PrintWriter w = new PrintWriter(new File(args[0],
					currentProgram.getName() + ".structret.txt"), "UTF-8")) {
				for (Map.Entry<Function, String> e : found.entrySet())
					if (!e.getKey().getParentNamespace().isGlobal())
						w.println(key(e.getKey()) + "\t"
							+ (e.getValue() == null ? "?" : e.getValue()));
			}
		}
		println(currentProgram.getName() + ": " + local + " local struct returns ("
			+ typed + " with known type), " + imports + " imports");
	}

	private static boolean alreadyFixed(Function f) {
		Parameter[] ps = f.getParameters();
		return ps.length > 0 && "__return".equals(ps[0].getName());
	}

	private boolean hasRet4(Function f) {
		for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
			if (ins.getMnemonicString().equalsIgnoreCase("RET") && ins.getNumOperands() == 1
					&& ins.getScalar(0) != null && ins.getScalar(0).getValue() == 4)
				return true;
		}
		return false;
	}

	/** "Class::Method<TAB>declared params", the key format of the lists. */
	private static String key(Function f) {
		int n = 0;
		for (Parameter p : f.getParameters())
			if (!p.isAutoParameter() && !"__return".equals(p.getName())
					&& !"this".equals(p.getName()))
				n++;
		return f.getParentNamespace().getName() + "::" + f.getName() + "\t" + n;
	}

	/** Class constructed in (or struct returned into) the hidden pointer. */
	private String returnedClass(Function f, DecompInterface ifc) {
		HighFunction hf = decompile(f, ifc);
		if (hf == null)
			return null;
		LocalSymbolMap lsm = hf.getLocalSymbolMap();
		if (lsm.getNumParams() == 0)
			return null;
		HighVariable ret = lsm.getParamSymbol(0).getHighVariable();
		if (ret == null)
			return null;
		Map<String, Integer> votes = new HashMap<>();
		String viaCall = classUses(hf, v -> v.getHigh() == ret, votes);
		if (viaCall != null)
			return viaCall;
		if (!votes.isEmpty())
			return best(votes);
		String c = fromCallers(f, ifc);
		return c != null ? c : byShape(f, hf, ret);
	}

	/**
	 * Last resort for bodies that fill the result inline: stores at exactly
	 * 0/4/8/12 in a Rect/Frame/Bounds function -> BRect; 0/4 in a
	 * Point/Center/Origin function -> BPoint.
	 */
	private String byShape(Function f, HighFunction hf, HighVariable ret) {
		Set<Long> offs = new TreeSet<>();
		Iterator<PcodeOpAST> ops = hf.getPcodeOps();
		while (ops.hasNext()) {
			PcodeOpAST op = ops.next();
			if (op.getOpcode() != PcodeOp.STORE)
				continue;
			long off = offsetFrom(op.getInput(1), ret);
			if (off < 0)
				continue;
			if (op.getInput(2).getSize() != 4)
				return null;
			offs.add(off);
		}
		String n = f.getName().toLowerCase();
		if (offs.equals(new TreeSet<>(List.of(0L, 4L, 8L, 12L)))
				&& n.matches(".*(rect|frame|bounds|box).*"))
			return "BRect";
		if (offs.equals(new TreeSet<>(List.of(0L, 4L)))
				&& n.matches(".*(point|center|origin|lefttop|pos).*"))
			return "BPoint";
		return null;
	}

	private static long offsetFrom(Varnode v, HighVariable base) {
		long off = 0;
		for (int i = 0; i < 6 && v != null; i++) {
			if (v.getHigh() == base)
				return off;
			PcodeOp d = v.getDef();
			if (d == null)
				return -1;
			int oc = d.getOpcode();
			if (oc == PcodeOp.COPY || oc == PcodeOp.CAST)
				v = d.getInput(0);
			else if ((oc == PcodeOp.INT_ADD || oc == PcodeOp.PTRSUB) && d.getInput(1).isConstant()) {
				off += d.getInput(1).getOffset();
				v = d.getInput(0);
			} else if (oc == PcodeOp.PTRADD && d.getInput(1).isConstant() && d.getInput(2).isConstant()) {
				off += d.getInput(1).getOffset() * d.getInput(2).getOffset();
				v = d.getInput(0);
			} else
				return -1;
		}
		return -1;
	}

	/**
	 * Look at calls whose first argument satisfies isTarget: a constructor
	 * call returns its class at once; a struct-returning callee (the target is
	 * its result) or a method of X (the target is its `this`) adds a vote.
	 */
	private String classUses(HighFunction hf, java.util.function.Predicate<Varnode> isTarget,
			Map<String, Integer> votes) {
		Iterator<PcodeOpAST> ops = hf.getPcodeOps();
		while (ops.hasNext()) {
			PcodeOpAST op = ops.next();
			if (op.getOpcode() != PcodeOp.CALL || op.getNumInputs() < 2)
				continue;
			Varnode a = unwrap(op.getInput(1));
			if (!isTarget.test(a))
				continue;
			Function callee = getFunctionAt(op.getInput(0).getAddress());
			if (callee == null)
				continue;
			Namespace ns = callee.getParentNamespace();
			if (!ns.isGlobal() && callee.getName().equals(ns.getName()))
				return ns.getName(); // constructor
			String t = null;
			if (alreadyFixed(callee) || found.containsKey(callee)) {
				t = found.get(callee);
				if (t == null && alreadyFixed(callee))
					t = typeNameOfReturn(callee);
			} else if (callee.isExternal() && known.containsKey(key(callee)))
				t = known.get(key(callee));
			else if (candidates.contains(callee))
				t = null; // struct-returning: the target is its result, not its `this`
			else if (!ns.isGlobal() && "__thiscall".equals(callee.getCallingConventionName()))
				t = ns.getName();
			if (t != null && !t.equals("?") && !polymorphic(t))
				votes.merge(t, 1, Integer::sum);
		}
		return null;
	}

	/** Classes with a vtable pointer (BView, BWindow, ...) aren't returned by value. */
	private boolean polymorphic(String name) {
		DataType dt = structFor(name);
		return dt instanceof Structure && hasVptr((Structure) dt, 0);
	}

	private static boolean hasVptr(Structure s, int depth) {
		if (depth > 16)
			return false;
		for (DataTypeComponent c : s.getDefinedComponents()) {
			String n = c.getFieldName();
			if (n == null)
				continue;
			if (n.startsWith("_vptr"))
				return true;
			if (n.startsWith("super_") && c.getDataType() instanceof Structure
					&& hasVptr((Structure) c.getDataType(), depth + 1))
				return true;
		}
		return false;
	}

	/** Votes from callers: how they use the stack slot they pass as result. */
	private String fromCallers(Function f, DecompInterface ifc) {
		Map<String, Integer> votes = new HashMap<>();
		int callers = 0;
		for (ghidra.program.model.symbol.Reference r : getReferencesTo(f.getEntryPoint())) {
			if (!r.getReferenceType().isCall() || ++callers > 8)
				continue;
			Function caller = getFunctionContaining(r.getFromAddress());
			if (caller == null || caller == f)
				continue;
			HighFunction hf = decompile(caller, ifc);
			if (hf == null)
				continue;
			Iterator<PcodeOpAST> ops = hf.getPcodeOps(r.getFromAddress());
			while (ops.hasNext()) {
				PcodeOpAST op = ops.next();
				if (op.getOpcode() != PcodeOp.CALL || op.getNumInputs() < 2)
					continue;
				Long slot = stackSlot(unwrap(op.getInput(1)));
				if (slot == null)
					continue;
				Map<String, Integer> v = new HashMap<>();
				// the call to f itself adds nothing: f's type is what we look for
				String ctor = classUses(hf, x -> slot.equals(stackSlot(x)), v);
				if (ctor != null)
					votes.merge(ctor, 3, Integer::sum);
				for (Map.Entry<String, Integer> e : v.entrySet())
					votes.merge(e.getKey(), e.getValue(), Integer::sum);
			}
		}
		return votes.isEmpty() ? null : best(votes);
	}

	/** Stack offset of an address like &local (PTRSUB(sp, c)), or null. */
	private static Long stackSlot(Varnode v) {
		if (v == null)
			return null;
		PcodeOp d = v.getDef();
		if (d != null && (d.getOpcode() == PcodeOp.PTRSUB || d.getOpcode() == PcodeOp.INT_ADD)
				&& d.getInput(0).isRegister() && d.getInput(1).isConstant())
			return d.getInput(1).getOffset();
		HighVariable h = v.getHigh();
		if (h instanceof HighLocal && h.getRepresentative() != null
				&& h.getRepresentative().getAddress().isStackAddress())
			return h.getRepresentative().getAddress().getOffset();
		return null;
	}

	private static Varnode unwrap(Varnode a) {
		while (a.getDef() != null && (a.getDef().getOpcode() == PcodeOp.CAST
				|| a.getDef().getOpcode() == PcodeOp.COPY))
			a = a.getDef().getInput(0);
		return a;
	}

	private static String typeNameOfReturn(Function f) {
		DataType rt = f.getReturnType();
		if (rt instanceof Pointer && ((Pointer) rt).getDataType() != null
				&& !(((Pointer) rt).getDataType() instanceof DefaultDataType))
			return ((Pointer) rt).getDataType().getName();
		return null;
	}

	private static String best(Map<String, Integer> votes) {
		String b = null;
		int n = 0;
		for (Map.Entry<String, Integer> e : votes.entrySet())
			if (e.getValue() > n) {
				b = e.getKey();
				n = e.getValue();
			}
		return b;
	}

	private final Map<Function, HighFunction> cache = new HashMap<>();

	private HighFunction decompile(Function f, DecompInterface ifc) {
		if (cache.containsKey(f))
			return cache.get(f);
		DecompileResults r = ifc.decompileFunction(f, 60, monitor);
		HighFunction hf = r == null ? null : r.getHighFunction();
		if (cache.size() > 2000)
			cache.clear();
		cache.put(f, hf);
		return hf;
	}

	private DataType structFor(String name) {
		if (name == null || name.equals("?"))
			return null;
		List<DataType> l = new ArrayList<>();
		currentProgram.getDataTypeManager().findDataTypes(name, l);
		DataType best = null;
		for (DataType dt : l) {
			if (!(dt instanceof Composite) && !(dt instanceof TypeDef))
				continue;
			String p = dt.getCategoryPath().getPath();
			if (best == null || p.startsWith("/BeAPI") || p.startsWith("/Demangler"))
				best = dt;
		}
		return best;
	}

	private void apply(Function f, String typeName, boolean member) throws Exception {
		Program prog = currentProgram;
		DataType target = structFor(typeName);
		DataType retPtr = target != null ? new PointerDataType(target) : PointerDataType.dataType;

		DataType thisType = null;
		List<Parameter> declared = new ArrayList<>();
		for (Parameter p : f.getParameters()) {
			if (p.isAutoParameter()) {
				if ("this".equals(p.getName()))
					thisType = p.getDataType();
				continue;
			}
			declared.add(p);
		}
		if (member && thisType == null) {
			DataType cls = structFor(f.getParentNamespace().getName());
			thisType = cls != null ? new PointerDataType(cls) : PointerDataType.dataType;
		}

		List<Parameter> params = new ArrayList<>();
		int off = 4;
		params.add(new ParameterImpl("__return", retPtr, new VariableStorage(prog, off, 4), prog));
		off += 4;
		if (member) {
			params.add(new ParameterImpl("this", thisType, new VariableStorage(prog, off, 4), prog));
			off += 4;
		}
		for (Parameter p : declared) {
			// storage matches the type (a bool uses 1 byte of its 4-byte slot)
			int size = Math.max(1, p.getDataType().getLength());
			params.add(new ParameterImpl(p.getName(), p.getDataType(),
				new VariableStorage(prog, off, size), prog));
			off += (size + 3) & ~3;
		}
		Register eax = prog.getRegister("EAX");
		ReturnParameterImpl rp = new ReturnParameterImpl(retPtr,
			new VariableStorage(prog, eax), prog);
		f.updateFunction("__cdecl", rp, params, FunctionUpdateType.CUSTOM_STORAGE, true,
			SourceType.ANALYSIS);
	}
}
