// gcc2 passes `this` as the first stack argument, and its mangling does not
// distinguish static from non-static methods, so the demangled signatures
// leave many methods (and all imports from _APP_/libbe) without __thiscall.
// Classify every class method and set the convention:
//
//  - operator new/delete are static; constructors and destructors are members;
//  - imports are static only if listed (with the same parameter count) in
//    known_statics.txt (Be API, from Haiku's headers) or app_statics.txt
//    (statics found in the main app), otherwise members;
//  - a local method in a vtable is a member (gcc2 vtables, `_vt.<class>`, are
//    8-byte entries {offset, function pointer});
//  - a body reading a stack argument beyond the declared arguments (and any
//    hidden struct-return pointer) proves a hidden `this`. The code has no
//    frame pointer, so offsets are [ESP + c] corrected by the stack depth at
//    each instruction (return address at 0, first argument at +4);
//  - a method whose body reads its declared arguments but nothing past them,
//    and has no other evidence, is static.
//
// Call-site stack cleanup is not used: gcc2 often merges the cleanup of
// several calls into one `add esp`.
//
// Re-runnable: statics set to __thiscall earlier are switched back to __cdecl.
// With a script argument (directory) it writes <program>.statics.txt and the
// per-method evidence <program>.thiscall.tsv there.
//@category BeOS

import java.io.File;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import ghidra.app.cmd.function.CallDepthChangeInfo;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.data.Composite;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.TypeDef;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SourceType;

public class FixThiscall extends GhidraScript {
	private static final Pattern ESP_OPERAND =
		Pattern.compile("\\[ESP(?: \\+ 0x([0-9a-fA-F]+))?\\]");

	private Listing listing;
	private final Set<String> knownStatics = new HashSet<>();
	private final AddressSet inVtable = new AddressSet();
	private final StringBuilder evidence = new StringBuilder(
		"name\texternal\treason\targBytes\thidden\tmaxArgOff\t\t\tmember\n");

	@Override
	protected void run() throws Exception {
		listing = currentProgram.getListing();
		File dir = getSourceFile().getParentFile().getFile(false);
		for (String list : new String[] { "known_statics.txt", "app_statics.txt" }) {
			File f = new File(dir, list);
			if (!f.exists())
				continue;
			for (String line : Files.readAllLines(f.toPath())) {
				if (!line.isBlank() && !line.startsWith("#"))
					knownStatics.add(line.trim());
			}
		}

		collectVtableTargets();

		int members = 0, statics = 0, changed = 0;
		List<String> staticNames = new ArrayList<>();
		for (FunctionIterator it : new FunctionIterator[] {
				currentProgram.getFunctionManager().getFunctions(true),
				currentProgram.getFunctionManager().getExternalFunctions() })
		while (it.hasNext() && !monitor.isCancelled()) {
			Function f = it.next();
			Namespace ns = f.getParentNamespace();
			if (ns.isGlobal() || ns.isLibrary() || f.isThunk())
				continue;
			if (f.getName().startsWith("__tf") || f.getName().contains("type_info"))
				continue;
			// struct returns already rewritten with explicit storage
			Parameter[] ps = f.getParameters();
			if (ps.length > 0 && "__return".equals(ps[0].getName()))
				continue;

			boolean member = isMember(f, ns);
			String want = member ? "__thiscall" : "__cdecl";
			if (member)
				members++;
			else {
				statics++;
				staticNames.add(key(f));
			}
			if (!want.equals(f.getCallingConventionName())) {
				f.setCallingConvention(want);
				f.setSignatureSource(SourceType.IMPORTED);
				changed++;
			}
		}

		String[] args = getScriptArgs();
		if (args.length > 0) {
			new File(args[0]).mkdirs();
			try (PrintWriter w = new PrintWriter(new File(args[0],
					currentProgram.getName() + ".statics.txt"), "UTF-8")) {
				for (String s : staticNames)
					w.println(s);
			}
			try (PrintWriter w = new PrintWriter(new File(args[0],
					currentProgram.getName() + ".thiscall.tsv"), "UTF-8")) {
				w.print(evidence);
			}
		}
		println(currentProgram.getName() + ": " + members + " member methods, "
			+ statics + " static, changed " + changed);
	}

	/** "Class::Method<TAB>params", the format of the statics lists. */
	private static String key(Function f) {
		int n = 0;
		for (Parameter p : f.getParameters())
			if (!p.isAutoParameter())
				n++;
		return f.getParentNamespace().getName() + "::" + f.getName() + "\t" + n;
	}

	private boolean isMember(Function f, Namespace ns) throws Exception {
		String name = f.getName();
		if (name.matches("operator[ .](new|delete).*"))
			return log(f, "operator new/delete", false);
		if (name.equals(ns.getName()) || name.startsWith("~"))
			return log(f, "ctor/dtor", true);
		if (isConstMethod(f))
			return log(f, "const method", true);
		if (f.isExternal())
			return log(f, "import list", !knownStatics.contains(key(f)));

		// by-value structs the demangler only knows by name have placeholder
		// sizes, which makes the byte counts meaningless
		boolean sizesKnown = true;
		int argBytes = 0;
		for (Parameter p : f.getParameters()) {
			if (p.isAutoParameter())
				continue;
			DataType dt = p.getDataType();
			while (dt instanceof TypeDef)
				dt = ((TypeDef) dt).getBaseDataType();
			if (dt instanceof Composite && (dt.isNotYetDefined() || dt.getLength() <= 1))
				sizesKnown = false;
			argBytes += (Math.max(p.getLength(), 1) + 3) & ~3;
		}

		// structs are returned through a hidden pointer argument, which the
		// callee pops itself (`ret 4`), so it only shifts the body offsets
		DataType rt = f.getReturnType();
		while (rt instanceof TypeDef)
			rt = ((TypeDef) rt).getBaseDataType();
		int hidden = rt instanceof Composite || hasRet4(f) ? 4 : 0;
		for (Parameter p : f.getParameters())
			if (p.isAutoParameter() && p.getName().startsWith("__return_storage"))
				hidden = 4;

		long maxOff = maxArgOffset(f);
		String reason;
		boolean member;
		if (inVtable.contains(f.getEntryPoint())) {
			reason = "vtable";
			member = true;
		} else if (maxOff >= 4 + hidden + argBytes) {
			reason = "body reads this";
			member = true;
		} else if (!sizesKnown) {
			reason = "sizes unknown";
			member = true;
		} else if (isPostfixIncDec(f)) {
			reason = "postfix ++/--";
			member = true;
		} else if (argBytes > 0 && maxOff >= 4 + hidden + argBytes - 4) {
			// Reads its last declared argument slot and nothing beyond. That is
			// also what a member ignoring its last parameter looks like, so the
			// callers have to agree.
			int[] votes = pushVotes(f, argBytes + hidden);
			if (addressTaken(f)) {
				reason = "args only + address taken";
				member = false;
			} else if (votes[1] > 0 && votes[0] == 0) {
				reason = "args only + call sites";
				member = false;
			} else {
				reason = "args only, callers disagree (" + votes[0] + "/" + votes[1] + ")";
				member = true;
			}
		} else {
			reason = "no evidence";
			member = true;
		}
		evidence.append(f.getName(true)).append('\t').append(false)
			.append('\t').append(reason).append('\t').append(argBytes)
			.append('\t').append(hidden).append('\t').append(maxOff)
			.append("\t\t\t").append(member).append('\n');
		return member;
	}

	/** Mark every function a gcc2 vtable points to. */
	private void collectVtableTargets() throws Exception {
		SymbolIterator syms = currentProgram.getSymbolTable().getAllSymbols(false);
		while (syms.hasNext()) {
			Symbol sym = syms.next();
			String n = sym.getName();
			if (sym.isExternal() || !(n.startsWith("_vt.") || n.startsWith("_vt$")
					|| n.contains("virtual_table") || n.equals("vtable")))
				continue;
			Address a = sym.getAddress();
			// entries are {offset, pointer}; slot 0 is the type_info function
			for (int i = 0; i < 1024; i++) {
				Address slot = a.add(8L * i);
				if (i > 0 && hasLabel(slot))
					break; // next vtable
				int off = getInt(slot);
				Address target = toAddr(getInt(slot.add(4)) & 0xffffffffL);
				Function t = getFunctionAt(target);
				if (off != 0 && (off & 0xffff0000) != 0)
					break;
				if (t == null) {
					if (i == 0 || getInt(slot.add(4)) == 0)
						continue;
					break;
				}
				if (i > 0)
					inVtable.add(target);
			}
		}
	}

	private boolean hasLabel(Address a) {
		for (Symbol s : currentProgram.getSymbolTable().getSymbols(a))
			if (!s.isDynamic())
				return true;
		return false;
	}

	/**
	 * gcc2 marks const member functions with C after the name separator
	 * (DrawAngleEdit__C9TGradientP5BViewf); a static method can't be const.
	 */
	private boolean isConstMethod(Function f) {
		java.util.regex.Pattern p = java.util.regex.Pattern.compile("__C(\\d|Q)");
		if (f.isExternal()) {
			String orig = f.getExternalLocation().getOriginalImportedName();
			return orig != null && p.matcher(orig).find();
		}
		for (Symbol s : currentProgram.getSymbolTable().getSymbols(f.getEntryPoint()))
			if (p.matcher(s.getName()).find())
				return true;
		return false;
	}

	/** `ret 4`: the callee pops a hidden struct-return pointer. */
	private boolean hasRet4(Function f) {
		for (Instruction ins : listing.getInstructions(f.getBody(), true)) {
			if (ins.getMnemonicString().equalsIgnoreCase("RET") && ins.getNumOperands() == 1
					&& ins.getScalar(0) != null && ins.getScalar(0).getValue() == 4)
				return true;
		}
		return false;
	}

	private static boolean isPostfixIncDec(Function f) {
		if (!f.getName().equals("operator++") && !f.getName().equals("operator--"))
			return false;
		int n = 0;
		for (Parameter p : f.getParameters())
			if (!p.isAutoParameter())
				n++;
		return n == 1;
	}

	/** The function's address is used as a value (a callback), not called. */
	private boolean addressTaken(Function f) {
		for (Reference r : getReferencesTo(f.getEntryPoint())) {
			if (r.getReferenceType().isCall() || r.getReferenceType().isJump())
				continue;
			if (listing.getInstructionAt(r.getFromAddress()) != null)
				return true;
		}
		return false;
	}

	/**
	 * At each call site, measure the bytes pushed since the previous call,
	 * stack adjustment or block start, using the caller's stack depths.
	 * Returns {member votes, static votes}: a member call pushes the
	 * arguments plus `this`.
	 */
	private int[] pushVotes(Function f, int argBytes) throws Exception {
		int[] votes = new int[2];
		for (Reference r : getReferencesTo(f.getEntryPoint())) {
			if (!r.getReferenceType().isCall())
				continue;
			Instruction call = listing.getInstructionAt(r.getFromAddress());
			Function caller = getFunctionContaining(r.getFromAddress());
			if (call == null || caller == null)
				continue;
			CallDepthChangeInfo depths = depthInfo(caller);
			Instruction start = call;
			while (true) {
				if (isJumpTarget(start))
					break;
				Instruction p = start.getPrevious();
				if (p == null || !caller.getBody().contains(p.getAddress()))
					break;
				String m = p.getMnemonicString().toUpperCase();
				if (m.startsWith("CALL") || m.startsWith("RET")
						|| p.getFlowType().isJump() || modifiesEspDirectly(p))
					break;
				start = p;
			}
			int d0 = depths.getDepth(start.getAddress());
			int d1 = depths.getDepth(call.getAddress());
			if (d0 == Function.INVALID_STACK_DEPTH_CHANGE
					|| d1 == Function.INVALID_STACK_DEPTH_CHANGE
					|| d0 == Function.UNKNOWN_STACK_DEPTH_CHANGE
					|| d1 == Function.UNKNOWN_STACK_DEPTH_CHANGE)
				continue;
			int pushed = d0 - d1;
			if (pushed == argBytes + 4)
				votes[0]++;
			else if (pushed == argBytes)
				votes[1]++;
		}
		return votes;
	}

	private final java.util.Map<Function, CallDepthChangeInfo> depthCache =
		new java.util.HashMap<>();

	private CallDepthChangeInfo depthInfo(Function f) throws Exception {
		CallDepthChangeInfo d = depthCache.get(f);
		if (d == null) {
			d = new CallDepthChangeInfo(f, monitor);
			depthCache.put(f, d);
		}
		return d;
	}

	private boolean isJumpTarget(Instruction ins) {
		for (Reference r : getReferencesTo(ins.getAddress()))
			if (r.getReferenceType().isJump())
				return true;
		return false;
	}

	/**
	 * add/lea/mov on ESP (cleanup of an earlier call, frame setup). `sub esp`
	 * is not a boundary: gcc2 pushes float/double arguments with
	 * `sub esp, 4/8; fstp [esp]`.
	 */
	private static boolean modifiesEspDirectly(Instruction ins) {
		String m = ins.getMnemonicString().toUpperCase();
		if (!(m.equals("ADD") || m.equals("LEA") || m.equals("MOV")))
			return false;
		return ins.getRegister(0) != null && ins.getRegister(0).getName().equals("ESP");
	}

	private boolean log(Function f, String reason, boolean member) {
		evidence.append(f.getName(true)).append('\t').append(f.isExternal())
			.append('\t').append(reason).append("\t\t\t\t\t\t").append(member).append('\n');
		return member;
	}

	/**
	 * Highest stack offset (from ESP at entry: return address at 0, first
	 * argument at +4) the body reads. The code has no frame pointer, so each
	 * [ESP + c] operand is corrected by the stack depth at that instruction.
	 * Ghidra's own stack references miss many of these accesses.
	 */
	private long maxArgOffset(Function f) throws Exception {
		CallDepthChangeInfo depths = new CallDepthChangeInfo(f, monitor);
		long max = 0;
		for (Instruction ins : listing.getInstructions(f.getBody(), true)) {
			int depth = depths.getDepth(ins.getMinAddress());
			if (depth == Function.INVALID_STACK_DEPTH_CHANGE
					|| depth == Function.UNKNOWN_STACK_DEPTH_CHANGE)
				continue;
			for (int i = 0; i < ins.getNumOperands(); i++) {
				Matcher m = ESP_OPERAND.matcher(ins.getDefaultOperandRepresentation(i));
				if (m.find()) {
					long c = m.group(1) == null ? 0 : Long.parseLong(m.group(1), 16);
					max = Math.max(max, c + depth);
				}
			}
		}
		return max;
	}
}
