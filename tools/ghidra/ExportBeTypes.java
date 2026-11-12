// Convert the Be API types imported from DWARF (tools/betypes: Haiku's public
// headers compiled for i586-pc-haiku with clang) to the gcc2 / BeOS R5 class
// layout and save them as a Ghidra data type archive.
//
// clang (Itanium ABI) puts a class's vtable pointer at offset 0. gcc2 puts it
// after the data members of the first class in the hierarchy that has virtual
// functions; derived classes share it. Base class sizes don't change, so only
// the classes that introduce a vtable pointer need fixing: the pointer moves
// from the front to after the class's own members, which shift down by 4.
//
// Itanium also lays out the first polymorphic base first even when a
// non-polymorphic base is declared before it; gcc2 keeps declaration order.
// DWARF offsets no longer show the declaration order, so classes with more
// than one base are listed for review.
//
// Script argument: output .gdt path.
//@category BeOS

import java.io.File;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;

public class ExportBeTypes extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File out = new File(args.length > 0 ? args[0] : "BeAPI_gcc2.gdt");
		DataTypeManager dtm = currentProgram.getDataTypeManager();

		int moved = 0;
		List<String> multiBase = new ArrayList<>();
		List<Structure> all = new ArrayList<>();
		Iterator<Structure> it = dtm.getAllStructures();
		while (it.hasNext())
			all.add(it.next());

		for (Structure s : all) {
			int bases = 0;
			for (DataTypeComponent c : s.getDefinedComponents())
				if (c.getFieldName() != null && c.getFieldName().startsWith("super_"))
					bases++;
			if (bases > 1)
				multiBase.add(s.getName());
			if (moveVptr(s))
				moved++;
		}

		// /DWARF/<header>.h/... -> /BeAPI/<header>.h/...
		Category dwarf = dtm.getCategory(new CategoryPath("/DWARF"));
		if (dwarf != null)
			dwarf.setName("BeAPI");

		if (out.exists())
			out.delete();
		FileDataTypeManager archive = FileDataTypeManager.createFileArchive(out);
		int tx = archive.startTransaction("Be API gcc2 types");
		int copied = 0;
		try {
			Category be = dtm.getCategory(new CategoryPath("/BeAPI"));
			for (Category cat : allCategories(be)) {
				for (DataType dt : cat.getDataTypes()) {
					archive.resolve(dt, DataTypeConflictHandler.REPLACE_HANDLER);
					copied++;
				}
			}
		} finally {
			archive.endTransaction(tx, true);
		}
		archive.save();
		archive.close();

		println("moved vptr in " + moved + " classes; exported " + copied
			+ " types to " + out);
		println("classes with several bases (check base order): " + multiBase);
	}

	/** Move a leading _vptr$X field after the class's own members. */
	private boolean moveVptr(Structure s) {
		DataTypeComponent vptr = null;
		for (DataTypeComponent c : s.getDefinedComponents()) {
			if (c.getFieldName() != null && c.getFieldName().startsWith("_vptr"))
				vptr = c;
		}
		if (vptr == null || vptr.getOffset() != 0)
			return false;

		// Itanium: [vptr][bases][own]. gcc2: [bases][own][vptr]. A class that
		// introduces the pointer has no polymorphic bases, so its non-virtual
		// bases and members all shift down by its size.
		int size = s.getLength();
		int vsize = vptr.getLength();
		List<DataTypeComponent> rest = new ArrayList<>();
		for (DataTypeComponent c : s.getDefinedComponents())
			if (c != vptr)
				rest.add(c);
		DataType vtype = vptr.getDataType();
		String vname = vptr.getFieldName();

		int end = 0;
		for (DataTypeComponent c : rest)
			end = Math.max(end, c.getOffset() - vsize + c.getLength());
		int vOff = (end + 3) & ~3;

		List<Object[]> fields = new ArrayList<>();
		for (DataTypeComponent c : rest) {
			if (c.isBitFieldComponent()) {
				println("WARNING: bitfields in " + s.getName() + ", left in clang layout");
				return false;
			}
			fields.add(new Object[] { c.getOffset() - vsize, c.getDataType(),
				c.getFieldName(), c.getComment() });
		}

		s.deleteAll();
		s.setPackingEnabled(false);
		s.growStructure(size);
		for (Object[] f : fields)
			s.replaceAtOffset((Integer) f[0], (DataType) f[1], -1, (String) f[2],
				(String) f[3]);
		s.replaceAtOffset(vOff, vtype, vsize, vname, "gcc2 vtable pointer");
		if (s.getLength() > size)
			println("WARNING: " + s.getName() + " grew from " + size + " to " + s.getLength());
		return true;
	}

	private static List<Category> allCategories(Category root) {
		List<Category> res = new ArrayList<>();
		if (root == null)
			return res;
		res.add(root);
		for (Category c : root.getCategories())
			res.addAll(allCategories(c));
		return res;
	}
}
