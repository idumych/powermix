// Give Gobe's classes typed vtables so virtual calls decompile as
// this->_vptr->Method(...) instead of raw pointer arithmetic.
//
// gcc2 (without -fvtable-thunks) vtable entries are 8 bytes:
//     short delta;   // added to `this` before the call
//     short index;
//     void *pfn;
// Slot 0 holds the type_info function. For each `<Class>_virtual_table`
// symbol this builds /GobeVtables/<Class>_vtbl with, per slot,
// <Method>_delta, <Method>_index and <Method> (a pointer to a function
// definition made from the target's signature), then types the `_vptr`
// field of the class struct (where the class introduces it) with it.
// Pure virtual slots (__pure_virtual) are named after the method a derived
// class puts in the same slot.
//
// Run after RecoverClasses (class structs and their _vptr fields exist).
// Classes whose vtable pointer lives in a Be API base (BArchivable etc.) are
// not changed: that field is shared by every Be class.
//
// Most virtual calls go through untyped values (`*(int *)(x + 8) + 0x134`),
// which no struct type reaches. So every indirect call `call [reg + d]`
// whose reg was loaded from `[obj + k]` also gets a PRE comment naming the
// methods in slot (d - 4) / 8 of all classes whose vtable pointer is at k,
// e.g. "vcall slot 38: Draw (57 classes)".
//@category BeOS

import java.util.*;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class TypeVtables extends GhidraScript {
	private static final String SUFFIX = "_virtual_table";
	private static final CategoryPath CAT = new CategoryPath("/GobeVtables");

	static class Slot {
		short delta, index;
		Function target; // null for 0 / unknown
		boolean pure;
	}

	private DataTypeManager dtm;

	@Override
	protected void run() throws Exception {
		dtm = currentProgram.getDataTypeManager();

		Map<String, List<Slot>> tables = new TreeMap<>();
		SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(false);
		while (it.hasNext()) {
			Symbol s = it.next();
			String n = s.getName();
			if (!n.endsWith(SUFFIX) || s.isExternal() || !s.isPrimary())
				continue;
			String cls = n.substring(0, n.length() - SUFFIX.length());
			if (classStruct(cls) == null)
				continue; // secondary vtables (Class_Base_virtual_table) etc.
			tables.put(cls, readTable(s.getAddress()));
		}

		int built = 0, typed = 0;
		for (Map.Entry<String, List<Slot>> e : tables.entrySet()) {
			String cls = e.getKey();
			Structure vt = buildVtbl(cls, e.getValue(), tables);
			built++;
			if (typeVptr(cls, vt))
				typed++;
		}
		int commented = commentVirtualCalls(tables);
		println(currentProgram.getName() + ": " + built + " vtable structs, " + typed
			+ " _vptr fields typed, " + commented + " virtual calls annotated");
	}

	// ---- call-site annotations -------------------------------------------

	private int commentVirtualCalls(Map<String, List<Slot>> tables) throws Exception {
		// vptr offset -> slot -> method name -> classes
		Map<Integer, Map<Integer, Map<String, Integer>>> bySlot = new HashMap<>();
		for (Map.Entry<String, List<Slot>> e : tables.entrySet()) {
			Structure st = classStruct(e.getKey());
			int vp = st == null ? -1 : vptrOffset(st, 0, 0);
			if (vp < 0)
				continue;
			List<Slot> slots = e.getValue();
			for (int i = 1; i < slots.size(); i++) {
				Slot sl = slots.get(i);
				if (sl.target == null || sl.pure)
					continue;
				String n = sl.target.getName().startsWith("~") ? "~dtor" : sl.target.getName();
				bySlot.computeIfAbsent(vp, k -> new HashMap<>())
					.computeIfAbsent(i, k -> new HashMap<>()).merge(n, 1, Integer::sum);
			}
		}

		int n = 0;
		ghidra.program.model.listing.Listing listing = currentProgram.getListing();
		for (ghidra.program.model.listing.Instruction ins : listing.getInstructions(true)) {
			if (!ins.getFlowType().isCall() || !ins.getFlowType().isComputed())
				continue;
			// gcc2: mov v,[obj+k]; mov r,[v+d]; call r   (or call [v+d])
			String op = ins.getDefaultOperandRepresentation(0);
			ghidra.program.model.listing.Instruction from = ins;
			java.util.regex.Matcher m = MEM.matcher(op);
			if (!m.matches()) {
				if (!op.matches("E[A-Z]{2}"))
					continue;
				ghidra.program.model.listing.Instruction def = definingMov(ins, op);
				if (def == null)
					continue;
				m = MEM.matcher(def.getDefaultOperandRepresentation(1));
				if (!m.matches())
					continue;
				from = def;
			}
			String reg = m.group(1);
			// negative displacements print as 0xffffff60: never a vtable slot
			long disp = m.group(2) == null ? 0 : Long.parseLong(m.group(2), 16);
			if (disp > 0x7fff || (disp - 4) % 8 != 0 || disp < 12)
				continue;
			int slot = (int) ((disp - 4) / 8);
			ghidra.program.model.listing.Instruction vload = definingMov(from, reg);
			if (vload == null)
				continue;
			java.util.regex.Matcher lm = MEM.matcher(vload.getDefaultOperandRepresentation(1));
			if (!lm.matches())
				continue;
			long vpl = lm.group(2) == null ? 0 : Long.parseLong(lm.group(2), 16);
			if (vpl > 0xffff)
				continue;
			Integer vp = (int) vpl;
			Map<String, Integer> names = bySlot.getOrDefault(vp, Collections.emptyMap())
				.get(slot);
			if (names == null || names.isEmpty())
				continue;
			List<Map.Entry<String, Integer>> l = new ArrayList<>(names.entrySet());
			l.sort((a, b) -> b.getValue() - a.getValue());
			StringBuilder c = new StringBuilder("vcall slot " + slot + ": ");
			for (int i = 0; i < Math.min(4, l.size()); i++) {
				if (i > 0)
					c.append(" | ");
				c.append(l.get(i).getKey()).append(" (").append(l.get(i).getValue())
					.append(l.get(i).getValue() == 1 ? " class)" : " classes)");
			}
			if (l.size() > 4)
				c.append(" | +").append(l.size() - 4).append(" more");
			listing.setComment(ins.getAddress(), ghidra.program.model.listing.CodeUnit.PRE_COMMENT,
				c.toString());
			n++;
		}
		return n;
	}

	private static final java.util.regex.Pattern MEM = java.util.regex.Pattern.compile(
		"dword ptr \\[(E[A-Z]{2})(?: \\+ 0x([0-9a-fA-F]+))?\\]");

	/** The `mov reg, ...` that last set reg before ins in the same block. */
	private static ghidra.program.model.listing.Instruction definingMov(
			ghidra.program.model.listing.Instruction ins, String reg) {
		ghidra.program.model.listing.Instruction p = ins.getPrevious();
		for (int i = 0; i < 16 && p != null; i++, p = p.getPrevious()) {
			if (p.getFlowType().isCall() || p.getFlowType().isJump())
				return null;
			if (p.getNumOperands() < 2 || p.getRegister(0) == null
					|| !p.getRegister(0).getName().equals(reg))
				continue;
			return p.getMnemonicString().equalsIgnoreCase("MOV") ? p : null;
		}
		return null;
	}

	/** Offset of the vtable pointer, following super_ fields; -1 if none. */
	private static int vptrOffset(Structure s, int base, int depth) {
		if (depth > 16)
			return -1;
		for (DataTypeComponent c : s.getDefinedComponents()) {
			String n = c.getFieldName();
			if (n == null)
				continue;
			if (n.startsWith("_vptr"))
				return base + c.getOffset();
			if (n.startsWith("super_") && c.getDataType() instanceof Structure) {
				int v = vptrOffset((Structure) c.getDataType(), base + c.getOffset(), depth + 1);
				if (v >= 0)
					return v;
			}
		}
		return -1;
	}

	private List<Slot> readTable(Address a) throws Exception {
		List<Slot> slots = new ArrayList<>();
		for (int i = 0; i < 1024; i++) {
			Address p = a.add(8L * i);
			if (i > 0 && hasOtherLabel(p))
				break;
			Slot s = new Slot();
			s.delta = getShort(p);
			s.index = getShort(p.add(2));
			long pfn = getInt(p.add(4)) & 0xffffffffL;
			if (pfn != 0) {
				Function f = getFunctionAt(toAddr(pfn));
				if (f == null) {
					Symbol sym = getSymbolAt(toAddr(pfn));
					if (sym == null)
						break; // not a vtable entry any more
				}
				s.target = f;
				s.pure = f != null && f.getName().contains("pure_virtual");
			} else if (i > 0) {
				break;
			}
			slots.add(s);
		}
		return slots;
	}

	private boolean hasOtherLabel(Address a) {
		for (Symbol s : currentProgram.getSymbolTable().getSymbols(a))
			if (!s.isDynamic())
				return true;
		return false;
	}

	private Structure buildVtbl(String cls, List<Slot> slots, Map<String, List<Slot>> tables) {
		StructureDataType s = new StructureDataType(CAT, cls + "_vtbl", slots.size() * 8, dtm);
		s.setPackingEnabled(false);
		Set<String> used = new HashSet<>();
		for (int i = 0; i < slots.size(); i++) {
			Slot sl = slots.get(i);
			String name;
			Function sig = sl.target;
			if (i == 0)
				name = "type_info";
			else if (sl.target == null)
				name = "slot_" + i;
			else if (sl.pure) {
				Function derived = derivedTarget(cls, i, tables);
				name = derived != null ? clean(derived.getName()) : "pure_" + i;
				sig = derived;
			} else
				name = clean(sl.target.getName());
			if (!used.add(name)) {
				name = name + "_" + i;
				used.add(name);
			}
			DataType pfn = PointerDataType.dataType;
			if (sig != null && i > 0) {
				FunctionDefinitionDataType fd = new FunctionDefinitionDataType(sig, false);
				try {
					fd.setNameAndCategory(new CategoryPath(CAT, "sig", cls), name);
				} catch (Exception ex) {
					// keep the default name
				}
				pfn = new PointerDataType(fd);
			}
			s.replaceAtOffset(i * 8, ShortDataType.dataType, 2, name + "_delta", null);
			s.replaceAtOffset(i * 8 + 2, ShortDataType.dataType, 2, name + "_index", null);
			s.replaceAtOffset(i * 8 + 4, pfn, 4, name,
				sl.target == null ? null : sl.target.getName(true));
		}
		return (Structure) dtm.resolve(s, DataTypeConflictHandler.REPLACE_HANDLER);
	}

	/** The function a class derived from cls puts in slot i (for pure slots). */
	private Function derivedTarget(String cls, int i, Map<String, List<Slot>> tables) {
		for (Map.Entry<String, List<Slot>> e : tables.entrySet()) {
			if (e.getKey().equals(cls) || e.getValue().size() <= i)
				continue;
			Slot s = e.getValue().get(i);
			if (s.target == null || s.pure)
				continue;
			Structure st = classStruct(e.getKey());
			if (st != null && derivesFrom(st, cls, 0))
				return s.target;
		}
		return null;
	}

	private static boolean derivesFrom(Structure s, String base, int depth) {
		if (depth > 16)
			return false;
		for (DataTypeComponent c : s.getDefinedComponents()) {
			String n = c.getFieldName();
			if (n == null || !n.startsWith("super_") || !(c.getDataType() instanceof Structure))
				continue;
			Structure b = (Structure) c.getDataType();
			if (b.getName().equals(base) || derivesFrom(b, base, depth + 1))
				return true;
		}
		return false;
	}

	/** Type the _vptr field the class introduces itself. */
	private boolean typeVptr(String cls, Structure vt) {
		Structure s = classStruct(cls);
		if (s == null)
			return false;
		for (DataTypeComponent c : s.getDefinedComponents()) {
			if ("_vptr".equals(c.getFieldName())) {
				s.replaceAtOffset(c.getOffset(), new PointerDataType(vt), 4, "_vptr",
					"gcc2 vtable pointer");
				return true;
			}
		}
		return false;
	}

	private Structure classStruct(String c) {
		List<DataType> found = new ArrayList<>();
		dtm.findDataTypes(c, found);
		for (DataType dt : found) {
			if (dt instanceof Structure && !dt.getCategoryPath().getPath().startsWith("/BeAPI")
					&& !dt.isNotYetDefined() && dt.getLength() > 1)
				return (Structure) dt;
		}
		return null;
	}

	private static String clean(String name) {
		if (name.startsWith("~"))
			return "dtor";
		String s = name.replaceAll("[^A-Za-z0-9_]", "_");
		return s.isEmpty() ? "slot" : s;
	}
}
